/*
 * LoRa_RAK3172.c
 *
 *  Created on: Apr 29, 2025
 *      Author: JONE
 */
#include <stdlib.h>
#include "LoRa_RAK3172.h"

#include "ONE_Serial.h"
#include "ONE_ATCommend.h"
#include "ONE_Common.h"
#include "ONE_Time.h"

#define RAK3172_COMPARE_COMMEND(str, cmd)	(strncmp(str, cmd, strlen(cmd)) == 0)

RAK3172_t RAK3172Dev;
static char TxStringBuffer[600];

void RAK3172_ReceiveCallback(char *Message, uint32_t SizeOfMessage)
{
	uint32_t Len;
	char *pStr;

	if(Message == NULL || SizeOfMessage <= 0){
		return;
	}

	if((pStr=strstr(Message, "+EVT:")) != NULL){
		pStr += strlen("+EVT:");

		oSerial_Log("RAK3172", "Receive EVT | %s", pStr);

		if(RAK3172Dev.Status.IsOpened){
			if(RAK3172_COMPARE_COMMEND(pStr, "JOINED")){
				RAK3172Dev.Status.IsJoined = 1;
				RAK3172Dev.Status.IsJoinFail = 0;
			}
			else if(RAK3172_COMPARE_COMMEND(pStr, "AT_NO_NETWORK_JOINED")){

				RAK3172Dev.Status.IsJoined = 0;
				RAK3172Dev.Status.IsJoinFail = 1;
			}
			else if(RAK3172_COMPARE_COMMEND(pStr, "JOIN_FAILED")){

				RAK3172Dev.Status.IsJoined = 0;
				RAK3172Dev.Status.IsJoinFail = 1;
			}
		}

		if(RAK3172_COMPARE_COMMEND(pStr, "SEND_CONFIRMED_OK")){
			RAK3172Dev.Status.IsSendComformed = 1;
		}
		else if(RAK3172_COMPARE_COMMEND(pStr, "SEND_CONFIRMED_FAILED")){
			RAK3172Dev.Status.IsSendFailed = 1;
		}
		else if(RAK3172_COMPARE_COMMEND(pStr, "TX_DONE")){
			RAK3172Dev.Status.IsTxDone = 1;
		}


		if(RAK3172_COMPARE_COMMEND(pStr, "TIMEREQ_OK")){
			RAK3172Dev.Status.IsTimeSynced = 1;
			RAK3172Dev.Status.IsTimeSyncEnabled = 0;
		}
		else if(RAK3172_COMPARE_COMMEND(pStr, "TIMEREQ_FAILED")){
			RAK3172Dev.Status.IsTimeSynced = 0;
			RAK3172Dev.Status.IsTimeSyncEnabled = 0;
		}

		//+EVT:LINKCHECK:0:21:1:-60:11 (RUI3 new) or +EVT:LINKCHECK:0,21,1,-60,11 (RUI3 old)
		if((pStr=strstr(pStr, "LINKCHECK:")) != NULL){
			int linkcheck, margin, gateways, rssi, snr;
			pStr += strlen("LINKCHECK:");

			if(sscanf(pStr, "%d:%d:%d:%d:%d", &linkcheck, &margin, &gateways, &rssi, &snr) == 5 ||
			   sscanf(pStr, "%d,%d,%d,%d,%d", &linkcheck, &margin, &gateways, &rssi, &snr) == 5){
				RAK3172Dev.RSSI = (int16_t)rssi;
				RAK3172Dev.SNR = (int16_t)snr;
			}
		}
	}

	if((pStr=strstr(Message, "RX_1:")) != NULL || (pStr=strstr(Message, "RX_B:")) != NULL){
		pStr += 5;
		oSerial_Log("RAK3172", "Receive | RX_DOWNLINK | %s", pStr);

		if(RAK3172Dev.DownlinkCallback){
			int rssi, snr, port;
			char payload[601];
			uint8_t Buffer[300];

			//-74:5:UNICAST:201:0a003c00
			int cnt = sscanf(pStr, "%d:%d:UNICAST:%d:%600s", &rssi, &snr, &port, payload);

			if(cnt == 4){
				Len = strlen(payload) / 2;

				if(port > 0 && port < 233 && Len > 0 && Len <= sizeof(Buffer)){
					oHexStringToBytes(payload, Buffer, sizeof(Buffer));
					RAK3172Dev.DownlinkCallback(port, Buffer, Len);
				}
			}
		}

		return;
	}

}

oResult_t RAK3172_TimeStringToDT(char *TimeString, oDateAndTime_t *pDT)
{
    int hour, min, sec;
    int month, day, year;

    if(TimeString == NULL || strlen(TimeString) <= 0 || pDT == NULL){
		return RESULT_ERROR;
    }

    //07h48m53s on 05/12/2025
	if(sscanf(TimeString, "%02dh%02dm%02ds on %02d/%02d/%04d", &hour, &min, &sec, &month, &day, &year) != 6){
		return RESULT_ERROR;
	}

	if(year == 0 || month == 0 || day == 0){
		return RESULT_ERROR;
	}

	pDT->Year = year;
	pDT->Month = month;
	pDT->Day = day;
	pDT->Hour = hour;
	pDT->Minute = min;
	pDT->Second = sec;

	return RESULT_OK;
}

uint16_t RAK3172_MaxPayloadSize(uint8_t DR)
{
	switch(DR)
	{
		default:
		case 0:
		case 1:
		case 2:
			return 51;
		case 3:
			return 115;
		case 4:
			return 222;
		case 5:
		case 6:
		case 7:
			return 242;
	}
}

