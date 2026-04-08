# NAND Flash Application Design Document

> **Summary**: NAND 플래시 활용 — Parameter 이중 저장, IoT 데이터 로깅, 시리얼 데이터 전송, 데이터 색인 상세 설계
>
> **Project**: SIA100-2A (STM32L433RBT7)
> **Version**: FW 1.0
> **Author**: JONE
> **Date**: 2026-02-24
> **Status**: Draft
> **Plan Reference**: `docs/01-plan/features/nand-flash-application.plan.md`

---

## 1. Architecture Overview

### 1.1 Layer Structure

```
┌──────────────────────────────────────────────────────────────────┐
│  Application Layer                                                │
│  ┌─────────────┐ ┌──────────────┐ ┌───────────────────────────┐ │
│  │ Mi_IoT.c    │ │ Mi_Serial.c  │ │ Mi_Main_Node_SIA100.c     │ │
│  │ (측정/LoRa) │ │ (시리얼 명령)│ │ (초기화)                  │ │
│  └──────┬──────┘ └──────┬───────┘ └──────────┬────────────────┘ │
├─────────┼───────────────┼────────────────────┼──────────────────┤
│  Storage Layer          │                    │                    │
│  ┌──────┴───────────────┴────────────────────┴──────────────────┐│
│  │ Mi_Storage.c                                                  ││
│  │ - MiStorage_ReadIoTParameter / WriteIoTParameter (FR-01)     ││
│  │ - MiStorage_WriteIoTData / ReadIoTData           (FR-02/03)  ││
│  │ - MiStorage_SensorDataIndexing                   (FR-04/05)  ││
│  │ - MiStorage_EraseDataArea                        (FR-06)     ││
│  └──────────────────────────┬───────────────────────────────────┘│
├──────────────────────────────┼──────────────────────────────────┤
│  Driver Layer                │                                    │
│  ┌───────────────────┐ ┌────┴─────────────────────┐             │
│  │ Mi_Native (MCU FL)│ │ NAND_MT29F2G01 (QSPI)   │             │
│  │ Native_FlashRead  │ │ MT29F2G_ReadByte         │             │
│  │ Native_FlashWrite │ │ MT29F2G_WriteByte        │             │
│  └───────────────────┘ │ MT29F2G_Erase            │             │
│                         │ MT29F2G_BadBlockCheck    │             │
│                         └──────────────────────────┘             │
└──────────────────────────────────────────────────────────────────┘
```

### 1.2 NAND Memory Map (변경 없음)

```
Block 0-1     : Reserved
Block 2       : Parameter (Page 0 only) — FR-01 활성화
Block 3-2045  : IoT Data Area (130,752 pages max) — FR-02/03/04/05
Block 2046-47 : Reserved
```

---

## 2. Detailed Design per Feature

### 2.1 FR-01: Parameter 이중 저장 (MCU Flash + NAND)

**수정 파일**: `Core/Src/Mi_Storage.c`

#### 2.1.1 MiStorage_WriteIoTParameter — 수정

현재: MCU 플래시만 저장 (NAND 주석 처리)
변경: MCU 플래시 저장 후 NAND에도 저장. NAND 실패 시 경고만 기록 (MCU 성공이면 OK 반환)

```c
oResult_t MiStorage_WriteIoTParameter(IoTParameter_t *pParameter)
{
    static uint8_t WriteParameterStep = 0;
    static uint8_t WriteParameterOk = 0;
    oResult_t result = RESULT_RUN;

    if(pParameter == NULL){
        WriteParameterStep = 0;
        return RESULT_NULL;
    }

    switch(WriteParameterStep)
    {
        case 0:
            WriteParameterStep++;
            WriteParameterOk = 0;
            // fall through
        case 1:
            // Step 1: NAND 저장 (실패해도 계속 진행)
            if(MiStorage_NandHeader.Status.IsFault){
                WriteParameterStep++;
            }
            else{
                if((result = MiStorage_Write(MISTORAGE_PARAMETER_STARTADR,
                    (uint8_t *)pParameter, sizeof(IoTParameter_t))) != RESULT_RUN){
                    if(result == RESULT_OK){
                        WriteParameterOk |= 0x01;  // NAND OK 플래그
                    }
                    WriteParameterStep++;
                    result = RESULT_RUN;
                }
                break;
            }
            // fall through
        case 2:
            // Step 2: MCU 내부 플래시 저장
            if((result = Native_FlashWrite((uint8_t *)pParameter,
                sizeof(IoTParameter_t))) != RESULT_RUN){
                if(result == RESULT_OK){
                    WriteParameterOk |= 0x02;  // MCU Flash OK 플래그
                }
                WriteParameterStep++;
                result = RESULT_RUN;
            }
            break;
        case 3:
            // Step 3: 결과 판단 (MCU 플래시 성공이면 OK)
            if(WriteParameterOk & 0x02){
                result = RESULT_OK;
            }
            else{
                result = RESULT_ERROR;
            }
            break;
    }

    if(result != RESULT_RUN){
        WriteParameterStep = 0;
    }

    return result;
}
```

