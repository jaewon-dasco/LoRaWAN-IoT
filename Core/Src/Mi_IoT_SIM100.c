/*
 * Mi_IoT.c
 *
 *  Created on: Nov 29, 2024
 *      Author: JONE
 */

#include "Mi_IoT.h"

const IoTParameter_t MiIoT_DefaultParameter = {
	.Information.ProductCode = IoTProductType_SIM100_6C,
	.Operating.OperatingMode = IoTOperatingMode_Stop,
	.Operating.SamplingInterval = 60,
	.Operating.UpdateInterval = 60,
	.ChannelConfig = {
		[0] = { .TypeOfSensor = IoTSensorType_mV, .WarmupTime = 1000, .SupplySource = 1, },
		[1] = { .TypeOfSensor = IoTSensorType_mV, .WarmupTime = 1000, .SupplySource = 1, },
		[3] = { .TypeOfSensor = IoTSensorType_mV, .WarmupTime = 1000, .SupplySource = 2, },
		[4] = { .TypeOfSensor = IoTSensorType_mV, .WarmupTime = 1000, .SupplySource = 2, },
		[6] = { .TypeOfSensor = IoTSensorType_mV, .WarmupTime = 1000, .SupplySource = 3, },
		[7] = { .TypeOfSensor = IoTSensorType_mV, .WarmupTime = 1000, .SupplySource = 3, },
	},
};

oResult_t MiIoT_IsValidParameter(IoTParameter_t *pParameter)
{
	uint8_t i = 0;
	uint8_t Enabled = 0;

	if(pParameter == NULL || pParameter->Information.ProductCode != IoTProductType_SIM100_6C){
		return RESULT_NULL;
	}

	for(i=0; i<MIIOT_CHANNEL_MAXCOUNT; i++)
	{
		if(pParameter->ChannelConfig[i].TypeOfSensor == IoTSensorType_FullBridge || pParameter->ChannelConfig[i].TypeOfSensor == IoTSensorType_Differential){
			if(i%3 != 0){
				pParameter->ChannelConfig[i].TypeOfSensor = IoTSensorType_NULL;
			}
			else{
				pParameter->ChannelConfig[i+1].TypeOfSensor = IoTSensorType_NULL;
			}
		}

		if(pParameter->ChannelConfig[i].TypeOfSensor == IoTSensorType_Resistance || pParameter->ChannelConfig[i].TypeOfSensor == IoTSensorType_Thermistor){
			if(i%3 != 2){
				pParameter->ChannelConfig[i].TypeOfSensor = IoTSensorType_NULL;
			}
		}

		switch(pParameter->ChannelConfig[i].TypeOfSensor)
		{
			case IoTSensorType_mV:
			case IoTSensorType_FullBridge:
			case IoTSensorType_mA:
			case IoTSensorType_Differential:
			case IoTSensorType_Resistance:
			case IoTSensorType_Thermistor:
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
	if(pPayload == NULL || ProductCode != IoTProductType_SIM100_6C){
		return RESULT_NULL;
	}

	if(pPayload->TypeOfData == IoTDataType_DataArray_Type1){
		return RESULT_OK;
	}

	return RESULT_ERROR;
}

/* History

2026-06-26 | v0.1
	- baseline (Mi_IoT_SIM100.c)
*/