oResult_t RAK3172_GetDataRate(uint8_t *pDR)
{
	oResult_t result = RESULT_RUN;

	switch(oAT_TransmitCommend(&RAK3172Dev.AT, "AT+DR=?", "+DR=", 3000, 3, 1))
	{
		case AT_RESULT_RUN:
			break;
		case AT_RESULT_OK:
	        if(isdigit((unsigned char)*RAK3172Dev.AT.pResponseData)) {
	        	RAK3172Dev.Information.DataRate = (uint8_t)atoi(RAK3172Dev.AT.pResponseData);

				if(pDR){
					*pDR = RAK3172Dev.Information.DataRate;
				}

				result = RESULT_OK;
	        }
	        else{
	        	result = RESULT_ERROR;
	        }
			break;
		default:
			oSerial_Log("RAK3172", "Commend fail | AT+DR");
			result = RESULT_ERROR;
			break;
	}

	RAK3172Dev.Status.IsBusy = result == RESULT_RUN ? 1: 0;

	return result;
}

/* 	RAK3172_GetLocalTime
	- Input
		pDT			| oDateAndTime pointer
		UtcOffset	| UTC offset minute
	- Retrun
		Result
*/
oResult_t RAK3172_GetLocalTime(oDateAndTime_t *pDT, oUtcOffsetMinute_t UtcOffsetMinute)
{
    oResult_t result = RESULT_RUN;

    if(!RAK3172Dev.Status.IsTimeSynced){
    	result =  RESULT_ERROR;
		goto EXIT;
    }

	switch(oAT_TransmitCommend(&RAK3172Dev.AT, "AT+LTIME=?", "LTIME=", 3000, 3, 1))
	{
		case AT_RESULT_RUN:
			break;
		case AT_RESULT_OK:
	        if(strlen(RAK3172Dev.AT.pResponseData)) {
				if((result=RAK3172_TimeStringToDT(RAK3172Dev.AT.pResponseData, pDT)) == RESULT_OK){
					oDT_AddMinute(pDT, (int32_t)UtcOffsetMinute);
				}
	        }
	        else{
	        	result = RESULT_ERROR;
	        }
			break;
		default:
			oSerial_Log("RAK3172", "Commend fail | AT+LTIME");
			result = RESULT_ERROR;
			break;
	}

EXIT:
	RAK3172Dev.Status.IsBusy = result == RESULT_RUN ? 1: 0;

	return result;
}

oResult_t RAK3172_GetSignalQuality()
{
	static uint8_t GetSignalQualityStep = 0;
	oResult_t result = RESULT_RUN;

	switch(GetSignalQualityStep)
	{
		case 0:
			switch(oAT_TransmitCommend(&RAK3172Dev.AT, "AT+RSSI=?", "AT+RSSI=", 3000, 3, 1))
			{
				case AT_RESULT_RUN:
					break;
				case AT_RESULT_OK:
					RAK3172Dev.RSSI = (int16_t)atoi(RAK3172Dev.AT.pResponseData);
					GetSignalQualityStep++;
					break;
				default:
					oSerial_Log("RAK3172", "Commend fail | AT+RSSI");
					GetSignalQualityStep++;
					break;
			}
			break;
		case 1:
			switch(oAT_TransmitCommend(&RAK3172Dev.AT, "AT+SNR=?", "AT+SNR=", 3000, 3, 1))
			{
				case AT_RESULT_RUN:
					break;
				case AT_RESULT_OK:
					RAK3172Dev.SNR = (int16_t)atoi(RAK3172Dev.AT.pResponseData);
					GetSignalQualityStep++;
					break;
				default:
					oSerial_Log("RAK3172", "Commend fail | AT+SNR");
					GetSignalQualityStep++;
					break;
			}
			break;
		case 2:
			result = RESULT_OK;
			GetSignalQualityStep = 0;
			break;
	}

	RAK3172Dev.Status.IsBusy = result == RESULT_RUN ? 1: 0;

	return result;
}

oResult_t RAK3172_JoinState()
{
	oResult_t result = RESULT_RUN;

	switch(oAT_TransmitCommend(&RAK3172Dev.AT, "AT+NJS=?", "+NJS=", 5000, 3, 1))
	{
		case AT_RESULT_RUN:
			break;
		case AT_RESULT_OK:
			if(strstr(RAK3172Dev.AT.pResponseData, "1") != NULL){
				result = RESULT_OK;
				RAK3172Dev.Status.IsJoined = 1;
			}
			else if(strstr(RAK3172Dev.AT.pResponseData, "0") != NULL){
				result = RESULT_ERROR;
				RAK3172Dev.Status.IsJoined = 0;
			}
			else{
				result = RESULT_ERROR;
			}
			break;
		default:
			oSerial_Log("RAK3172", "Commend fail | AT+NJS");
			result = RESULT_ERROR;
			break;
	}

	RAK3172Dev.Status.IsBusy = result == RESULT_RUN ? 1: 0;

	return result;
}