#### 2.1.2 MiStorage_ReadIoTParameter — 수정

현재: MCU 플래시만 읽기
변경: MCU 플래시 읽기 → CRC 실패 시 NAND에서 복원 시도

```c
oResult_t MiStorage_ReadIoTParameter(IoTParameter_t *pParameter)
{
    static uint8_t ReadParameterStep = 0;
    static MiStorage_PageHeader_t Header;
    oResult_t result = RESULT_RUN;

    if(pParameter == NULL){
        ReadParameterStep = 0;
        return RESULT_NULL;
    }

    switch(ReadParameterStep)
    {
        case 0:
            // Step 1: MCU 플래시 읽기 시도
            result = Native_FlashRead((uint8_t *)pParameter, sizeof(IoTParameter_t));

            if(result == RESULT_OK){
                // MCU 플래시 성공 → 완료
                break;
            }
            else if(result != RESULT_RUN){
                // MCU 플래시 실패 → NAND 복원 시도
                ReadParameterStep++;
                result = RESULT_RUN;
            }
            break;
        case 1:
            // Step 2: NAND에서 복원 시도
            if(MiStorage_NandHeader.Status.IsFault){
                result = RESULT_ERROR;  // NAND도 불가
            }
            else{
                if((result = MiStorage_Read(MISTORAGE_PARAMETER_STARTADR,
                    &Header, (uint8_t *)pParameter, sizeof(IoTParameter_t)))
                    != RESULT_RUN){
                    // NAND 결과 (OK 또는 ERROR) 그대로 반환
                }
            }
            break;
    }

    if(result != RESULT_RUN){
        ReadParameterStep = 0;
    }

    return result;
}
```

---

### 2.2 FR-02: IoT 데이터 NAND 저장

**수정 파일**: `Core/Src/Mi_IoT.c`

#### 2.2.1 MiIoT_UpdateMeasurement — 수정

측정 완료 후 LoRa mailbox에 추가하는 시점에서 NAND 저장도 함께 수행.

```c
// Mi_IoT.c : MiIoT_UpdateMeasurement() 내 case 1: 에서
// 기존 코드 (line 612):
MiIoT_MailBox_NewItem(&MiIoT_LoRaMailBox, &DataPacket, MILORA_QOS_MAX, MeasurementSendDelay);

// 추가할 코드:
MiStorage_WriteIoTData(&DataPacket);  // NAND 저장 (비동기 state machine)
```

**주의**: `MiStorage_WriteIoTData()`는 state machine (RESULT_RUN 반환)이므로, 호출 시점에서 바로 완료되지 않음. `MiStorage()` 메인 루프에서 auto-off 타이머가 NAND 전원을 관리.

#### 2.2.2 MiStorage_WriteIoTData — 이미 수정 완료

이전 단계에서 수정한 사항:
- `&pPayload` → `pPayload` (포인터 오류 수정)
- Address 순환: `MISTORAGE_DATA_STARTADR`로 복귀하도록 수정

#### 2.2.3 MiIoT_LoRaDataProcess SyncAddress 순환 버그 수정

**수정 파일**: `Core/Src/Mi_IoT.c` (line 473, 486)

```c
// 기존 (버그):
MiStorage_NandHeader.SensorData.SyncAddress =
    (MiStorage_NandHeader.SensorData.SyncAddress+1) % (MISTORAGE_DATA_ENDADR+1);

// 수정:
MiStorage_NandHeader.SensorData.SyncAddress++;
if(MiStorage_NandHeader.SensorData.SyncAddress > MISTORAGE_DATA_ENDADR
   || MiStorage_NandHeader.SensorData.SyncAddress < MISTORAGE_DATA_STARTADR){
    MiStorage_NandHeader.SensorData.SyncAddress = MISTORAGE_DATA_STARTADR;
}
```

---

### 2.3 FR-03: 시리얼 저장 데이터 전송 (Get/StoredData)

