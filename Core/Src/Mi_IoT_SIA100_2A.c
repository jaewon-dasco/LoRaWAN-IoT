/*
 * Mi_IoT.c
 *
 *  Created on: Nov 29, 2024
 *      Author: JONE
 */

#include "Mi_IoT.h"

const IoTParameter_t MiIoT_DefaultParameter = {
	.Information.ProductCode = IoTProductType_SIA100_2A,
	.Operating.OperatingMode = IoTOperatingMode_Stop,
	.Operating.SamplingInterval = 60,
	.Operating.UpdateInterval = 60,
	.ChannelConfig = {{
		.TypeOfSensor = IoTSensorType_Tilt,
		.WarmupTime = 1000,
	}},
};

oResult_t MiIoT_IsValidParameter(IoTParameter_t *pParameter)
{
	uint8_t i = 0;
	uint8_t Enabled = 0;

	if(pParameter == NULL || pParameter->Information.ProductCode != IoTProductType_SIA100_2A){
		return RESULT_NULL;
	}

	for(i=0; i<MIIOT_CHANNEL_MAXCOUNT; i++)
	{
		switch(pParameter->ChannelConfig[i].TypeOfSensor)
		{
			case IoTSensorType_Tilt:
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
	if(pPayload == NULL || ProductCode != IoTProductType_SIA100_2A){
		return RESULT_NULL;
	}

	if(pPayload->TypeOfData == IoTDataType_Tilt){
		return RESULT_OK;
	}

	return RESULT_ERROR;
}
