/*
 * Mi_Revision.h
 *
 *  Created on: Dec 16, 2024
 *      Author: JONE
 */

#ifndef INC_MI_SOFTWAREREVISION_H_
#define INC_MI_SOFTWAREREVISION_H_

#include "Mi_Main.h"

#define MI_SW_REVISION				0.55

/* History

2025-08-10 | HW - | FW 0.1
	- Start
2026-03-27 | HW - | FW 0.2
	- 각도측정/센서교정/현시점 SIC/SIM/SIV IOT 알고리즘 동일 적용
2026-04-01 | HW - | FW 0.3
	- 공통 라이브러리 메모리 안전성 패치 적용 (OneLibrary/oThirdParty)
	  NULL 포인터 검증, sprintf→snprintf, DMA 에러복구, 버퍼 오버플로우 수정
2026-05-08 | HW - | FW 0.4
	- VDD 동적 보정 추가 (VREFINT 기반): SystemSupply 온도 드리프트 개선
	- ADC offset 교정을 Measurement_CalibrateVDD로 단일화 (Native_ADCRead 중복 제거)
	- MiMain_GPIOInit/DeInit 리팩터링:
	  · 불필요 초기 출력값 IO_WRITE 블록 제거, 변수명 cfg로 단축
	  · Init 시작부에 MiMain_GPIOControl() 호출 추가 → 마지막 IO 상태 복원
	  · DeInit은 출력 핀을 ANALOG 모드로 전환 (저전력)
	  · LORA_ENABLE Init/DeInit을 MiLoRa_IsSleep 조건부로 변경
	- MiIoT_Sleep LED 시퀀스 단순화: ON 50ms / OFF 200ms 명시 사이클
	- SYSTEM_SUPPLY_LOW_LIMIT 3200mV로 통일
	- Mi_LoRa.c/h를 SIM100에서 복사하여 MiLoRa_IsSleep 플래그 제공
	- LoRa Join 실패 시 backoff 적용: 5s → 10s → 20s → 40s → 80s → 160s → 300s(cap, 5min)
2026-05-08 | HW - | FW 0.5
	- LoRa Open 안정화 개선:
	  · 전원 OFF 대기 500ms → 1000ms
	  · 전원 ON 부팅 대기 500ms → 2000ms
	  · AT 명령 실패 시 Close 후 즉시 재Open (Step 1 복귀), MiLoRa_RadioFailCount 누적
	  · 단계별 진단 로그 추가 (LoRaEnable=0, Power off, Off wait done, boot wait, error restart)
2026-05-11 | HW - | FW 0.51
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
2026-05-12 | HW - | FW 0.52
	- LoRa Join 안정화 대기 추가 (MILORA_JOIN_STABILIZE_MS = 10000ms):
	  · Join/Open 직후 라디오 link 안정화로 첫 송신 패킷(FCnt 0,1) 손실 방지
	  · scheduler case 1에서 oTMR_Elapsed로 10초 대기 후 송신 시작
2026-05-15 | HW - | FW 0.53
	- LoRa Encoder DataArray 케이스 누락 수정:
	  · MiLoRa_Encode switch에 IoTDataType_DataArray 분기 없어 모든 측정값 송신 실패
	    ("Mailbox encode error" 로그 후 폐기)
	  · Analog 패턴 본떠서 시퀀스 헤더(1) + Time(6) + Items 분할 인코딩 추가
	- LORA_DATAARRAY_SPLIT_COUNT = 7 (시퀀스당 7개, 페이로드 49B / 51B 한도)
	  · 코덱 R10/R11의 SplitSensorCount=7과 일치
2026-05-15 | HW 0.1 | FW 0.54
	- RAK3172_Transmit hex 송신 잔존 데이터 버그 수정:
	  · oHexStringFromBytes가 null-termination 안 함 → TxStringBuffer에 남은 이전 송신 hex가
	    strlen 길이로 같이 전송 (짧은 페이로드일 때 뒷부분 옛 데이터가 그대로 송신)
	  · 예: 직전 Operating 5B(30 hex) 후 Status 10B(20 hex) 송신 → 15B로 송신되어 서버 디코드 실패
	  · pStr[SizeofData * 2] = '\0' 명시적 종료 추가 (LoRa_RAK3172.c:388)
2026-05-21 | HW 0.1 | FW 0.55
	- MiLoRa_SendMailbox 전송 실패 시 백오프 적용:
	  · 실패 시 case 2로 복귀 → 5초 × SendTryCount 대기 후 재전송
	  · 첫 시도는 즉시 송신 (SendTryCount==0 분기), 재시도마다 +5초 누적 (5/10/15초)
	  · SendErrorCount 리셋을 case 1(새 시퀀스 시작)로 이동 → QoS 재시도 카운트 유실 방지
	- IoT 제품 코드 enum 리네이밍: `IoTProductType_SIA100_2A` -> `IoTProductType_SIA100_SD`
      (Mi_IoT.h + Mi_IoT_SIA100_VB.c 동기화, 값 5 유지)
2026-06-17 | HW 0.1 | FW 0.55 (continued)
	- RAK3172 Open 시퀀스 안정화: LPM wakeup / AT 검증 / NWM·NJM 선설정 / 무한루프 보호
	- Mi_LoRa.c 디바이스별 인코더 분리: Mi_LoRa_SIA100VB.c 신규 (Tilt/Status/Operating 분기만 유지)
	- enum 재편: DataArray → Type1(100, VMX)/Type2(101, SIC100-2C), Vibration 5번
2026-06-23 | HW 0.1 | FW 0.55 (continued)
	- MiSerial_Handler 초기화 designated initializer로 수정 (pRxBuffer가 IndexOfTxFirst에 박히던 버그 해결)
2026-06-24 | HW 0.1 | FW 0.55 (continued)
	- ONE_Serial TX 비동기 송신 안정화 (PHM 검증 후 전 프로젝트 동기화):
	  · oSerial_PutChar / vPrint / Printf / PrintLine: gState != READY 가드 제거
	    (DMA TX 진행 중 후속 출력 silent drop 버그 수정)
	  · oSerial_Write fallback: pTxBuffer NULL 또는 SizeOfTxBuffer < 50 시 blocking 전송
	  · 링버퍼 적재/콜백에 __disable_irq() / __enable_irq() critical section 추가
	- MISERIAL_TX_BUFFER_SIZE: 1 → 1000 (DMA 링버퍼 비동기 송신 활성화)
2026-06-25 | HW 0.1 | FW 0.55 (continued)
	- ONE_Serial DMA race 수정 + 송신 효율 개선:
	  · oSerial_vPrint/PrintLine/Log: char-by-char → vsnprintf 한 번에 oSerial_Write (256B 로컬 버퍼)
	  · oSerial_Write txInProgress 판정: HW state만 → IsTxBusy(SW) || hwBusy 결합
	    (DMA 종료~콜백 race window 에서 청크 재전송되던 버그 해결)
*/

#endif /* INC_MI_SOFTWAREREVISION_H_ */