**수정 파일**: `Core/Src/Mi_Serial.c` — `MiSerial_Get()`

#### 2.3.1 프로토콜 정의

**요청**:
```
Get/StoredData {"Address":"192"};\r\n
```

**응답** (성공):
```json
{
"Type" : "StoredData",
"Address" : "192",
"Time" : "2025-06-15 12:00:00",
"Temp" : "25.50",
"TiltX" : "1.234",
"TiltY" : "-0.567"
};
```

**응답** (실패 — 빈 페이지 또는 CRC 오류):
```json
{
"Type" : "Response",
"Request" : "Get/StoredData",
"Value" : "ERROR"
};
```

#### 2.3.2 구현

```c
// Mi_Serial.c : MiSerial_Get() 내 추가
if(strstr(Message, "Get/StoredData") != NULL){
    char StrVal[32];
    uint32_t Address = 0;

    if(oJSON_GetValue(Message, "\"Address\"", StrVal, sizeof(StrVal)) == RESULT_OK){
        Address = (uint32_t)strtol(StrVal, NULL, 10);
    }

    if(Address < MISTORAGE_DATA_STARTADR || Address > MISTORAGE_DATA_ENDADR){
        MiSerial_PrintResponse("Get/StoredData", "InvalidAddress");
        return RESULT_OK;
    }

    // 동기식 읽기 (while 루프로 state machine 완료 대기)
    IoT_DataPacket_t Packet;
    MiStorage_PageHeader_t Header;
    oResult_t r;

    while((r = MiStorage_ReadIoTData(Address, &Packet, &Header)) == RESULT_RUN);

    if(r == RESULT_OK && Packet.TypeOfData == IoTDataType_Tilt){
        IoTDataTilt_t *pTilt = (IoTDataTilt_t *)&Packet.Frame;

        oSerial_Printf(&MiSerial_Handler, "{\r\n");
        oSerial_Printf(&MiSerial_Handler, "\"Type\" : \"StoredData\",\r\n");
        oSerial_Printf(&MiSerial_Handler, "\"Address\" : \"%d\",\r\n", (int)Address);
        oSerial_Printf(&MiSerial_Handler, "\"Time\" : \"%04d-%02d-%02d %02d:%02d:%02d\",\r\n",
            (int)(pTilt->Time.Year+2000), (int)pTilt->Time.Month,
            (int)pTilt->Time.Day, (int)pTilt->Time.Hour,
            (int)pTilt->Time.Minute, (int)pTilt->Time.Second);
        oSerial_Printf(&MiSerial_Handler, "\"Temp\" : \"%.2f\",\r\n",
            MIIOT_DATA_DECODE_TEMP(pTilt->Temperature));
        oSerial_Printf(&MiSerial_Handler, "\"TiltX\" : \"%.3f\",\r\n",
            MIIOT_DATA_DECODE_ANGLE(pTilt->Sensor.AxisX));
        oSerial_Printf(&MiSerial_Handler, "\"TiltY\" : \"%.3f\"\r\n",
            MIIOT_DATA_DECODE_ANGLE(pTilt->Sensor.AxisY));
        oSerial_Printf(&MiSerial_Handler, "};\r\n");
    }
    else{
        MiSerial_PrintResponse("Get/StoredData", "ERROR");
    }

    return RESULT_OK;
}
```

**참고**: `while` 루프로 동기 읽기 수행. NAND 읽기는 ~5ms 이내 완료되므로 IWDG(~11초) 안전 범위 내.

---

### 2.4 FR-04: 데이터 색인 조회 (Get/DataIndex)

**수정 파일**: `Core/Src/Mi_Serial.c` — `MiSerial_Get()`

#### 2.4.1 프로토콜 정의

**요청**:
```
Get/DataIndex;\r\n
```

**응답**:
```json
{
"Type" : "DataIndex",
"Initialized" : "1",
"Count" : "1234",
"OlderAddr" : "192",
"OlderTime" : "2025-01-15 08:00:00",
"NewerAddr" : "3456",
"NewerTime" : "2025-06-15 12:00:00",
"SyncAddr" : "3400",
"IsFault" : "0",
"IsError" : "0"
};
```

#### 2.4.2 구현

