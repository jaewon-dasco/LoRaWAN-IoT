/*
 * MiLoRa.c
 *
 *  Created on: Nov 29, 2024
 *      Author: JONE
 */
#include "Mi_IoT.h"
#include "Mi_Serial.h"
#include "Mi_Main.h"
#include "Mi_LoRa.h"
#include "Mi_Storage.h"
#include "ONE_Serial.h"

#define LORA_PAYLOAD_SIZE				51
#define MILORA_JOIN_STABILIZE_MS		10000	// Join 후 라디오 link 안정화 대기 (첫 송신 손실 방지)
/* 데이터 타입별 분할 상수와 MiLoRa_Encode 본문은 디바이스별 Mi_LoRa_<MODEL>.c 로 이동 */

UART_HandleTypeDef *pLoRaUART = NULL;
RAK3172_t *pLoRaDevice = &RAK3172Dev;
IoT_Mailbox_t MiIoT_LoRaMailBox;
oDateAndTime_t MiLoRa_StatusUpdateTime;
uint8_t MiLoRa_OpenStep = 0;

uint8_t MiLoRa_IsPowerOn;
uint8_t MiLoRa_IsOpen;
uint8_t MiLoRa_IsOkay;
uint8_t MiLoRa_IsBusy;
uint8_t MiLoRa_IsMailBoxReady;
uint8_t MiLoRa_IsTimeSynced;
uint8_t MiLoRa_IsReachable;
uint8_t MiLoRa_IsSleep;

uint32_t MiLoRa_RadioFailCount = 0;
uint32_t MiLoRa_OpenFailCount = 0;

uint32_t MiLoRa_OpenFailTimestamp = 0;
uint32_t MiLoRa_TransmitTimestamp = 0;
uint32_t MiLoRa_ReceiveTimestamp = 0;
uint32_t MiLoRa_OpenTimestamp = 0;
uint32_t MiLoRa_CloseTimestamp = 0;

/* LoRaWAN 다운링크 수신 콜백 - 서버에서 디바이스로 설정 변경 명령 처리
 * fPort 201: 운영 설정 변경 [OpMode(1) | SamplingTime(2) | UpdateTime(2)] = 5bytes
 * 변경 발생 시 MiStorage_ParameterSaveCmd로 플래시 저장 트리거 */
void DownlinkCallback(uint8_t fPort, uint8_t *Payload, uint32_t SizeOfPayload)
{
	uint8_t IsChanged = 0;

	switch(fPort)
	{
		case 201: //OperatingConfig
			if(SizeOfPayload == 5){
				uint8_t OpMode = Payload[0];
				uint16_t SamplingTime = (uint16_t)Payload[1] | (uint16_t)Payload[2] << 8;	// Little-endian
				uint16_t UpdateTime = (uint16_t)Payload[3] | (uint16_t)Payload[4] << 8;

				if(OpMode == IoTOperatingMode_Operating || OpMode == IoTOperatingMode_PreOperation){
					IsChanged |= MiIoT_Parameter.Operating.OperatingMode != OpMode;
					MiIoT_Parameter.Operating.OperatingMode = OpMode;
				}

				if(SamplingTime != 0){
					SamplingTime = MATH_MAX(SamplingTime, 2); //최소 값 2분

					IsChanged |= MiIoT_Parameter.Operating.SamplingInterval != SamplingTime;
					MiIoT_Parameter.Operating.SamplingInterval = SamplingTime;
				}

				if(UpdateTime != 0){
					UpdateTime = MATH_LIMIT(UpdateTime, 60, 1440); //60분(1시간) ~ 1440분(24시간)

					IsChanged |= MiIoT_Parameter.Operating.UpdateInterval != UpdateTime;
					MiIoT_Parameter.Operating.UpdateInterval = UpdateTime;
				}
			}
			break;
	}

	if(IsChanged){
		MiStorage_ParameterSaveCmd = 1;	// 파라미터 변경 → NAND/Flash 저장 요청
	}
}

/* LoRaWAN OTAA 키 생성 - MCU 고유 ID(UID_BASE) 기반
 * AppEUI: "06000000" + ProductCode (8자리 hex)
 * AppKEY: UID 12바이트 + UID 반전 4바이트 → 16바이트 HEX 문자열 */
