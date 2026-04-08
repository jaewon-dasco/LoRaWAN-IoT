# Memory Safety Fix Plan

## Feature : memory-safety-fix
## Date    : 2026-04-01
## Status  : Do Complete → Ready for Check

---

## 1. Overview

SIA100_2A 프로젝트 전체 소스 코드에 대한 메모리 누수(Memory Leak) 및 메모리 오염(Memory Corruption) 정적 분석을 수행하고, 발견된 위험 요소를 수정한다.

### Scope
- Mi_xxx.c  파일 10개
- OneLibrary 파일  7개
- ThirdParty 파일  4개
- **총 21개 파일**

---

## 2. Analysis Summary

### 2.1 Total Findings

| Category       | Files  | High   | Medium | Low    | Total   |
|----------------|:------:|:------:|:------:|:------:|:-------:|
| Mi_xxx.c       |     10 |     22 |     33 |     16 |      71 |
| OneLibrary     |      7 |     24 |     21 |      5 |      50 |
| ThirdParty     |      4 |     11 |     22 |      7 |      40 |
| **Total**      | **21** | **57** | **76** | **28** | **161** |

### 2.2 Memory Leak
- ONE_Common.c `oString_LineAdd`의 malloc Off-by-one 1건 외 동적 할당 거의 미사용
- 전체적으로 메모리 누수 위험 극히 낮음 (임베디드 환경에 적합한 정적 할당 설계)

### 2.3 Common Patterns (Recurring Issues)

| #  | Pattern                              | Count | Description                                          |
|----|--------------------------------------|:-----:|------------------------------------------------------|
|  1 | NULL pointer dereference             |    37 | 거의 모든 모듈에서 함수 파라미터 NULL 검사 누락      |
|  2 | Macro argument parentheses missing   |     8 | MISTORAGE_ADDRESS, MIIOT_PAYLOAD_TO_*_COUNT 등        |
|  3 | sprintf without size limit           |    12 | snprintf 미사용                                      |
|  4 | Static variable reentrancy           |    15 | 상태 머신 함수의 static 변수 공유 문제               |
|  5 | Unaligned memory access              |     8 | uint8_t 버퍼를 다른 타입으로 캐스팅                  |
|  6 | Unsigned type with negative return   |     5 | uint8_t에 -1 반환                                    |
|  7 | Excessive stack usage                |     5 | 1000+ byte 지역 변수                                 |

---

## 3. Fix Priority

### P0 - Immediate Fix (Active Bugs / Crash Causes) [DONE]

| #  | File                     | Line          | Issue                                                                        | Status |
|----|--------------------------|---------------|------------------------------------------------------------------------------|:------:|
|  1 | Mi_Storage.h             | 17-19         | `MISTORAGE_ADDRESS` macro parentheses missing (bad block skip malfunction)   |  DONE  |
|  2 | ONE_Time.c               | 268           | `oDT_Compare` copy-paste bug `pBase==NULL\|\|pBase==NULL`                    |  DONE  |
|  3 | ONE_Signal.c             | 14,202,220,238| `oThreashold`/`oDelay_*` NULL dereference after NULL check                   |  DONE  |
|  4 | Mi_Calibration.c         | 66            | `PacketBuffer[20]` overflow (DLC allows up to 64)                            |  DONE  |
|  5 | Mi_Serial_SIA100_2A.c    | 101,110       | `SetChannelConfig` Channel upper bound missing (arbitrary memory write)      |  DONE  |

### P1 - Early Fix (Data Corruption Possible) [DONE]