```c
// Mi_Serial.c : MiSerial_Get() 내 추가
if(strstr(Message, "Get/DataIndex") != NULL){
    oSerial_Printf(&MiSerial_Handler, "{\r\n");
    oSerial_Printf(&MiSerial_Handler, "\"Type\" : \"DataIndex\",\r\n");
    oSerial_Printf(&MiSerial_Handler, "\"Initialized\" : \"%d\",\r\n",
        (int)MiStorage_NandHeader.Status.IsInitialized);
    oSerial_Printf(&MiSerial_Handler, "\"Count\" : \"%d\",\r\n",
        (int)MiStorage_NandHeader.SensorData.CountOfData);
    oSerial_Printf(&MiSerial_Handler, "\"OlderAddr\" : \"%d\",\r\n",
        (int)MiStorage_NandHeader.SensorData.OlderAddress);
    oSerial_Printf(&MiSerial_Handler, "\"OlderTime\" : \"%04d-%02d-%02d %02d:%02d:%02d\",\r\n",
        (int)MiStorage_NandHeader.SensorData.OlderDateTime.Year,
        (int)MiStorage_NandHeader.SensorData.OlderDateTime.Month,
        (int)MiStorage_NandHeader.SensorData.OlderDateTime.Day,
        (int)MiStorage_NandHeader.SensorData.OlderDateTime.Hour,
        (int)MiStorage_NandHeader.SensorData.OlderDateTime.Minute,
        (int)MiStorage_NandHeader.SensorData.OlderDateTime.Second);
    oSerial_Printf(&MiSerial_Handler, "\"NewerAddr\" : \"%d\",\r\n",
        (int)MiStorage_NandHeader.SensorData.NewerAddress);
    oSerial_Printf(&MiSerial_Handler, "\"NewerTime\" : \"%04d-%02d-%02d %02d:%02d:%02d\",\r\n",
        (int)MiStorage_NandHeader.SensorData.NewerDateTime.Year,
        (int)MiStorage_NandHeader.SensorData.NewerDateTime.Month,
        (int)MiStorage_NandHeader.SensorData.NewerDateTime.Day,
        (int)MiStorage_NandHeader.SensorData.NewerDateTime.Hour,
        (int)MiStorage_NandHeader.SensorData.NewerDateTime.Minute,
        (int)MiStorage_NandHeader.SensorData.NewerDateTime.Second);
    oSerial_Printf(&MiSerial_Handler, "\"SyncAddr\" : \"%d\",\r\n",
        (int)MiStorage_NandHeader.SensorData.SyncAddress);
    oSerial_Printf(&MiSerial_Handler, "\"IsFault\" : \"%d\",\r\n",
        (int)MiStorage_NandHeader.Status.IsFault);
    oSerial_Printf(&MiSerial_Handler, "\"IsError\" : \"%d\"\r\n",
        (int)MiStorage_NandHeader.Status.IsError);
    oSerial_Printf(&MiSerial_Handler, "};\r\n");

    return RESULT_OK;
}
```

---

### 2.5 FR-05: 시작 시 색인 스캔 활성화

**수정 파일**: `Core/Src/Mi_Storage.c` — `MiStorage()`

#### 2.5.1 변경 사항

```c
// MiStorage() 내 case 2: 주석 해제
case 2:
    if(MiStorage_SensorDataIndexing() != RESULT_RUN){
        MiStorageStep++;
    }
    break;
```

#### 2.5.2 MiStorage_SensorDataIndexing 개선

IWDG 타임아웃 방지를 위해 스캔 루프에서 Watchdog 리프레시 필요.
현재 구현에는 `oTMR_Elapsed(&IndexingTimer, 1, ...)` (1ms 딜레이)가 있어 각 페이지 읽기 사이에 1ms 대기. 전체 스캔 시간이 ~130초 소요될 수 있으므로, 블록 단위 최적화 적용:

```c
// 최적화: 블록 첫 페이지가 비어있으면 해당 블록 전체 스킵
case 4:
    r = MiStorage_ReadIoTData(SearchAddress, &IoTData, &Header);
    switch(r)
    {
        case RESULT_RUN:
            break;
        case RESULT_FAULT:
            FaultCount++;
            // fall through → 다음 주소
        case RESULT_ERROR:
            ErrorCount++;
            // 블록 첫 페이지(Page 0)에서 에러 → 블록 전체 스킵
            if(Page == 0){
                SearchAddress = MISTORAGE_ADDRESS(Block+1, 0);
            }
            else{
                SearchAddress++;
            }
            NandInitStep = 1;
            IndexingTimer = oTMR_GetTick(TICKBASE_SYSTICK);
            break;
        case RESULT_OK:
            ReadCount++;
            MiStorage_NandHeader.SensorData.CountOfData++;
            // ... (기존 Older/Newer 비교 로직 유지)
            SearchAddress++;
            NandInitStep = 1;
            IndexingTimer = oTMR_GetTick(TICKBASE_SYSTICK);
            break;
    }
    break;
```

