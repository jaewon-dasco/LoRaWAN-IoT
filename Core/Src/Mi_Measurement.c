/*
 * Mi_Measurement.c
 *
 *  Created on: Dec 9, 2024
 *      Author: JONE
 *
 *  통합 측정 시퀀스 (FIFO + STOP1 슬립 + 직접 구현 FFT)
 *   ① FIFO watermark=30 sets(=90 slots) 설정
 *   ② Native_SleepMode 진입 (INT1 EXTI 또는 RTC backstop wake)
 *   ③ FIFO 30 세트 읽기 (~60 ms 분 가속도)
 *   ④ 각 세트마다: SW LPF(0.1Hz) → Pitch/Roll 보상 → oMoveAverage(25)
 *   ⑤ 각 세트마다: SW LPF(100Hz) → 적분 → SW HPF(1Hz) → PPV / VelocityRing
 *   ⑥ 1초 윈도우(500 세트) 충족 시 직접 radix-2 FFT 512 → fx/y/z 산출
 *   ⑦ 임계 초과 → IoTDataVibration 패킷 패킹·반환, 미달 → 다시 슬립
 */

#include <math.h>
#include "ONE_Math.h"
#include "ONE_Filter.h"
#include "ONE_Common.h"
#include "Mi_Native.h"
#include "Mi_IoT.h"
#include "Mi_Main.h"
#include "Mi_Measurement.h"
#include "Mi_Serial.h"

/* VREFINT_CAL_ADDR / VREFINT_CAL_VREF 은 HAL (stm32l4xx_ll_adc.h)에서 제공 */
#define ADC_VREF						(MiIoT_Parameter.SystemConfig.ActualVRef != 0 ? MiIoT_Parameter.SystemConfig.ActualVRef : g_VddaActual_mV)
#define ADC_MAXDIGIT					4095.0f
#define ADC_TO_AI(x)					((double)(x)/(double)ADC_MAXDIGIT*(double)ADC_VREF)
#define ADC_TO_SUPPLY(x)				(ADC_TO_AI(x)/VOLT_DIV_RATIO(6980,20000))
#define ADC_TO_VBAT(x)					(ADC_TO_AI(x)*3)

#define SUPPLY_AVERAGE_SIZE				15
#define ANGLE_AVERAGE_SIZE				25

#define MEASUREMENT_ODR_HZ				500
#define MEASUREMENT_SAMPLE_DT			(1.0f / (float)MEASUREMENT_ODR_HZ)    /* 2 ms */
#define MEASUREMENT_FIFO_SETS_WMARK		30                                    /* watermark = 30 sets = 90 slots */
#define MEASUREMENT_FIFO_BATCH_BYTES	(MEASUREMENT_FIFO_SETS_WMARK * 9U)    /* 270 bytes */
#define MEASUREMENT_BATCH_PERIOD_MS		60                                    /* 30 sets / 500 Hz, RTC backstop */
#define MEASUREMENT_WINDOW_SETS			500                                   /* 1 s = 500 sets */
#define MEASUREMENT_FFT_SIZE			512
#define MEASUREMENT_STABILIZE_MS		250                                   /* HPF 안정화 */
#define MEASUREMENT_POWER_WAIT_MS		100

#define MEASUREMENT_PPV_THRESHOLD_MMS	0.10f                                 /* PPVmax 임계 (축별 max) — DIN 4150-3 표준 부합 */
#define MEASUREMENT_EVENT_PRETRIGGER_SETS  3                                /* 임계 초과 시 직전 N초 pre-trigger 전송 */
#define MEASUREMENT_OUTPUT_PERIOD_SETS	50                                  /* 결과 갱신 주기 (50 sets = 0.1s = 10 Hz output) */
#define MEASUREMENT_SLIDING_WINDOW_SETS	MEASUREMENT_WINDOW_SETS             /* sliding PPV/FFT 입력 윈도우 (500 = 1s) */
#define MEASUREMENT_PPV_SCAN_PERIOD_SETS	5                               /* PPV scan 빈도 (5 sets = 10ms = 100 Hz, 부하 ↓) */

/* ─────────────────────────────────────────────────────────────────────────
 * 측정 방식 토글
 *   1 = FIFO batch + STOP1 슬립 (배터리 절약, 상용 모드)
 *   0 = Continuous polling (실시간 디버깅·값 검증 모드)
 *   검증 완료 후 1로 변경하여 재빌드.
 * ────────────────────────────────────────────────────────────────────── */
#define MEASUREMENT_USE_FIFO			1

#define MEASUREMENT_FC_LPF_DYN_HZ		100.0f
#define MEASUREMENT_FC_HPF_DYN_HZ		1.0f

/* ============================================================================
 * Globals — 외부 노출
 * ========================================================================= */

ADXL355_t           ADXL355;
TMP1075_t           TMP1075;

oRPY_t              Measure_RawAngle;
oRPY_t              Measure_CalibratedAngle;
double              Measure_Temperature = 0.0;
Measure_Vibrating_t  Measure_Vibrating;        /* live 누적 (윈도우 진행 중) */
Measure_Vibrating_t  Measure_Vibrating_Last;   /* 마지막 완료 윈도우 — Sensor·history push용 */

/* 1분 링버퍼 — ONE_RingBuffer 사용 (매 1초 윈도우 완료 시 Measure_Vibrating snapshot push) */
static uint8_t       VibratingHistoryStorage[MEASURE_HISTORY_SIZE * sizeof(Measure_Vibrating_t)];
oRingBuffer_t        Measure_VibratingHistoryRB;

/* ============================================================================
 * 공급전압 측정
 * ========================================================================= */

uint16_t SupplyAverageBuffer[2][SUPPLY_AVERAGE_SIZE];
oMoveAverage_t SupplyAverage[2] = {\
	MOVEAVERAGE_INITIALIZER(&SupplyAverageBuffer[0], sizeof(SupplyAverageBuffer[0]), DataType_UInt16),\
	MOVEAVERAGE_INITIALIZER(&SupplyAverageBuffer[1], sizeof(SupplyAverageBuffer[1]), DataType_UInt16),\
};

static uint32_t g_VddaActual_mV = 2800;

/* ============================================================================
 * 공통 필터·적분기 타입 (OneLibrary oFilter_t 사용 — Biquad 2차 IIR Butterworth)
 * ========================================================================= */

typedef struct {
	float PrevAccel;
	float Velocity;
} Measurement_Integrator_t;

typedef struct {
	oFilter_t                Lpf;          /* 100 Hz LPF — 가속도 노이즈 제거 */
	Measurement_Integrator_t Integrator;
	oFilter_t                Hpf;          /* 1 Hz HPF — 속도 드리프트 제거 */
} Measurement_AxisPipeline_t;

/* ============================================================================
 * 각도 경로 — SW LPF(0.1Hz) + 6차 다항식 보상 + oMoveAverage(25)
 * ========================================================================= */

static double AngleAvrgBuffer[2][ANGLE_AVERAGE_SIZE];
static oMoveAverage_t AngleAvrg[2] = {\
	MOVEAVERAGE_INITIALIZER(&AngleAvrgBuffer[0], sizeof(AngleAvrgBuffer[0]), DataType_Double),\
	MOVEAVERAGE_INITIALIZER(&AngleAvrgBuffer[1], sizeof(AngleAvrgBuffer[1]), DataType_Double),\
};