oResult_t RAK3172_Transmit(uint8_t Port, uint8_t *pData, uint32_t SizeofData, uint8_t Confirmed)
{
	static uint8_t RAK3172TransmitStep = 0;
	static uint32_t RAK3172TransmitTimer = 0;
	char *pStr;
	oResult_t result = RESULT_RUN;

	if(pData == NULL || Port < 1 || Port > 233 || SizeofData < 1 || SizeofData > 256){
		result = RESULT_NULL;
		goto EXIT;
	}

	switch(RAK3172TransmitStep)
	{
		case 0:
			if(!RAK3172Dev.Status.IsBusy){
				RAK3172TransmitStep++;
			}
			break;
		case 1:
			if(RAK3172Dev.Status.IsJoined){
				RAK3172TransmitStep++;
			}
			else{
				result = RESULT_FAULT;
			}
			break;
		case 2:
			switch(oAT_TransmitCommend(&RAK3172Dev.AT, Confirmed ? "AT+CFM=1" :  "AT+CFM=0", "OK", 3000, 3, 1))
			{
				case AT_RESULT_RUN:
					break;
				case AT_RESULT_OK:
					RAK3172TransmitStep++;
					break;
				default:
					oSerial_Log("RAK3172", "Commend fail | AT+CFM");
					result = RESULT_FAULT;
					break;
			}
			break;
		case 3:
			if(!RAK3172Dev.Status.IsTimeSyncEnabled){
				RAK3172TransmitStep++;
			}
			else{
				switch(oAT_TransmitCommend(&RAK3172Dev.AT, "AT+TIMEREQ=1", "OK", 3000, 3, 1))
				{
					case AT_RESULT_RUN:
						break;
					case AT_RESULT_OK:
						RAK3172Dev.Status.IsTimeSynced = 0;
						RAK3172TransmitStep++;
						break;
					default:
						oSerial_Log("RAK3172", "Commend fail | AT+TIMEREQ");

						RAK3172Dev.Status.IsJoined = 0;
						result = RESULT_FAULT;
						break;
				}
			}
			break;
		case 4:
			if(RAK3172Dev.AT.ProcessStep <= 1){
				pStr = (char *)&TxStringBuffer;

				snprintf(pStr, sizeof(TxStringBuffer), "AT+SEND=%d:", (int)Port);
				pStr += strlen(pStr);

				if(oHexStringFromBytes(pStr, pData, SizeofData) == 0){
					result = RESULT_FAULT;
					goto EXIT;
				}
				pStr[SizeofData * 2] = '\0';	// 이전 송신 잔존 데이터 차단

				RAK3172Dev.Status.IsTxDone = 0;
				RAK3172Dev.Status.IsSendComformed = 0;
				RAK3172Dev.Status.IsSendFailed = 0;
				RAK3172Dev.Status.IsSending = 1;
			}

			switch(oAT_TransmitCommend(&RAK3172Dev.AT, TxStringBuffer, "OK", 10000, 3, 1))
			{
				case AT_RESULT_RUN:
					break;
				case AT_RESULT_OK:
					RAK3172TransmitStep++;
					RAK3172TransmitTimer = oTMR_GetTick(TICKBASE_SYSTICK);
					break;
				default:
					oSerial_Log("RAK3172", "Commend fail | AT+SEND");

					result = RESULT_FAULT;
					break;
			}
			break;
		case 5:
			if(RAK3172Dev.Status.IsTxDone && !Confirmed){
				result = RESULT_OK;
			}
			else if(RAK3172Dev.Status.IsSendComformed){
				result = RESULT_OK;
			}
			else if(RAK3172Dev.Status.IsSendFailed){
				result = RESULT_ERROR;
			}
			else if(oTMR_Elapsed(&RAK3172TransmitTimer, SECOND_TO_MS(10), TICKBASE_SYSTICK)){
				result = RESULT_TIMEOUT;
			}
			break;
	}

EXIT:
	RAK3172Dev.Status.IsBusy = result == RESULT_RUN ? 1: 0;

	if(result != RESULT_RUN){
		RAK3172TransmitStep = 0;
		RAK3172Dev.Status.IsSending = 0;
	}

	return result;
}

oResult_t RAK3172_Join()
{
	static uint8_t JoinStep = 0;
	static uint8_t JoinRetryCount = 0;
	static uint32_t JoinTimer = 0;
	oResult_t result = RESULT_RUN;
	char Commned[50];

	switch(JoinStep)
	{
		case 0:
			if(!RAK3172Dev.IsReachable){
				// 이전 Join 이력 없음 → NJS 확인 불필요, 바로 Join
				JoinStep++;
				break;
			}
			// 이전 Join 이력 있음 → NJS 확인 (세션 유효하면 Join 생략)
			switch(oAT_TransmitCommend(&RAK3172Dev.AT, "AT+NJS=?", "=", 1000, 3, 1))
			{
				case AT_RESULT_RUN:
					break;
				case AT_RESULT_OK:
					if(RAK3172Dev.AT.pResponseData[0] == '1'){
						RAK3172Dev.Status.IsJoined = 1;
						oSerial_Log("RAK3172", "Session valid, skip join");
						result = RESULT_OK;
					}
					else{
						JoinStep++;
					}
					break;
				default:
					JoinStep++; // NJS 확인 실패 시 Join 진행
					break;
			}
			break;
		case 1:
			// 0: Join commend [1=Joining, 0=Stop join]
			// 1: Auto join [1=Auto join on power up, 0=No auto join]
			// 2: Join timeout time [Second = 7-255]
			// 3: Join retry count [0-255]

			snprintf(Commned, sizeof(Commned), "AT+JOIN=1:0:%d:%d", RAK3172Dev.Config.JoinInterval, RAK3172Dev.Config.JoinAttempts);
			switch(oAT_TransmitCommend(&RAK3172Dev.AT, Commned, "OK", 10000, 3, 1))
			{
				case AT_RESULT_RUN:
					break;
				case AT_RESULT_OK:
					JoinTimer = oTMR_GetTick(TICKBASE_SYSTICK);
					RAK3172Dev.Status.IsJoined = 0;
					RAK3172Dev.Status.IsJoinFail = 0;
					JoinStep++;
					break;
				default:
					oSerial_Log("RAK3172", "Commend fail | AT+JOIN");
					result = RESULT_FAULT;
					break;
			}
			break;
		case 2:
			if(RAK3172Dev.Status.IsJoined){
				result = RESULT_OK;
			}
			else if(RAK3172Dev.Status.IsJoinFail){
				JoinRetryCount++;
				oSerial_Log("RAK3172", "Join failed (%d/%d)", JoinRetryCount, RAK3172Dev.Config.JoinMaxRetry);

				if(JoinRetryCount >= RAK3172Dev.Config.JoinMaxRetry){
					result = RESULT_ERROR;
				}
				else{
					JoinStep = 0; // AT+JOIN 재시도
				}
			}
			else if(oTMR_Elapsed(&JoinTimer, SECOND_TO_MS(RAK3172Dev.Config.JoinInterval*RAK3172Dev.Config.JoinAttempts+1 + 10), TICKBASE_SYSTICK)){
				JoinRetryCount++;
				oSerial_Log("RAK3172", "Join timeout (%d/%d)", JoinRetryCount, RAK3172Dev.Config.JoinMaxRetry);

				if(JoinRetryCount >= RAK3172Dev.Config.JoinMaxRetry){
					result = RESULT_TIMEOUT;
				}
				else{
					JoinStep = 0; // AT+JOIN 재시도
				}
			}
			break;
	}

	if(result != RESULT_RUN){
		JoinStep = 0;
		JoinRetryCount = 0;

		if(result == RESULT_OK){
			RAK3172Dev.IsReachable = 1;
		}
	}

	return result;
}