#### 2.5.3 IWDG 리프레시

`MiStorage_SensorDataIndexing()`은 `MiStorage()`에서 호출되고, `MiStorage()`는 메인 루프(`MiMain()`)에서 호출됨. 메인 루프에 IWDG 리프레시가 있으므로 별도 처리 불필요 (1ms 딜레이로 인해 매 호출마다 메인 루프로 복귀).

---

### 2.6 FR-06: 데이터 영역 삭제 (Set/EraseStoredData)

**수정 파일**: `Core/Src/Mi_Serial.c` — `MiSerial_Set()`, `Core/Src/Mi_Storage.c`

#### 2.6.1 MiStorage_EraseDataArea — 신규 함수

**파일**: `Core/Src/Mi_Storage.c`

```c
oResult_t MiStorage_EraseDataArea()
{
    static uint32_t EraseBlock = 0;
    static uint8_t EraseStep = 0;
    oResult_t result = RESULT_RUN;

    switch(EraseStep)
    {
        case 0:
            if(MiStorage_IsOpen){
                EraseStep++;
                EraseBlock = MISTORAGE_ADR_BLOCK(MISTORAGE_DATA_STARTADR);
            }
            else{
                if(MiStorage_Open() == RESULT_ERROR){
                    result = RESULT_ERROR;
                }
            }
            break;
        case 1:
            if(EraseBlock > MISTORAGE_ADR_BLOCK(MISTORAGE_DATA_ENDADR)){
                // 삭제 완료
                memset(&MiStorage_NandHeader.SensorData, 0,
                    sizeof(MiStorage_NandHeader.SensorData));
                result = RESULT_OK;
            }
            else{
                if(MT29F2G_BadBlockCheck(&MT29F2G, EraseBlock) == RESULT_OK){
                    if(MT29F2G_Erase(&MT29F2G, EraseBlock) != RESULT_OK){
                        // Erase 실패 시 스킵 (Bad Block 가능성)
                    }
                }
                EraseBlock++;
            }
            break;
    }

    if(result != RESULT_RUN){
        EraseStep = 0;
    }

    return result;
}
```

#### 2.6.2 시리얼 명령 처리

```c
// Mi_Serial.c : MiSerial_Set() 내 추가
if(strstr(Message, "\"Type\" : \"EraseStoredData\"") != NULL)
{
    oResult_t r;
    while((r = MiStorage_EraseDataArea()) == RESULT_RUN);

    if(r == RESULT_OK){
        MiSerial_PrintResponse("Set/EraseStoredData", "OK");
    }
    else{
        MiSerial_PrintResponse("Set/EraseStoredData", "ERROR");
    }

    return RESULT_OK;
}
```

**주의**: 전체 삭제는 2043 블록 소요 (~30초). IWDG 타임아웃 방지를 위해 `while` 루프 내에서 `HAL_IWDG_Refresh()` 호출 필요하거나, 비동기로 분할 수행.

---

## 3. API Summary

### 3.1 신규/수정 함수

| Function | File | Type | Description |
|----------|------|------|-------------|
| `MiStorage_WriteIoTParameter` | Mi_Storage.c | **수정** | NAND+MCU 이중 저장 |
| `MiStorage_ReadIoTParameter` | Mi_Storage.c | **수정** | MCU 우선, 실패 시 NAND 복원 |
| `MiStorage_EraseDataArea` | Mi_Storage.c | **신규** | NAND 데이터 영역 블록 삭제 |
| `MiStorage_SensorDataIndexing` | Mi_Storage.c | **수정** | 블록 스킵 최적화, ReadCount 초기화 |
| `MiStorage` | Mi_Storage.c | **수정** | 색인 스캔 활성화 (case 2 주석 해제) |

### 3.2 신규 시리얼 명령

| Command | Direction | File | Description |
|---------|-----------|------|-------------|
| `Get/DataIndex` | Request→Response | Mi_Serial.c | NAND 데이터 색인 정보 |
| `Get/StoredData {"Address":"N"}` | Request→Response | Mi_Serial.c | 지정 주소 데이터 읽기 |
| `Set/EraseStoredData` | Request→Response | Mi_Serial.c | NAND 데이터 전체 삭제 |

### 3.3 수정 파일 요약