/* 최신 가속도 (mG) — 매 sample 갱신, Sensor가 각도 계산에 사용
 *   ADXL355 HW LPF가 이미 평활화하므로 SW 평균 불필요 */
static oVector3_t Measure_LatestAccel_mG;

/* ============================================================================
 * 진동 경로 — 동적 파이프라인 + 속도 링버퍼
 * ========================================================================= */

static Measurement_AxisPipeline_t Pipeline[3];

static int16_t  VelocityRingBuf[MEASUREMENT_FFT_SIZE][3];   /* mm/s × 10 */
static uint16_t VelocityRingIdx;
static uint16_t VelocityRingCount;

/* ============================================================================
 * FIFO read buffer — SRAM 배치
 * ========================================================================= */

#if MEASUREMENT_USE_FIFO
static uint8_t FIFOReadBuf[MEASUREMENT_FIFO_BATCH_BYTES];
#endif

/* ============================================================================
 * 직접 구현 FFT — radix-2 in-place (CMSIS-DSP 의존 제거)
 *   - Twiddle, Hanning 테이블은 .ram2 (boot 시 1회 채움)
 *   - 작업 버퍼 Re/Im 도 .ram2
 * ========================================================================= */

__attribute__((section(".ram2"))) static float FFT_WorkRe[MEASUREMENT_FFT_SIZE];
__attribute__((section(".ram2"))) static float FFT_WorkIm[MEASUREMENT_FFT_SIZE];
__attribute__((section(".ram2"))) static float FFT_TwiddleRe[MEASUREMENT_FFT_SIZE / 2];
__attribute__((section(".ram2"))) static float FFT_TwiddleIm[MEASUREMENT_FFT_SIZE / 2];
__attribute__((section(".ram2"))) static float FFT_HannWin[MEASUREMENT_FFT_SIZE];

static uint8_t FFT_Initialized = 0;

/* 1초 윈도우 누적 카운터 */
static uint16_t WindowSetCount = 0;

/* 상태 플래그 */
static uint8_t Measurement_Initialized = 0;
static uint8_t Measurement_EventActive = 0;   /* PVS > 임계 동안 1 (rising→falling edge 추적) */

/* 현재 동작 중인 센서 타입 캐시 — 전환 감지용 */
static IoTSensorType_t g_RunningSensorType = IoTSensorType_NULL;

/* 모드 전환 감지 — 변경됐으면 모든 리소스 닫고 Initialized=0 → 다음 tick에 Init 재실행 */
static void Measurement_HandleModeTransition(void)
{
	IoTSensorType_t current = MiIoT_Parameter.ChannelConfig[0].TypeOfSensor;
	if(current == g_RunningSensorType) return;

	/* 모드 변경됨 — 리소스 정리 */
	if(ADXL355.State.IsOpen) ADXL355_Close(&ADXL355);
	if(TMP1075.State.IsOpen) TMP1075_Close(&TMP1075);
	GPIOs.DO.MEMSEnable = 0;
	GPIOs.DO.TEMPEnable = 0;

	/* 상태 플래그 리셋 */
	Measurement_Initialized = 0;
	Measurement_EventActive = 0;
	WindowSetCount = 0;
	memset(&Measure_Vibrating_Last, 0, sizeof(Measure_Vibrating_Last));
	memset(&Measure_LatestAccel_mG, 0, sizeof(Measure_LatestAccel_mG));
	VelocityRingIdx = 0;
	VelocityRingCount = 0;

	/* 링버퍼·이동평균 초기화 */
	Measure_VibratingHistoryRB.Count = 0;
	Measure_VibratingHistoryRB.FirstIndex = 0;
	Measure_VibratingHistoryRB.LastIndex = 0;
	AngleAvrg[0].Reset = 1;
	AngleAvrg[1].Reset = 1;

	g_RunningSensorType = current;
}

/* ============================================================================
 * 내부 함수 prototype
 * ========================================================================= */

static void     Measurement_InitPipeline(void);
static void     Measurement_ResetWindow(void);
static float    Measurement_Integrate(Measurement_Integrator_t *intg, float accel);
static void     Measurement_ProcessSet(const oVector3_t *pAccel_uG);
static void     Measurement_InitFFT(void);
static void     Measurement_FFT512(float *re, float *im);
static float    Measurement_CalcAxisFreq(int axis);
static oResult_t Measurement_CalcFFT(void);
static oResult_t Measurement_ReadTempOnce(double *pTemp);
static void      Measurement_ScanRingForPPV(void);    /* sliding window — ring 스캔으로 PPV/PVS 계산 */
#if MEASUREMENT_USE_FIFO
static oResult_t Measurement_ConfigFIFO(void);
#endif
static void      Measurement_FillVibrationPacket(IoT_DataPacket_t *pPacket, const Measure_Vibrating_t *pSamples, uint8_t count);
static void      Measurement_QueueEventSample(const Measure_Vibrating_t *pSrc);
static void      Measurement_SendPreTrigger(uint8_t records);
static oResult_t Measurement_ReadAngle(oRPY_t *pRPY);     /* Tilt 모드 — SIA100_SD 패턴 */

/* ============================================================================
 * 공급전압 측정 (유지)
 * ========================================================================= */

oResult_t Measurement_CalibrateVDD(void)
{
	static uint8_t CalStep = 0;
	uint16_t vrefint_raw;
	oResult_t result = RESULT_RUN;

	switch(CalStep)
	{
		case 0:
			HAL_ADC_Stop(&hadc1);
			if(HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED) == HAL_OK){
				oSerial_Log("Mearment", "ADC offset calibrated");
			}
			CalStep++;
			break;
		case 1:
			result = Native_ADCRead(ADC_CHANNEL_VREFINT, ADC_SAMPLETIME_640CYCLES_5, &vrefint_raw, 20);
			if(result == RESULT_OK){
				if(vrefint_raw > 0){
					uint32_t vdda = ((uint32_t)VREFINT_CAL_VREF * (uint32_t)(*VREFINT_CAL_ADDR)) / vrefint_raw;
					if(vdda >= 2000 && vdda <= 3600){
						g_VddaActual_mV = vdda;
					}
				}
				oSerial_Log("Mearment", "VrefCal VDDA=%lu mV", (unsigned long)g_VddaActual_mV);
			}
			break;
	}

	if(result != RESULT_RUN){
		CalStep = 0;
	}

	return result;
}

oResult_t Measurement_Supply(uint8_t Count)
{
	static uint8_t ReadSupplyStep = 0;
	static uint32_t ReadSupplyTimer = 0;

	uint16_t Data = 0;
	oResult_t result = RESULT_RUN;

	switch(ReadSupplyStep)
	{
		case 0:
			memset(&SupplyAverageBuffer, 0, sizeof(SupplyAverageBuffer));
			ReadSupplyStep++;
		case 1:
			ReadSupplyTimer = oTMR_GetTick(TICKBASE_SYSTICK);
			GPIOs.DO.ADCRefEnable = 1;
			ReadSupplyStep++;
		case 2:
			if(oTMR_Trigger(&ReadSupplyTimer, 5, 1, TICKBASE_SYSTICK)){
				ReadSupplyStep++;
			}
			break;
		case 3:
			if(Measurement_CalibrateVDD() != RESULT_RUN){
				ReadSupplyStep++;
			}
			break;
		case 4:
			switch(Native_ADCRead(ADC_CHANNEL_3, ADC_SAMPLETIME_247CYCLES_5, &Data, Count))
			{
				case RESULT_RUN: break;
				case RESULT_OK:
					Data = ADC_TO_SUPPLY(Data);
					oMoveAverage_SetData(&SupplyAverage[0], &Data);
					oMoveAverage_GetData(&SupplyAverage[0], &GPIOs.ADC.SystemSupply);
					ReadSupplyStep++;
					break;
				default: ReadSupplyStep++; break;
			}
			break;
		case 5:
			switch(Native_ADCRead(ADC_CHANNEL_VBAT, ADC_SAMPLETIME_247CYCLES_5, &Data, Count))
			{
				case RESULT_RUN: break;
				case RESULT_OK:
					Data = ADC_TO_VBAT(Data);
					oMoveAverage_SetData(&SupplyAverage[1], &Data);
					oMoveAverage_GetData(&SupplyAverage[1], &GPIOs.ADC.InternalBAT);
					ReadSupplyStep++;
					break;
				default: ReadSupplyStep++; break;
			}
			break;
		case 6:
			result = RESULT_OK;
			break;
	}

	if(result != RESULT_RUN){
		ReadSupplyStep = 1;
		GPIOs.DO.ADCRefEnable = 0;
	}

	return result;
}

