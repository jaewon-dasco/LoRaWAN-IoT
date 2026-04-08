#include "MCP23S17.h"
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

#define SYSTEM_SUPPLY_LOW_LIMIT		3250
#define RTC_SUPPLY_LOW_LIMIT		2100

GPIOs_t GPIOs;
oDebounce_t DB_USBConnected	= DEBOUNCE_INITIALIZER(30,30);
oDebounce_t DebounceError	= DEBOUNCE_INITIALIZER(1000,300);

MCP23S17_Param_t MCP23S17Para = {
	// Direction: All Output (0=Output, 1=Input)
	.Dir_GPA0 = MCP23S17_DIR_OUTPUT, .Dir_GPA1 = MCP23S17_DIR_OUTPUT,
	.Dir_GPA2 = MCP23S17_DIR_OUTPUT, .Dir_GPA3 = MCP23S17_DIR_OUTPUT,
	.Dir_GPA4 = MCP23S17_DIR_OUTPUT, .Dir_GPA5 = MCP23S17_DIR_OUTPUT,
	.Dir_GPA6 = MCP23S17_DIR_OUTPUT, .Dir_GPA7 = MCP23S17_DIR_OUTPUT,
	.Dir_GPB0 = MCP23S17_DIR_OUTPUT, .Dir_GPB1 = MCP23S17_DIR_OUTPUT,
	.Dir_GPB2 = MCP23S17_DIR_OUTPUT, .Dir_GPB3 = MCP23S17_DIR_OUTPUT,
	.Dir_GPB4 = MCP23S17_DIR_OUTPUT, .Dir_GPB5 = MCP23S17_DIR_OUTPUT,
	.Dir_GPB6 = MCP23S17_DIR_OUTPUT, .Dir_GPB7 = MCP23S17_DIR_OUTPUT,
	// Pull-Up: All Disable
	.PullUp_GPA0 = MCP23S17_PULLUP_DISABLE, .PullUp_GPA1 = MCP23S17_PULLUP_DISABLE,
	.PullUp_GPA2 = MCP23S17_PULLUP_DISABLE, .PullUp_GPA3 = MCP23S17_PULLUP_DISABLE,
	.PullUp_GPA4 = MCP23S17_PULLUP_DISABLE, .PullUp_GPA5 = MCP23S17_PULLUP_DISABLE,
	.PullUp_GPA6 = MCP23S17_PULLUP_DISABLE, .PullUp_GPA7 = MCP23S17_PULLUP_DISABLE,
	.PullUp_GPB0 = MCP23S17_PULLUP_DISABLE, .PullUp_GPB1 = MCP23S17_PULLUP_DISABLE,
	.PullUp_GPB2 = MCP23S17_PULLUP_DISABLE, .PullUp_GPB3 = MCP23S17_PULLUP_DISABLE,
	.PullUp_GPB4 = MCP23S17_PULLUP_DISABLE, .PullUp_GPB5 = MCP23S17_PULLUP_DISABLE,
	.PullUp_GPB6 = MCP23S17_PULLUP_DISABLE, .PullUp_GPB7 = MCP23S17_PULLUP_DISABLE,
};
MCP23S17_t MCP23S17;

