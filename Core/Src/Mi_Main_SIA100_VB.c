#include "ONE_Signal.h"
#include "ONE_Time.h"
#include "ONE_Memory.h"
#include "Mi_Native.h"
#include "Mi_LoRa.h"
#include "Mi_IoT.h"
#include "Mi_SoftwareRevision.h"
#include "Mi_Measurement.h"
#include "Mi_Storage.h"
#include "Mi_Serial.h"
#include "Mi_Calibration.h"

#define SYSTEM_SUPPLY_LOW_LIMIT		3200
#define RTC_SUPPLY_LOW_LIMIT		2100

oIO_t DO_NAND_ENABLE	= {DO_NAND_ENABLE_GPIO_Port,	DO_NAND_ENABLE_Pin,			IO_LOW};
oIO_t DO_TEMP_ENALBE	= {DO_TMEP_ENABLE_GPIO_Port, 	DO_TMEP_ENABLE_Pin,			IO_LOW};
oIO_t DO_MEMS_ENALBE	= {DO_MEMS_ENABLE_GPIO_Port, 	DO_MEMS_ENABLE_Pin,			IO_LOW};
oIO_t DO_CAN_EANBLE		= {DO_CAN_ENABLE_GPIO_Port,		DO_CAN_ENABLE_Pin,		 	IO_LOW};
oIO_t DI_MEMS_EVENT1	= {DI_MEMS_INT1_GPIO_Port,		DI_MEMS_INT1_Pin,			IO_HIGH};
oIO_t DI_MEMS_EVENT2	= {DI_MEMS_INT2_GPIO_Port,		DI_MEMS_INT2_Pin,			IO_HIGH};

oIO_t DO_LORA_ENABLE	= {DO_LORA_ENABLE_GPIO_Port,	DO_LORA_ENABLE_Pin,			IO_LOW};
oIO_t DO_LED_OPERATING	= {DO_LED_OPERATING_GPIO_Port,	DO_LED_OPERATING_Pin,		IO_HIGH};
oIO_t DI_USB_CONNECTED	= {DI_USBC_CONNECTED_GPIO_Port,	DI_USBC_CONNECTED_Pin,		IO_HIGH};
oIO_t DI_MEMS_INT1		= {DI_MEMS_INT1_GPIO_Port,		DI_MEMS_INT1_Pin,			IO_HIGH};
oIO_t DI_MEMS_INT2		= {DI_MEMS_INT2_GPIO_Port,		DI_MEMS_INT2_Pin,			IO_HIGH};

GPIOs_t GPIOs;
oDebounce_t DB_DataReady	= DEBOUNCE_INITIALIZER(100,100);
oDebounce_t DB_USBConnected	= DEBOUNCE_INITIALIZER(10,10);
oDebounce_t DebounceError	= DEBOUNCE_INITIALIZER(1000,300);

oResult_t MiMain_GPIOControl()
{
	GPIOs.DO.OperatingLED = MiIoT_LED;

	//GPIO output
	IO_WRITE(DO_LORA_ENABLE, GPIOs.DO.LoRaEnable);
	IO_WRITE(DO_NAND_ENABLE, GPIOs.DO.NANDEnable);
	IO_WRITE(DO_CAN_EANBLE, GPIOs.DO.CANEnable);
	IO_WRITE(DO_TEMP_ENALBE, GPIOs.DO.TEMPEnable);
	IO_WRITE(DO_MEMS_ENALBE, GPIOs.DO.MEMSEnable);
	IO_WRITE(DO_LED_OPERATING, GPIOs.DO.OperatingLED);

	//GPIO input
	oDebounce_GPIO(DI_USB_CONNECTED, &DB_USBConnected);

	GPIOs.DI.UsbConnected = DB_USBConnected.Output || DB_USBConnected.Input;
	GPIOs.DI.MemsEvent1 = IO_READ(DI_MEMS_EVENT1);
	GPIOs.DI.MemsEvent2 = IO_READ(DI_MEMS_EVENT2);

	uint32_t IORun = 0;
	IORun += GPIOs.DO.LoRaEnable && !MiLoRa_IsSleep;
	//IORun += GPIOs.DO.MEMSEnable;
	IORun += GPIOs.DO.NANDEnable;
	IORun += GPIOs.DO.CANEnable;
	IORun += GPIOs.DO.TEMPEnable;
	IORun += GPIOs.DO.OperatingLED;
	IORun += GPIOs.DI.UsbConnected;

	return IORun ? RESULT_RUN : RESULT_ERROR;
}

