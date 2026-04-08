/*
 * Mi_IoT.c
 *
 *  Created on: Nov 29, 2024
 *      Author: JONE
 */

#include "Mi_IoT.h"

const IoTParameter_t MiIoT_DefaultParameter = {
	.Information.ProductCode = IoTProductType_SIV100_5C,
	.Operating.OperatingMode = IoTOperatingMode_Stop,
	.Operating.SamplingInterval = 60,
	.Operating.UpdateInterval = 60,
	.ChannelConfig = {
		[0] = { .TypeOfSensor = IoTSensorType_VibratingWire, .WarmupTime = 1000, .SupplySource = 1, .Properties.VibrationWrie.StartFreq = 400, .Properties.VibrationWrie.EndFreq = 6000, },
		[1] = { .TypeOfSensor = IoTSensorType_Thermistor, .WarmupTime = 1000, .SupplySource = 1, },
		[2] = { .TypeOfSensor = IoTSensorType_VibratingWire, .WarmupTime = 1000, .SupplySource = 2, .Properties.VibrationWrie.StartFreq = 400, .Properties.VibrationWrie.EndFreq = 6000, },
		[3] = { .TypeOfSensor = IoTSensorType_Thermistor, .WarmupTime = 1000, .SupplySource = 2, },
		[4] = { .TypeOfSensor = IoTSensorType_VibratingWire, .WarmupTime = 1000, .SupplySource = 3, .Properties.VibrationWrie.StartFreq = 400, .Properties.VibrationWrie.EndFreq = 6000, },
		[5] = { .TypeOfSensor = IoTSensorType_Thermistor, .WarmupTime = 1000, .SupplySource = 3, },
		[6] = { .TypeOfSensor = IoTSensorType_VibratingWire, .WarmupTime = 1000, .SupplySource = 4, .Properties.VibrationWrie.StartFreq = 400, .Properties.VibrationWrie.EndFreq = 6000, },
		[7] = { .TypeOfSensor = IoTSensorType_Thermistor, .WarmupTime = 1000, .SupplySource = 4, },
		[8] = { .TypeOfSensor = IoTSensorType_VibratingWire, .WarmupTime = 1000, .SupplySource = 5, .Properties.VibrationWrie.StartFreq = 400, .Properties.VibrationWrie.EndFreq = 6000, },
		[9] = { .TypeOfSensor = IoTSensorType_Thermistor, .WarmupTime = 1000, .SupplySource = 5, },
	},
};

oResult_t MiIoT_IsValidParameter(IoTParameter_t *pParameter)
{
	uint8_t i = 0;
	uint8_t Enabled = 0;

	if(pParameter == NULL || pParameter->Information.ProductCode != IoTProductType_SIV100_5C){
		return RESULT_NULL;
	}

	for(i=0; i<MIIOT_CHANNEL_MAXCOUNT; i++)
	{
		switch(pParameter->ChannelConfig[i].TypeOfSensor)
		{
			case IoTSensorType_Thermistor:
			case IoTSensorType_VibratingWire:
				Enabled++;
				break;
			default:
				break;
		}
	}

	return Enabled > 0 ? RESULT_OK : RESULT_ERROR;
}

oResult_t MiIoT_IsSensorData(IoTProductType_t ProductCode, IoT_DataPacket_t *pPayload)
{
	if(pPayload == NULL || ProductCode != IoTProductType_SIV100_5C){
		return RESULT_NULL;
	}

	if(pPayload->TypeOfData == IoTDataType_Analog){
		return RESULT_OK;
	}

	return RESULT_ERROR;
}