oResult_t RAK3172_IsChangedConfig()
{
	static uint8_t ConfigCheckStep = 0;
	oResult_t result = RESULT_RUN;

	switch (ConfigCheckStep)
	{
		case 0:
			switch(oAT_TransmitCommend(&RAK3172Dev.AT, "AT+NJM=?", "AT+NJM=", 3000, 3, 1))
			{
				case AT_RESULT_RUN:
					break;
				case AT_RESULT_OK:
					if((int)RAK3172Dev.Config.NJM == atoi(RAK3172Dev.AT.pResponseData)){
						ConfigCheckStep++;
					}
					else{
						result = RESULT_OK;
					}
					break;
				default:
					result = RESULT_FAULT;
					break;
			}
			break;
		case 1:
			switch(oAT_TransmitCommend(&RAK3172Dev.AT, "AT+CLASS=?", "AT+CLASS=", 3000, 3, 1))
			{
				case AT_RESULT_RUN:
					break;
				case AT_RESULT_OK:
					if(RAK3172Dev.Config.CLASS == RAK3172Dev.AT.pResponseData[0]){
						ConfigCheckStep++;
					}
					else{
						result = RESULT_OK;
					}
					break;
				default:
					result = RESULT_FAULT;
					break;
			}
			break;
		case 2:
			switch(oAT_TransmitCommend(&RAK3172Dev.AT, "AT+BAND=?", "AT+BAND=", 3000, 3, 1))
			{
				case AT_RESULT_RUN:
					break;
				case AT_RESULT_OK:
					if((int)RAK3172Dev.Config.BAND == atoi(RAK3172Dev.AT.pResponseData)){
						ConfigCheckStep++;
					}
					else{
						result = RESULT_OK;
					}
					break;
				default:
					result = RESULT_FAULT;
					break;
			}
			break;
		case 3:
			switch(oAT_TransmitCommend(&RAK3172Dev.AT, "AT+ADR=?", "AT+ADR=", 3000, 3, 1))
			{
				case AT_RESULT_RUN:
					break;
				case AT_RESULT_OK:
					if((int)RAK3172Dev.Config.ADR == atoi(RAK3172Dev.AT.pResponseData)){
						ConfigCheckStep++;
					}
					else{
						result = RESULT_OK;
					}
					break;
				default:
					result = RESULT_FAULT;
					break;
			}
			break;
		case 4:
			switch(oAT_TransmitCommend(&RAK3172Dev.AT, "AT+LINKCHECK=?", "AT+LINKCHECK=", 3000, 3, 1))
			{
				case AT_RESULT_RUN:
					break;
				case AT_RESULT_OK:
					if((int)RAK3172Dev.Config.LINKCHECK == atoi(RAK3172Dev.AT.pResponseData)){
						ConfigCheckStep++;
					}
					else{
						result = RESULT_OK;
					}
					break;
				default:
					result = RESULT_FAULT;
					break;
			}
			break;
		case 5:
			switch(oAT_TransmitCommend(&RAK3172Dev.AT, "AT+NWM=?", "AT+NWM=", 3000, 3, 1))
			{
				case AT_RESULT_RUN:
					break;
				case AT_RESULT_OK:
					if((int)RAK3172Dev.Config.NWM == atoi(RAK3172Dev.AT.pResponseData)){
						ConfigCheckStep++;
					}
					else{
						result = RESULT_OK;
					}
					break;
				default:
					result = RESULT_FAULT;
					break;
			}
			break;
		case 6:
			switch(oAT_TransmitCommend(&RAK3172Dev.AT, "AT+LBT=?", "AT+LBT=", 3000, 3, 1))
			{
				case AT_RESULT_RUN:
					break;
				case AT_RESULT_OK:
					if((int)RAK3172Dev.Config.LBT == atoi(RAK3172Dev.AT.pResponseData)){
						ConfigCheckStep++;
					}
					else{
						result = RESULT_OK;
					}
					break;
				default:
					result = RESULT_FAULT;
					break;
			}
			break;
		case 7:
			switch(oAT_TransmitCommend(&RAK3172Dev.AT, "AT+RX1DL=?", "AT+RX1DL=", 3000, 3, 1))
			{
				case AT_RESULT_RUN:
					break;
				case AT_RESULT_OK:
					if((int)RAK3172Dev.Config.RX1DL == atoi(RAK3172Dev.AT.pResponseData)){
						ConfigCheckStep++;
					}
					else{
						result = RESULT_OK;
					}
					break;
				default:
					result = RESULT_FAULT;
					break;
			}
			break;
		case 8:
			switch(oAT_TransmitCommend(&RAK3172Dev.AT, "AT+RX2DL=?", "AT+RX2DL=", 3000, 3, 1))
			{
				case AT_RESULT_RUN:
					break;
				case AT_RESULT_OK:
					if((int)RAK3172Dev.Config.RX2DL == atoi(RAK3172Dev.AT.pResponseData)){
						ConfigCheckStep++;
					}
					else{
						result = RESULT_OK;
					}
					break;
				default:
					result = RESULT_FAULT;
					break;
			}
			break;
		case 9:
			switch(oAT_TransmitCommend(&RAK3172Dev.AT, "AT+RETY=?", "AT+RETY=", 3000, 3, 1))
			{
				case AT_RESULT_RUN:
					break;
				case AT_RESULT_OK:
					if((int)RAK3172Dev.Config.RETY == atoi(RAK3172Dev.AT.pResponseData)){
						ConfigCheckStep++;
					}
					else{
						result = RESULT_OK;
					}
					break;
				default:
					result = RESULT_FAULT;
					break;
			}
			break;
		case 10:
			result = RESULT_ERROR;
			break;
	}

	if(result != RESULT_RUN){
		ConfigCheckStep = 0;
	}

	return result;
}

