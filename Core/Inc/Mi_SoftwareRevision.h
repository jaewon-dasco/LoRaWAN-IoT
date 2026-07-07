/*
 * Mi_Revision.h
 *
 *  Created on: Dec 16, 2024
 *      Author: JONE
 */

#ifndef INC_MI_SOFTWAREREVISION_H_
#define INC_MI_SOFTWAREREVISION_H_

#include "Mi_Main.h"

#define MI_SW_REVISION				1.12

/* History

2024-12-16 | HW - | FW 0.1
	- Start
2025-03-17 | HW - | FW 0.2
	- HW2.0 펌웨어
2025-11-11 | HW - | FW 0.3
	- CAN 데이터 측정/스캔/3DS 측정 추가 / LoRa RxDealy 2로 변경
2025-12-09 | HW - | FW 0.4
	- 측정시 진동으로 인한 오차 발생하였을때 직전 측정값 대비 오차르 확인하고 재측정 하는 알고리즘 추가 (3DS 레이일변이)
2026-01-30 | HW - | FW 0.5
	- Revision No 수정 2.X -> 0.X
	- 시간 동기화
	   1. 정오12시에 1회 동기화
	   2. RTC 현재 시간과 비교해서 30분이상 다르면 동가화 재시도
	   3. 3회 동기화 재시도 후 에도 시간이 30분이상 다르다면 그냥 시간 동기화 완료 함
2026-03-05 | HW - | FW 0.6
	- RAK3172 LinckCheck 설정 값 2에서 1로 수정
    - 재측정 알고리즘 수정
      1. 오차 작은 값만 가져다 최종 측정값으로 사용하도록 수정
      2. 최종 측정값이 모두 완성되면 즉시 측정 종료
2026-03-11 | HW - | FW 0.61
	- NAND 플래시에 파레마터 저장 추가
	- LoRa Open/Transmit 시 RESULT_NULL 시 메일박스 삭제
	- RAK3172 ATE 실패히도 다음 시퀀스 넘어가도록 수정
	- Dwonlink DeviceInfo 설정 추가
	- MiIoT DataPacket Pointer 수정 (pPacket->ppPacket)
2026-04-01 | HW - | FW 0.7
	- 공통 라이브러리 메모리 안전성 패치 적용 (OneLibrary/oThirdParty)
	  NULL 포인터 검증, sprintf→snprintf, DMA 에러복구, 버퍼 오버플로우 수정
2026-05-08 | HW - | FW 0.8
	- VDD 동적 보정 추가 (VREFINT 기반): SystemSupply 온도 드리프트 개선
	- ADC offset 교정을 Measurement_CalibrateVDD로 단일화 (Native_ADCRead 중복 제거)
	- MiMain_GPIOInit/DeInit 리팩터링:
	  · 불필요 초기 출력값 IO_WRITE 블록 제거, 변수명 cfg로 단축
	  · Init 시작부에 MiMain_GPIOControl() 호출 추가 → 마지막 IO 상태 복원
	  · DeInit은 출력 핀을 ANALOG 모드로 전환 (저전력)
	  · LORA_ENABLE Init/DeInit을 MiLoRa_IsSleep 조건부로 변경
	- MiIoT_Sleep LED 시퀀스 단순화: ON 50ms / OFF 200ms 명시 사이클
	- SYSTEM_SUPPLY_LOW_LIMIT 3200mV로 통일
	- LoRa Join 실패 시 backoff 적용: 5s → 10s → 20s → 40s → 80s → 160s → 300s(cap, 5min)
2026-05-08 | HW - | FW 0.9
	- LoRa Open 안정화 개선:
	  · 전원 OFF 대기 500ms → 1000ms
	  · 전원 ON 부팅 대기 500ms → 2000ms
	  · AT 명령 실패 시 Close 후 즉시 재Open (Step 1 복귀), MiLoRa_RadioFailCount 누적
	  · 단계별 진단 로그 추가 (LoRaEnable=0, Power off, Off wait done, boot wait, error restart)
2026-05-11 | HW - | FW 0.91
	- LoRa Open/Sleep 동작 중 MCU STOP 진입 방지:
	  · MiLoRa_IsBusy를 MiIoT_IsBusy에 포함 → Open Step 1-2 (전원 OFF 구간) sleep 차단
	  · Wakeup 전환 구간(IsSleep=1, IORun=0)에 IsBusy=1 명시 가드 추가
	  · Open 성공 시 MiLoRa_RadioFailCount 리셋 (Control fault reset 오발 방지)
	- MiLoRa_IsBusy: Open Error/Fail 시 0 명시 추가 (Open 실패 후 busy 잔존 방지)
	- MiStorage_IsBusy 플래그 도입:
	  · MiStorage() 진입부 1줄 동기화 → IsBusy = (NANDEnable || IsOpen)
	  · NAND 전원 ON 또는 MT29F2G 초기화 완료 상태 추적
	  · MCU sleep 차단 — Open Step 2-3 진행 + 운영 + Close 진행 전체 구간 커버
	- MiStorage_AutoOffTimer 오타 수정 (MiStroage → MiStorage, 5곳)
	- MiIoT_IsBusy 식 정리: MiStorage_IsBusy || MiLoRa_IsBusy (IsOpen 중복 제거)
2026-05-12 | HW - | FW 0.92
	- LoRa Join 안정화 대기 추가 (MILORA_JOIN_STABILIZE_MS = 10000ms):
	  · Join/Open 직후 라디오 link 안정화로 첫 송신 패킷(FCnt 0,1) 손실 방지
	  · scheduler case 1에서 oTMR_Elapsed로 10초 대기 후 송신 시작
2026-05-15 | HW - | FW 0.93
	- LoRa Encoder DataArray 케이스 누락 수정:
	  · MiLoRa_Encode switch에 IoTDataType_DataArray 분기 없어 모든 측정값 송신 실패
	    ("Mailbox encode error" 로그 후 폐기)
	  · Analog 패턴 본떠서 시퀀스 헤더(1) + Time(6) + Items 분할 인코딩 추가
	- LORA_DATAARRAY_SPLIT_COUNT = 7 (시퀀스당 7개, 페이로드 49B / 51B 한도)
	  · 코덱 R10/R11의 SplitSensorCount=7과 일치
2026-05-15 | HW 2.2 | FW 0.94
	- RAK3172_Transmit hex 송신 잔존 데이터 버그 수정:
	  · oHexStringFromBytes가 null-termination 안 함 → TxStringBuffer에 남은 이전 송신 hex가
	    strlen 길이로 같이 전송 (짧은 페이로드일 때 뒷부분 옛 데이터가 그대로 송신)
	  · 예: 직전 Operating 5B(30 hex) 후 Status 10B(20 hex) 송신 → 15B로 송신되어 서버 디코드 실패
	  · pStr[SizeofData * 2] = '\0' 명시적 종료 추가 (LoRa_RAK3172.c:388)
2026-05-21 | HW 2.2 | FW 0.95
	- MiLoRa_SendMailbox 전송 실패 시 백오프 적용:
	  · 실패 시 case 2로 복귀 → 5초 × SendTryCount 대기 후 재전송
	  · 첫 시도는 즉시 송신 (SendTryCount==0 분기), 재시도마다 +5초 누적 (5/10/15초)
	  · SendErrorCount 리셋을 case 1(새 시퀀스 시작)로 이동 → QoS 재시도 카운트 유실 방지
	- IoT 제품 코드 enum 리네이밍: `IoTProductType_SIA100_2A` -> `IoTProductType_SIA100_SD`
      (99_Common 및 전 디바이스 로컬 Mi_IoT.h 사본 동기화, 값 5 유지)
2026-06-04 | HW 2.4 | FW 0.96
	- HW2.4 — CH1(CAN Array) + CH2(Analog mV/mA) 이중 채널 측정 시퀀스 구현:
	  · MiMain_UpdateMeasure case 1: mV/mA 센서 타입 측정 분기 추가
	  · MiMain_UpdateMeasure case 4: TypeOfData 디스패처(MiMain_GetMeasureError/StableData) 도입
	    → Array(oRange_t Min/Max)와 Analog(double) 양쪽 재측정 알고리즘 공통 골격 사용
	  · MiMain_UpdateMeasure case 5: ChannelDone 시 항상 RESULT_OK 반환 (마지막 채널 인큐 누락 수정),
	    모듈러 wrap 제거 → case 1의 `ChannelNo >= MAXCOUNT` 경로에서 RESULT_DONE 도착
	- Mi_Main_SIC100.c: MiMain_GetAnalogDataError / GetAnalogStableData 신규 (SIM100 패턴)
	- Mi_Main_SIC100.c: MiMain_GetMeasureError / GetMeasureStableData 디스패처 신규
	- Measurement_Analog: RESULT_OK 시 TypeOfData=Analog, DLC=MIIOT_IOTDATA_SIZE_ANALOG(1) 설정 추가
	  → 단일 채널/패킷 정책으로 Channel[0]에 저장
2026-06-17 | HW 2.4 | FW 0.96 (continued)
	- RAK3172 Open 시퀀스 안정화: LPM wakeup / AT 검증 / NWM·NJM 선설정 / 무한루프 보호
	- Mi_LoRa.c 디바이스별 인코더 분리 (Mi_LoRa_<MODEL>.c 6개, 미사용 분기 제거)
	- enum 재편: DataArray → Type1(100, VMX) / Type2(101, SIC100-2C), Vibration 5번
	- SIC100-2C 통합 페이로드 인코더 (fPort 101): Seq 0~49=Analog, 50~100=Tilt
	- Measurement_Sensor 재구조: 단일 호출로 CH1+CH2 통합 패킷 emit
	- ArraySensorSingle 측정 함수 완성 (이전 미구현)
	- MiSerial_PrintSensorData 새 통합 패킷 대응 (config 의존 제거)
2026-06-23 | HW 2.4 | FW 0.96 (continued)
	- MiSerial_Handler 초기화 designated initializer로 수정 (pRxBuffer가 IndexOfTxFirst에 박히던 버그 해결)
	- 측정 자기 일관성 검증 추가: 직전값과 ErrorTolerance 내 연속 2회 일치 시 즉시 ChannelDone
2026-06-24 | HW 2.4 | FW 0.96 (continued)
	- ONE_Serial TX 비동기 송신 안정화 (PHM 검증 후 전 프로젝트 동기화):
	  · oSerial_PutChar / vPrint / Printf / PrintLine: gState != READY 가드 제거
	    (DMA TX 진행 중 후속 출력 silent drop 버그 수정)
	  · oSerial_Write fallback: pTxBuffer NULL 또는 SizeOfTxBuffer < 50 시 blocking 전송
	  · 링버퍼 적재/콜백에 __disable_irq() / __enable_irq() critical section 추가
	- MISERIAL_TX_BUFFER_SIZE: 1 → 1000 (DMA 링버퍼 비동기 송신 활성화)
2026-06-25 | HW 2.4 | FW 0.96 (continued)
	- ONE_Serial DMA race 수정 + 송신 효율 개선:
	  · oSerial_vPrint/PrintLine/Log: char-by-char → vsnprintf 한 번에 oSerial_Write (256B 로컬 버퍼)
	  · oSerial_Write txInProgress 판정: HW state만 → IsTxBusy(SW) || hwBusy 결합
	    (DMA 종료~콜백 race window 에서 청크 재전송되던 버그 해결)
2026-06-26 | HW 2.4 | FW 0.97
	- 공통 라이브러리 canonical(`100_Library/`) 동기화:
	  · ONE_Common.h: STM32U5/H5/U0 ↔ L4 DMA 호환 매크로 신규 (ONE_DMA_GET_COUNTER/IS_CIRCULAR/SET_CIRCULAR)
	  · ONE_ATCommend.h/.c: 직접 CNDTR/Init.Mode 접근을 ONE_DMA_* 매크로 사용으로 전환
	  · ADC_NAU7802.c: switch default 줄바꿈 형식 정리
	- 파일 인코딩 정규화: 일부 파일 LF → CRLF (EmbededCodingStyle 규칙)
	- git 저장소 위치 컨벤션 확정: `WORK/.git`에서 관리 (PROJECT.md `## Git 저장소` 섹션 신규)
2026-06-29 | HW 2.4 | FW 0.98
	- 측정 시퀀스 수정 — encode error 해결:
	  · Measurement_ArraySensorDual/Single: TiltArray.Type / CountOfArraySensor 설정을
	    RESULT_OK 분기 안 → EXIT 직전(result != RESULT_RUN)으로 이동
	  · 측정 sub-func가 RESULT_ERROR(CAN open 실패 등)로 끝나도 채널 메타가 유효하게 채워짐
	    → MiLoRa_Encode 첫 시퀀스에서 hasTilt=0/totalSensors=0로 SizeofData=0 반환되던 버그 해결
	  · 효과: 측정 실패 시에도 빈 패킷이 정상 인코딩되어 송신, 서버는 슬롯 0(-180)으로 디코드
	- .c 헤더 주석 형식 일괄 정리:
	  · "ONE_Debug.c" 같은 잘못된 파일명 → 실제 파일명으로 교정 (181개)
	  · Created on/Author 라인 제거 → "Version: X.Y (YYYY-MM-DD)" 한 줄로 통일
	  · OneLibrary/oThirdParty/Core/Mi_*.c 184개 일괄 적용 (canonical + 7 IoT 프로젝트)
2026-07-02 | HW 2.4 | FW 0.99
	- MiSerial_Handler·MiSerial_RxBuffer 정의를 공통 Mi_Serial.c → 모델별 Mi_Serial_SIC100.c로 이동
	  · 모델별 TX/RX 버퍼 구성 분리 (SIV100 패턴 통일)
	  · pTxBuffer=NULL, MiSerial_TxBuffer 정의 주석 처리 (미사용)
2026-07-03 | HW 2.4 | FW 1.0
	- MiLoRa(pUART) NULL 가드 추가: pUART==NULL 시 즉시 return (SIA100_VB 동기화)
2026-07-07 | HW 2.4 | FW 1.1
	- Mi_IoT v0.1 → v0.2:
	  · IoTProcessState_t 신규 도입 (bitfield union + uint32_t IsBusy 오버레이)
	    필드: SamplingPeriod/SamplingSensor/SamplingSupply/BusyLora/BusyMemory/BusyGPIO/Pause (bit)
	         + SleepMode (byte, union 외부)
	    IsBusy==0 한 번으로 7-flag 통합 검사 → Sleep 진입 조건 단순화
	  · 개별 전역 flag → ProcessState 필드로 통합
	    MiIoT_IsRunIO / MiIoT_IsRunSamplingSensor / MiIoT_IsRunSamplingSupply 제거
	    MiIoT_IsSleep / MiIoT_IsPause 제거 → ProcessState.SleepMode / .Pause
	  · IoTStatus_t 정리: SoftwareVersion 추가, StatusBit → StatusBits 통일
	  · IoTStatusBit_t typedef 순서 정정 (forward reference 해결)
	  · MiIoT_StatusCallback 타입 변경: ResultCallbackHandler_t → IoTDataPacketCallbackHandler_t
	    (UpdateStatus가 IoT_DataPacket_t 반환하도록 시그니처 확장)
	- Mi_Main v0.1 → v0.2 (Mi_Main_SIC100.c):
	  · MiMain_UpdateSampling 시퀀스 재작성 — UpdateMeasure 골격 + 재측정 알고리즘만 제거
	    (ChannelNo 순회 제거, 1회 측정 → emit → DONE)
	  · MiMain_UpdateStatus 시그니처 확장: (void) → (IoT_DataPacket_t **ppPacket)
	    IoTDataStatus_t 페이로드 packing (SoftwareVersion/StatusBits/SystemSupply/TroubleCode)
	    *ppPacket = &DataPacket 로 반환 (기존 ppPacket = ... 오타 수정)
	  · MiIoT_Status.UpdateTimestmap → UpdateSupplyTimestamp 필드명 정합화
	- 인터록 정합성 검증 완료:
	  · 3자 상호배제(SamplingSensor/UpdatePeriod/UpdateStatus) + Sleep 자동 대기
	  · IsBusy union bitfield로 race 없이 통합 검사
2026-07-07 | HW 2.4 | FW 1.11
	- Mi_Main v0.2 → v0.3 (Mi_Main_SIC100.c 재작성 — SIM100 신형 패턴):
	  · MiMain_UpdateMeasure: 공급전압 선검사(저전압 RESULT_FAULT) + ChDoneBits 채널별 재측정
	    + RESULT_WAIT 대기 + MEASURE_UPDATE_RETRY_MAX(10) 상한 구조로 교체
	  · StableData 합성·자기 일관성 2회 검증 제거 → 채널별 RetryCount cap으로 대체
	  · MiMain_IsError/GetMeasureStableData → MiMain_DataIsError(채널 단위, CH1=Tilt/CH2=Analog)
	  · CH2 Analog 측정 실패 마커(Analog.Type==NULL) 감지 시 해당 채널 재측정
	  · MiMain_UpdateSampling: 공급전압 선검사 + 저전압 가드 추가 (SensorSamplingProgress 유지)
	  · MiMain(): Init 단계 HAL_I2C_DeInit(&hi2c2) 추가
	- Mi_IoT v0.2 → v0.3 (SIM100 발신, 전 프로젝트 + 99_Common 동기화):
	  · MiIoT_Sleep 상태머신 정리 — SleepMode=0 클리어를 default로 일원화,
	    case 4 wake 완료 시 default 경유 리셋
	  · 5ms idle 유예 + SleepMode=1 설정 + step 전진의 원자적 결합 유지
	    (유예 구간 = period/sensor/status가 busy 비트를 선점하는 시간)
2026-07-07 | HW 2.4 | FW 1.11 (continued)
	- Mi_IoT_SIC100 v0.11: MiIoT_IsSensorData 인정 페이로드에 IoTDataType_DataArray_Type2 추가
	  · SIC100은 FW 0.96부터 fPort 101 통합 페이로드(DataArray_Type2) 사용
	  · NAND 저장 관문(MiStorage_WriteIoTData)이 현행 페이로드를 인정하도록 (잠재 지뢰 제거)
	  · 이력 위치 이동: 공유 Mi_IoT.h → 본 파일 (공유 헤더에 제품 전용 이력 미기재 규약)
2026-07-08 | HW 2.4 | FW 1.12
	- Mi_Measurement v0.2 (Mi_Measurement.c 잔여 OOB 수정):
	  · Measurement_RecieveCallback_ArrayDual/ArraySingle 경계 off-by-one (111, 150행):
	    (ReceivedId - SensorStartId) <= MIIOT_ARRAYSENSOR_MAX_COUNT → < 로 변경
	    (MAX_COUNT=30, 배열 [30] 유효 0~29 — <=는 인덱스 30 통과 시
	     TiltArray.Dual[30].AxisX/Y 또는 Single[30].Axis 쓰기로 인접 필드
	     CountOfArraySensor·Analog 영역 오염 유발)
	  · Measurement_Scan case 4 조회 루프 OOB read 차단 (251행):
	    종료 조건에 || IdIndex >= MEASUREMENT_NODE_MAXCOUNT 추가
	    (실호출 Measurement_Scan(ch, 1, 255, ...) 시 IdIndex 250~254에서
	     Measure_ScanIdList[IdIndex] 읽기가 배열 밖(250칸)으로 넘어가던 문제 해소)
	- Mi_Main v0.3 → v0.31 (Mi_Main.h 매크로/History 3면 정합):
	  · MI_MAIN_VERSION 0.3 → 0.31 (.c Version 0.31 · History v0.31과 일치)
	  · MiMain_UpdateStatus 60초 가드 즉시 반환 경로 항목 추가:
	    Measurement_Supply 재실행 창(60초) 안쪽이면 else { result = RESULT_OK; }로
	    이전 캐시된 MiIoT_Status를 DataPacket에 packing 후 즉시 OK 반환 →
	    상위 시퀀스가 STEP 전진 가능 (이전 RUN 유지로 stall되던 문제 해소)
*/

#endif /* INC_MI_SOFTWAREREVISION_H_ */