/* ============================================================================
 * 필터·적분 기본 연산
 * ========================================================================= */

static float Measurement_Integrate(Measurement_Integrator_t *intg, float accel)
{
	intg->Velocity += 0.5f * (accel + intg->PrevAccel) * MEASUREMENT_SAMPLE_DT;
	intg->PrevAccel = accel;
	return intg->Velocity;
}

/* ============================================================================
 * 파이프라인 초기화
 * ========================================================================= */

static void Measurement_InitPipeline(void)
{
	const double Fs = (double)MEASUREMENT_ODR_HZ;          /* ADXL355 ODR */
	const double Q  = 0.707;                                /* Butterworth flat response */

	for(int i = 0; i < 3; i++){
		/* 동적 100 Hz LPF — 가속도 노이즈 제거 */
		Pipeline[i].Lpf = (oFilter_t)FILTER_PASS_INITIALIZER(Fs, (double)MEASUREMENT_FC_LPF_DYN_HZ, Q);
		/* 동적 1 Hz HPF — 속도 드리프트 제거 */
		Pipeline[i].Hpf = (oFilter_t)FILTER_PASS_INITIALIZER(Fs, (double)MEASUREMENT_FC_HPF_DYN_HZ, Q);

		Pipeline[i].Integrator.PrevAccel = 0;
		Pipeline[i].Integrator.Velocity = 0;
	}

	memset(&Measure_LatestAccel_mG, 0, sizeof(Measure_LatestAccel_mG));

	VelocityRingIdx = 0;
	VelocityRingCount = 0;
	WindowSetCount = 0;

	AngleAvrg[0].Reset = 1;
	AngleAvrg[1].Reset = 1;

	Measurement_ResetWindow();
}

static void Measurement_ResetWindow(void)
{
	Measure_Vibrating.PPV_X = 0;
	Measure_Vibrating.PPV_Y = 0;
	Measure_Vibrating.PPV_Z = 0;
	Measure_Vibrating.PVS   = 0;
	Measure_Vibrating.Freq_X = 0.0f;
	Measure_Vibrating.Freq_Y = 0.0f;
	Measure_Vibrating.Freq_Z = 0.0f;
}

/* ============================================================================
 * 1세트 처리 — 정적/동적 양 경로 + 각도 oMoveAverage
 * ========================================================================= */

static void Measurement_ProcessSet(const oVector3_t *pAccel_uG)
{
	/* µg → mG */
	float accel_mG[3] = {
		(float)(pAccel_uG->X / 1000.0),
		(float)(pAccel_uG->Y / 1000.0),
		(float)(pAccel_uG->Z / 1000.0),
	};

	/* 최신 가속도 스냅샷 (각도용) — 매 sample 단순 저장 (ADXL HW LPF로 이미 평활화) */
	Measure_LatestAccel_mG.X = (double)accel_mG[0];
	Measure_LatestAccel_mG.Y = (double)accel_mG[1];
	Measure_LatestAccel_mG.Z = (double)accel_mG[2];

	/* [동적] LPF 100Hz → Gal → 적분 → HPF 1Hz → 속도 → PPV / 링버퍼 */
	float vx_mms = 0, vy_mms = 0, vz_mms = 0;

	for(int axis = 0; axis < 3; axis++){
		Measurement_AxisPipeline_t *p = &Pipeline[axis];

		float filtered = (float)oFilter_LPF(&p->Lpf, (double)accel_mG[axis]);
		float accel_gal = filtered * 0.980665f;
		float vel_cms = Measurement_Integrate(&p->Integrator, accel_gal);
		float vel_final_cms = (float)oFilter_HPF(&p->Hpf, (double)vel_cms);
		float vel_mms = vel_final_cms * 10.0f;

		switch(axis){
			case 0: vx_mms = vel_mms; break;
			case 1: vy_mms = vel_mms; break;
			case 2: vz_mms = vel_mms; break;
		}

		VelocityRingBuf[VelocityRingIdx][axis] = (int16_t)(vel_mms * 10.0f);
	}

	(void)vx_mms; (void)vy_mms; (void)vz_mms;

	VelocityRingIdx = (VelocityRingIdx + 1) % MEASUREMENT_FFT_SIZE;
	if(VelocityRingCount < MEASUREMENT_FFT_SIZE) VelocityRingCount++;

	/* sliding 1초 윈도우 PPV/PVS — N sample마다 ring 스캔 (MCU 부하 조절)
	 *   500 Hz → 100 Hz 갱신 (PERIOD=5), 부하 15% → 3% */
	{
		static uint8_t ScanCounter = 0;
		if(++ScanCounter >= MEASUREMENT_PPV_SCAN_PERIOD_SETS){
			ScanCounter = 0;
			Measurement_ScanRingForPPV();
		}
	}

	WindowSetCount++;
}

/* ============================================================================
 * Measurement_ScanRingForPPV — sliding 1초 윈도우 PPV/PVS 계산
 *   링버퍼의 최근 MEASUREMENT_SLIDING_WINDOW_SETS 샘플 스캔 → 축별 max + max PVS
 * ========================================================================= */

static void Measurement_ScanRingForPPV(void)
{
	uint16_t count = (VelocityRingCount < MEASUREMENT_SLIDING_WINDOW_SETS) ?
	                  VelocityRingCount : MEASUREMENT_SLIDING_WINDOW_SETS;
	if(count == 0){
		Measure_Vibrating.PPV_X = 0; Measure_Vibrating.PPV_Y = 0; Measure_Vibrating.PPV_Z = 0;
		Measure_Vibrating.PVS = 0;
		return;
	}

	uint16_t startIdx = (uint16_t)((VelocityRingIdx + MEASUREMENT_FFT_SIZE - count) % MEASUREMENT_FFT_SIZE);

	/* 최적화: int16 직접 비교 + sqrt 1회만 (loop 내 sqrtf·division 제거) */
	int32_t maxAbsX = 0, maxAbsY = 0, maxAbsZ = 0;
	int32_t maxPVS_sq = 0;   /* mm/s × 10 단위 squared */

	for(uint16_t i = 0; i < count; i++){
		uint16_t idx = (uint16_t)((startIdx + i) % MEASUREMENT_FFT_SIZE);
		int32_t vx = (int32_t)VelocityRingBuf[idx][0];   /* mm/s × 10, int16 → int32 */
		int32_t vy = (int32_t)VelocityRingBuf[idx][1];
		int32_t vz = (int32_t)VelocityRingBuf[idx][2];

		int32_t ax = (vx < 0) ? -vx : vx;
		int32_t ay = (vy < 0) ? -vy : vy;
		int32_t az = (vz < 0) ? -vz : vz;
		if(ax > maxAbsX) maxAbsX = ax;
		if(ay > maxAbsY) maxAbsY = ay;
		if(az > maxAbsZ) maxAbsZ = az;

		int32_t pvs_sq = vx*vx + vy*vy + vz*vz;
		if(pvs_sq > maxPVS_sq) maxPVS_sq = pvs_sq;
	}

	/* int16(×10) → float(mm/s) — loop 종료 후 1회만 변환 */
	Measure_Vibrating.PPV_X = (float)maxAbsX * 0.1f;
	Measure_Vibrating.PPV_Y = (float)maxAbsY * 0.1f;
	Measure_Vibrating.PPV_Z = (float)maxAbsZ * 0.1f;
	Measure_Vibrating.PVS   = sqrtf((float)maxPVS_sq) * 0.1f;
}

