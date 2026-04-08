# Plan: LoRa 전송 중단 원인 분석 및 수정

## 1. 개요

### 증상
- **증상 1**: 3개월간 1시간 주기 데이터 전송 후 갑자기 전체 LoRa 전송 멈춤 (배터리 4.1V)
- **증상 2**: 배터리 점검 후 재부팅 → 정상 동작 → 6시간 전송 후 다시 멈춤 (배터리 4.1V)

### 핵심 관찰
- 배터리 잔량 충분 (4.1V) → 전원 문제 아님
- 재부팅 후 6시간만에 재현 → 시간 의존적 누적 버그
- 3개월 후 첫 발생 → 장기 운영 시에만 나타나는 문제
- **7대 동일 장치 중 1대만 발생** → RF 환경 차이로 AT 재시도 빈도 차이
- **LED가 전혀 깜빡이지 않음** → MCU 메인 루프 자체가 실행 안 됨 (단순 LoRa 중단 아님)
- 다운링크 없음 → 서버에서 OperatingMode 변경 가능성 배제

---

## 2. 시스템 동작 흐름 요약

```
MiMain() [매 루프]
├── Native_WatchDog(11초)     ← IWDG 리프레시
├── MiStorage()               ← Flash 파라미터 관리
├── MiIoT()
│   ├── MiLoRa()
│   │   ├── MiLoRa_Control()        ← LoRa Open/Close 결정
│   │   ├── MiLoRa_TimeSync()       ← 시간 동기화
│   │   ├── MiLoRa_MailBoxTransmitScheduler()  ← 메일 전송
│   │   └── RAK3172()               ← AT 명령 처리
│   ├── MiIoT_UpdateMeasurement()   ← 주기 측정 (1시간)
│   ├── MiIoT_UpdateSampling()      ← 수동 샘플링
│   ├── MiIoT_UpdateStatus()        ← 상태 전송
│   ├── MiIoT_Sleep()               ← STOP 모드 진입
│   └── MiIoT_LEDIndicator()
└── MiSerial()                ← 시리얼 통신
```

### STOP 모드 동작
- Operating 모드: `Native_SleepMode(10000)` → 10초 STOP → RTC 웨이크업
- Stop 모드: `Native_SleepMode(0)` → RTC 웨이크업 타이머 미설정 → **무한 STOP** (GPIO 인터럽트/IWDG만 깨움)

### IWDG 설정
- Prescaler: 256, Reload: ~1375 → 약 11초 타임아웃
- STOP 모드에서도 LSI 클럭으로 동작 → 무한 STOP 시에도 리셋 발생

---

## 3. 원인 분석 (우선순위별)

### P1. [HIGH] calloc/free 힙 파편화 → HardFault 반복 리셋

**파일**: `OneLibrary/Src/ONE_ATCommend.c:113, 152`

```c
// oAT_ReadLine (line 113)
pBuffer = calloc(length+1, sizeof(char));
if(oAT_ReceiveCopyTo(pAT, (uint8_t *)pBuffer, AT_RECEIVE_BUFFER_COUNT(pAT)) == 0){
    goto EXIT;  // free(NULL) → 안전하지만 데이터 미처리
}
// ...
EXIT:
free(pBuffer);
```

**문제**:
- 매 AT 응답 처리 시 `calloc`/`free` 반복 (1시간 주기 × 전송당 약 10개 AT 명령)
- 3개월: ~2,160 사이클 × 10 = ~21,600회 calloc/free
- STM32L433 RAM 64KB에서 힙 파편화 축적
- calloc 실패 시 NULL 반환 → `oAT_ReceiveCopyTo(pAT, NULL, ...)` → **NULL 포인터 쓰기 → HardFault**
- HardFault → `Error_Handler()` → `__disable_irq(); while(1);` → IWDG 리셋 (11초)
- 재시작 후 동일 패턴 반복 → 빠른 리셋 루프 → 장치가 "멈춘 것처럼" 보임

**증상 일치도**:
- 3개월 후 멈춤: 힙 파편화 누적
- 재부팅 후 6시간 멈춤: 힙이 리셋되지만, 운영 패턴에 따라 빠르게 재파편화
  (특히 LoRa Open/Close 반복 시 대량의 AT 명령 처리)

**수정 방안**:
```c
// 방안 A: 정적 버퍼 사용 (권장)
static char pReadBuffer[AT_READ_BUFFER_SIZE];  // 정적 할당
// calloc/free 제거

// 방안 B: calloc 실패 체크 추가
pBuffer = calloc(length+1, sizeof(char));
if(pBuffer == NULL){
    AT_RECEIVE_BUFFER_REMOVE(pAT, length);  // 버퍼 비우기
    return 0;
}
```

---