oResult_t RAK3172_SetConfig()
{
	static uint8_t SetConfigStep = 0;
	oResult_t result = RESULT_RUN;
	char Commend[50]={0,};

	switch (SetConfigStep)
	{
		case 0:
			switch(RAK3172_IsChangedConfig())
			{
				case RESULT_RUN:
					break;
				case RESULT_ERROR: // 모든 설정 동일 → SetConfig skip
					oSerial_Log("RAK3172", "SetConfig skip (no change)");
					result = RESULT_OK;
					break;
				default: // RESULT_OK(변경 필요) 또는 RESULT_FAULT → SetConfig 진행
					oSerial_Log("RAK3172", "SetConfig start");
					SetConfigStep++;
					break;
			}
			break;
		case 1:
			snprintf(Commend, sizeof(Commend), "AT+NWM=%d", RAK3172Dev.Config.NWM);

			switch(oAT_TransmitCommend(&RAK3172Dev.AT, Commend, "OK", 3000, 3, 1))
			{
				case AT_RESULT_RUN:
					break;
				case AT_RESULT_OK:
					SetConfigStep++;
					break;
				default:
					oSerial_Log("RAK3172", "Commend fail | AT+NWM");

					SetConfigStep++;
					//result = RESULT_ERROR;
					break;
			}
			break;
		case 2:
			snprintf(Commend, sizeof(Commend), "AT+CLASS=%c", RAK3172Dev.Config.CLASS);

			switch(oAT_TransmitCommend(&RAK3172Dev.AT, Commend, "OK", 3000, 3, 1))
			{
				case AT_RESULT_RUN:
					break;
				case AT_RESULT_OK:
					SetConfigStep++;
					break;
				default:
					oSerial_Log("RAK3172", "Commend fail | AT+CLASS");

					SetConfigStep++;
					//result = RESULT_ERROR;
					break;
			}
			break;
		case 3:
			snprintf(Commend, sizeof(Commend), "AT+BAND=%d", RAK3172Dev.Config.BAND);

			switch(oAT_TransmitCommend(&RAK3172Dev.AT, Commend, "OK", 3000, 3, 1))
			{
				case AT_RESULT_RUN:
					break;
				case AT_RESULT_OK:
					SetConfigStep++;
					break;
				default:
					oSerial_Log("RAK3172", "Commend fail | AT+BAND");

					SetConfigStep++;
					//result = RESULT_ERROR;
					break;
			}
			break;
		case 4:
			snprintf(Commend, sizeof(Commend), "AT+ADR=%d", RAK3172Dev.Config.ADR);

			switch(oAT_TransmitCommend(&RAK3172Dev.AT, Commend, "OK", 3000, 3, 1))
			{
				case AT_RESULT_RUN:
					break;
				case AT_RESULT_OK:
					SetConfigStep++;
					break;
				default:
					oSerial_Log("RAK3172", "Commend fail | AT+ADR");

					SetConfigStep++;
					//result = RESULT_ERROR;
					break;
			}
			break;
		case 5:
			snprintf(Commend, sizeof(Commend), "AT+LINKCHECK=%d", RAK3172Dev.Config.LINKCHECK);

			switch(oAT_TransmitCommend(&RAK3172Dev.AT, Commend, "OK", 3000, 3, 1))
			{
				case AT_RESULT_RUN:
					break;
				case AT_RESULT_OK:
					SetConfigStep++;
					break;
				default:
					oSerial_Log("RAK3172", "Commend fail | AT+LINKCHECK");

					SetConfigStep++;
					//result = RESULT_ERROR;
					break;
			}
			break;
		case 6:
			snprintf(Commend, sizeof(Commend), "AT+NJM=%d", RAK3172Dev.Config.NJM);

			switch(oAT_TransmitCommend(&RAK3172Dev.AT, Commend, "OK", 3000, 3, 1))
			{
				case AT_RESULT_RUN:
					break;
				case AT_RESULT_OK:
					SetConfigStep++;
					break;
				default:
					oSerial_Log("RAK3172", "Commend fail | AT+NJM");

					SetConfigStep++;
					//result = RESULT_ERROR;
					break;
			}
			break;
		case 7:
			snprintf(Commend, sizeof(Commend), "AT+LBT=%d", RAK3172Dev.Config.LBT);

			switch(oAT_TransmitCommend(&RAK3172Dev.AT, Commend, "OK", 3000, 3, 1))
			{
				case AT_RESULT_RUN:
					break;
				case AT_RESULT_OK:
					SetConfigStep++;
					break;
				default:
					oSerial_Log("RAK3172", "Commend fail | AT+LBT");

					SetConfigStep++;
					//result = RESULT_ERROR;
					break;
			}
			break;
		case 8:
			snprintf(Commend, sizeof(Commend), "AT+RX1DL=%d", RAK3172Dev.Config.RX1DL);

			switch(oAT_TransmitCommend(&RAK3172Dev.AT, Commend, "OK", 3000, 3, 1))
			{
				case AT_RESULT_RUN:
					break;
				case AT_RESULT_OK:
					SetConfigStep++;
					break;
				default:
					oSerial_Log("RAK3172", "Commend fail | AT+RX1DL");

					SetConfigStep++;
					//result = RESULT_ERROR;
					break;
			}
			break;
		case 9:
			snprintf(Commend, sizeof(Commend), "AT+RX2DL=%d", RAK3172Dev.Config.RX2DL);

			switch(oAT_TransmitCommend(&RAK3172Dev.AT, Commend, "OK", 3000, 3, 1))
			{
				case AT_RESULT_RUN:
					break;
				case AT_RESULT_OK:
					SetConfigStep++;
					break;
				default:
					oSerial_Log("RAK3172", "Commend fail | AT+RX2DL");

					SetConfigStep++;
					//result = RESULT_ERROR;
					break;
			}
			break;
		case 10:
			snprintf(Commend, sizeof(Commend), "AT+RETY=%d", RAK3172Dev.Config.RETY);

			switch(oAT_TransmitCommend(&RAK3172Dev.AT, Commend, "OK", 3000, 3, 1))
			{
				case AT_RESULT_RUN:
					break;
				case AT_RESULT_OK:
					SetConfigStep++;
					break;
				default:
					oSerial_Log("RAK3172", "Commend fail | AT+RETY");

					SetConfigStep++;
					//result = RESULT_ERROR;
					break;
			}
			break;
		case 11:
			snprintf(Commend, sizeof(Commend), "AT+LPMLVL=%d", RAK3172Dev.Config.LPMLVL);

			switch(oAT_TransmitCommend(&RAK3172Dev.AT, Commend, "OK", 3000, 3, 1))
			{
				case AT_RESULT_RUN:
					break;
				case AT_RESULT_OK:
					SetConfigStep++;
					break;
				default:
					oSerial_Log("RAK3172", "Commend fail | AT+LPMLVL");
					SetConfigStep++;
					break;
			}
			break;
		case 12:
			result = RESULT_OK;
			break;
	}

	if(result != RESULT_RUN){
		SetConfigStep = 0;
	}

	return result;
}