/* ============================================================================
 * 직접 구현 FFT — radix-2 in-place 512점
 *   알고리즘: Cooley-Tukey decimation-in-time
 *   - Twiddle: W_N^k = cos(2π·k/N) − j·sin(2π·k/N), k = 0..N/2−1
 *   - bit-reversal 전개 후 log2(N) 단계 butterfly
 * ========================================================================= */

static void Measurement_InitFFT(void)
{
	if(FFT_Initialized) return;

	const float twoPiOverN = 2.0f * (float)MATH_PI / (float)MEASUREMENT_FFT_SIZE;
	for(int k = 0; k < MEASUREMENT_FFT_SIZE / 2; k++){
		float ang = twoPiOverN * (float)k;
		FFT_TwiddleRe[k] =  cosf(ang);
		FFT_TwiddleIm[k] = -sinf(ang);
	}

	const float twoPiOverNm1 = 2.0f * (float)MATH_PI / (float)(MEASUREMENT_FFT_SIZE - 1);
	for(int i = 0; i < MEASUREMENT_FFT_SIZE; i++){
		FFT_HannWin[i] = 0.5f * (1.0f - cosf(twoPiOverNm1 * (float)i));
	}

	FFT_Initialized = 1;
}

static uint16_t Measurement_BitReverse(uint16_t x, uint8_t bits)
{
	uint16_t r = 0;
	for(uint8_t i = 0; i < bits; i++){
		r = (uint16_t)((r << 1) | (x & 1));
		x >>= 1;
	}
	return r;
}

static void Measurement_FFT512(float *re, float *im)
{
	const uint16_t N = MEASUREMENT_FFT_SIZE;
	const uint8_t  LOG2N = 9;        /* log2(512) */

	/* bit-reverse permutation */
	for(uint16_t i = 0; i < N; i++){
		uint16_t j = Measurement_BitReverse(i, LOG2N);
		if(j > i){
			float tRe = re[i]; re[i] = re[j]; re[j] = tRe;
			float tIm = im[i]; im[i] = im[j]; im[j] = tIm;
		}
	}

	/* butterfly stages */
	for(uint16_t step = 1; step < N; step <<= 1){
		uint16_t halfStep = step;
		uint16_t fullStep = step << 1;
		uint16_t twStride = (uint16_t)(N / fullStep);

		for(uint16_t group = 0; group < N; group += fullStep){
			for(uint16_t k = 0; k < halfStep; k++){
				uint16_t twIdx = (uint16_t)(k * twStride);
				float wRe = FFT_TwiddleRe[twIdx];
				float wIm = FFT_TwiddleIm[twIdx];

				uint16_t i0 = group + k;
				uint16_t i1 = i0 + halfStep;

				float tRe = wRe * re[i1] - wIm * im[i1];
				float tIm = wRe * im[i1] + wIm * re[i1];

				re[i1] = re[i0] - tRe;
				im[i1] = im[i0] - tIm;
				re[i0] = re[i0] + tRe;
				im[i0] = im[i0] + tIm;
			}
		}
	}
}

static float Measurement_CalcAxisFreq(int axis)
{
	uint16_t count = (VelocityRingCount < MEASUREMENT_FFT_SIZE) ? VelocityRingCount : MEASUREMENT_FFT_SIZE;
	uint16_t startIdx = (VelocityRingCount < MEASUREMENT_FFT_SIZE) ? 0 : VelocityRingIdx;

	for(uint16_t i = 0; i < count; i++){
		uint16_t srcIdx = (uint16_t)((startIdx + i) % MEASUREMENT_FFT_SIZE);
		FFT_WorkRe[i] = ((float)VelocityRingBuf[srcIdx][axis] / 10.0f) * FFT_HannWin[i];
		FFT_WorkIm[i] = 0.0f;
	}
	for(uint16_t i = count; i < MEASUREMENT_FFT_SIZE; i++){
		FFT_WorkRe[i] = 0.0f;
		FFT_WorkIm[i] = 0.0f;
	}

	Measurement_FFT512(FFT_WorkRe, FFT_WorkIm);

	float freqResolution = (float)MEASUREMENT_ODR_HZ / (float)MEASUREMENT_FFT_SIZE;  /* ~0.977 Hz/bin */
	uint16_t binMin = 1;
	uint16_t binMax = (uint16_t)(100.0f / freqResolution);
	if(binMax > (MEASUREMENT_FFT_SIZE/2 - 1)) binMax = (MEASUREMENT_FFT_SIZE/2 - 1);

	float maxMag = 0;
	uint16_t maxBin = 0;
	for(uint16_t bin = binMin; bin <= binMax; bin++){
		float mag = FFT_WorkRe[bin] * FFT_WorkRe[bin] + FFT_WorkIm[bin] * FFT_WorkIm[bin];
		if(mag > maxMag){
			maxMag = mag;
			maxBin = bin;
		}
	}

	if(maxMag > 0){
		return (float)maxBin * freqResolution;   /* Hz (실수) */
	}
	return 0.0f;
}

/* FFT를 3 tick에 1축씩 분할 수행 — 단일 tick 부하 170ms → ~57ms 분산
 *   호출 측은 RESULT_OK 반환 시 1초 윈도우 결과 확정으로 처리
 */
static oResult_t Measurement_CalcFFT(void)
{
	static uint8_t AxisStep = 0;

	Measurement_InitFFT();

	switch(AxisStep)
	{
		case 0:
			Measure_Vibrating.Freq_X = Measurement_CalcAxisFreq(0);
			AxisStep = 1;
			return RESULT_RUN;
		case 1:
			Measure_Vibrating.Freq_Y = Measurement_CalcAxisFreq(1);
			AxisStep = 2;
			return RESULT_RUN;
		default:
			Measure_Vibrating.Freq_Z = Measurement_CalcAxisFreq(2);
			AxisStep = 0;
			return RESULT_OK;
	}
}

/* ============================================================================
 * 온도 1회 읽기
 * ========================================================================= */

