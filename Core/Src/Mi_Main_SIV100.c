#include "ONE_Signal.h"
#include "ONE_Time.h"
#include "ONE_Memory.h"
#include "Mi_LoRa.h"
#include "Mi_IoT.h"
#include "Mi_SoftwareRevision.h"
#include "Mi_Measurement.h"
#include "Mi_Storage.h"
#include "Mi_Serial.h"
#include "Mi_Native.h"

//#define SYSTEM_SUPPLY_LOW_LIMIT		3250
#define SYSTEM_SUPPLY_LOW_LIMIT		3000
#define RTC_SUPPLY_LOW_LIMIT		2100

GPIOs_t GPIOs;
oDebounce_t DB_USBConnected	= DEBOUNCE_INITIALIZER(30,30);
oDebounce_t DebounceError	= DEBOUNCE_INITIALIZER(1000,300);

oIO_t DO_NAND_ENABLE		= {DO_NAND_ENABLE_GPIO_Port,		DO_NAND_ENABLE_Pin,		 IO_LOW};
oIO_t DO_ADC_REF_EANBLE		= {DO_BAT_CHECK_GPIO_Port,			DO_BAT_CHECK_Pin,		 IO_HIGH};
oIO_t DO_12V_ENABLE			= {DO_PW_12V_GPIO_Port,				DO_PW_12V_Pin,			 IO_LOW};
oIO_t DO_AMP_SW				= {DO_AMPSW_GPIO_Port,				DO_AMPSW_Pin,			 IO_HIGH};
oIO_t DO_MX489_DE			= {DO_MX489_DE_GPIO_Port,			DO_MX489_DE_Pin,		 IO_HIGH};
oIO_t DO_MUX_SEL1			= {DO_MUX_SEL1_GPIO_Port,			DO_MUX_SEL1_Pin,		 IO_HIGH};
oIO_t DO_MUX_SEL2			= {DO_MUX_SEL2_GPIO_Port,			DO_MUX_SEL2_Pin,		 IO_HIGH};
oIO_t DO_MUX_A				= {DO_MUX_A_GPIO_Port,				DO_MUX_A_Pin,			 IO_HIGH};
oIO_t DO_MUX_B				= {DO_MUX_B_GPIO_Port,				DO_MUX_B_Pin,			 IO_HIGH};
oIO_t DO_VW_REALY1			= {DO_VW_RELAY1_GPIO_Port,			DO_VW_RELAY1_Pin,		 IO_HIGH};
oIO_t DO_VW_REALY2			= {DO_VW_RELAY2_GPIO_Port,			DO_VW_RELAY2_Pin,		 IO_HIGH};
oIO_t DO_VW_REALY3			= {DO_VW_RELAY3_GPIO_Port,			DO_VW_RELAY3_Pin,		 IO_HIGH};
oIO_t DO_VW_REALY4			= {DO_VW_RELAY4_GPIO_Port,			DO_VW_RELAY4_Pin,		 IO_HIGH};
oIO_t DO_VW_REALY5			= {DO_VW_RELAY5_GPIO_Port,			DO_VW_RELAY5_Pin,		 IO_HIGH};

oIO_t DO_LORA_ENABLE		= {DO_LORA_ENABLE_GPIO_Port,		DO_LORA_ENABLE_Pin,		 IO_LOW};
oIO_t DI_USB_CONNECTED		= {DI_USBC_CONNECTED_GPIO_Port,		DI_USBC_CONNECTED_Pin,	 IO_HIGH};
oIO_t DO_LED_OPERATING		= {DO_LED_OPERATING_GPIO_Port,		DO_LED_OPERATING_Pin,	 IO_HIGH};

