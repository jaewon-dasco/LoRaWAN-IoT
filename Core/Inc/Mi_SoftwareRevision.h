/*
 * Mi_Revision.h
 *
 *  Created on: Dec 16, 2024
 *      Author: JONE
 */

#ifndef INC_MI_SOFTWAREREVISION_H_
#define INC_MI_SOFTWAREREVISION_H_

#include "Mi_Main.h"

#define MI_SW_REVISION				0.8

/* History

2024-12-16 | HW - | FW 0.1
	- Start
2025-03-25 | HW - | FW 0.2
	- 하드웨어 2.0 버전 펌웨어 적용
	- LoRa Tiemsync LoRaNode에서 처리
	- LoRa 모듈 펌웨어 R16
2026-01-30 | HW - | FW 0.3
	- Revision No 수정 2.X -> 0.X
	- 시간 동기화
	   1. 정오12시에 1회 동기화
	   2. RTC 현재 시간과 비교해서 30분이상 다르면 동가화 재시도
	   3. 3회 동기화 재시도 후 에도 시간이 30분이상 다르다면 그냥 시간 동기화 완료 함
	- 아날로그 재측정 알고리즘 추가
	- mA/mV/uA/Deff 측정 모두 테슽 완료 함 (기존에는 mV만 테스트해서 현장에 적용)
2026-03-11 | HW - | FW 0.4
	- NAND 플래시에 파레마터 저장 추가
	- LoRa Open/Transmit 시 RESULT_NULL 시 메일박스 삭제
	- RAK3172 ATE 실패히도 다음 시퀀스 넘어가도록 수정
	- Dwonlink DeviceInfo 설정 추가
	- MiIoT DataPacket Pointer 수정 (pPacket->ppPacket)
	- DCDC On 후 대기시간 연장 (300 -> 500ms)
2026-04-01 | HW - | FW 0.5
	- 공통 라이브러리 메모리 안전성 패치 적용 (OneLibrary/oThirdParty)
	  NULL 포인터 검증, sprintf→snprintf, DMA 에러복구, 버퍼 오버플로우 수정
2026-05-08 | HW - | FW 0.6
	- VDD 동적 보정 추가 (VREFINT 기반): ADC_VREF 고정 2800mV → 매 측정 사이클 VDDA 역산
	  → 외부 LDO 온도 드리프트로 인한 SystemSupply 17mV 변동 → 수 mV 수준으로 개선
	- ADC offset 교정을 Measurement_CalibrateVDD로 단일화 (Native_ADCRead 내부 중복 제거)
	- Measurement_ReadAnalog: Analog=0 또는 오류 발생 시 3회 재시도 후 채널 스킵
	- MiMain_GPIOInit/DeInit 리팩터링:
	  · 불필요 초기 출력값 IO_WRITE 블록 제거, 변수명 cfg로 단축
	  · Init 시작부에 MiMain_GPIOControl() 호출 추가 → 마지막 IO 상태 복원
	  · DeInit은 출력 핀을 ANALOG 모드로 전환 (저전력), USB DI EXTI는 wake source로 보존
	  · LORA_ENABLE Init/DeInit을 MiLoRa_IsSleep 조건부로 변경
	- MiIoT_Sleep LED 시퀀스 단순화: BlinkSleep 제거, ON 50ms / OFF 200ms 명시 사이클
	- SYSTEM_SUPPLY_LOW_LIMIT 3200mV로 통일
	- LoRa Join 실패 시 backoff 적용: 5s → 10s → 20s → 40s → 80s → 160s → 300s(cap, 5min)
2026-05-08 | HW - | FW 0.61
	- LoRa Open 안정화 개선:
	  · 전원 OFF 대기 500ms → 1000ms
	  · 전원 ON 부팅 대기 500ms → 2000ms
	  · AT 명령 실패 시 Close 후 즉시 재Open (Step 1 복귀), MiLoRa_RadioFailCount 누적
	  · Open 성공 시 MiLoRa_RadioFailCount 리셋 (Control의 fault reset 오발 방지)
	  · 단계별 진단 로그 추가 (LoRaEnable=0, Power off, Off wait done, boot wait, error restart)
	- LoRa Open/Sleep 동작 중 MCU STOP 진입 방지:
	  · MiLoRa_IsBusy를 MiIoT_IsBusy에 포함 → Open Step 1-2(전원 OFF 구간) sleep 차단
	  · Wakeup 전환 구간(IsSleep=1, IORun=0)에 IsBusy=1 명시 가드 추가
	  · Sleep enter는 LoRaEnable=1, IsSleep=0 이라 IORun=1 가드로 충분 (LPM 진입 후 MCU도 STOP 의도된 동작)
2026-05-11 | HW - | FW 0.62
	- MiStorage_IsBusy 플래그 도입:
	  · MiStorage() 진입부 1줄 동기화 → IsBusy = (NANDEnable || IsOpen)
	  · NAND 전원 ON 또는 MT29F2G 초기화 완료 상태 추적
	  · MCU sleep 차단 — Open Step 2-3 진행 + 운영 + Close 진행 전체 구간 커버
	- MiStorage_AutoOffTimer 오타 수정 (MiStroage → MiStorage, 5곳)
	- MiIoT_IsBusy 식 정리: MiStorage_IsBusy || MiLoRa_IsBusy (IsOpen 중복 제거)
	- MiLoRa_IsBusy: Open Error/Fail 시 0 명시 추가 (Open 실패 후 busy 잔존 방지)
2026-05-12 | HW - | FW 0.63
	- LoRa Join 안정화 대기 추가 (MILORA_JOIN_STABILIZE_MS = 10000ms):
	  · Join/Open 직후 라디오 link 안정화로 첫 송신 패킷(FCnt 0,1) 손실 방지
	  · scheduler case 1에서 oTMR_Elapsed로 10초 대기 후 송신 시작
2026-05-15 | HW - | FW 0.64
	- LoRa Encoder DataArray 케이스 누락 수정:
	  · MiLoRa_Encode switch에 IoTDataType_DataArray 분기 없어 모든 측정값 송신 실패
	    ("Mailbox encode error" 로그 후 폐기)
	  · Analog 패턴 본떠서 시퀀스 헤더(1) + Time(6) + Items 분할 인코딩 추가
	- LORA_DATAARRAY_SPLIT_COUNT = 7 (시퀀스당 7개, 페이로드 49B / 51B 한도)
	  · 코덱 R10/R11의 SplitSensorCount=7과 일치
2026-05-15 | HW 2.2 | FW 0.65
	- RAK3172_Transmit hex 송신 잔존 데이터 버그 수정:
	  · oHexStringFromBytes가 null-termination 안 함 → TxStringBuffer에 남은 이전 송신 hex가
	    strlen 길이로 같이 전송 (짧은 페이로드일 때 뒷부분 옛 데이터가 그대로 송신)
	  · 예: 직전 Operating 5B(30 hex) 후 Status 10B(20 hex) 송신 → 15B로 송신되어 서버 디코드 실패
	  · pStr[SizeofData * 2] = '\0' 명시적 종료 추가 (LoRa_RAK3172.c:388)
2026-05-21 | HW 2.2 | FW 0.66
	- MiLoRa_SendMailbox 전송 실패 시 백오프 적용:
	  · 실패 시 case 2로 복귀 → 5초 × SendTryCount 대기 후 재전송
	  · 첫 시도는 즉시 송신 (SendTryCount==0 분기), 재시도마다 +5초 누적 (5/10/15초)
	  · SendErrorCount 리셋을 case 1(새 시퀀스 시작)로 이동 → QoS 재시도 카운트 유실 방지
	- IoT 제품 코드 enum 리네이밍: `IoTProductType_SIA100_2A` -> `IoTProductType_SIA100_SD`
      (99_Common 및 전 디바이스 로컬 Mi_IoT.h 사본 동기화, 값 5 유지)
2026-06-05 | HW 2.2 | FW 0.67
	- 0V 측정값 에러 처리 제거 — 측정 안 된 값도 그대로 송신:
	  · Analog_ReadSingle: ReadAnalog != 0 가드 제거 → 0V도 센서 변환·캘리브레이션 통과
	  · Measurement_ReadAnalog 저장 조건에서 Analog != 0 제거 → RESULT_OK면 0이라도 저장
	  · 재시도 분기에서 Analog=0 조건 제거 → 실제 HW 오류(RESULT_ERROR)만 재시도
2026-06-09 | HW 2.4 | FW 0.7
	- 하드웨어 2.4 적용 — 프로젝트명 HW2_2 → HW2_4 변경
	  (.ioc 파일명·내부 ProjectName/ProjectFileName, .project name, CLAUDE.md 동기화)
2026-06-17 | HW 2.4 | FW 0.7 (continued)
	- RAK3172 Open 시퀀스 안정화: LPM wakeup / AT 검증 / NWM·NJM 선설정 / 무한루프 보호
	- Mi_LoRa.c 디바이스별 인코더 분리: Mi_LoRa_SIM100.c 신규 (Analog/Status/Operating 분기만 유지)
	- enum 재편: DataArray → Type1(100, VMX)/Type2(101, SIC100-2C), Vibration 5번
2026-06-18 | HW 2.5 | FW 0.8
	- HW 2.5 적용 (프로젝트명 HW2_4 → HW2_5)
	- LoRa 전송 포맷 Analog → DataArray_Type1 (측정·LoRa·Serial 일괄)
	- 외부 ADC NAU7802 분리 (Internal=thermistor / External=mV·mA·Diff·FullBridge), PGA Bypass 적용
	- MiMain_GPIOInit를 CubeMX 설정과 일치하도록 재편 (12V/ADC_REF→OD, AMP→PP NoPull, PWSUPPLY→PullUp)
2026-06-24 | HW 2.5 | FW 0.8 (continued)
	- ONE_Serial TX 비동기 송신 안정화 (PHM 검증 후 전 프로젝트 동기화):
	  · oSerial_PutChar / vPrint / Printf / PrintLine: gState != READY 가드 제거
	    (DMA TX 진행 중 후속 출력 silent drop 버그 수정)
	  · oSerial_Write fallback: pTxBuffer NULL 또는 SizeOfTxBuffer < 50 시 blocking 전송
	  · 링버퍼 적재/콜백에 __disable_irq() / __enable_irq() critical section 추가
	- MISERIAL_TX_BUFFER_SIZE: 1 → 1000 (DMA 링버퍼 비동기 송신 활성화)
2026-06-25 | HW 2.5 | FW 0.8 (continued)
	- ONE_Serial DMA race 수정 + 송신 효율 개선:
	  · oSerial_vPrint/PrintLine/Log: char-by-char → vsnprintf 한 번에 oSerial_Write (256B 로컬 버퍼)
	  · oSerial_Write txInProgress 판정: HW state만 → IsTxBusy(SW) || hwBusy 결합
	    (DMA 종료~콜백 race window 에서 청크 재전송되던 버그 해결)
*/

#endif /* INC_MI_SOFTWAREREVISION_H_ */