static oResult_t Measurement_ReadTempOnce(double *pTemp)
{
	static uint8_t Step = 0;
	static uint32_t Timer = 0;
	oResult_t result = RESULT_RUN;

	switch(Step)
	{
		default: Step = 0;
		case 0:
			GPIOs.DO.TEMPEnable = 1;
			Timer = oTMR_GetTick(TICKBASE_SYSTICK);
			Step++;
			break;
		case 1:
			if(oTMR_Elapsed(&Timer, 50, TICKBASE_SYSTICK)) Step++;
			break;
		case 2:
			switch(TMP1075_Open(&hi2c2, &TMP1075))
			{
				default: break;
				case RESULT_OK:    Step++; break;
				case RESULT_ERROR: result = RESULT_ERROR; break;
			}
			break;
		case 3:
			switch(TMP1075_GetData(&TMP1075, 1))
			{
				default: break;
				case RESULT_OK:
					if(pTemp) *pTemp = TMP1075.Temperature;
					result = RESULT_OK;
					break;
				case RESULT_ERROR: result = RESULT_ERROR; break;
			}
			break;
	}

	if(result != RESULT_RUN){
		TMP1075_Close(&TMP1075);
		GPIOs.DO.TEMPEnable = 0;
		Step = 0;
	}

	return result;
}

/* ============================================================================
 * ADXL355 FIFO·INT 설정 — watermark + INT1 routing
 * ========================================================================= */

#if MEASUREMENT_USE_FIFO
static oResult_t Measurement_ConfigFIFO(void)
{
	ADXL355_SetFilter(&ADXL355, 0, MEASUREMENT_ODR_HZ);
	ADXL355.Register.Parameter.POWER_CTL.DRDY_OFF      = 1;                              /* DRY 비활성 — INT1 사용 */
	ADXL355.Register.Parameter.Range.GravityRange      = ADXL355_G_RANGE_2G;
	ADXL355.Register.Parameter.FIFO_SAMPLES            = MEASUREMENT_FIFO_SETS_WMARK * 3;/* 90 slots watermark */
	ADXL355.Register.Parameter.INT_MAP.Byte            = 0;
	ADXL355.Register.Parameter.INT_MAP.FULL_EN1        = 1;                              /* FIFO_FULL/watermark → INT1 */

	return ADXL355_SetParameter(&ADXL355);
}
#endif

/* ============================================================================
 * Measurement_Init — MiMain init 단계에서 호출, ADXL355 Open·FIFO·온도 1회·안정화
 *   상태머신: 호출자가 RESULT_RUN 동안 반복 호출. RESULT_OK 시 Measurement_Initialized=1.
 * ========================================================================= */

oResult_t Measurement_Init(void)
{
	static uint8_t  InitStep = 0;
	static uint32_t Timer = 0;
	oResult_t result = RESULT_RUN;

	MiIoT_Status.StatusBits.DisconnectedSensor = False;

	switch(InitStep)
	{
		default: InitStep = 0;

		case 0:
			if(ADXL355.State.IsOpen) ADXL355_Close(&ADXL355);
			GPIOs.DO.MEMSEnable = 0;
			oRingBuffer_Init(&Measure_VibratingHistoryRB, VibratingHistoryStorage, sizeof(VibratingHistoryStorage));
			Timer = oTMR_GetTick(TICKBASE_SYSTICK);
			InitStep++;
			break;

		case 1:
			if(oTMR_Elapsed(&Timer, MEASUREMENT_POWER_WAIT_MS, TICKBASE_SYSTICK)){
				GPIOs.DO.MEMSEnable = 1;
				Timer = oTMR_GetTick(TICKBASE_SYSTICK);
				InitStep++;
			}
			break;

		case 2:
			if(oTMR_Elapsed(&Timer, MEASUREMENT_POWER_WAIT_MS, TICKBASE_SYSTICK)) InitStep++;
			break;

		case 3:
			switch(ADXL355_Open(&hi2c2, &ADXL355))
			{
				default: break;
				case RESULT_OK:    InitStep++; break;
				case RESULT_ERROR: result = RESULT_ERROR; break;
			}
			break;

		case 4:    /* 모드별 분기 — Vibration: FIFO+pipeline+FFT, Tilt: 스킵 (Sensor가 처리) */
		{
			IoTSensorType_t type = MiIoT_Parameter.ChannelConfig[0].TypeOfSensor;

			if(type == IoTSensorType_Vibration){
#if MEASUREMENT_USE_FIFO
				if(Measurement_ConfigFIFO() == RESULT_OK){
					Measurement_InitPipeline();
					Measurement_InitFFT();
					Timer = oTMR_GetTick(TICKBASE_SYSTICK);
					InitStep++;
				}
				else{
					result = RESULT_ERROR;
				}
#else
				/* Continuous 모드 — FIFO 미사용, DRDY 폴링 기반 */
				ADXL355_SetFilter(&ADXL355, 0, MEASUREMENT_ODR_HZ);
				ADXL355.Register.Parameter.POWER_CTL.DRDY_OFF = 0;
				ADXL355.Register.Parameter.Range.GravityRange = ADXL355_G_RANGE_2G;
				ADXL355.Register.Parameter.FIFO_SAMPLES = 0;       /* FIFO disable */
				ADXL355.Register.Parameter.INT_MAP.Byte = 0;

				if(ADXL355_SetParameter(&ADXL355) == RESULT_OK){
					Measurement_InitPipeline();
					Measurement_InitFFT();
					Timer = oTMR_GetTick(TICKBASE_SYSTICK);
					InitStep++;
				}
				else{
					result = RESULT_ERROR;
				}
#endif
			}
			else{
				/* Tilt 모드는 ADXL을 Sensor가 매 cycle 열고 닫음 — 여기서 미리 닫고 진행 */
				ADXL355_Close(&ADXL355);
				GPIOs.DO.MEMSEnable = 0;
				AngleAvrg[0].Reset = 1;
				AngleAvrg[1].Reset = 1;
				Timer = oTMR_GetTick(TICKBASE_SYSTICK);
				InitStep++;
			}
			break;
		}

		case 5:    /* HPF 안정화 + 온도 1회 측정 (Vibration만), Tilt는 즉시 완료 */
			if(MiIoT_Parameter.ChannelConfig[0].TypeOfSensor == IoTSensorType_Vibration){
				if(Measurement_ReadTempOnce(&Measure_Temperature) != RESULT_RUN){
					if(oTMR_Elapsed(&Timer, MEASUREMENT_STABILIZE_MS, TICKBASE_SYSTICK)){
						Measurement_ResetWindow();
						WindowSetCount = 0;
						Measurement_EventActive = 0;
						Measurement_Initialized = 1;
						result = RESULT_OK;
					}
				}
			}
			else{
				Measurement_Initialized = 1;
				result = RESULT_OK;
			}
			break;
	}

	if(result == RESULT_ERROR){
		ADXL355_Close(&ADXL355);
		GPIOs.DO.MEMSEnable = 0;
		MiIoT_Status.StatusBits.DisconnectedSensor = True;
		Measurement_Initialized = 0;
	}

	if(result != RESULT_RUN){
		InitStep = 0;
	}

	return result;
}

/* ============================================================================
 * Measurement_Sensor — 패킷 패킹 전용 (MiIoT 샘플링 콜백)
 *   PacketReady=1 시 결과를 IoTDataVibration_t로 패킹 후 RESULT_OK 반환
 *   소비 후 플래그·윈도우 리셋 → 다음 측정 cycle 진행
 * ========================================================================= */

/* ============================================================================
 * 패킷 헬퍼 — Measure_Vibrating snapshot → IoTDataVibration_t 패킹
 * ========================================================================= */

