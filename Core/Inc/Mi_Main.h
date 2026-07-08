#ifndef INC_MI_MAIN_H_
#define INC_MI_MAIN_H_

#define MI_MAIN_VERSION		0.4

#include "ONE_Math.h"
#include "ONE_Signal.h"
#include "ONE_Time.h"
#include "ONE_Serial.h"
#include "main.h"

#define SLEEPMODE_ENABLE	1
#define DAS_SERIALNUMBER(year,week,number)	((MATH_MAX((uint32_t)(year),2022)-2000) * 1000000) + ((MATH_MIN((uint32_t)(week),52)) * 10000) + (uint32_t)(MATH_MIN(number,9999))

#pragma pack(1)
#pragma pack()


typedef struct{
	struct{
		uint8_t LoRaEnable;
		uint8_t ADCRefEnable;
		uint8_t CANEnable;
		uint8_t NANDEnable;
		uint8_t PwrEnable_U0;
		uint8_t PwrEnable_U1;
		uint8_t PwrSupplySel;

		uint8_t AmpEnable;
		uint8_t CurrentModeEnable;
		uint8_t PwrSupply5VEnable;

		uint8_t OperatingLED;
	}DO;

	struct{
		uint8_t UsbConnected;
	}DI;

	struct{
		uint16_t SystemSupply;
		uint16_t InternalBAT;
	}ADC;
}GPIOs_t;

extern GPIOs_t GPIOs;

#if defined(IWDG_PRESCALER_4)
extern IWDG_HandleTypeDef hiwdg;
#endif

extern ADC_HandleTypeDef hadc1;
extern CAN_HandleTypeDef hcan1;
extern I2C_HandleTypeDef hi2c2;
extern QSPI_HandleTypeDef hqspi;

extern UART_HandleTypeDef hlpuart1;
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart3;
extern DMA_HandleTypeDef hdma_lpuart_rx;
extern DMA_HandleTypeDef hdma_lpuart_tx;
extern DMA_HandleTypeDef hdma_usart1_rx;
extern DMA_HandleTypeDef hdma_usart1_tx;
extern DMA_HandleTypeDef hdma_usart2_rx;
extern DMA_HandleTypeDef hdma_usart2_tx;
extern DMA_HandleTypeDef hdma_usart3_rx;
extern DMA_HandleTypeDef hdma_usart3_tx;

extern void MiMain(void);

#endif /* INC_MI_MAIN_H_ */

