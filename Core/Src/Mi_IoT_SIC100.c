/*
 * Mi_IoT.c
 *
 *  Created on: Nov 29, 2024
 *      Author: JONE
 */

#include "Mi_IoT.h"

const IoTParameter_t MiIoT_DefaultParameter = {
	.Information.ProductCode = IoTProductType_SIC100_2C,
	.Operating.OperatingMode = IoTOperatingMode_Stop,
	.Operating.SamplingInterval = 60,
	.Operating.UpdateInterval = 60,
	.ChannelConfig = {{
		.TypeOfSensor = IoTSensorType_ArrayDualTilt,
		.WarmupTime = 1000,
		.SupplySource = 1,
		.Properties.Array.CountOfSensor = 30,
		.Properties.Array.SensorId = 1,
	}},
};

oResult_t MiIoT_IsValidParameter(IoTParameter_t *pParameter)
{
	uint8_t i = 0;
	uint8_t Enabled = 0;

	if(pParameter == NULL || pParameter->Information.ProductCode != IoTProductType_SIC100_2C){
		return RESULT_NULL;
	}

	for(i=0; i<MIIOT_CHANNEL_MAXCOUNT; i++)
	{
		if(i==0){
			switch(pParameter->ChannelConfig[i].TypeOfSensor)
			{
				case IoTSensorType_ArrayDualTilt:
				case IoTSensorType_ArraySingleTilt:
					Enabled++;
					break;
				default:
					break;
			}
		}
		else{
			switch(pParameter->ChannelConfig[i].TypeOfSensor)
			{
				case IoTSensorType_mV:
				case IoTSensorType_mA:
					Enabled++;
					break;
				default:
					break;
			}
		}
	}

	return Enabled > 0 ? RESULT_OK : RESULT_ERROR;
}

oResult_t MiIoT_IsSensorData(IoTProductType_t ProductCode, IoT_DataPacket_t *pPayload)
{
	if(pPayload == NULL || ProductCode != IoTProductType_SIC100_2C){
		return RESULT_NULL;
	}

	if(pPayload->TypeOfData == IoTDataType_ArrayDualTilt || pPayload->TypeOfData == IoTDataType_ArraySingleTilt || pPayload->TypeOfData == IoTDataType_Tilt || pPayload->TypeOfData == IoTDataType_Analog){
		return RESULT_OK;
	}

	return RESULT_ERROR;
}