| #  | File                     | Line          | Issue                                                                        | Status |
|----|--------------------------|---------------|------------------------------------------------------------------------------|:------:|
|  6 | ONE_Common.c             | 227,247       | `oString_LineAdd` malloc off-by-one heap overflow                            |  DONE  |
|  7 | ONE_Common.c             | 151,169       | `oHexCharToByte` uint8_t returns -1 (error undetectable)                     |  DONE  |
|  8 | Mi_Native_L433RBT7.c    | 36-43         | Flash Read/Write address boundary validation order                           |  DONE  |
|  9 | Mi_Native_L433RBT7.c    | 86 vs 197     | FlashRead/Write address formula mismatch (2-byte offset)                     |  DONE  |
| 10 | LoRa_RAK3172.c           | 1112          | `strncpy` null termination not guaranteed                                    |  DONE  |
| 11 | Mi_LoRa.c                | 125-128       | `MiLoRa_Encode` NULL check after dereference                                |  DONE  |
| 12 | Mi_Storage.h             | 29-36         | `pack(1)` + `aligned(8)` conflict, sizeof mismatch                          |  DONE  |
| 13 | MEMS_ADXL355.c           | header        | Bitfield struct cast to I2C buffer (compiler-dependent, moved to P2)         |  P2    |
| 14 | Mi_Measurement.c         | 44            | `TMPAvrg` moving average buffer size error                                   |  DONE  |
| 15 | ONE_Time.c               | 349-369       | `oDT_IsTimeOver` comparison logic error                                      |  DONE  |

### P2 - Gradual Improvement (Defensive Coding) [DONE]

| #  | Category                 | Scope                                                                        | Status |
|----|--------------------------|------------------------------------------------------------------------------|:------:|
| 13 | MEMS bitfield cast       | ADXL355 bitfield→I2C buffer (ARM GCC target에서 정상 동작, accepted risk)    |  N/A   |
| 16 | NULL pointer validation  | ONE_Common(5), ONE_Memory(5), NAND ReadMemory/WriteMemory (2) — 12건 추가    |  DONE  |
| 17 | sprintf -> snprintf      | LoRa_RAK3172.c(16), Mi_LoRa.c(1), Mi_Serial.c(1) — 18건 변환               |  DONE  |
| 18 | volatile declaration     | ISR callback=weak default, 변수 main-loop only → volatile 불필요            |  N/A   |
| 19 | Stack size review        | Mi_Serial.c `Message[1000]` → static 전환 (스택 1KB 절감)                   |  DONE  |
| 20 | oString_Right fix        | ONE_Common.c loop range + NULL guard 추가                                    |  DONE  |
| 21 | NAND driver validation   | ReadMemory/WriteMemory pData NULL + Size==0 검증 추가                        |  DONE  |

---

## 4. File-by-File Detailed Findings

### 4.1 Mi_Main_Node_SIA100_2A.c

| Severity | Count | Key Issues                                                                   |
|----------|:-----:|------------------------------------------------------------------------------|
| High     |     0 |                                                                              |
| Medium   |     3 | Format string argument missing (`%s` without argument)                       |
| Low      |     3 | Bitfield union endian dependency, double pointer NULL check                  |

### 4.2 Mi_Native_L433RBT7.c

| Severity | Count | Key Issues                                                                   |
|----------|:-----:|------------------------------------------------------------------------------|
| High     |     6 | Flash address boundary validation order, SRAM boundary, CRC range            |
| Medium   |     6 | FlashRead/Write formula mismatch, wake-up counter overflow                   |
| Low      |     3 | ADC_ChannelConfTypeDef partial init, float precision                         |

### 4.3 Mi_Serial.c + Mi_Serial_SIA100_2A.c

| Severity | Count | Key Issues                                                                   |
|----------|:-----:|------------------------------------------------------------------------------|
| High     |     5 | Stack overflow risk (1300+ bytes), Channel OOB write                         |
| Medium   |     7 | ISR variables missing volatile, TX buffer size=1                             |
| Low      |     4 | NULL check missing, strncpy pattern                                          |

### 4.4 Mi_IoT.c + Mi_IoT_SIA100_2A.c + Mi_LoRa.c

| Severity | Count | Key Issues                                                                   |
|----------|:-----:|------------------------------------------------------------------------------|
| High     |     4 | memcpy DLC not validated, NULL check after dereference                       |
| Medium   |     8 | MIIOT_DT_TO_IOTTIME macro, macro parentheses, ring buffer index              |
| Low      |     3 | sprintf usage, pointer cast                                                  |

### 4.5 Mi_Measurement.c + Mi_Calibration.c