void MiMain_GPIODeInit(void)
{
	GPIO_InitTypeDef cfg = {.Mode = GPIO_MODE_ANALOG, .Pull = GPIO_NOPULL};

	cfg.Pin = DO_NAND_ENABLE.Pin;
	HAL_GPIO_Init(DO_NAND_ENABLE.Port, &cfg);
	cfg.Pin = DO_TEMP_ENALBE.Pin;
	HAL_GPIO_Init(DO_TEMP_ENALBE.Port, &cfg);
	cfg.Pin = DO_MEMS_ENALBE.Pin;
	HAL_GPIO_Init(DO_MEMS_ENALBE.Port, &cfg);
	cfg.Pin = DO_CAN_EANBLE.Pin;
	HAL_GPIO_Init(DO_CAN_EANBLE.Port, &cfg);
	cfg.Pin = DO_LED_OPERATING.Pin;
	HAL_GPIO_Init(DO_LED_OPERATING.Port, &cfg);

	if(!MiLoRa_IsSleep){
		cfg.Pin = DO_LORA_ENABLE.Pin;
		HAL_GPIO_Init(DO_LORA_ENABLE.Port, &cfg);
	}

	cfg.Mode = GPIO_MODE_IT_RISING;
	cfg.Pull = GPIO_PULLDOWN;
	cfg.Pin = DI_USBC_CONNECTED_Pin;
	HAL_GPIO_Init(DI_USBC_CONNECTED_GPIO_Port, &cfg);
}