oResult_t MiMain_GPIOControl()
{
	IO_WRITE(DO_LORA_ENABLE, 	GPIOs.DO.LoRaEnable);
	IO_WRITE(DO_NAND_ENABLE,	GPIOs.DO.NANDEnable);
	IO_WRITE(DO_ADC_REF_EANBLE, GPIOs.DO.ADCRefEnable);

	IO_WRITE(DO_12V_ENABLE,		GPIOs.DO.Dc12VEnable);
	IO_WRITE(DO_MX489_DE,		GPIOs.DO.MAX489_DE);
	IO_WRITE(DO_MUX_SEL1, 		GPIOs.DO.MUX_SEL1);
	IO_WRITE(DO_MUX_SEL2, 		GPIOs.DO.MUX_SEL2);
	IO_WRITE(DO_MUX_A, 			GPIOs.DO.MUX_A);
	IO_WRITE(DO_MUX_B, 			GPIOs.DO.MUX_B);
	IO_WRITE(DO_LED_OPERATING,	GPIOs.DO.OperatingLED);

	IO_WRITE(DO_VW_REALY1,		GPIOs.DO.VWRelay1);
	IO_WRITE(DO_VW_REALY2,		GPIOs.DO.VWRelay2);
	IO_WRITE(DO_VW_REALY3,		GPIOs.DO.VWRelay3);
	IO_WRITE(DO_VW_REALY4,		GPIOs.DO.VWRelay4);
	IO_WRITE(DO_VW_REALY5,		GPIOs.DO.VWRelay5);

	IO_WRITE(DO_AMP_SW,			GPIOs.DO.AmpSW);

	oDebounce_GPIO(DI_USB_CONNECTED, &DB_USBConnected);
	GPIOs.DI.UsbConnected = DB_USBConnected.Output || DB_USBConnected.Input;

	uint32_t IORun = 0;
	//IORun += GPIOs.DO.LoRaEnable;
	IORun += GPIOs.DO.ADCRefEnable;
	IORun += GPIOs.DO.NANDEnable;
	IORun += GPIOs.DO.Dc12VEnable;
	IORun += GPIOs.DO.MAX489_DE;
	IORun += GPIOs.DO.MUX_SEL1;
	IORun += GPIOs.DO.MUX_SEL2;
	IORun += GPIOs.DO.MUX_A;
	IORun += GPIOs.DO.MUX_B;
	IORun += GPIOs.DO.AmpSW;
	IORun += GPIOs.DO.VWRelay1;
	IORun += GPIOs.DO.VWRelay2;
	IORun += GPIOs.DO.VWRelay3;
	IORun += GPIOs.DO.VWRelay4;
	IORun += GPIOs.DO.VWRelay5;
	IORun += GPIOs.DO.OperatingLED;
	IORun += GPIOs.DI.UsbConnected;

	GPIOs.DO.OperatingLED = MiIoT_LED;

	return IORun ? RESULT_RUN : RESULT_ERROR;
}

void MiMain_GPIODeInit(void)
{
	HAL_TIM_PWM_DeInit(&htim1);
	HAL_TIM_IC_DeInit(&htim2);

	memset(&GPIOs.DO, 0, sizeof(GPIOs.DO));
	GPIOs.DO.LoRaEnable = 1;
	MiMain_GPIOControl();
	memset(&GPIOs.DI, 0, sizeof(GPIOs.DI));

	Native_DisableGPIOs(&DO_LORA_ENABLE, 3);

	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = DI_USBC_CONNECTED_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
	GPIO_InitStruct.Pull = GPIO_PULLDOWN;
	HAL_GPIO_Init(DI_USBC_CONNECTED_GPIO_Port, &GPIO_InitStruct);
}

oResult_t MiMain_UpdateSampling(IoT_DataPacket_t **ppPacket)
{
	static uint8_t UpdateMeasureStep = 0;
	static IoT_DataPacket_t SmaplingData;
	oResult_t result = RESULT_RUN;

	switch(UpdateMeasureStep)
	{
		default:
			UpdateMeasureStep = 0;
			break;
		case 0:
			if((result = Measurement_Supply(20)) == RESULT_OK)
			{
				oSerial_Log("UpdateMeasure", "SupplyVolt %d(mV)", (int)GPIOs.ADC.SystemSupply);

				if(GPIOs.ADC.SystemSupply < SYSTEM_SUPPLY_LOW_LIMIT){
					oSerial_Log("UpdateMeasure", "Fail low battery");
					result = RESULT_FAULT;
				}
				else{
					UpdateMeasureStep++;
					oSerial_Log("UpdateMeasure", "Start");
					result = RESULT_RUN;
				}
			}
			break;
		case 1:
			switch(Measurement_Sensor(&SmaplingData))
			{
				case RESULT_RUN:
					break;
				case RESULT_OK:
					*ppPacket = &SmaplingData;
					result = RESULT_OK;
					UpdateMeasureStep++;
					break;
				default:
					result = RESULT_ERROR;
					oSerial_Log("UpdateSampling", "sampling error \r\n");
					break;
			}
			break;
		case 2:
			result = RESULT_DONE;
			break;
	}

	if(result != RESULT_RUN && result != RESULT_OK){
		UpdateMeasureStep = 0;
		oSerial_Log("UpdateMeasure", "finish");
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

	Native_WatchDog(SECOND_TO_MS(11));

	switch(MiMainStep)
	{
		case 0: //Init

			MiIoT_SamplingCallback = MiMain_UpdateSampling;
			MiIoT_MeasurementCallback = MiMain_UpdateSampling;
			MiIoT_StatusCallback = MiMain_UpdateStatus;
			MiIoT_IOControlCallback = MiMain_GPIOControl;
			MiIoT_GPIOInitCallback = main_GPIOInit;
			MiIoT_GPIODeInitCallback = MiMain_GPIODeInit;

			MiMainStep++;
		case 1:
			HAL_QSPI_DeInit(&hqspi);
			MiMainStep++;
			break;
		default:
			MiStorage();
			MiIoT(&huart3);
			MiSerial(&huart1);
			break;
	}
}