| Severity | Count | Key Issues                                                                   |
|----------|:-----:|------------------------------------------------------------------------------|
| High     |     4 | PacketBuffer[20] overflow, TMPAvrg sizeof error                              |
| Medium   |     4 | Unaligned access (Data[3] with int32_t), ADC truncation                      |
| Low      |     2 | Static variable reentrancy, variable naming typo                             |

### 4.6 Mi_Storage.c

| Severity | Count | Key Issues                                                                   |
|----------|:-----:|------------------------------------------------------------------------------|
| High     |     3 | MISTORAGE_ADDRESS macro (active bug), pack+aligned conflict                  |
| Medium   |     5 | SizeOfData overflow, pData NULL, static reentrancy                           |
| Low      |     3 | Erase result ignored, uninitialized variable                                 |

### 4.7 OneLibrary (7 files)

| File               | High | Medium | Low | Key Issues                                             |
|--------------------|:----:|:------:|:---:|--------------------------------------------------------|
| ONE_Memory.c       |    5 |      2 |   0 | All functions missing NULL validation                  |
| ONE_Common.c       |    7 |      3 |   1 | malloc off-by-one, uint8_t returns -1, oString_Right   |
| ONE_Math.c         |    2 |      2 |   0 | Alignment violation, SIZE_OF_DATATYPE(0)=0             |
| ONE_Signal.c       |    4 |      1 |   1 | NULL check then NULL dereference                       |
| ONE_Time.c         |    2 |      5 |   1 | Copy-paste bug, integer overflow, IsTimeOver logic     |
| ONE_Serial.c       |    1 |      4 |   1 | Null termination not guaranteed                        |
| ONE_ATCommend.c    |    3 |      4 |   1 | Static non-reentrant, strlen(NULL)                     |

### 4.8 ThirdParty (4 files)

| File                        | High | Medium | Low | Key Issues                                    |
|-----------------------------|:----:|:------:|:---:|-----------------------------------------------|
| LoRa_RAK3172.c              |    3 |      7 |   3 | strncpy null term, stack 901B in callback     |
| NAND_MT29F2G01ABAGDWB_IT.c  |    3 |      8 |   2 | ReadMemory/WriteMemory Size not validated     |
| MEMS_ADXL355BEZRL7.c        |    3 |      4 |   2 | Bitfield struct cast to I2C buffer            |
| TEMP_TMP1075.c              |    2 |      5 |   2 | Hardcoded I2C size vs struct size             |

---

## 5. Implementation Order

1. **P0 (Done)** : 5 critical fixes applied
2. **P1 (Done)** : 9 fixes applied, 1 moved to P2 (#13 MEMS bitfield - compiler-dependent)
3. **P2 (Done)** : 5 fixes applied, 2 items N/A after investigation

## 6. Implementation Summary

### Total Changes
| Priority | Fixed | N/A | Files Modified |
|----------|:-----:|:---:|:--------------:|
| P0       |     5 |   0 |              5 |
| P1       |     9 |   0 |              7 |
| P2       |     5 |   2 |              5 |
| **Total**| **19**|**2**|   **10 files** |

### Modified Files
1. `Mi_Storage.h` — P0#1 macro parens, P1#12 aligned(8) removal
2. `ONE_Time.c` — P0#2 oDT_Compare, P1#15 oDT_IsTimeOver
3. `ONE_Signal.c` — P0#3 NULL dereference 4건
4. `Mi_Calibration.c` — P0#4 PacketBuffer overflow
5. `Mi_Serial_SIA100_2A.c` — P0#5 Channel boundary
6. `ONE_Common.c` — P1#6 malloc, P1#7 hex detect, P2#16 NULL(5), P2#20 oString_Right
7. `Mi_Native_L433RBT7.c` — P1#8 flash boundary, P1#9 formula
8. `LoRa_RAK3172.c` — P1#10 strncpy, P2#17 snprintf(16)
9. `Mi_LoRa.c` — P1#11 NULL order, P2#17 snprintf(1)
10. `Mi_Measurement.c` — P1#14 TMPAvrg sizeof
11. `ONE_Memory.c` — P2#16 NULL(5)
12. `NAND_MT29F2G01ABAGDWB_IT.c` — P2#21 pData/Size validation
13. `Mi_Serial.c` — P2#19 static Message, P2#17 snprintf(1)