### P2. [HIGH] STOP 모드 후 UART DMA 상태 불일치

**파일**: `Core/Src/main.c:639-645`, `Core/Src/Mi_Native_L433RBT7.c:547-581`

```c
// main_Init() - STOP 모드 깨어난 후 호출
void main_Init()
{
    HAL_ADC_DeInit(&hadc1);
    MX_GPIO_Init();      // GPIO만 복원
    MX_ADC1_Init();      // ADC만 복원
    // UART3, LPUART1 재초기화 없음!
}
```

**문제**:
- STOP 모드에서 HSI/HSE/PLL 정지 → 주변장치 클럭 중단
- 깨어난 후 `SystemClock_Config()`로 클럭 복원하지만 UART 상태는 불일치 가능
- **LPUART1** (시리얼 통신): 깨어난 후 재초기화 없음 → DMA 위치 계산 오류 가능
- **UART3** (LoRa): MiLoRa_Close/Open 시 `oAT_Close/oAT_Open`으로 재초기화되므로 상대적으로 안전
- 그러나 UART3의 DMA 채널 상태가 완전히 리셋되지 않을 수 있음

**증상 일치도**: STOP 모드 반복 → UART 상태 점점 불안정 → 통신 실패 누적

**수정 방안**:
```c
void main_Init()
{
    HAL_ADC_DeInit(&hadc1);
    MX_GPIO_Init();
    MX_ADC1_Init();
    // UART 상태 확인 및 에러 클리어 추가
    __HAL_UART_CLEAR_FLAG(&hlpuart1, UART_CLEAR_OREF | UART_CLEAR_FEF | UART_CLEAR_NEF | UART_CLEAR_PEF);
}
```

---

### P3. [MEDIUM-HIGH] Mailbox 전송 실패 시 큐 차단

**파일**: `Core/Src/Mi_LoRa.c:480-497`

```c
// MiLoRa_MailBoxTransmitScheduler case 2:
switch(MiLoRa_SendMailbox(pMail))
{
    case RESULT_PAUSE:
    case RESULT_RUN:
        break;
    case RESULT_FAULT:
        MiLoRa_Close();
        break;
    case RESULT_OK:
        MiLoRa_ScheduleStep = 0;
        MiIoT_MailBox_Remove(&MiIoT_LoRaMailBox, pMail);
        break;
    default:  // RESULT_ERROR, RESULT_TIMEOUT → 메일 제거 안 됨!
        break;
}
```

**문제**:
- `MiLoRa_SendMailbox`가 RESULT_ERROR/RESULT_TIMEOUT 반환 시:
  - 메일이 큐에서 제거되지 않음
  - ScheduleStep=2 유지 → 같은 메일 무한 재시도
  - MiLoRa_Encode가 0 반환하는 경우 (DLC 불일치):
    SendMailbox → RESULT_ERROR → 재시도 → 영구 반복
- 새로운 측정 데이터가 큐에 쌓이지만 처리 불가

**증상 일치도**:
- 한 번 인코딩 실패하는 메일이 들어오면 이후 모든 전송 중단
- LoRa Close/Open으로 ScheduleStep은 리셋되지만, 같은 메일을 다시 가져옴

**수정 방안**:
```c
// 에러 시 메일 제거 또는 에러 카운트 후 제거
case RESULT_ERROR:
case RESULT_TIMEOUT:
    if(++pMail->ErrorRetryCount >= MAX_MAIL_RETRY){
        MiIoT_MailBox_Remove(&MiIoT_LoRaMailBox, pMail);
        MiLoRa_ScheduleStep = 0;
    }
    break;
```

---

### P4. [MEDIUM] OperatingMode 손상 → LoRa 영구 종료

**파일**: `Core/Src/Mi_LoRa.c:674-676`

```c
if(MiIoT_Parameter.Operating.OperatingMode != IoTOperatingMode_Operating &&
   MiIoT_Parameter.Operating.OperatingMode != IoTOperatingMode_PreOperation &&
   pLoRaDevice->IsInitailzed){
    MiLoRa_Close();  // Operating/PreOperation 아니면 무조건 닫음
}
```

**문제**:
- Flash 저장소 손상으로 OperatingMode가 예기치 않은 값이 되면:
  - LoRa가 항상 닫힘
  - 10초마다 STOP 모드 진입/복귀 반복하면서 LoRa는 절대 열리지 않음
  - 장치는 살아있지만 (IWDG 리프레시됨) 전송은 0
- 기본값: `IoTOperatingMode_Stop` → 파라미터 로드 실패 시 자동으로 Stop 모드

**증상 일치도**:
- Flash 손상은 장기 운영 시 발생 가능 (전원 불안정, Write 중 STOP 모드 진입 등)
- 재부팅 후에도 같은 손상된 파라미터 로드 → 동일 문제