void MiMain_GPIOInit(void)
{
	GPIO_InitTypeDef cfg = {.Speed = GPIO_SPEED_FREQ_LOW};

	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOH_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();
	__HAL_RCC_GPIOD_CLK_ENABLE();

	MiMain_GPIOControl();

	//OD outputs
	cfg.Mode  = GPIO_MODE_OUTPUT_OD;
	cfg.Pull  = GPIO_NOPULL;

	if(!MiLoRa_IsSleep){
		cfg.Pin   = DO_LORA_ENABLE.Pin;
		HAL_GPIO_Init(DO_LORA_ENABLE.Port, &cfg);
	}

	cfg.Pin   = DO_NAND_ENABLE.Pin;
	HAL_GPIO_Init(DO_NAND_ENABLE.Port, &cfg);
	cfg.Pin   = DO_TEMP_ENALBE.Pin;
	HAL_GPIO_Init(DO_TEMP_ENALBE.Port, &cfg);
	cfg.Pin   = DO_MEMS_ENALBE.Pin;
	HAL_GPIO_Init(DO_MEMS_ENALBE.Port, &cfg);
	cfg.Pin   = DO_CAN_EANBLE.Pin;
	HAL_GPIO_Init(DO_CAN_EANBLE.Port, &cfg);

	//PP outputs (Active HIGH) | LED=GPIOA
	cfg.Mode = GPIO_MODE_OUTPUT_PP;
	cfg.Pull  = GPIO_NOPULL;

	cfg.Pin  = DO_LED_OPERATING.Pin;
	HAL_GPIO_Init(DO_LED_OPERATING.Port, &cfg);

	//EXTI Rising inputs NoPull | MEMS_INT1/INT2=GPIOC
	cfg.Mode = GPIO_MODE_IT_RISING;
	cfg.Pull = GPIO_NOPULL;

	cfg.Pin  = DI_MEMS_INT1.Pin;
	HAL_GPIO_Init(DI_MEMS_INT1.Port, &cfg);
	cfg.Pin  = DI_MEMS_INT2.Pin;
	HAL_GPIO_Init(DI_MEMS_INT2.Port, &cfg);

	HAL_NVIC_SetPriority(EXTI9_5_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
	HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

	HAL_ADC_DeInit(&hadc1);
	HAL_ADC_Init(&hadc1);
}

oResult_t MiMain_UpdateMeasure(IoT_DataPacket_t **ppPacket)
{
	static uint8_t UpdateSamplingStep = 0;
	static IoT_DataPacket_t DataPacket;
	oResult_t result = RESULT_RUN;

	switch(UpdateSamplingStep)
	{
		case 0:
			oSerial_Log("UpdateMeasure", "start\r\n");
			UpdateSamplingStep++;
			break;
		case 1:
			if(MiIoT_Parameter.ChannelConfig[0].TypeOfSensor != IoTSensorType_Tilt){
				oSerial_Log("UpdateMeasure", "Channel type error | %s\r\n");
				result = RESULT_ERROR;
			}
			else{
				switch(result = Measurement_Sensor(&DataPacket))
				{
					case RESULT_RUN:
						break;
					case RESULT_OK:
						*ppPacket = &DataPacket;
						result = RESULT_OK;
						UpdateSamplingStep++;
						break;
					default:
						oSerial_Log("UpdateMeasure", "sampling error | %s\r\n");
						result = RESULT_ERROR;
						break;
				}
			}
			break;
		case 2:
			result = RESULT_DONE;
			break;
	}

	if(result != RESULT_RUN && result != RESULT_OK){
		UpdateSamplingStep = 0;
		oSerial_Log("UpdateMeasure", "finish\r\n");
	}

	return result;
}

oResult_t MiMain_UpdateStatus()
{
	oResult_t result = RESULT_RUN;

	MiIoT_Status.SoftwareVersion = (uint16_t)(MI_SW_REVISION*100);

	if((result = Measurement_Supply(20)) == RESULT_OK){
		MiIoT_Status.UpdateTimestmap = oTMR_GetTick(TICKBASE_SYSTICK);
		MiIoT_Status.StatusBits.SystemSupplyVoltTooLow = GPIOs.ADC.SystemSupply < SYSTEM_SUPPLY_LOW_LIMIT;
		MiIoT_Status.StatusBits.ClockSupplyVoltTooLow = GPIOs.ADC.InternalBAT < RTC_SUPPLY_LOW_LIMIT;
		MiIoT_Status.StatusBits.Okay = (MiIoT_Status.StatusBits.Bits & 0xFFFFFFFE) == 0 ? 1 : 0;
		MiIoT_Status.SystemSupply = GPIOs.ADC.SystemSupply;

		oSerial_Log("IoTStatus", "SupplyVolt : %d | SWVersion : %d | StatusBit : %d\r\n", (int)MiIoT_Status.SystemSupply, (int)MiIoT_Status.SoftwareVersion, (int)MiIoT_Status.StatusBits.Bits);
	}

	return result;
}

void MiMain (void)
{
	static uint8_t MiMainStep = 0;

	Native_WatchDog(MIIOT_SLEEP_TIME*2);

	switch(MiMainStep)
	{
		case 0: //Init

			MiIoT_SamplingCallback = MiMain_UpdateMeasure;
			//MiIoT_MeasurementCallback = MiMain_UpdateMeasure;
			MiIoT_StatusCallback = MiMain_UpdateStatus;
			MiIoT_IOControlCallback = MiMain_GPIOControl;

			MiIoT_GPIOInitCallback = MiMain_GPIOInit;
			MiIoT_GPIODeInitCallback = MiMain_GPIODeInit;

			MiMainStep++;
			break;
		case 1:
			HAL_QSPI_DeInit(&hqspi);
			MiMainStep++;
			break;
		case 2:    /* ADXL355·TMP1075 초기화 — 완료까지 반복 호출 */
			if(Measurement_Init() != RESULT_RUN){
				MiMainStep++;
			}
			break;
		default:
			MiStorage();
			MiIoT(&huart3);
			MiSerial(&huart1);
			MiCalibration();
			Measurement();
			break;
	}
}

/* History

2026-06-26 | v0.1
	- baseline (Mi_Main_SIA100_VB.c)
*/