/* History

2026-06-26 | v0.1
	- baseline (Mi_Main.h)
2026-06-29 | v0.2
	- Mi_Main_SIC100.c: MiMain_UpdateSampling 시퀀스 재작성
	  · MiMain_UpdateMeasure와 동일 골격, 재측정(retry) 알고리즘만 제거
	  · ChannelNo 순회 로직 제거 (Measurement_Sensor가 통합 처리)
	  · 1회 측정 → emit → DONE 단순 흐름
	- Mi_Main_SIC100.c: MiMain_UpdateStatus 시그니처 확장
	  · oResult_t (void) → oResult_t (IoT_DataPacket_t **ppPacket)
	  · IoTDataStatus_t 페이로드 packing 로직 추가 (SoftwareVersion / StatusBits / SystemSupply / TroubleCode)
	  · *ppPacket = &DataPacket 로 호출자에게 반환 (기존 ppPacket = ... 오타 수정)
	- Mi_Main_SIC100.c: MiIoT_Status.UpdateTimestmap → UpdateSupplyTimestamp 필드명 정합화
2026-07-07 | v0.3
	- Mi_Main_SIC100.c: MiMain_UpdateMeasure 재작성 — SIM100 신형 패턴 적용
	  · 시작 시 Measurement_Supply(20) + 저전압(3200mV 미만) 가드 → RESULT_FAULT
	  · 채널별 확정 비트맵(ChDoneBits) 도입 — 실패 채널만 재측정, 전 채널 확정 시 emit
	  · 재측정 대기 중 RESULT_WAIT 반환 (공통 Mi_IoT v0.2 계약)
	  · MEASURE_UPDATE_RETRY_MAX(10) 전체 상한 + 채널별 RetryCount 도달 시 현재 값 수락
	  · StableData 합성·자기 일관성 2회 검증 제거 (채널별 RetryCount cap으로 대체)
	  · MiMain_IsError/GetMeasureStableData → MiMain_DataIsError(채널 단위)로 교체
	- Mi_Main_SIC100.c: MiMain_UpdateSampling에 공급전압 선검사 + 저전압 가드 추가 (SIM100 동일)
	- Mi_Main_SIC100.c: MiMain() case 1에 HAL_I2C_DeInit(&hi2c2) 추가 (SIM100 동일)
2026-07-08 | v0.4 (재검증 반영)
	- MiMain_GetMeasureStableData 재작성 — Sampling 실패값이 Best 오염 방지:
	  · 기본 반환값 base = *pBest (Sampling 실패 시 Best 이전값 유지 보장)
	  · pS->Analog.Type == NULL → 해당 채널 Best 유지 (기존 로직은 pS 강제 덮어씀 → 회귀)
	  · pS->TiltArray.Type == NULL → CH1 Best 유지
	  · 동률 오차는 Best 유지로 통일 (`<`, 기존은 `<=`로 sampling 우선 → 안정성 저하)
	  · Time 필드 소스: pRef → pS (새 측정 시각 반영, 기존은 오래된 Reference 시각)
	  · CH1 memcpy → 구조체 대입(TiltArray = pS->TiltArray)으로 union 안전성 확보
	  · pBest->DLC == 0 방어 분기 추가 (첫 진입 sampling 채택)
2026-07-08 | v0.4
	- Mi_Main_SIC100.c: 재측정 로직에 Best 값 합성 복원 + Type=NULL 실패 마커 규약 통합
	  · MiMain_GetMeasureStableData(pReference, pBest, pSampling) 신규 — 요소별 best 선택
	    Reference(PastSampling) 대비 Best와 Sampling 중 오차 작은 쪽을 요소 단위(Analog.Data,
	    Dual[i].AxisX/Y, Single[i].Axis)로 선택하여 새 Best 반환. 재측정 사이 누적 개선.
	  · MiMain_UpdateMeasure: static BestSampling 도입, case 0에서 zero init
	  · case 2 흐름: 첫 사이클 Best=SamplingData → 이후 Best=GetMeasureStableData(Past, Best, New)
	    → Best 오차 검사 → 통과 시 PastSampling=Best, *ppPacket=&BestSampling
	    → 실패 시 SamplingData의 실패 채널 Type=NULL로 세팅 (다음 사이클 부분 재측정 유도)
	  · Type=NULL 실패 마커 규약: sub-func 실패 or 오차 초과 판정 채널을 NULL로 표시
	    → Measurement_Sensor case 1이 Type==NULL 채널만 재측정 (부분 재측정 지원)
2026-07-07 | v0.31
	- Mi_Main_SIC100.c: Analog 실패 마커 소비측을 새 규약(Data=0)으로 정합화
	  · UpdateMeasure 재측정 트리거: pSampling->Analog.Type == NULL → .Data == 0
	    (Measurement_Analog v0.2에서 실패 시에도 Type = TypeOfSensor 유지하므로 Type 검사 dead)
	  · MiMain_DataIsError mV/mA 케이스: Data==0 배제 가드 추가
	    (실패 마커를 -34000mV로 오인해 다음 정상값과 큰 오차로 판정 → 불필요한 재측정 반복 방지)
	- Mi_Main_SIC100.c: MiMain_UpdateStatus 60초 가드 즉시 반환 경로 추가
	  · Measurement_Supply 재실행 창(60초) 안쪽이면 else { result = RESULT_OK; } 로
	    이전 캐시된 MiIoT_Status(SoftwareVersion/StatusBits/SystemSupply/TroubleCode)를
	    그대로 DataPacket에 packing 후 즉시 OK 반환 → 상위 시퀀스가 STEP 전진 가능
	  · 기존 흐름은 60초 가드 실패 시 result가 RESULT_RUN으로 유지되어 status 시퀀스 stall
*/