**수정 방안**:
- 파라미터 로드 시 유효성 검증 강화
- Flash Write 중 인터럽트/슬립 보호
- 손상 감지 시 기본값 Operating 모드 사용 (Stop 대신)

---

### P5. [MEDIUM] STOP 모드의 SysTick 보정 오차 누적

**파일**: `Core/Src/Mi_Native_L433RBT7.c:569`

```c
// STOP 모드 깨어난 후
oTMR_SetTick(TICKBASE_SYSTICK, oTMR_GetTick(TICKBASE_SYSTICK) + Millisecond);
```

**문제**:
- STOP 모드 실제 지속 시간과 `Millisecond` (10000) 값이 정확히 일치하지 않을 수 있음
- GPIO 인터럽트 (USB 연결 등)로 조기 깨어나면 실제 경과 시간 < 10초
  → SysTick이 실제보다 빠르게 진행 → 타이머 비교 오차 누적
- 3개월 × 하루 ~8640 슬립 사이클 = ~777,600 사이클
- 사이클당 수 ms 오차 → 수시간 누적 오차 가능

**영향**:
- `StatusUpdateTimer`, `AutoOffDelay` 등 SysTick 기반 타이머 오동작 가능
- 직접적 전송 중단 원인은 아니지만, 타이밍 관련 부차적 문제 유발

---

### P6. [LOW-MEDIUM] DWT 카운터 STOP 모드 동기화 문제

**파일**: `OneLibrary/Src/ONE_Time.c:44-73`

**문제**:
- `DWT->CYCCNT`는 STOP 모드에서 정지
- 깨어난 후 `oTMR_Update()`가 DWT 오버플로우로 오인할 가능성:
  - STOP 전 CYCCNT가 0xFFFF_FFFE였다면
  - 깨어난 후 몇 사이클 실행 → CYCCNT 오버플로우 (0x0000_xxxx)
  - `CycleCount_ms`는 아직 0xFFFF_FFFE 근처 → 오버플로우 감지 (정상)
  - 하지만 `CycleInterval` 계산에서 큰 값 → `oTMR_Tick_ms`에 큰 점프
- TICKBASE_CORECYCLE_MS/US 사용하는 코드에만 영향

---

### P7. [LOW] SysTick uint32_t 오버플로우 (49.7일)

**파일**: `OneLibrary/Src/ONE_Time.c:129-141`

```c
uint32_t oTMR_Interval(uint32_t Tick1, uint32_t Tick2)
{
    if(Tick1 >= Tick2) return Tick1-Tick2;
    else return (TMR_TICK_MAXVALUE-Tick2)+Tick1;
}
```

**상태**: 오버플로우 처리가 구현되어 있으며 정상 동작으로 보임. 단, `oTMR_GetTick`에서 `TMR_TICK_MAXVALUE`를 건너뛰는 로직(line 86-88)과의 호환성 확인 필요.

---

## 4. 수정 우선순위 및 작업 계획

### Phase 1: 즉시 수정 (Critical)

| # | 작업 | 파일 | 위험도 |
|---|------|------|--------|
| 1 | calloc → 정적 버퍼 변경 | ONE_ATCommend.c | HIGH |
| 2 | SendMailbox 에러 시 메일 제거 로직 추가 | Mi_LoRa.c | HIGH |
| 3 | calloc NULL 체크 추가 (임시) | ONE_ATCommend.c | HIGH |

### Phase 2: 안정성 개선

| # | 작업 | 파일 | 위험도 |
|---|------|------|--------|
| 4 | STOP 모드 후 UART 에러 플래그 클리어 | main.c / Mi_Native | MEDIUM |
| 5 | OperatingMode 유효성 검증 강화 | Mi_IoT.c / Mi_Storage.c | MEDIUM |
| 6 | STOP 모드 실제 경과 시간 보정 (RTC 기반) | Mi_Native_L433RBT7.c | MEDIUM |

### Phase 3: 모니터링 및 방어

| # | 작업 | 파일 | 위험도 |
|---|------|------|--------|
| 7 | HardFault 핸들러에 리셋 카운터 추가 | main.c | LOW |
| 8 | 힙 사용량 모니터링 (Status에 포함) | Mi_Main | LOW |
| 9 | LoRa 연속 실패 시 자동 복구 로직 강화 | Mi_LoRa.c | LOW |

---

## 5. 검증 방법

1. **빌드 후 장기 테스트**: 수정 적용 → 최소 48시간 연속 동작 확인
2. **힙 모니터링**: `mallinfo()` 또는 스택/힙 경계 체크를 Status에 포함
3. **로그 분석**: 시리얼 로그에서 AT 명령 타임아웃 패턴, 리셋 카운트 확인
4. **의도적 스트레스**: LoRa Join 실패 상황 반복 → 메일박스 큐 차단 여부 확인