static void Measurement_FillVibrationPacket(IoT_DataPacket_t *pPacket, const Measure_Vibrating_t *pSamples, uint8_t count)
{
	if(pPacket == NULL || pSamples == NULL || count == 0) return;
	if(count > MIIOT_VIBRATION_MAX_COUNT) count = MIIOT_VIBRATION_MAX_COUNT;

	pPacket->TypeOfData = IoTDataType_Vibration;
	pPacket->DLC = MIIOT_IOTDATA_SIZE_VIBRATION(count);

	IoTDataVibration_t *pVib = (IoTDataVibration_t *)&pPacket->Frame;
	memset(pVib, 0, sizeof(IoTDataVibration_t));
	MIIOT_DT_TO_IOTTIME(&MiIoT_DT, &pVib->Time);

	for(uint8_t i = 0; i < count; i++){
		/* 속도 (mm/s × 100) — MIIOT_DATA_ENCODE_mmPerSec 사용 */
		pVib->Data[i].PPV_X  = MIIOT_DATA_ENCODE_mmPerSec(pSamples[i].PPV_X);
		pVib->Data[i].PPV_Y  = MIIOT_DATA_ENCODE_mmPerSec(pSamples[i].PPV_Y);
		pVib->Data[i].PPV_Z  = MIIOT_DATA_ENCODE_mmPerSec(pSamples[i].PPV_Z);
		pVib->Data[i].PVS    = MIIOT_DATA_ENCODE_mmPerSec(pSamples[i].PVS);

		/* 주파수 (Hz × 100 wire) — 내부는 float Hz, ENCODE_FREQUENCY가 ×100 변환 */
		pVib->Data[i].Freq_X = (uint16_t)MIIOT_DATA_ENCODE_FREQUENCY(pSamples[i].Freq_X);
		pVib->Data[i].Freq_Y = (uint16_t)MIIOT_DATA_ENCODE_FREQUENCY(pSamples[i].Freq_Y);
		pVib->Data[i].Freq_Z = (uint16_t)MIIOT_DATA_ENCODE_FREQUENCY(pSamples[i].Freq_Z);
	}
}

/* 단일 진동 샘플 → LoRa Mailbox 큐잉 (이벤트 라이브용 — 1샘플 1패킷) */
static void Measurement_QueueEventSample(const Measure_Vibrating_t *pSrc)
{
	if(pSrc == NULL || !MiLoRa_IsReachable) return;

	IoT_DataPacket_t pkt;
	memset(&pkt, 0, sizeof(pkt));
	Measurement_FillVibrationPacket(&pkt, pSrc, 1);
	MiIoT_MailBox_NewItem(&MiIoT_LoRaMailBox, &pkt, MILORA_QOS_MAX, 0);
}

/* Pre-trigger 전송 — 링버퍼에 저장된 가장 최근 N 레코드를 시간 순으로 mailbox에 큐잉
 *   링버퍼는 byte 단위 ONE_RingBuffer — peek 위해 인덱스 직접 사용
 *   FirstIndex = 다음 write 위치(=newest 직후), LastIndex = oldest
 *   대상 = (FirstIndex - records*recordSize) ~ (FirstIndex - 1)
 *   1패킷 = MIIOT_VIBRATION_MAX_COUNT 샘플 묶음으로 송신 */
static void Measurement_SendPreTrigger(uint8_t records)
{
	const uint16_t recSize = sizeof(Measure_Vibrating_t);
	uint16_t available = Measure_VibratingHistoryRB.Count / recSize;
	if(records > available) records = (uint8_t)available;
	if(records == 0) return;
	if(!MiLoRa_IsReachable) return;

	const uint16_t totalLen = Measure_VibratingHistoryRB.Length;
	const uint16_t backBytes = (uint16_t)records * recSize;

	uint16_t startIdx;
	if(Measure_VibratingHistoryRB.FirstIndex >= backBytes){
		startIdx = (uint16_t)(Measure_VibratingHistoryRB.FirstIndex - backBytes);
	}
	else{
		startIdx = (uint16_t)(totalLen - (backBytes - Measure_VibratingHistoryRB.FirstIndex));
	}

	/* MIIOT_VIBRATION_MAX_COUNT개씩 묶어 1패킷으로 전송 */
	Measure_Vibrating_t batch[MIIOT_VIBRATION_MAX_COUNT];
	uint8_t filled = 0;

	for(uint8_t i = 0; i < records; i++){
		uint16_t srcIdx = (uint16_t)((startIdx + (uint16_t)i * recSize) % totalLen);
		memcpy(&batch[filled], &VibratingHistoryStorage[srcIdx], recSize);
		filled++;

		if(filled >= MIIOT_VIBRATION_MAX_COUNT){
			IoT_DataPacket_t pkt;
			memset(&pkt, 0, sizeof(pkt));
			Measurement_FillVibrationPacket(&pkt, batch, filled);
			MiIoT_MailBox_NewItem(&MiIoT_LoRaMailBox, &pkt, MILORA_QOS_MAX, 0);
			filled = 0;
		}
	}

	/* 남은 잔여 샘플 (records가 MAX_COUNT 배수가 아닐 때) */
	if(filled > 0){
		IoT_DataPacket_t pkt;
		memset(&pkt, 0, sizeof(pkt));
		Measurement_FillVibrationPacket(&pkt, batch, filled);
		MiIoT_MailBox_NewItem(&MiIoT_LoRaMailBox, &pkt, MILORA_QOS_MAX, 0);
	}
}

/* ============================================================================
 * Measurement_Sensor — MiIoT 샘플링 콜백 (설정 주기마다 호출)
 *   ① 최신 가속도 스냅샷 → 각도 계산 (atan2 + 다항식 보상 + oMoveAverage 25)
 *   ② 마지막 측정 진동값을 IoTDataVibration_t로 패킹 → MiIoT가 LoRaMailBox에 큐잉
 * ========================================================================= */

/* ============================================================================
 * Measurement_ReadAngle — Tilt 모드 (SIA100_SD 패턴)
 *   ADXL을 직접 열고 정적 모드(ODR 62Hz·DRDY_OFF=1)로 1샘플 GetAngle
 *   → 다항식 6차 보상 → oMoveAverage(25) → 종료 시 ADXL Close
 * ========================================================================= */