oIO_t DO_RS485_ENABLE = {DO_RS485_ENABLE_GPIO_Port, DO_RS485_ENABLE_Pin, IO_LOW};
oIO_t DO_NAND_ENALBE = {DO_NAND_ENALBE_GPIO_Port, DO_NAND_ENALBE_Pin, IO_LOW};
oIO_t DO_IOEXT_RESET = {DO_IOEXT_RESET_GPIO_Port, DO_IOEXT_RESET_Pin, IO_LOW};
oIO_t DO_IOEXT_CS = {DO_IOEXT_CS_GPIO_Port, DO_IOEXT_CS_Pin, IO_LOW};
oIO_t DO_RS232_ENABLE = {DO_RS232_ENABLE_GPIO_Port, DO_RS232_ENABLE_Pin, IO_HIGH};
oIO_t DO_VO5V_ENABLE = {DO_VO5V_ENABLE_GPIO_Port, DO_VO5V_ENABLE_Pin, IO_HIGH};
oIO_t DO_VO12V_ENABLE = {DO_VO12V_ENABLE_GPIO_Port, DO_VO12V_ENABLE_Pin, IO_HIGH};
oIO_t DO_5V_ENABLE = {DO_5V_ENABLE_GPIO_Port, DO_5V_ENABLE_Pin, IO_HIGH};
oIO_t DO_BAT_CHECK = {DO_BAT_CHECK_GPIO_Port, DO_BAT_CHECK_Pin, IO_HIGH};
oIO_t DO_VW_ENABLE = {DO_VW_ENABLE_GPIO_Port, DO_VW_ENABLE_Pin, IO_HIGH};
oIO_t DO_VW_DE = {DO_VW_DE_GPIO_Port, DO_VW_DE_Pin, IO_HIGH};
oIO_t DO_AI_0548P_MA_ENABLE = {DO_AI_0548P_MA_ENABLE_GPIO_Port, DO_AI_0548P_MA_ENABLE_Pin, IO_HIGH};
oIO_t DO_AI_0548M_MA_ENABLE = {DO_AI_0548M_MA_ENABLE_GPIO_Port, DO_AI_0548M_MA_ENABLE_Pin, IO_HIGH};
oIO_t DO_CH1_ENABLE = {DO_CH1_ENABLE_GPIO_Port, DO_CH1_ENABLE_Pin, IO_HIGH};
oIO_t DO_CH2_ENABLE = {DO_CH2_ENABLE_GPIO_Port, DO_CH2_ENABLE_Pin, IO_HIGH};
oIO_t DO_AI_AMP_EANBLE = {DO_AI_AMP_EANBLE_GPIO_Port, DO_AI_AMP_EANBLE_Pin, IO_HIGH};
oIO_t DO_VW_MODE_ENABLE = {DO_VW_MODE_ENABLE_GPIO_Port, DO_VW_MODE_ENABLE_Pin, IO_HIGH};
oIO_t DO_TEMP_MODE_ENABLE = {DO_TEMP_MODE_ENABLE_GPIO_Port, DO_TEMP_MODE_ENABLE_Pin, IO_HIGH};
oIO_t DO_AI_MODE_ENABLE = {DO_AI_MODE_ENABLE_GPIO_Port, DO_AI_MODE_ENABLE_Pin, IO_HIGH};
oIO_t DO_LED_COM = {DO_LED_COM_GPIO_Port, DO_LED_COM_Pin, IO_HIGH};

oIO_t DO_LORA_ENABLE = {DO_LORA_ENABLE_GPIO_Port, DO_LORA_ENABLE_Pin, IO_LOW};
oIO_t DO_LED_OPERATING = {DO_LED_OPERATING_GPIO_Port, DO_LED_OPERATING_Pin, IO_HIGH};
oIO_t DI_USB_CONNECTED = {DI_USB_CONNECTED_GPIO_Port, DI_USB_CONNECTED_Pin, IO_HIGH};

