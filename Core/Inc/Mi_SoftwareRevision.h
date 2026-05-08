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

2024-12-16	|  0.1
	- Start
2025-03-17	|  0.2
	- HW2.0 펌웨어
2025-11-11	|  0.3
	- CAN 데이터 측정/스캔/3DS 측정 추가 / LoRa RxDealy 2로 변경
2025-12-09	|  0.4
	- 측정시 진동으로 인한 오차 발생하였을때 직전 측정값 대비 오차르 확인하고 재측정 하는 알고리즘 추가 (3DS 레이일변이)
2026-01-30	|  0.5
	- Revision No 수정 2.X -> 0.X
	- 시간 동기화
	   1. 정오12시에 1회 동기화
	   2. RTC 현재 시간과 비교해서 30분이상 다르면 동가화 재시도
	   3. 3회 동기화 재시도 후 에도 시간이 30분이상 다르다면 그냥 시간 동기화 완료 함
2026-03-05	|  0.6
	- RAK3172 LinckCheck 설정 값 2에서 1로 수정
    - 재측정 알고리즘 수정
      1. 오차 작은 값만 가져다 최종 측정값으로 사용하도록 수정
      2. 최종 측정값이 모두 완성되면 즉시 측정 종료
2026-03-11	|  0.61
	- NAND 플래시에 파레마터 저장 추가
	- LoRa Open/Transmit 시 RESULT_NULL 시 메일박스 삭제
	- RAK3172 ATE 실패히도 다음 시퀀스 넘어가도록 수정
	- Dwonlink DeviceInfo 설정 추가
	- MiIoT DataPacket Pointer 수정 (pPacket->ppPacket)
2026-04-01	|  0.7
	- 공통 라이브러리 메모리 안전성 패치 적용 (OneLibrary/oThirdParty)
	  NULL 포인터 검증, sprintf→snprintf, DMA 에러복구, 버퍼 오버플로우 수정
2026-05-08	|  0.8
	- VDD 동적 보정 추가 (VREFINT 기반): SystemSupply 온도 드리프트 개선
	- ADC offset 교정을 Measurement_CalibrateVDD로 단일화 (Native_ADCRead 중복 제거)
	- MiMain_GPIOInit/DeInit 리팩터링:
	  · 불필요 초기 출력값 IO_WRITE 블록 제거, 변수명 cfg로 단축
	  · Init 시작부에 MiMain_GPIOControl() 호출 추가 → 마지막 IO 상태 복원
	  · DeInit은 출력 핀을 ANALOG 모드로 전환 (저전력)
	  · LORA_ENABLE Init/DeInit을 MiLoRa_IsSleep 조건부로 변경
	- MiIoT_Sleep LED 시퀀스 단순화: ON 50ms / OFF 200ms 명시 사이클
	- SYSTEM_SUPPLY_LOW_LIMIT 3200mV로 통일
*/

#endif /* INC_MI_SOFTWAREREVISION_H_ */