oResult_t MiLoRa_GenLoRaWANKey()
{
	uint8_t key;

	memset(pLoRaDevice->Information.AppEUI, 0, sizeof(pLoRaDevice->Information.AppEUI));
	memset(pLoRaDevice->Information.AppKEY, 0, sizeof(pLoRaDevice->Information.AppKEY));

	snprintf(pLoRaDevice->Information.AppEUI, sizeof(pLoRaDevice->Information.AppEUI), "06000000%08X", (unsigned int)MiIoT_Parameter.Information.ProductCode);

    // AppKey 생성 (16 bytes): [0-11] UID 원본, [12-15] UID 비트 반전
    for (int i = 0; i < 16; i++) {
    	if (i < 12) {
    		key = *((uint8_t *)(UID_BASE+(i%12)));
    	}
    	else{
    		key = ~*((uint8_t *)(UID_BASE+(i%12)));
    	}

		oHexStringFromBytes(&pLoRaDevice->Information.AppKEY[i*2], &key, 1);
    }

	return RESULT_OK;
}


/*
 * MiLoRa_SendMailbox - 메일박스 아이템을 LoRa로 전송하는 비동기 상태머신
 *
 * 동작 흐름:
 *   Step 0: 메일 상태 초기화, 시퀀스 카운터 리셋
 *   Step 1: MiLoRa_Encode()로 시퀀스별 페이로드 인코딩
 *           - 인코딩 데이터 있음 → Step 2 (전송 대기)
 *           - 인코딩 데이터 없음 + 이전 시퀀스 성공 → RESULT_OK (전체 전송 완료)
 *           - 인코딩 데이터 없음 + 첫 시퀀스 → RESULT_ERROR (인코딩 실패)
 *   Step 2: 시퀀스 간 5초 간격 대기 (LoRa duty cycle 준수)
 *   Step 3: RAK3172_Transmit()으로 실제 RF 전송
 *           - 성공 → Step 1로 복귀 (다음 시퀀스)
 *           - 실패 → QoS 횟수만큼 재시도 후 다음 시퀀스로 이동
 *           - Fault 10회 → RESULT_FAULT (라디오 하드웨어 이상)
 *
 * 반환값:
 *   RESULT_NULL   - pMail이 NULL이거나 DLC=0 (스케줄러에서 메일 제거 필요)
 *   RESULT_PAUSE  - LoRa 미연결 또는 미Join (스케줄러에서 대기)
 *   RESULT_RUN    - 전송 진행 중
 *   RESULT_OK     - 모든 시퀀스 전송 완료
 *   RESULT_ERROR  - 인코딩 실패 (스케줄러에서 재시도, RadioFailCount 누적 → LoRa 리셋)
 *   RESULT_FAULT  - 라디오 Fault 10회 (스케줄러에서 MiLoRa_Close 호출)
 */