oResult_t MiMain_GPIOControl()
{
	if(!MCP23S17.IsFault){
		if(!MCP23S17.IsOpen){
			if(MCP23S17_Init(&MCP23S17, &hspi1, &DO_IOEXT_CS, &DO_IOEXT_RESET, 1, &MCP23S17Para) == RESULT_FAULT){
				oSerial_Log("GPIOControl", "MCP23S17 init fault");
			}
		}
		else{
			MCP23S17_WritePin(&MCP23S17, MCP23S17_PORTA, 0, GPIOs.DO.Port16Enable);
			MCP23S17_WritePin(&MCP23S17, MCP23S17_PORTA, 1, GPIOs.DO.Port15Enable);
			MCP23S17_WritePin(&MCP23S17, MCP23S17_PORTA, 2, GPIOs.DO.Port14Enable);
			MCP23S17_WritePin(&MCP23S17, MCP23S17_PORTA, 3, GPIOs.DO.Port13Enable);
			MCP23S17_WritePin(&MCP23S17, MCP23S17_PORTA, 4, GPIOs.DO.Port12Enable);
			MCP23S17_WritePin(&MCP23S17, MCP23S17_PORTA, 5, GPIOs.DO.Port11Enable);
			MCP23S17_WritePin(&MCP23S17, MCP23S17_PORTA, 6, GPIOs.DO.Port10Enable);
			MCP23S17_WritePin(&MCP23S17, MCP23S17_PORTA, 7, GPIOs.DO.Port9Enable);

			MCP23S17_WritePin(&MCP23S17, MCP23S17_PORTB, 0, GPIOs.DO.Port1Enable);
			MCP23S17_WritePin(&MCP23S17, MCP23S17_PORTB, 1, GPIOs.DO.Port2Enable);
			MCP23S17_WritePin(&MCP23S17, MCP23S17_PORTB, 2, GPIOs.DO.Port3Enable);
			MCP23S17_WritePin(&MCP23S17, MCP23S17_PORTB, 3, GPIOs.DO.Port4Enable);
			MCP23S17_WritePin(&MCP23S17, MCP23S17_PORTB, 4, GPIOs.DO.Port5Enable);
			MCP23S17_WritePin(&MCP23S17, MCP23S17_PORTB, 5, GPIOs.DO.Port6Enable);
			MCP23S17_WritePin(&MCP23S17, MCP23S17_PORTB, 6, GPIOs.DO.Port7Enable);
			MCP23S17_WritePin(&MCP23S17, MCP23S17_PORTB, 7, GPIOs.DO.Port8Enable);
		}
	}

	IO_WRITE(DO_VW_DE, GPIOs.DO.VwDe);

	IO_WRITE(DO_VO5V_ENABLE, GPIOs.DO.Vo5vEnable);
	IO_WRITE(DO_VO12V_ENABLE, GPIOs.DO.Vo12vEnable);
	IO_WRITE(DO_BAT_CHECK, GPIOs.DO.BatCheck);
	IO_WRITE(DO_5V_ENABLE, GPIOs.DO.Dc5vEnable);

	IO_WRITE(DO_RS485_ENABLE, GPIOs.DO.Rs485Enable);
	IO_WRITE(DO_RS232_ENABLE, GPIOs.DO.Rs232Enable);

	IO_WRITE(DO_LORA_ENABLE, GPIOs.DO.LoRaEnable);
	IO_WRITE(DO_NAND_ENALBE, GPIOs.DO.NANDEnable);
	IO_WRITE(DO_VW_ENABLE, GPIOs.DO.VwEnable);
	IO_WRITE(DO_AI_0548P_MA_ENABLE, GPIOs.DO.AnalogPCurrentEnable);
	IO_WRITE(DO_AI_0548M_MA_ENABLE, GPIOs.DO.AnalogMCurrentEnable);

	IO_WRITE(DO_AI_AMP_EANBLE, GPIOs.DO.AnalogAmpEnable);
	IO_WRITE(DO_CH1_ENABLE, GPIOs.DO.Ch1Enable);
	IO_WRITE(DO_CH2_ENABLE, GPIOs.DO.Ch2Enable);
	IO_WRITE(DO_VW_MODE_ENABLE, GPIOs.DO.VwModeEnable);
	IO_WRITE(DO_TEMP_MODE_ENABLE, GPIOs.DO.TempModeEnable);
	IO_WRITE(DO_AI_MODE_ENABLE, GPIOs.DO.AnalogModeEnable);

	IO_WRITE(DO_LED_OPERATING, GPIOs.DO.LedPower);
	IO_WRITE(DO_LED_COM, GPIOs.DO.LedCom);

	oDebounce_GPIO(DI_USB_CONNECTED, &DB_USBConnected);
	GPIOs.DI.UsbConnected = DB_USBConnected.Output || DB_USBConnected.Input;

	uint32_t IORun = 0;
	//IORun += GPIOs.DO.LoRaEnable;
	IORun += GPIOs.DO.VwDe;
	IORun += GPIOs.DO.Vo5vEnable;
	IORun += GPIOs.DO.Vo12vEnable;
	IORun += GPIOs.DO.BatCheck;
	IORun += GPIOs.DO.Dc5vEnable;
	IORun += GPIOs.DO.Rs485Enable;
	IORun += GPIOs.DO.Rs232Enable;
	IORun += GPIOs.DO.NANDEnable;
	IORun += GPIOs.DO.VwEnable;
	IORun += GPIOs.DO.AnalogPCurrentEnable;
	IORun += GPIOs.DO.AnalogMCurrentEnable;
	IORun += GPIOs.DO.AnalogAmpEnable;
	IORun += GPIOs.DO.VwModeEnable;
	IORun += GPIOs.DO.TempModeEnable;
	IORun += GPIOs.DO.AnalogModeEnable;
	IORun += GPIOs.DO.LedPower;
	IORun += GPIOs.DO.LedCom;
	IORun += GPIOs.DO.Ch1Enable;
	IORun += GPIOs.DO.Ch2Enable;
	for(uint8_t i = 0; i < 16; i++) IORun += GPIOs.DO.PortArray[i];
	IORun += GPIOs.DI.UsbConnected;

	GPIOs.DO.LedPower = MiIoT_LED;

	return IORun ? RESULT_RUN : RESULT_ERROR;
}

void MiMain_GPIODeInit(void)
{
	HAL_ADC_Stop_DMA(&hadc1);
	MCP23S17_DeInit(&MCP23S17);

	memset(&GPIOs.DO, 0, sizeof(GPIOs.DO));
	GPIOs.DO.LoRaEnable = 1;
	MiMain_GPIOControl();
	memset(&GPIOs.DI, 0, sizeof(GPIOs.DI));

	Native_DisableGPIOs(&DO_LORA_ENABLE, 3);

	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = DI_USB_CONNECTED_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
	GPIO_InitStruct.Pull = GPIO_PULLDOWN;
	HAL_GPIO_Init(DI_USB_CONNECTED_GPIO_Port, &GPIO_InitStruct);

	HAL_TIM_PWM_DeInit(&htim1);
	HAL_TIM_IC_DeInit(&htim2);
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
			MiIoT(&huart1);
			MiSerial(&huart2);
			break;
	}
}

