/*
 * ONE_Timer.c
 *
 * Created: 2024-08-27 오전 11:55:38
 *  Author: JONE
 */
#include "ONE_Time.h"
#include "ONE_Common.h"

oResult_t oJSON_GetDoubleArray(const char *JsonStr, const char *Keyword, double *pArray, uint32_t SizeOfArray)
{
	if (!JsonStr || !Keyword || !pArray || SizeOfArray == 0) {
		return RESULT_ERROR;
	}

	uint8_t array_ended = 0;
	uint8_t readcount = 0;
    char *key_pos = strstr(JsonStr, Keyword);

    if (!key_pos) {
        // Keyword가 없으면
        return RESULT_NULL;
    }

    char *array_start = strchr(key_pos + strlen(Keyword), '[');
    if (!array_start) {
    	// [ 괄호가 없으면
        return RESULT_ERROR;
    }
    array_start++;  // '[' 다음으로 이동

    memset(pArray, 0, sizeof(double)*SizeOfArray);

    for (int i = 0; i < SizeOfArray && !array_ended; i++) {
        while (*array_start == ' ' || *array_start == '\"') array_start++;

        pArray[i] = strtod(array_start, &array_start);
        readcount++;

        while (*array_start && *array_start != ',' && *array_start != ']') array_start++;

        if(*array_start == ',') array_start++;
        if(*array_start == ']') array_ended = 1;
    }

    if(!array_ended){
    	// ] 괄호가 없으면
    	return RESULT_ERROR;
    }

    return readcount * array_ended;
}

oResult_t oJSON_GetValue(const char *JsonStr, const char *Keyword, char *pData, uint32_t SizeOfData)
{
    if (!JsonStr || !Keyword || !pData || SizeOfData == 0) {
        return RESULT_ERROR;
    }

    char *key_pos = strstr(JsonStr, Keyword);
    if (!key_pos) {
        return RESULT_NULL;
    }

    char *colon = strchr(key_pos, ':');
    if (!colon) {
        return RESULT_ERROR;
    }

    colon++;
    while (*colon == ' ' || *colon == '\t' || *colon == '\n' || *colon == '\r') colon++;

    memset(pData, 0, SizeOfData);
    char *val_start = colon;
    char *val_end = NULL;

    if (*val_start == '"') {
        // 문자열 값
        val_start++;  // skip opening "
        val_end = val_start;
        while (*val_end) {
            if (*val_end == '\\' && *(val_end + 1)) {
                val_end += 2;
            } else if (*val_end == '"') {
                break;
            } else {
                val_end++;
            }
        }
        if (*val_end != '"') return RESULT_ERROR;
    } else if (*val_start == '[') {
        // 배열 값
        int depth = 1;
        val_end = val_start + 1;
        while (*val_end && depth > 0) {
            if (*val_end == '[') depth++;
            else if (*val_end == ']') depth--;
            else if (*val_end == '"') {
                val_end++;
                while (*val_end && *val_end != '"') {
                    if (*val_end == '\\' && *(val_end + 1)) val_end += 2;
                    else val_end++;
                }
            }
            val_end++;
        }
        if (depth != 0) return RESULT_ERROR;
    } else if (*val_start == '{') {
        // 객체 값
        int depth = 1;
        val_end = val_start + 1;
        while (*val_end && depth > 0) {
            if (*val_end == '{') depth++;
            else if (*val_end == '}') depth--;
            else if (*val_end == '"') {
                val_end++;
                while (*val_end && *val_end != '"') {
                    if (*val_end == '\\' && *(val_end + 1)) val_end += 2;
                    else val_end++;
                }
            }
            val_end++;
        }
        if (depth != 0) return RESULT_ERROR;
    } else if (strncmp(val_start, "true", 4) == 0) {
        strncpy(pData, "true", SizeOfData - 1);
        return RESULT_OK;
    } else if (strncmp(val_start, "false", 5) == 0) {
        strncpy(pData, "false", SizeOfData - 1);
        return RESULT_OK;
    } else if (strncmp(val_start, "null", 4) == 0) {
        strncpy(pData, "null", SizeOfData - 1);
        return RESULT_OK;
    } else {
        // 숫자 또는 기타 값
        val_end = strpbrk(val_start, ",}]\" \t\r\n");
        if (!val_end) val_end = val_start + strlen(val_start);
    }

    if (!val_end) return RESULT_ERROR;

    size_t len = val_end - val_start;
    if (len >= SizeOfData) len = SizeOfData - 1;

    strncpy(pData, val_start, len);
    pData[len] = '\0';

    return RESULT_OK;
}

uint8_t oHexCharToByte(char c) {
    if ('0' <= c && c <= '9') return (uint8_t)(c - '0');
    else if ('a' <= c && c <= 'f') return (uint8_t)(c - 'a' + 10);
    else if ('A' <= c && c <= 'F') return (uint8_t)(c - 'A' + 10);
    else return -1;
}

char oByteToHexChar(uint8_t b) {
	if (b >= 0 && b <= 9) return (char)('0'+b);
	else if (b >= 10 && b <= 15) return (char)('A'+(b-10));
	else return ' ';
}