oResult_t MiLoRa_SendMailbox(IoT_MailboxItem_t *pMail)
{
	static uint8_t SendMailboxStep = 0;
	static uint32_t SendSqcTimer = 0;
	static uint8_t SendSqcCount = 0;		// 현재까지 전송 완료된 시퀀스 번호
	static uint8_t SendErrorCount = 0;		// 현재 시퀀스의 연속 전송 실패 횟수
	static uint8_t SendFaultCount = 0;		// 라디오 Fault 연속 횟수 (10회 시 RESULT_FAULT)
	static uint8_t SendTryCount = 0;
	static uint8_t SendBuffer[LORA_PAYLOAD_SIZE] = {0,};
	static uint8_t SendBufferSize;
	oResult_t result = RESULT_RUN;

	// 유효하지 않은 메일 → 즉시 RESULT_NULL (스케줄러에서 메일 제거)
	if(pMail == NULL || pMail->Payload.DLC == 0){
		SendMailboxStep = 0;
		result = RESULT_NULL;
		goto EXIT;
	}
	// LoRa 미연결 또는 Sleep 중 → 일시 정지 (스케줄러에서 대기, 메일 유지)
	if(!pLoRaDevice->Status.IsJoined || !MiLoRa_IsOpen || pLoRaDevice->Status.IsSleeping){
		SendMailboxStep = 0;
		result = RESULT_PAUSE;
		goto EXIT;
	}

	switch(SendMailboxStep)
	{
		case 0: // 전송 시작 - 메일 상태 초기화
			pMail->IsDone = 0;
			pMail->IsError = 0;
			pMail->IsBusy = 0;
			pMail->IsSuccess = 0;
			SendSqcTimer = 0;
			SendSqcCount = 0;
			SendErrorCount = 0;
			SendMailboxStep++;
			oSerial_Log("MiLoRa", "Mailbox sent start | %s", MiIoT_DataTypeToString(pMail->Payload.TypeOfData));
			break;
		case 1: // 시퀀스 인코딩 - 대용량 데이터를 LoRa 페이로드 크기로 분할
			if((SendBufferSize = MiLoRa_Encode(pMail, SendSqcCount, (uint8_t *)&SendBuffer)) > 0){
				SendMailboxStep++;	// 인코딩 성공 → 전송 대기로
				SendErrorCount = 0;	// 새 시퀀스 시작 → QoS 재시도 카운트 리셋
			}
			else if(SendSqcCount > 0){
				result = RESULT_OK;	// 더 이상 인코딩할 데이터 없음 → 전체 전송 완료
				oSerial_Log("MiLoRa", "Mailbox sent success");
			}
			else{
				result = RESULT_NULL;	// 첫 시퀀스부터 인코딩 실패 → 데이터 형식 오류
				oSerial_Log("MiLoRa", "Mailbox encode error");
			}

			SendTryCount = 0;
			SendSqcTimer = oTMR_GetTick(TICKBASE_SYSTICK);
			break;
		case 2: // 전송 대기 - 시퀀스 간격 또는 실패 시 백오프 (5초 × SendTryCount)
			if(SendTryCount == 0 || oTMR_Elapsed(&SendSqcTimer, SECOND_TO_MS(5) * MATH_MAX(SendTryCount, 1), TICKBASE_SYSTICK)){
				SendMailboxStep++;
				SendTryCount++;
				SendFaultCount = 0;
				oSerial_Log("MiLoRa", "Mailbox transmit SQC=%d try=%d", (int)SendSqcCount, (int)SendTryCount);
			}
			break;
		case 3: // RF 전송 - RAK3172 AT+SEND 실행
			switch(RAK3172_Transmit(pMail->Payload.TypeOfData, (uint8_t *)&SendBuffer, SendBufferSize, pMail->QoS > 0))
			{
				case RESULT_RUN:	// 전송 진행 중
					break;
				case RESULT_OK:		// 전송 성공 → 다음 시퀀스 인코딩으로
					SendSqcCount++;
					SendMailboxStep = 1;
					SendSqcTimer = 0;
					MiLoRa_RadioFailCount = 0;
					break;
				case RESULT_TIMEOUT:
				case RESULT_ERROR:	// 전송 실패 → QoS 재시도
					SendMailboxStep = 2;
					SendSqcTimer = oTMR_GetTick(TICKBASE_SYSTICK);
					MiLoRa_RadioFailCount++;	// 누적 5회 초과 시 MiLoRa_Control에서 LoRa 리셋

					// QoS 횟수만큼 재시도 후 전송된 것으로 간주하고 다음 시퀀스로 이동
					if(++SendErrorCount >= MATH_MIN(pMail->QoS, 10)){
						SendSqcCount++;
						SendMailboxStep = 1;
						oSerial_Log("MiLoRa", "Mailbox sent error");
					}
					break;
				default:			// 라디오 비정상 응답 (RESULT_NULL, RESULT_FAULT 등)
					if(++SendFaultCount >= 10){
						result = RESULT_FAULT;	// 스케줄러에서 MiLoRa_Close() 호출
						oSerial_Log("MiLoRa", "Mailbox transmit fault");
					}
					break;
			}
			break;
	}

EXIT:
	// 메일 상태 업데이트 - 스케줄러가 참조하는 플래그
	switch(pMail->Result = result)
	{
		default:
			break;
		case RESULT_NULL:
			pMail->IsDone = 1;
			pMail->IsSuccess = 0;
			pMail->IsBusy = 0;
			SendMailboxStep = 0;
			break;
		case RESULT_RUN:
			pMail->IsBusy = 1;
			break;
		case RESULT_OK:		// 전송 완료 → 스케줄러에서 메일 제거
			pMail->IsDone = 1;
			pMail->IsSuccess = 1;
			pMail->IsBusy = 0;
			SendMailboxStep = 0;
			break;
		case RESULT_FAULT:	// 라디오 Fault → 스케줄러에서 MiLoRa_Close()
		case RESULT_ERROR:	// 인코딩 실패 → 스케줄러에서 재시도 (RadioFailCount로 LoRa 리셋)
		case RESULT_TIMEOUT:
			pMail->IsError = 1;
			pMail->IsDone = 1;
			pMail->IsBusy = 0;
			SendMailboxStep = 0;
			break;
	}

	return result;
}

/* LoRaWAN 네트워크 시간 동기화 - 비동기 상태머신
 * 동기화 정책:
 *   1. 최초 1회 무조건 동기화
 *   2. 매일 정오(12시)에 1회 재동기화
 *   3. 30분 이내 오차 → 즉시 적용
 *   4. 30분 초과 오차 → 3회 연속 수신 시 강제 적용 (오동기화 방지) */