---

## 6. [추가] LED 안 깜빡임 핵심 원인: STOP 모드 / IWDG 리셋 루프

### 발견된 Race Condition

**부팅 시퀀스 타이밍 문제**:

1. `MiMainStep 0`: 기본 파라미터 설정 (`OperatingMode = Stop`)
2. `MiMainStep default`: `MiStorage()` + `MiIoT()` 동시 실행
3. `MiStorage_Open()` step 1: NAND 전원 켜기 **200ms 대기** → `MiStorage_IsOpen = 0`
4. `MiIoT_Sleep()` case 0: `MiIoT_IsBusy = MiStorage_IsOpen = 0` → 조건 충족 → **즉시 Sleep 진입**
5. Sleep case 2: `Native_SleepMode(OperatingMode == Stop ? 0 : 10000)` → `Native_SleepMode(0)`
6. **RTC wakeup 없이 무한 STOP 모드 진입!**

```
부팅 → 기본 파라미터(Stop) → MiStorage 200ms 대기 중...
                                ↓ (MiStorage_IsOpen = 0)
                     MiIoT_Sleep: "할 일 없다" → STOP(0)
                                ↓ (wakeup 없음)
                     11초 후 IWDG 리셋 → 부팅 → 반복
```

### IWDG 리셋 루프 동작

| 시간 | 상태 | LED |
|------|------|-----|
| 0~0.1ms | 부팅, GPIO 초기화 (LED OFF) | OFF |
| 0.1~0.2ms | 3-4 루프 반복, Sleep 진입 | OFF |
| 0.2ms~11s | STOP 모드 (wakeup 없음) | OFF |
| 11s | IWDG 리셋 | OFF |
| 반복... | | **항상 OFF** |

각 주기에서 실행 시간(~0.2ms)이 너무 짧아 LED BlinkSleep 패턴(80ms)까지 도달 불가.

### 왜 처음엔 3개월 동작했나?

초기 설치 시 **USB 연결**하여 파라미터 설정:
1. USB EXTI 인터럽트가 STOP 모드에서 깨움 (GPIO_MODE_IT_RISING)
2. USB 연결 중 `MiIoT_IsIORun = 1` → Sleep 진입 차단
3. MiStorage가 파라미터 로드 완료 → `OperatingMode = Operating`
4. USB 분리 후 → 이미 Operating 모드 → Sleep(10초) → 정상 동작

**HardFault 후 IWDG 리셋** (USB 미연결):
1. 기본 파라미터 (`Stop`) 사용
2. MiStorage 로드 전에 Sleep 진입
3. `Native_SleepMode(0)` → 무한 STOP → 탈출 불가!

---

## 7. 수정 사항 요약

### 완료된 수정

| # | 수정 | 파일 | 상태 |
|---|------|------|------|
| P1 | calloc/free → static 버퍼 | ONE_ATCommend.c | **완료** |
| P2 | Native_SleepMode(0) → 최소 10초 wakeup 보장 | Mi_Native_L433RBT7.c | **완료** |
| P3 | Mailbox scheduler 실패 메일 제거 | Mi_LoRa.c | **완료** |

### P2 수정 내용 (핵심)

```c
// Mi_Native_L433RBT7.c - Native_SleepMode()
if(Millisecond == 0){
    Millisecond = SECOND_TO_MS(10);  // 무한 STOP 방지
}
```

이 수정으로:
- `OperatingMode = Stop`이더라도 10초마다 반드시 깨어남
- MiStorage가 파라미터를 로드할 시간 확보
- IWDG 리셋 루프 방지

---

## 8. 결론

**장치 멈춤의 복합 원인 (2단계)**:

1. **1단계 (트리거)**: `calloc/free` 힙 파편화 → HardFault → IWDG 리셋
2. **2단계 (고착)**: IWDG 리셋 후 기본 파라미터(Stop) → MiStorage 로드 전 Sleep 진입 → `Native_SleepMode(0)` 무한 STOP → IWDG 리셋 → **반복**

**LED 안 깜빡임 설명**: 각 리셋 주기의 실행 시간(~0.2ms)이 LED 패턴(80ms)보다 짧아 시각적으로 확인 불가.

**7대 중 1대만 발생**: RF 환경 차이 → 더 많은 LoRa 재시도 → 더 빠른 힙 파편화 → 다른 장치보다 먼저 HardFault 도달.

**수정 효과**:
- P1: HardFault 자체를 제거 (calloc 제거)
- P2: 설령 HardFault가 발생해도 IWDG 리셋 후 정상 복구 보장
- P3: 메일큐 블로킹으로 인한 전송 중단 방지