oResult_t RAK3172_Open(UART_HandleTypeDef *pUART, DownlinkHandler_t DownlinkCallback)
{
	static uint8_t InitStep = 0;
	static uint8_t CmdRetryCount = 0;
	oResult_t result = RESULT_RUN;

	RAK3172Dev.Config.NJM = 1;
	RAK3172Dev.Config.CLASS = 'A';
	RAK3172Dev.Config.BAND = 7;
	RAK3172Dev.Config.ADR = 1;
	RAK3172Dev.Config.LINKCHECK = 1; //0 = disabled, 1 = once, 2 = everytime
	RAK3172Dev.Config.NWM = 1;
	RAK3172Dev.Config.LBT = 1;
	RAK3172Dev.Config.RX1DL = 5;
	RAK3172Dev.Config.RX2DL = 6;
	RAK3172Dev.Config.RETY = 0;
	RAK3172Dev.Config.LPMLVL = 1; //1=Stop1(UART wakeup), 2=Stop2(lower power)
	RAK3172Dev.Config.JoinInterval = 10;
	RAK3172Dev.Config.JoinAttempts = 3; //
	RAK3172Dev.Config.JoinMaxRetry = 3;

	if(pUART == NULL){
		result = RESULT_ERROR;
		goto EXIT;
	}
	else if(RAK3172Dev.Status.IsOpened || RAK3172Dev.Status.IsJoined){
		result = RESULT_OK;
		goto EXIT;
	}

	switch (InitStep) {
		case 0:
			pUART->Init.BaudRate = 115200;

			memset(&RAK3172Dev.Status, 0, sizeof(RAK3172Dev.Status));
			RAK3172Dev.Status.IsOpening = 1;
			CmdRetryCount = 0;

			RAK3172Dev.AT.pUART = pUART;
			RAK3172Dev.AT.ReceiveCallback = RAK3172_ReceiveCallback;
			RAK3172Dev.AT.TransmitCallback = NULL;
			RAK3172Dev.AT.ResponseCallback = NULL;

			RAK3172Dev.AT.pBufferTx = (char *)&RAK3172Dev.Buffer.TxBuffer;
			RAK3172Dev.AT.LengthOfBufferTx = sizeof(RAK3172Dev.Buffer.TxBuffer);

			RAK3172Dev.AT.pBufferRx = (char *)&RAK3172Dev.Buffer.RxBuffer;
			RAK3172Dev.AT.LengthOfBufferRx = sizeof(RAK3172Dev.Buffer.RxBuffer);

			RAK3172Dev.AT.pResponseData = (char *)&RAK3172Dev.Buffer.ResponseBuffer;
			RAK3172Dev.AT.LengthOfResponse = sizeof(RAK3172Dev.Buffer.ResponseBuffer);

			if(DownlinkCallback != NULL){
				RAK3172Dev.DownlinkCallback = DownlinkCallback;
			}

			InitStep++;
			break;
		case 1:
			if(oAT_Open(&RAK3172Dev.AT) == AT_RESULT_OK){
				RAK3172Dev.AT.State.IsReceiveLine = 1;
				InitStep++;
			}
			else{
				result = RESULT_ERROR;
			}
			break;
		case 2: // Wakeup: LPM 해제 시도 (첫 byte 가 wakeup 으로 손실될 수 있어 AT 대신 LPM=0 송신, 실패해도 다음 단계 진행)
			switch(oAT_TransmitCommend(&RAK3172Dev.AT, "AT+LPM=0", "OK", 1000, 3, 1))
			{
				case AT_RESULT_RUN:
					break;
				case AT_RESULT_OK:
					InitStep += 2;
					break;
				default:
					oSerial_Log("RAK3172", "Commend fail | AT+LPM=0 (wakeup)");
					InitStep++; // 실패해도 다음 단계로 진행 — AT 응답 여부로 최종 판정
					break;
			}
			break;
		case 3: // AT 응답 검증 — wakeup 후 모듈이 실제로 통신 가능한지 확인
			switch(oAT_TransmitCommend(&RAK3172Dev.AT, "AT", "OK", 1000, 3, 1))
			{
				case AT_RESULT_RUN:
					break;
				case AT_RESULT_OK:
					RAK3172Dev.IsNotworking = 0;
					InitStep++;
					break;
				default:
					oSerial_Log("RAK3172", "Commend fail | AT");

					result = RESULT_ERROR;
					RAK3172Dev.IsNotworking = 1;
					break;
			}
			break;
		case 4:
			switch(oAT_TransmitCommend(&RAK3172Dev.AT, "ATE", "OK", 1000, 1, 1))
			{
				case AT_RESULT_RUN:
					break;
				default:
				case AT_RESULT_OK:
					InitStep++;
					break;
			}
			break;
		case 5:
			switch(oAT_TransmitCommend(&RAK3172Dev.AT, "AT", "AT", 500, 2, 1))
			{
				default:
				case AT_RESULT_RUN:
					break;
				case AT_RESULT_OK: //Echo 잡힘 → ATE 재시도, 단 누적 10회 초과 시 포기
					if(++CmdRetryCount >= 10){
						oSerial_Log("RAK3172", "ATE retry exceeded (%d) — proceed", CmdRetryCount);
						CmdRetryCount = 0;
						InitStep++; // 포기하고 다음 단계로
					}
					else{
						InitStep--; //ATE 재시도
					}
					break;
				case AT_RESULT_TIMEOUT:
					CmdRetryCount = 0;
					InitStep++; //Echo check ok
					break;
			}
			break;
		case 6: // LPM 해제 재시도 (case 2 wakeup 이후 다시 sleep 진입 가능성 대비)
			switch(oAT_TransmitCommend(&RAK3172Dev.AT, "AT+LPM=0", "OK", 1000, 5, 1))
			{
				case AT_RESULT_RUN:
					break;
				case AT_RESULT_OK:
					InitStep++;
					break;
				default:
					oSerial_Log("RAK3172", "Commend fail | AT+LPM=0");
					InitStep++; // 실패해도 진행 (LPM 미활성 상태일 수 있음)
					break;
			}
			break;
		case 7: //Get FW Version
			switch(oAT_TransmitCommend(&RAK3172Dev.AT, "AT+VER=?", "VER=", 1000, 3, 1))
			{
				case AT_RESULT_RUN:
					break;
				case AT_RESULT_OK:
					if(strlen(RAK3172Dev.AT.pResponseData) > 0){
						strncpy(RAK3172Dev.Information.Version, RAK3172Dev.AT.pResponseData, sizeof(RAK3172Dev.Information.Version) - 1);
						RAK3172Dev.Information.Version[sizeof(RAK3172Dev.Information.Version) - 1] = '\0';
						oSerial_Log("RAK3172", "VER=%s", RAK3172Dev.Information.Version);
					}
					InitStep++;
					break;
				default:
					oSerial_Log("RAK3172", "Commend fail | AT+VER");
					InitStep++; // 실패해도 진행
					break;
			}
			break;
		case 8: //Get LoRaWAN DevEUI
			if(strlen(RAK3172Dev.Information.DevEUI) > 0){
				CmdRetryCount = 0;
				InitStep++;
			}
			else{
				switch(oAT_TransmitCommend(&RAK3172Dev.AT, "AT+DEVEUI=?", "DEVEUI=", 1000, 3, 1))
				{
					case AT_RESULT_RUN:
						break;
					case AT_RESULT_OK:
						if(strlen(RAK3172Dev.AT.pResponseData) > 0){
							strncpy(RAK3172Dev.Information.DevEUI, RAK3172Dev.AT.pResponseData, sizeof(RAK3172Dev.Information.DevEUI) - 1);
							RAK3172Dev.Information.DevEUI[sizeof(RAK3172Dev.Information.DevEUI) - 1] = '\0';
							CmdRetryCount = 0;
							InitStep++;
						}
						else if(++CmdRetryCount >= 10){ //빈 응답 무한 루프 방지
							oSerial_Log("RAK3172", "DEVEUI empty response exceeded (%d)", CmdRetryCount);
							result = RESULT_ERROR;
						}
						break;
					default:
						oSerial_Log("RAK3172", "Commend fail | AT+DEVEUI");
						result = RESULT_ERROR;
						break;
				}
			}
			break;
		case 9: //Set LoRaWAN NWM (LoRa 통신 프로토콜 LoRaWAN Mode로 설정)
			snprintf(TxStringBuffer, sizeof(TxStringBuffer), "AT+NWM=%d", RAK3172Dev.Config.NWM);

			switch(oAT_TransmitCommend(&RAK3172Dev.AT, TxStringBuffer, "OK", 3000, 3, 1))
			{
				case AT_RESULT_RUN:
					break;
				case AT_RESULT_OK:
					InitStep++;
					break;
				default:
					oSerial_Log("RAK3172", "Commend fail | AT+NWM");
					InitStep++; // 실패해도 진행 (이미 동일 값일 수 있음)
					break;
			}
			break;
		case 10: //Set LoRaWAN NJM (LoRaWAN Join Mode - OTAA 모드로 설정)
			snprintf(TxStringBuffer, sizeof(TxStringBuffer), "AT+NJM=%d", RAK3172Dev.Config.NJM);

			switch(oAT_TransmitCommend(&RAK3172Dev.AT, TxStringBuffer, "OK", 3000, 3, 1))
			{
				case AT_RESULT_RUN:
					break;
				case AT_RESULT_OK:
					InitStep++;
					break;
				default:
					oSerial_Log("RAK3172", "Commend fail | AT+NJM");
					InitStep++; // 실패해도 진행
					break;
			}
			break;
		case 11: //Set LoRaWAN AppEUI
			snprintf(TxStringBuffer, sizeof(TxStringBuffer), "AT+APPEUI=%s", RAK3172Dev.Information.AppEUI);

			switch(oAT_TransmitCommend(&RAK3172Dev.AT, TxStringBuffer, "OK", 5000, 3, 1))
			{
				case AT_RESULT_RUN:
					break;
				case AT_RESULT_OK:
					InitStep++;
					break;
				default:
					oSerial_Log("RAK3172", "Commend fail | AT+APPEUI");
					result = RESULT_ERROR;
					break;
			}
			break;
		case 12: //Set LoRaWAN AppKEY
			if(strlen(RAK3172Dev.Information.AppKEY) <= 0){
				result = RESULT_NULL;
			}
			else{
				snprintf(TxStringBuffer, sizeof(TxStringBuffer), "AT+APPKEY=%s", RAK3172Dev.Information.AppKEY);

				switch(oAT_TransmitCommend(&RAK3172Dev.AT, TxStringBuffer, "OK", 5000, 3, 1))
				{
					case AT_RESULT_RUN:
						break;
					case AT_RESULT_OK:
						InitStep++;
						break;
					default:
						oSerial_Log("RAK3172", "Commend fail | AT+APPKEY");
						result = RESULT_ERROR;
						break;
				}
			}
			break;
		case 13:
			if((result=RAK3172_SetConfig()) == RESULT_OK){
				result = RESULT_RUN;
				InitStep++;
			}
			break;
		default:
			RAK3172Dev.IsInitailzed = 1;
			result = RESULT_OK;
			break;
	}

EXIT:
	if(RAK3172Dev.Status.IsOpening){
		RAK3172Dev.Status.IsBusy = result == RESULT_RUN ? 1: 0;
	}

	if(result != RESULT_RUN){
		InitStep = 0;

		RAK3172Dev.Status.IsOpened = result == RESULT_OK ? 1: 0;
		RAK3172Dev.Status.IsOpening = 0;
	}

	return result;
}