oResult_t MiLoRa_TimeSync()
{
	static uint8_t TimeSyncStep = 0;
	static uint8_t TimeSyncedFirstOne = 0;		// 최초 동기화 완료 여부
	static uint8_t TimeSyncedPeriodic = 0;		// 정오 동기화 트리거 (1일 1회 제한)
	static uint8_t LargeDiffRetryCount = 0;		// 30분 초과 오차 연속 수신 횟수
	static oDateAndTime_t LastReceivedDT = {0};
	oResult_t result = RESULT_RUN;
	oDateAndTime_t ReceivedDT;
	oDateAndTime_t CurrentDT;
	int32_t DiffMinutes;

	if(!pLoRaDevice->Status.IsOpened){
		return RESULT_NULL;
	}

	oDT_GetNow(&CurrentDT);

	switch(TimeSyncStep)
	{
		case 0: // 동기화 필요 여부 판단
			// 1. 동기화되지 않으면 1회는 무조건 동기화
			// 2. 하루에 1회만 동기화 (정오 12시)
			if(CurrentDT.Hour == 12){
				if(!TimeSyncedPeriodic){
					TimeSyncedPeriodic = 1;
					MiLoRa_IsTimeSynced = 0;
				}
			}
			else {
				TimeSyncedPeriodic = 0;
			}

			if(!TimeSyncedFirstOne || !MiLoRa_IsTimeSynced){
				RAK3172Dev.Status.IsTimeSyncEnabled = 1;
				MiLoRa_IsTimeSynced = 0;
				TimeSyncStep++;
			}
			else{
				result = RESULT_OK;
			}
			break;
		case 1: // Join 완료 및 네트워크 시간 수신 대기
			if(pLoRaDevice->Status.IsJoined && pLoRaDevice->Status.IsTimeSynced){
				TimeSyncStep++;	// 수신 완료 → 시간 검증 단계로
			}
			else{
				result = RESULT_ERROR;	// 미Join 또는 시간 미수신 → 재시도
			}
			break;
		case 2: // 수신된 시간 검증 및 적용
			if((result=RAK3172_GetLocalTime(&ReceivedDT, UTC_PLUS_09_00)) == RESULT_OK){
				// 1. 최초 동기화: 무조건 적용
				if(!TimeSyncedFirstOne){
					oDT_SetNow(&ReceivedDT);
					MiLoRa_IsTimeSynced = 1;
					TimeSyncedFirstOne = 1;
					LargeDiffRetryCount = 0;
					oSerial_Log("MiLoRa", "TimeSync first sync applied");
				}
				else{
					// 시간 차이 계산 (초 단위로 얻은 후 분으로 변환)
					// oDT_Sub는 항상 절대값 차이를 반환 (MATH_MAX - MATH_MIN)
					DiffMinutes = SECOND_TO_MINUTE((int32_t)oDT_Sub(CurrentDT, ReceivedDT));

					// 3. 30분 이내 오차면 바로 동기화
					if(DiffMinutes <= 30){
						oDT_SetNow(&ReceivedDT);
						MiLoRa_IsTimeSynced = 1;
						LargeDiffRetryCount = 0;
						oSerial_Log("MiLoRa", "TimeSync applied (diff: %ld min)", DiffMinutes);
					}
					else{
						// 30분 이상 차이
						LastReceivedDT = ReceivedDT;
						LargeDiffRetryCount++;

						// 4. 3회 이상 큰 오차로 수신하면 강제 적용
						if(LargeDiffRetryCount >= 3){
							oDT_SetNow(&LastReceivedDT);
							MiLoRa_IsTimeSynced = 1;
							LargeDiffRetryCount = 0;
							oSerial_Log("MiLoRa", "TimeSync force applied after 3 retries (diff: %ld min)", DiffMinutes);
						}
						else{
							oSerial_Log("MiLoRa", "TimeSync rejected (diff: %ld min), retry %d/3", DiffMinutes, LargeDiffRetryCount);
						}
					}
				}
			}
			break;
	}

	if(result != RESULT_RUN){
		TimeSyncStep = 0;
	}

	return result;
}

/* 메일박스 전송 스케줄러 - 메일 정렬 → Wakeup 대기 → 전송 → 결과 처리
 * Step 0: 메일박스 정렬 후 전송 대상 메일 선택
 * Step 1: LoRa Open + Wakeup 완료 대기 (Sleep 중이면 전송 보류)
 * Step 2: MiLoRa_SendMailbox()로 전송, 결과에 따라 메일 제거 또는 재시도
 *
 * LoRa 미Open 시 Step 0으로 리셋 (일시 정지 → Open 후 재개) */
