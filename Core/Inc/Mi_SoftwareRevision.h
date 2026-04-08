/*
 * Mi_Revision.h
 *
 *  Created on: Dec 16, 2024
 *      Author: JONE
 */

#ifndef INC_MI_SOFTWAREREVISION_H_
#define INC_MI_SOFTWAREREVISION_H_

#include "Mi_Main.h"

#define MI_SW_REVISION				0.4

/* History

2024-12-16	|  0.1
	- Start
2025-03-25	|  0.2
	- 하드웨어 2.0 버전 펌웨어 적용
	- LoRa Tiemsync LoRaNode에서 처리
	- LoRa 모듈 펌웨어 R16
2026-03-11	|  0.3
	- NAND 플래시에 파레마터 저장 추가
	- LoRa Open/Transmit 시 RESULT_NULL 시 메일박스 삭제
	- RAK3172 ATE 실패히도 다음 시퀀스 넘어가도록 수정
	- Dwonlink DeviceInfo 설정 추가
	- MiIoT DataPacket Pointer 수정 (pPacket->ppPacket)
	- DCDC On 후 대기시간 수정 (500ms)
	- Relay Off 대기시간 (100ms) / On 대기시간(150ms) 추가
2026-04-01	|  0.4
	- 공통 라이브러리 메모리 안전성 패치 적용 (OneLibrary/oThirdParty)
	  NULL 포인터 검증, sprintf→snprintf, DMA 에러복구, 버퍼 오버플로우 수정
*/

#endif /* INC_MI_SOFTWAREREVISION_H_ */