oResult_t RAK3172_Sleep()
{
	if(RAK3172Dev.Status.IsSleeping){
		return RESULT_OK;
	}

	switch(oAT_TransmitCommend(&RAK3172Dev.AT, "AT+LPM=1", "OK", 3000, 3, 1))
	{
		case AT_RESULT_RUN:
			return RESULT_RUN;
		case AT_RESULT_OK:
			oSerial_Log("RAK3172", "LPM on");
			RAK3172Dev.Status.IsSleeping = 1;
			return RESULT_OK;
		default:
			oSerial_Log("RAK3172", "LPM on fail");
			return RESULT_FAULT;
	}
}

oResult_t RAK3172_Wakeup()
{
	if(!RAK3172Dev.Status.IsSleeping){
		return RESULT_OK;
	}

	switch(oAT_TransmitCommend(&RAK3172Dev.AT, "AT+LPM=0", "OK", 1000, 5, 1))
	{
		case AT_RESULT_RUN:
			return RESULT_RUN;
		case AT_RESULT_OK:
			oSerial_Log("RAK3172", "LPM off");
			RAK3172Dev.Status.IsSleeping = 0;
			return RESULT_OK;
		default:
			oSerial_Log("RAK3172", "LPM off fail");
			return RESULT_FAULT;
	}
}

oResult_t RAK3172_Close()
{
	RAK3172Dev.DownlinkCallback = NULL;

	oAT_Close(&RAK3172Dev.AT);
	memset(&RAK3172Dev.Status, 0, sizeof(RAK3172Dev.Status));

	return RESULT_OK;
}

void RAK3172()
{
	oAT_Proc(&RAK3172Dev.AT);
}

/* History

2026-06-26 | v0.1
	- baseline (LoRa_RAK3172.c)
*/