void MiLoRa_MailBoxTransmitScheduler()
{
	static uint8_t MiLoRa_ScheduleStep = 0;	// 스케줄러 상태머신 단계
	static IoT_MailboxItem_t *pMail = NULL;		// 현재 전송 중인 메일 포인터

	// LoRa 미Open 시 스케줄러 일시 정지 (Open 완료 후 Step 0부터 재시작)
	if(!MiLoRa_IsOpen){
		MiLoRa_ScheduleStep = 0;
	}

	switch(MiLoRa_ScheduleStep)
	{
		case 0: // 메일박스 정렬 및 전송 대상 선택
			IoT_MailBox_Sort(&MiIoT_LoRaMailBox);

			if((pMail = MiIoT_MailBox_GetItem(&MiIoT_LoRaMailBox)) != NULL){
				MiLoRa_ScheduleStep++;	// 전송할 메일 있음 → Open 대기
				MiLoRa_IsMailBoxReady = 1;
			}
			else{
				MiLoRa_ScheduleStep = 0;	// 전송할 메일 없음
				MiLoRa_IsMailBoxReady = 0;
			}
			break;
		case 1: // LoRa Open + Wakeup 완료 대기 + Join 안정화 대기
			if(!MiLoRa_IsOpen){
				break;	// Open 미완료 → 대기
			}
			if(RAK3172Dev.Status.IsSleeping){
				break;	// Sleep 중 → Control()에서 Wakeup 완료까지 대기
			}
			// Join/Open 직후 N초 안정화 대기 — 라디오 link 안정화로 첫 송신 손실 방지
			if(!oTMR_Elapsed(&MiLoRa_OpenTimestamp, MILORA_JOIN_STABILIZE_MS, TICKBASE_SYSTICK)){
				break;
			}
			MiLoRa_ScheduleStep++;
			break;
		case 2: // 메일 전송 및 결과 처리
			switch(MiLoRa_SendMailbox(pMail))
			{
				case RESULT_PAUSE:	// LoRa 미연결 → 대기
				case RESULT_RUN:	// 전송 진행 중 → 대기
					break;
				case RESULT_FAULT:	// 라디오 Fault → LoRa 전원 리셋
					MiLoRa_Close();
					break;
				case RESULT_NULL:	// 유효하지 않은 메일 → 메일 제거
				case RESULT_OK:		// 전송 완료 → 메일 제거
				default:			// RESULT_ERROR 등 → 메일 제거
					MiLoRa_ScheduleStep = 0;
					MiIoT_MailBox_Remove(&MiIoT_LoRaMailBox, pMail);
					break;
			}
			break;
	}
}

oResult_t MiLoRa_Sleep(uint8_t Enable)
{
	static uint8_t SleepStep = 0;
	oResult_t result = RESULT_RUN;

	// Close 후 전원 꺼지면 SleepStep 초기화 (재Open 시 AT 명령 누락 방지)
	if(!MiLoRa_IsOpen || !GPIOs.DO.LoRaEnable){
		SleepStep = 0;
		MiLoRa_IsSleep = 0;
		return RESULT_ERROR;
	}

	// 이미 요청한 상태와 동일하면 즉시 반환
	if(MiLoRa_IsSleep == Enable){
		SleepStep = 0;
		return RESULT_OK;
	}

	if(Enable){
		// Sleep 진입: IsSleep=0이라 IORun=1 → MCU sleep 자연스럽게 차단됨
		// LPM 성공 후 IORun=0 되면 MCU도 STOP 진입 (의도된 동작)
		if((result = RAK3172_Sleep()) == RESULT_OK){
			MiLoRa_RadioFailCount = 0;
			MiLoRa_IsBusy = 0;			
		}
	}
	else{
		// Wakeup: IsSleep=1이라 IORun=0 → MCU sleep 가능 → AT 명령 진행 중 IsBusy=1로 명시 차단
		MiLoRa_IsBusy = 1;

		switch(SleepStep)
		{
			case 0:	// Step 0: UART 재초기화 (Stop 모드 복귀 후 GPIO Analog → AF 복원)
				if(oAT_Open(&RAK3172Dev.AT) == AT_RESULT_OK){
					RAK3172Dev.AT.State.IsReceiveLine = 1;
					SleepStep++;
				}
				else{
					result = RESULT_ERROR;
				}
				break;
			case 1:	// Step 1: RAK3172 Wakeup 명령 전송
				if((result = RAK3172_Wakeup()) == RESULT_OK){
					MiLoRa_RadioFailCount = 0;
				}
				break;
		}
	}

	if(result != RESULT_RUN){
		SleepStep = 0;
		if(result == RESULT_OK){
			MiLoRa_IsSleep = Enable;
			oSerial_Log("MiLoRa", "SleepMode=%s Success\r\n", Enable ? "On" : "Off");
		}
		else{
			oSerial_Log("MiLoRa", "SleepMode=%s Fail\r\n", Enable ? "On" : "Off");
			result = RESULT_ERROR;
		}
	}

	return result;
}