uint32_t oHexStringToBytes(char *HexString, uint8_t *pBytes, uint32_t SizeOfBytes)
{
	if (HexString == NULL || pBytes == NULL || SizeOfBytes == 0) {
		return 0;
	}

	uint8_t high;
	uint8_t low;
	uint32_t len;
	uint32_t i;

    len = strlen(HexString);

    if (len % 2 != 0) return -1; // 문자열 길이는 짝수여야 함

    for (i = 0; i < len / 2 && i < SizeOfBytes; i++) {
        high = oHexCharToByte(HexString[2 * i]);
        low  = oHexCharToByte(HexString[2 * i + 1]);

        if (high > 0x0F || low > 0x0F){
        	return -2;  // 잘못된 HEX 문자
        }

        *(pBytes+i) = (uint8_t)((high << 4) | low);
    }

    return i;  // 변환된 바이트 수 반환
}

uint32_t oHexStringFromBytes(char *HexString, uint8_t *pBytes, uint32_t SizeOfBytes)
{
	if (HexString == NULL || pBytes == NULL || SizeOfBytes == 0) {
		return 0;
	}

	uint32_t i;

	for (i = 0; i < SizeOfBytes; i++) {
    	HexString[2*i] = oByteToHexChar(*(pBytes+i) >> 4 & 0x0F);
    	HexString[2*i+1] = oByteToHexChar(*(pBytes+i) & 0x0F);
    }

    return i*2;  // 변환된 바이트 수 반환
}

uint8_t oString_LineAdd(oStringLine_t *Line, char *AddString)
{
	int32_t size = 0;
	char *pBuffer1, *pBuffer2;

	if(Line != NULL && Line->Count > STRINGLINE_MAXCOUNT+1){
		Line->Count = 0;
	}

	if(Line == NULL || AddString == NULL || strlen(AddString) <= 0 || Line->Count >= STRINGLINE_MAXCOUNT){
		return 0;
	}

	if(strstr(AddString, "\n") != NULL){
		pBuffer1 = AddString;

		do
		{
			pBuffer2 = strstr(pBuffer1, "\n");

			if(pBuffer2 != NULL)
			{
				size = pBuffer2-pBuffer1-1;

				if(size > 0){
					if(Line->Line[Line->Count] != NULL){
						free(Line->Line[Line->Count]);
						Line->Line[Line->Count] = NULL;
					}

					Line->Line[Line->Count] = (char * )malloc(size + 1);
					if(Line->Line[Line->Count] == NULL){
						return 0;
					}
					memset(Line->Line[Line->Count], 0, size + 1);
					strncpy(Line->Line[Line->Count], pBuffer1, size);

					//라인추가
					if(strlen(Line->Line[Line->Count]) > 0){
						Line->Count++;
					}
				}

				pBuffer1 = pBuffer2+1; //\n에서 한칸 밀기
			}
		} while(pBuffer2 != NULL && pBuffer2 <= (AddString + strlen(AddString)) && Line->Count < STRINGLINE_MAXCOUNT);
	}
	else{
		if(Line->Line[Line->Count] != NULL){
			free(Line->Line[Line->Count]);
			Line->Line[Line->Count] = NULL;
		}

		Line->Line[Line->Count] = (char * )malloc(strlen(AddString) + 1);
		if(Line->Line[Line->Count] == NULL){
			return 0;
		}
		memset(Line->Line[Line->Count], 0, strlen(AddString) + 1);
		strcpy(Line->Line[Line->Count], AddString);

		Line->Count++;
	}

	return 1;
}

void oString_LineFlush(oStringLine_t *Line)
{
	if(Line == NULL || Line->Count <= 0){
		return;
	}

	if(Line->Count > STRINGLINE_MAXCOUNT+1){
		Line->Count = STRINGLINE_MAXCOUNT;
	}

	while(Line->Count-- > 0)
	{
		if(Line->Line[Line->Count] != NULL){
			free(Line->Line[Line->Count]);
			Line->Line[Line->Count] = NULL;
		}
	}
}

uint8_t oString_Delete(char *Source, char *Delete)
{
	if (Source == NULL || Delete == NULL) {
		return 0;
	}

	uint32_t SrcLength, DelLength;
	char *i, *pStr;

	SrcLength = strlen(Source);
	DelLength = strlen(Delete);

	pStr = strstr(Source, Delete);

	if(pStr != NULL){
		//Delete filter string
		for(i = Source; i < (Source+SrcLength); i++){
			if(i >= (pStr+DelLength)){
				Source[i-(pStr+DelLength)] = *i;
			}
			*i = 0;
		}
	}

	return pStr != NULL ? 1 : 0;
}

void oString_Left(char *Source, char *Destination, uint32_t Count)
{
	if (Source == NULL || Destination == NULL || Count == 0) {
		return;
	}

	char *i;

	for(i = Source; i < (Source+Count); i++){
		*Destination++ = *i;
	}
}

void oString_Right(char *Source, char *Destination, uint32_t Count)
{
	char *i;
	uint32_t len;

	if(Source == NULL || Destination == NULL || Count == 0){
		return;
	}

	len = strlen(Source);
	if(Count > len){
		Count = len;
	}

	for(i = (Source+len-Count); i < (Source+len); i++){
		*Destination++ = *i;
	}
}

/* History

2026-06-26 | v0.1
	- baseline (ONE_Common.c)
*/