static oResult_t Measurement_ReadAngle(oRPY_t *pRPY)
{
	static uint8_t  ReadAngleStep = 0;
	static uint32_t ReadAngleTimer = 0;
	oRPY_t Angle;
	oResult_t result = RESULT_RUN;

	switch(ReadAngleStep)
	{
		default: ReadAngleStep = 0;

		case 0:
			if(GPIOs.DO.MEMSEnable && ADXL355.State.IsOpen){
				ReadAngleStep = 4;   /* 이미 열려 있으면 바로 측정 */
			}
			else{
				if(ADXL355.State.IsOpen) ADXL355_Close(&ADXL355);
				GPIOs.DO.MEMSEnable = 0;
				if(oTMR_Trigger(&ReadAngleTimer, 100, 1, TICKBASE_SYSTICK)){
					GPIOs.DO.MEMSEnable = 1;
					AngleAvrg[0].Reset = 1;
					AngleAvrg[1].Reset = 1;
					ReadAngleStep++;
				}
			}
			break;

		case 1:
			if(oTMR_Trigger(&ReadAngleTimer, 100, 1, TICKBASE_SYSTICK)) ReadAngleStep++;
			break;

		case 2:
			switch(ADXL355_Open(&hi2c2, &ADXL355))
			{
				default: break;
				case RESULT_OK:    ReadAngleStep++; break;
				case RESULT_ERROR: result = RESULT_ERROR; break;
			}
			break;

		case 3:
			ADXL355_SetFilter(&ADXL355, 0, 62);                                /* ODR 62Hz 정적 모드 */
			ADXL355.Register.Parameter.POWER_CTL.DRDY_OFF = 1;
			ADXL355.Register.Parameter.Range.GravityRange = ADXL355_G_RANGE_2G;

			if(ADXL355_SetParameter(&ADXL355) == RESULT_OK) ReadAngleStep++;
			else                                            result = RESULT_ERROR;
			break;

		case 4:
			switch(ADXL355_GetAngle(&ADXL355, 1, &Angle))
			{
				default: break;
				case RESULT_ERROR: result = RESULT_ERROR; break;
				case RESULT_OK:
					Angle.Pitch *= -1;

					Measure_RawAngle.Pitch = Angle.Pitch;
					Measure_RawAngle.Roll  = Angle.Roll;

					/* 다항식 6차 보상 + 범위 제한 */
					Measure_CalibratedAngle.Pitch = oMath_Polynomial(Measure_RawAngle.Pitch * 1000, MiIoT_Parameter.SystemConfig.Calibration.Tilt.PolynomialX, 6) / 1000;
					Measure_CalibratedAngle.Pitch = MATH_LIMIT(Measure_CalibratedAngle.Pitch, MiIoT_Parameter.SystemConfig.Calibration.Tilt.RangeX.Min, MiIoT_Parameter.SystemConfig.Calibration.Tilt.RangeX.Max);

					Measure_CalibratedAngle.Roll = oMath_Polynomial(Measure_RawAngle.Roll * 1000, MiIoT_Parameter.SystemConfig.Calibration.Tilt.PolynomialY, 6) / 1000;
					Measure_CalibratedAngle.Roll = MATH_LIMIT(Measure_CalibratedAngle.Roll, MiIoT_Parameter.SystemConfig.Calibration.Tilt.RangeY.Min, MiIoT_Parameter.SystemConfig.Calibration.Tilt.RangeY.Max);

					/* 이동평균 적용 */
					oMoveAverage_SetData(&AngleAvrg[0], &Measure_CalibratedAngle.Pitch);
					oMoveAverage_SetData(&AngleAvrg[1], &Measure_CalibratedAngle.Roll);
					oMoveAverage_GetData(&AngleAvrg[0], &Measure_CalibratedAngle.Pitch);
					oMoveAverage_GetData(&AngleAvrg[1], &Measure_CalibratedAngle.Roll);

					if(pRPY) *pRPY = Measure_CalibratedAngle;
					result = RESULT_OK;
					break;
			}
			break;
	}

	if(result == RESULT_ERROR){
		ADXL355_Close(&ADXL355);
		GPIOs.DO.MEMSEnable = 0;
	}

	if(result != RESULT_RUN){
		ReadAngleStep = 0;
	}

	return result;
}

oResult_t Measurement_Sensor(IoT_DataPacket_t *pPacket)
{
	if(pPacket){
		pPacket->DLC = 0;
		pPacket->TypeOfData = IoTDataType_NULL;
	}

	/* 모드 전환 감지 + 미초기화 시 자동 재실행 */
	Measurement_HandleModeTransition();

	if(!Measurement_Initialized){
		Measurement_Init();
		return RESULT_RUN;
	}

	IoTSensorType_t type = g_RunningSensorType;

	/* === Vibration 모드: 마지막 1초 진동값 → IoTDataVibration 주기 송신 === */
	if(type == IoTSensorType_Vibration){
		if(Measure_VibratingHistoryRB.Count < sizeof(Measure_Vibrating_t)){
			return RESULT_RUN;   /* 아직 첫 1초 윈도우 미달 */
		}
		Measurement_FillVibrationPacket(pPacket, &Measure_Vibrating_Last, 1);
		return RESULT_OK;
	}

	/* === Tilt 모드: ADXL 직접 read + 다항식 + 이동평균 → IoTDataTilt 송신 (SIA100_SD 패턴) === */
	if(type == IoTSensorType_Tilt){
		static uint8_t  SensorStep = 0;
		static uint32_t SensorTimer = 0;
		static uint8_t  ReadCount = 0;
		oResult_t r = RESULT_RUN;

		switch(SensorStep)
		{
			case 0:
				ReadCount = 0;
				SensorTimer = oTMR_GetTick(TICKBASE_SYSTICK);
				SensorStep++;
				/* fallthrough */
			case 1:
				if(Measurement_ReadAngle(NULL) != RESULT_RUN) SensorStep++;
				break;
			case 2:
				if(Measurement_ReadTempOnce(&Measure_Temperature) != RESULT_RUN) SensorStep++;
				break;
			case 3:
				if(oTMR_Elapsed(&SensorTimer, MATH_LIMIT(MiIoT_Parameter.ChannelConfig[0].WarmupTime, 1000, 3000), TICKBASE_SYSTICK)){
					ReadCount++;
				}
				if(ReadCount < 15){
					SensorStep = 1;            /* 15회 반복 sampling */
				}
				else{
					r = RESULT_OK;
				}
				break;
		}

		if(r != RESULT_RUN){
			ADXL355_Close(&ADXL355);
			TMP1075_Close(&TMP1075);
			GPIOs.DO.MEMSEnable = 0;
			GPIOs.DO.TEMPEnable = 0;
			SensorStep = 0;

			if(r == RESULT_OK && pPacket){
				pPacket->TypeOfData = IoTDataType_Tilt;
				pPacket->DLC = MIIOT_IOTDATA_SIZE_TILT;

				IoTDataTilt_t *pTilt = (IoTDataTilt_t *)&pPacket->Frame;
				memset(pTilt, 0, sizeof(IoTDataTilt_t));
				MIIOT_DT_TO_IOTTIME(&MiIoT_DT, &pTilt->Time);

				pTilt->Temperature  = MIIOT_DATA_ENCODE_TEMP(Measure_Temperature);
				pTilt->Sensor.AxisX = MIIOT_DATA_ENCODE_ANGLE(Measure_CalibratedAngle.Pitch);
				pTilt->Sensor.AxisY = MIIOT_DATA_ENCODE_ANGLE(Measure_CalibratedAngle.Roll);
			}
		}

		return r;
	}

	/* 설정 없음 — 미전송 */
	return RESULT_RUN;
}

/* ============================================================================
 * Measurement — MiMain 메인 루프에서 매 tick 호출
 *   동작:
 *     1) FIFO 잔량 < watermark → Native_SleepMode(60 ms) → 재진입 시 다시 확인
 *     2) FIFO ready → 30 sets read + ProcessSet × 30
 *     3) 1초 누적 시 FFT + 각도 평균 + 임계 판정 → 초과 시 Measurement_PacketReady=1 (윈도우 유지)
 *     4) 임계 미달 → ResetWindow, 다음 윈도우 누적
 *   PacketReady=1인 동안은 결과 동결 — Sensor가 소비할 때까지 대기
 * ========================================================================= */