/* LoRa 모듈 종료 - RAK3172 UART 해제 + 전원 차단
 * OpenStep 초기화로 다음 Open 시 처음부터 시작
 * 이미 종료 상태면 즉시 RESULT_OK 반환 */
oResult_t MiLoRa_Close()
{
	MiLoRa_OpenStep = 0;	// Open 상태머신 초기화

	// 이미 종료 + 전원 OFF → 중복 종료 방지
	if(!MiLoRa_IsOpen && !GPIOs.DO.LoRaEnable){
		return RESULT_OK;
	}

	if(RAK3172_Close() == RESULT_OK){
		if(MiLoRa_IsOpen){
			MiLoRa_CloseTimestamp = oTMR_GetTick(TICKBASE_SYSTICK);	// 종료 시각 기록 (Open 재시도 간격 계산용)
			oSerial_Log("MiLoRa", "Power off\r\n");
		}

		GPIOs.DO.LoRaEnable = 0;	// RAK3172 전원 차단

		// 상태 플래그 전체 초기화
		MiLoRa_RadioFailCount = 0;
		MiLoRa_IsPowerOn = 0;
		MiLoRa_IsBusy = 0;
		MiLoRa_IsOpen = 0;

		return RESULT_OK;
	}

	return RESULT_ERROR;	// RAK3172_Close() UART 해제 실패
}

/* LoRa 모듈 초기화 및 LoRaWAN 네트워크 Join - 비동기 상태머신
 * Step 0: Open 조건 확인 (ProductCode 설정 + 재시도 대기시간 경과)
 * Step 1: 기존 연결 정리 (전원 OFF + UART 해제)
 * Step 2: 전원 OFF 안정화 대기 (500ms)
 * Step 3: 전원 ON + 부팅 대기 (500ms)
 * Step 4: RAK3172 UART 초기화 + AT 설정
 * Step 5: LoRaWAN OTAA Join 시도
 *
 * 실패 시 OpenFailCount 누적, 재시도 간격 증가
 * OpenFailCount > 10 → IsConnected=0 (네트워크 도달 불가 판단) */