| File | Changes |
|------|---------|
| `Core/Src/Mi_Storage.c` | FR-01/05/06: Parameter 이중 저장, 색인 활성화, Erase 함수 |
| `Core/Inc/Mi_Storage.h` | FR-06: `MiStorage_EraseDataArea` 선언 추가 |
| `Core/Src/Mi_Serial.c` | FR-03/04/06: 3개 시리얼 명령 핸들러 추가 |
| `Core/Src/Mi_IoT.c` | FR-02: 측정 후 NAND 저장 호출, SyncAddress 순환 버그 수정 |

---

## 4. Data Flow Diagrams

### 4.1 측정 → NAND 저장 → LoRa 전송

```
[센서 측정 완료]
       │
       ▼
MiIoT_UpdateMeasurement()
       │
       ├─→ MiIoT_MailBox_NewItem()      → LoRa 전송 큐
       │
       └─→ MiStorage_WriteIoTData()     → NAND Block 3~2045
              │
              ├─ Address 계산 (NewerAddress+1, 범위 체크)
              ├─ MiStorage_Write() → Header(13B) + Payload
              └─ NewerAddress/NewerDateTime 갱신
```

### 4.2 시리얼 데이터 요청

```
[Host PC]
    │  Get/DataIndex;\r\n
    ▼
MiSerial_Get()
    │  MiStorage_NandHeader 읽기 (RAM)
    │  즉시 응답 (NAND 접근 없음)
    ▼
[Host PC]
    │  Get/StoredData {"Address":"192"};\r\n
    ▼
MiSerial_Get()
    │  MiStorage_ReadIoTData(192, ...)
    │  NAND Open → Read Page → CRC 검증
    │  JSON 응답 전송
    ▼
[Host PC]  (주소를 증가시키며 반복 요청)
```

### 4.3 시작 시 색인 스캔

```
[Power On]
    │
    ▼
MiStorage() case 0: ReadIoTParameter
    │  MCU Flash → IoTParameter (실패 시 NAND 복원)
    ▼
MiStorage() case 2: SensorDataIndexing
    │  NAND Open
    │  Block 3~2045 순차 스캔 (헤더만 읽기)
    │  Bad Block → 블록 스킵
    │  Page 0 Error → 블록 스킵 (최적화)
    │  유효 데이터 → Count++, Older/Newer 갱신
    ▼
MiStorage() case 3: Auto-off (idle 1초 후 NAND 전원 OFF)
```

---

## 5. Error Handling Strategy

| Scenario | Handling |
|----------|----------|
| NAND 전원 켜기 실패 | `MiStorage_Open()` → RESULT_ERROR, IsFault=1 |
| NAND 쓰기 실패 | `MiStorage_Write()` → RESULT_FAULT, 다음 블록으로 이동하지 않음 |
| NAND 읽기 CRC 오류 | `MiStorage_Read()` → RESULT_ERROR, 해당 페이지 무시 |
| Bad Block 발견 | 자동 스킵 (기존 `MT29F2G_BadBlockCheck` 활용) |
| MCU 플래시 쓰기 실패 | Parameter 이중 저장에서 MCU 실패 → RESULT_ERROR (NAND만으로 불충분) |
| 시리얼 잘못된 주소 | 범위 체크 후 "InvalidAddress" 응답 |
| 색인 스캔 중 다수 오류 | ErrorCount > 1 → RESULT_ERROR로 스캔 중단, FaultCount > 1 → RESULT_FAULT |

---

## 6. Implementation Order

| Step | Feature | Dependency | Files |
|------|---------|-----------|-------|
| 1 | SyncAddress 순환 버그 수정 | 없음 | Mi_IoT.c |
| 2 | FR-05: 색인 스캔 활성화 + 최적화 | 없음 | Mi_Storage.c |
| 3 | FR-01: Parameter 이중 저장 | Step 2 (NAND Open 필요) | Mi_Storage.c |
| 4 | FR-02: IoT 데이터 NAND 저장 연동 | Step 2 | Mi_IoT.c |
| 5 | FR-04: Get/DataIndex 시리얼 명령 | Step 2 | Mi_Serial.c |
| 6 | FR-03: Get/StoredData 시리얼 명령 | Step 2 | Mi_Serial.c |
| 7 | FR-06: Set/EraseStoredData + 함수 | Step 2 | Mi_Storage.c/h, Mi_Serial.c |

---

## Version History

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 0.1 | 2026-02-24 | Initial design | JONE |