oResult_t Measurement(void)
{
#if MEASUREMENT_USE_FIFO
	uint8_t entries = 0;
#endif

	/* 모드 전환 감지 — 변경됐으면 리소스 정리 + Initialized=0 (Init 재실행 트리거) */
	Measurement_HandleModeTransition();

	/* Vibration 모드 외에는 FIFO·이벤트 측정 스킵 (Tilt 모드는 Sensor가 처리) */
	if(g_RunningSensorType != IoTSensorType_Vibration){
		return RESULT_RUN;
	}

	IoTChannelPropertiesVibration_t *pVibProp = &MiIoT_Parameter.ChannelConfig[0].Properties.Vibration;

	/* Initialized=0 (boot 또는 모드 전환) → Init 자동 재실행 */
	if(!Measurement_Initialized){
		Measurement_Init();
		return RESULT_RUN;
	}

#if MEASUREMENT_USE_FIFO
	/* ────────────────────────────── FIFO batch 모드 ────────────────────────────── */
	if(ADXL355_GetCountOfFIFOEntries(&ADXL355, &entries) != RESULT_OK){
		return RESULT_ERROR;
	}

	if(entries < MEASUREMENT_FIFO_SETS_WMARK * 3){
		//Native_SleepMode(MEASUREMENT_BATCH_PERIOD_MS);   /* 슬립 모드 임시 비활성화 — 검증 단계 */
		return RESULT_RUN;
	}

	/* FIFO 읽기 주기·처리 시간 측정 — 임계 초과 시 로그 (희소하게만 출력해서 self-trigger 피함) */
	static uint32_t LastFifoReadTick = 0;
	static uint32_t MaxIntervalSinceLog = 0;
	static uint32_t LastLogTick = 0;
	uint32_t batchStartTick = oTMR_GetTick(TICKBASE_SYSTICK);
	if(LastFifoReadTick != 0){
		uint32_t interval = batchStartTick - LastFifoReadTick;
		if(interval > MaxIntervalSinceLog) MaxIntervalSinceLog = interval;

		/* 1초마다 한 번씩만 max interval 로그 (피드백 루프 방지) */
		if(batchStartTick - LastLogTick >= 1000){
			if(MaxIntervalSinceLog > MEASUREMENT_BATCH_PERIOD_MS + 20){
				oSerial_Log("Vib", "max_interval=%u ms (1s window, ovr=%u)",
				            (unsigned int)MaxIntervalSinceLog,
				            (unsigned int)ADXL355.Register.Status.FIFO_OVR);
			}
			MaxIntervalSinceLog = 0;
			LastLogTick = batchStartTick;
		}
	}
	LastFifoReadTick = batchStartTick;

	/* === 세부 timing instrumentation (1초당 1회 max 로그) === */
	uint32_t ts_a = oTMR_GetTick(TICKBASE_SYSTICK);   /* I2C read 시작 */

	if(ADXL355_ReadFIFO(&ADXL355, MEASUREMENT_FIFO_SETS_WMARK, FIFOReadBuf) != RESULT_OK){
		return RESULT_ERROR;
	}

	uint32_t ts_b = oTMR_GetTick(TICKBASE_SYSTICK);   /* I2C 종료, ProcessSet 시작 */

	for(uint8_t s = 0; s < MEASUREMENT_FIFO_SETS_WMARK; s++){
		oVector3_t accel;
		ADXL355_ParseFIFOSet(&ADXL355, &FIFOReadBuf[s * 9U], &accel);
		Measurement_ProcessSet(&accel);
	}

	uint32_t ts_c = oTMR_GetTick(TICKBASE_SYSTICK);   /* ProcessSet 종료 */

	/* batch 세부 timing — 1초당 1회만 출력 */
	{
		static uint32_t MaxI2C = 0, MaxProc = 0, MaxBatch = 0;
		static uint32_t LastBatchLogTick = 0;
		uint32_t i2cDur  = ts_b - ts_a;
		uint32_t procDur = ts_c - ts_b;
		uint32_t batchDuration = ts_c - batchStartTick;
		uint32_t now2 = oTMR_GetTick(TICKBASE_SYSTICK);
		if(i2cDur > MaxI2C) MaxI2C = i2cDur;
		if(procDur > MaxProc) MaxProc = procDur;
		if(batchDuration > MaxBatch) MaxBatch = batchDuration;
		if(now2 - LastBatchLogTick >= 1000){
			if(MaxBatch > 30){
				oSerial_Log("Vib", "max batch=%u i2c=%u proc=%u (ms)",
				            (unsigned int)MaxBatch, (unsigned int)MaxI2C, (unsigned int)MaxProc);
			}
			MaxI2C = MaxProc = MaxBatch = 0;
			LastBatchLogTick = now2;
		}
	}
#else
	/* ────────────────────────── Continuous polling 모드 ───────────────────────── */
	switch(ADXL355_GetData(&ADXL355))
	{
		case RESULT_RUN:   return RESULT_RUN;        /* DATA_RDY 미발생 → 다음 tick */
		case RESULT_ERROR: return RESULT_ERROR;
		default: break;
	}

	{
		Measurement_ProcessSet(&ADXL355.Acceleration);
	}
#endif

	/* sliding window: OUTPUT_PERIOD_SETS마다 출력 (ring buffer는 계속 누적) */
	if(WindowSetCount < MEASUREMENT_OUTPUT_PERIOD_SETS){
		return RESULT_RUN;
	}

	/* sliding 1초 윈도우 FFT — 3축 분할 (3 tick 누적 후 결과 확정) */
	if(Measurement_CalcFFT() != RESULT_OK){
		return RESULT_RUN;
	}

	/* === 이벤트 판정 — DIN 4150-3·한국 환경부 표준은 축별 PPVmax 사용 (PVS 아님) ===
	 *   임계는 사용자 파라미터 Vibrating.Warnning (mm/s) — 0이면 컴파일 기본값 사용 */
	float ppvMax = Measure_Vibrating.PPV_X;
	if(Measure_Vibrating.PPV_Y > ppvMax) ppvMax = Measure_Vibrating.PPV_Y;
	if(Measure_Vibrating.PPV_Z > ppvMax) ppvMax = Measure_Vibrating.PPV_Z;

	float threshold = (pVibProp->Warnning > 0) ? pVibProp->Warnning : MEASUREMENT_PPV_THRESHOLD_MMS;

	if(ppvMax >= threshold){
		if(!Measurement_EventActive){
			/* Rising edge — pre-trigger N초 전송 */
			Measurement_SendPreTrigger(MEASUREMENT_EVENT_PRETRIGGER_SETS);
			Measurement_EventActive = 1;
		}
		/* 이벤트 지속 — 방금 완료된 1초 결과 전송 (안정 snapshot은 직후 위에서 Last에 복사됨, 여기선 live 사용) */
		Measurement_QueueEventSample(&Measure_Vibrating);
	}
	else if(Measurement_EventActive){
		/* Falling edge — 이벤트 종료 */
		Measurement_EventActive = 0;
	}

	/* 윈도우 종료 — 안정 snapshot으로 보존 (Sensor는 이 값을 송신) */
	Measure_Vibrating_Last = Measure_Vibrating;

	/* 1분 링버퍼 push — 안정 snapshot 기준 (다음 윈도우의 pre-trigger에 반영) */
	oRingBuffer_SetPntr(&Measure_VibratingHistoryRB, (uint8_t *)&Measure_Vibrating_Last, sizeof(Measure_Vibrating_t));

	/* 다음 출력 주기까지 대기 — sliding window이므로 ring/PPV 리셋 안 함 */
	WindowSetCount = 0;
	return RESULT_RUN;
}

/* History

2026-06-26 | v0.1
	- baseline (Mi_Measurement.c)
*/