oResult_t MiLoRa_Open()
{
	static uint32_t MiLoRaOpenSqcTimer = 0;	// 단계 간 타이밍 타이머
	static uint8_t JoinTryCount = 0;		// Join 시도 횟수
	static uint8_t JoinTryTrig = 0;			// Join 시도 로그 1회 출력 플래그
	oResult_t result = RESULT_RUN;

	// 이미 Open 상태 → 즉시 성공
	if(MiLoRa_IsOpen){
		MiLoRa_OpenStep = 0;
		MiLoRa_OpenFailTimestamp = 0;
		return RESULT_OK;
	}

	switch(MiLoRa_OpenStep)
	{
		default:	// 비정상 Step 값 → 초기화
			MiLoRa_OpenStep = 0; // @suppress("No break at end of case")
		case 0: // Open 조건 확인: ProductCode 설정 + 재시도 대기시간 경과
			if(MiIoT_Parameter.Information.ProductCode != 0 && MILORA_OPEN_REMAINING_TIME <= 0){
				MiLoRa_OpenStep++;
				MiLoRa_IsBusy = 1;
				JoinTryCount = 0;
				MiLoRa_RadioFailCount = 0;
				MiLoRa_GenLoRaWANKey();	// MCU UID 기반 OTAA 키 생성
				MiLoRaOpenSqcTimer = oTMR_GetTick(TICKBASE_SYSTICK);
			}
			else{
				result = RESULT_NULL;	// 조건 미충족 → Control에서 재호출 대기
			}
			break;
		case 1: // 기존 연결 정리 - 전원 OFF + UART 해제
			oSerial_Log("MiLoRa", "Open start");
			GPIOs.DO.LoRaEnable = 0;	// RAK3172 전원 차단
			oSerial_Log("MiLoRa", "LoRaEnable=0");
			RAK3172_Close();			// UART 디바이스 해제
			oSerial_Log("MiLoRa", "Power off");

			MiLoRa_OpenStep++;
			break;
		case 2: // 전원 OFF 안정화 대기 (1000ms)
			if(oTMR_Elapsed(&MiLoRaOpenSqcTimer, 1000, TICKBASE_SYSTICK)){
				MiLoRaOpenSqcTimer = oTMR_GetTick(TICKBASE_SYSTICK);
				MiLoRa_IsOpen = 0;
				oSerial_Log("MiLoRa", "Off wait done (1000ms)");
				MiLoRa_OpenStep++;
			}
			break;
		case 3: // 전원 ON + 부팅 대기 (2000ms)
			MiLoRa_IsPowerOn = 1;
			GPIOs.DO.LoRaEnable = 1;	// RAK3172 전원 인가

			if(oTMR_Elapsed(&MiLoRaOpenSqcTimer, 2000, TICKBASE_SYSTICK)){
				oSerial_Log("MiLoRa", "Power on (boot wait 2000ms)");
				MiLoRa_OpenStep++;
			}
			break;
		case 4: // RAK3172 UART 초기화 + AT 설정 (Band, Class, OTAA 키 등)
			switch(RAK3172_Open(pLoRaUART, DownlinkCallback))
			{
				default:	// 초기화 진행 중
					break;
				case RESULT_OK:		// AT 설정 완료 → Join 단계로
					MiLoRa_OpenStep++;
					MiLoRaOpenSqcTimer = oTMR_GetTick(TICKBASE_SYSTICK);
					oSerial_Log("MiLoRa", "Open okay");
					break;
				case RESULT_ERROR:	// AT 명령 실패 → Close 후 즉시 재시도 (Step 1 복귀)
					MiLoRa_RadioFailCount++;
					MiLoRa_OpenStep = 1;
					MiLoRaOpenSqcTimer = oTMR_GetTick(TICKBASE_SYSTICK);
					oSerial_Log("MiLoRa", "Open error, restart (radio fail:%d)", (int)MiLoRa_RadioFailCount);
					break;
			}

			JoinTryTrig = 0;
			break;
		case 5: // LoRaWAN OTAA Join 시도
			switch(result = RAK3172_Join())
			{
				default:
					break;
				case RESULT_RUN:	// Join 진행 중 (1회만 로그)
					if(!JoinTryTrig){
						oSerial_Log("MiLoRa", "LoRaWAN try join");
						JoinTryTrig = 1;
					}
					break;
				case RESULT_OK:		// Join 성공
					oSerial_Log("MiLoRa", "LoRaWAN joined");
					break;
				case RESULT_FAULT:	// AT 명령 자체 실패
					oSerial_Log("MiLoRa", "AT commend fault");
					break;
				case RESULT_ERROR:	// Join 거부 (서버 응답 없음 등)
					JoinTryCount++;
					oSerial_Log("MiLoRa", "LoRaWAN join failed");
					break;
				case RESULT_TIMEOUT:	// Join 응답 타임아웃
					JoinTryCount++;
					oSerial_Log("MiLoRa", "LoRaWAN join timeout");
					break;
			}
			break;
	}

	// 상태머신 완료 (RUN 이외 결과) → 결과별 후처리
	if(result != RESULT_RUN){
		MiLoRaOpenSqcTimer = oTMR_GetTick(TICKBASE_SYSTICK);
		MiLoRa_OpenStep = 0;	// 상태머신 초기화

		switch(result)
		{
			case RESULT_NULL:	// 조건 미충족 (ProductCode 없음 등) → 스케줄러가 자체 처리
				break;
			default:
				break;
			case RESULT_OK:		// Join 성공 → Open 상태 전환
				MiLoRa_IsOpen = 1;
				MiLoRa_IsReachable = 1;
				MiLoRa_OpenFailTimestamp = 0;
				MiLoRa_OpenFailCount = 0;
				MiLoRa_RadioFailCount = 0;	// AT 재시도 누적분 리셋 (Control의 fault reset 오발 방지)
				MiLoRa_OpenTimestamp = oTMR_GetTick(TICKBASE_SYSTICK);
				break;
			case RESULT_FAULT:		// AT 명령 실패
			case RESULT_TIMEOUT:	// Join 타임아웃
			case RESULT_ERROR:		// Join 거부 → 재시도 대기
				MiLoRa_OpenFailTimestamp = oTMR_GetTick(TICKBASE_SYSTICK);	// 재시도 간격 기준 시각
				MiLoRa_OpenFailCount = MATH_LIMIT(MiLoRa_OpenFailCount+1, 0, 0xFFFFFFFE);
				oSerial_Log("MiLoRa", "Retry after %ds (fail:%d)", (int)(MILORA_OPEN_REMAINING_TIME/1000), (int)MiLoRa_OpenFailCount);

				if(MiLoRa_OpenFailCount > 10){	// 10회 초과 실패 → 네트워크 도달 불가 판단
					MiLoRa_IsReachable = 0;
				}
				break;
		}

		// Open 실패 시 전원 차단 (전력 절약) + Busy 해제
		if(!MiLoRa_IsOpen){
			MiLoRa_IsPowerOn = 0;
			MiLoRa_IsBusy = 0;
			GPIOs.DO.LoRaEnable = 0;
			RAK3172_Close();
		}
	}

	return result;
}

/* LoRa 전원 제어 - 운영 모드/전력 절약에 따른 Open/Close/Sleep 결정
 * 전력 절약 모드 정책:
 *   Open 조건: 전송할 메일 있음 또는 네트워크 미연결(재접속 필요)
 *   Sleep 조건: 메일 없음 + Open 상태 → AutoSleepDelay 후 Sleep
 *   Wakeup 조건: 메일 있음 → 즉시 Wakeup (debounce 무시)
 * RadioFailCount > 5 → 라디오 이상 → 강제 전원 리셋 */
void MiLoRa_Control()
{
	static oDebounce_t AutoSleepDelay = DEBOUNCE_INITIALIZER(1000, 500);

	// 자동 Sleep 조건: 메일 없음 + Open 상태 + 절전 모드 → 1초 후 Sleep
	oDebounce(&AutoSleepDelay, !MiLoRa_IsMailBoxReady && MiLoRa_IsOpen && MiIoT_IsPowerSaveMode);

	// 운영 모드가 아닌 경우 (비운영/유지보수) → 무조건 종료
	if(MiIoT_Parameter.Operating.OperatingMode != IoTOperatingMode_Operating && MiIoT_Parameter.Operating.OperatingMode != IoTOperatingMode_PreOperation && pLoRaDevice->IsInitailzed){
		MiLoRa_Close();
	}
	else if(MiIoT_IsPowerSaveMode){	// 절전 모드 → 필요할 때만 Open
		if(MiLoRa_IsOpen){
			if(!RAK3172Dev.Status.IsJoined){	// Join 실패 상태 → 전원 리셋 후 재시도
				MiLoRa_Close();
			}
			else if(MiLoRa_IsMailBoxReady){
				// 메일 있으면 즉시 Wakeup (debounce 무시, AT 충돌 방지)
				if(MiLoRa_Sleep(0) == RESULT_ERROR){
					MiLoRa_RadioFailCount++;
				}
			}
			else{
				// 메일 없으면 debounce 후 Sleep
				if(MiLoRa_Sleep(AutoSleepDelay.Output) == RESULT_ERROR){
					MiLoRa_RadioFailCount++;
				}
			}
		}
		else if(!MiLoRa_IsOpen){
			if(!MiLoRa_IsReachable){			// 네트워크 미연결 → 재접속 시도
				MiLoRa_Open();
			}
			else if(MiLoRa_IsMailBoxReady){		// 전송할 메일 있음 → Open
				MiLoRa_Open();
			}
		}
	}
	else{	// 비절전 모드 → 항상 Open 유지
		MiLoRa_Open();
	}

	// 라디오 연속 실패 5회 초과 → 하드웨어 이상 판단, 전원 리셋
	if(MiLoRa_IsOpen && MiLoRa_RadioFailCount > 5){
		oSerial_Log("MiLoRa", "Start radio fault reset\r\n");
		MiLoRa_Close();
	}
}

/* LoRa 메인 루프 - MiIoT() 메인루프에서 매 사이클 호출
 * 실행 순서: 전원제어 → 시간동기화 → 메일전송 → RAK3172 AT 처리 */
void MiLoRa(UART_HandleTypeDef *pUART)
{
	pLoRaUART = pUART;

	MiLoRa_Control();					// LoRa 전원 Open/Close 제어
	MiLoRa_TimeSync();					// LoRaWAN 네트워크 시간 동기화
	MiLoRa_MailBoxTransmitScheduler();	// 메일박스 전송 스케줄러
	RAK3172();							// RAK3172 AT 명령 송수신 처리
}

/* History

2026-06-26 | v0.1
	- baseline (Mi_LoRa.c)
*/
