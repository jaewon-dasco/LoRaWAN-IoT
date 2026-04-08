# LoRa Join 실패 시 재시도 중단 - 추가 원인 분석

> **Feature**: lora-join-failure
> **Phase**: Plan (추가 분석)
> **Date**: 2026-03-30
> **Status**: In Progress
> **참조**: lora-join-failure-analysis.plan.md (기존 분석 - 수정 완료)

## 1. 배경

기존 분석(`lora-join-failure-analysis`)에서 **근본 원인: MiLoRa_IsBusy가 MiIoT_IsBusy에 미반영 → Sleep 중 LoRa 프로세스 파괴**가 확인되어 수정 완료됨.

본 문서는 기존 수정으로 해결되지 않을 수 있는 **추가적인 잠재 원인**들을 상세 분석한다.

## 2. 분석 범위

기존 5개 원인(모두 Sleep/IsBusy 연쇄 반응) 외에, 코드 전체를 검토하여 23시간 공백을 유발할 수 있는 독립적인 원인을 탐색.

---

## 3. 추가 원인 분석

---

### 추가 원인 #A: RAK3172_Join() static 변수 잔류 [가능성: 중]

**문제:**

`RAK3172_Join()`의 `JoinStep`, `JoinRetryCount`가 함수 내부 `static` 변수로, `MiLoRa_Close()` → `RAK3172_Close()` 호출 시 **리셋되지 않음**.

```c
// LoRa_RAK3172.c:450
oResult_t RAK3172_Join()
{
    static uint8_t JoinStep = 0;         // ← Close에서 미리셋
    static uint8_t JoinRetryCount = 0;   // ← Close에서 미리셋
    static uint32_t JoinTimer = 0;       // ← Close에서 미리셋
```

```c
// LoRa_RAK3172.c:1227
oResult_t RAK3172_Close()
{
    RAK3172Dev.DownlinkCallback = NULL;
    oAT_Close(&RAK3172Dev.AT);
    memset(&RAK3172Dev.Status, 0, sizeof(RAK3172Dev.Status));
    // JoinStep, JoinRetryCount, JoinTimer → 리셋 없음!
    return RESULT_OK;
}
```

**시나리오:**
1. Join 진행 중 (`JoinStep=2`, EVT 대기), MCU가 `MiLoRa_Close()` 호출
2. `RAK3172_Close()`: Status 초기화, UART DeInit → **JoinStep=2 잔류**
3. 다음 `MiLoRa_Open()` → Step 4 RAK3172_Open() 성공 → Step 5 RAK3172_Join() 호출
4. `JoinStep=2`에서 시작: IsJoined=0, IsJoinFail=0 (Status 초기화됨)
5. `JoinTimer`가 이전 값 → `oTMR_Elapsed(31초)` 즉시 만료 → **TIMEOUT**
6. `JoinRetryCount++` → 이전 값이 2였다면 → 3 >= JoinMaxRetry(3) → **RESULT_TIMEOUT 즉시 반환**
7. 실제 Join을 한 번도 시도하지 않고 실패 처리됨

**영향:**
- 최악의 경우 실질 Join 시도 횟수가 3→1회로 줄어듦 (31초 낭비 1회 + 실제 시도 2회)
- RAK3172_Join() 완료 시 JoinRetryCount=0으로 리셋되므로 **영구적 stuck은 아님**
- 하지만 연속 Join 실패 시 매번 1회 낭비가 누적

**심각도:** LOW - 23시간 중단의 직접 원인이 되기 어렵지만, Join 효율 저하

---

### 추가 원인 #B: SysTick 보정과 RTC Wake-up 타이밍 불일치 [가능성: 중하]

**문제:**

STOP Mode에서 SysTick은 정지하고, wake-up 후 고정값(10000ms)을 더하여 보정:

```c
// Mi_Native_L433RBT7.c:622
oTMR_SetTick(TICKBASE_SYSTICK, oTMR_GetTick(TICKBASE_SYSTICK) + Millisecond);
```

실제 RTC wake-up 시간은 클럭 소스에 따라 다름:
- **LSE (32.768kHz)**: 높은 정확도 (±20ppm) → 오차 미미
- **LSI (~32kHz)**: 낮은 정확도 (**±5%**) → 10초당 최대 0.5초 오차

```c
// Mi_Native_L433RBT7.c:603
uint32_t wakeup_counter = (Millisecond * 2048) / 1000;  // ms → 2048Hz 변환
// 10000ms → 20480 카운트 (RTC 16분주 = 2048Hz 가정)
```

**오차 누적 계산 (LSI 사용 시):**
| 시간 | Sleep 횟수 | 최대 누적 오차 |
|------|-----------|---------------|
| 1시간 | 360회 | ±180초 (3분) |
| 8시간 | 2880회 | ±1440초 (24분) |
| 24시간 | 8640회 | ±4320초 (72분) |

**영향:**
- `oTMR_Elapsed()` 기반 타이머가 모두 영향받음
- `MILORA_OPEN_REMAINING_TIME` (5초 대기)가 실제로 더 길어지거나 짧아질 수 있음
- AT 명령 타임아웃이 늘어나 Join 사이클이 느려질 수 있음
- 하지만 단독으로 23시간 중단을 설명하기는 어려움

**심각도:** LOW - 타이밍 오차는 발생하나, 재시도 자체를 멈추지는 않음

**확인 필요:** RTC 클럭 소스가 LSE인지 LSI인지 확인

---

### 추가 원인 #C: DMA 수신 250ms 대기와 Join EVT 처리 지연 [가능성: 중]

**문제:**

`oAT_ReceiveFromDMA()`에서 DMA 수신 데이터가 있어도 **250ms의 silence** 후에야 처리:

```c
// ONE_ATCommend.c:195-211
if(pAT->IndexOfRxLast != AT_DMA_INDEX(pAT->pUART)){
    pAT->IndexOfRxLast = AT_DMA_INDEX(pAT->pUART);
    pAT->RxTimestamp = oTMR_GetTick(TICKBASE_SYSTICK);  // 새 데이터 → 타이머 리셋
}
else if(oTMR_Elapsed(&pAT->RxTimestamp, 250, TICKBASE_SYSTICK)){
    // 250ms 동안 추가 데이터 없음 → 수신 완료로 판단, 파싱 시작
    if(AT_RECEIVE_BUFFER_COUNT(pAT) > 0){
        oAT_ReadLine(pAT);
    }
}
```

**시나리오:**
1. RAK3172가 `+EVT:JOIN_FAILED\r\n` 전송 (약 20바이트)
2. MCU가 DMA로 수신 → `IndexOfRxLast` 변경 → `RxTimestamp` 리셋
3. **250ms 대기** (추가 데이터 없음 확인)
4. `oAT_ReadLine()` → `RAK3172_ReceiveCallback()` → `IsJoinFail=1`
5. 그제서야 `RAK3172_Join()` Step 2에서 `IsJoinFail` 감지

**일반적으로는 정상이나, 고속 연속 EVT 시:**
- RAK3172가 `+EVT:JOIN_FAILED` 직후 다른 EVT를 보내면 250ms 타이머가 재시작
- 여러 EVT가 연속으로 오면 처리가 계속 지연됨
- 극단적으로 RAK3172가 끊임없이 데이터를 보내면 처리가 영원히 안 됨

**영향:** 일반적으로 무시할 수준이지만, AT 명령 응답 + EVT가 동시에 오는 경우 250ms~수초 지연 가능

**심각도:** LOW - 지연이지 중단은 아님

---

### 추가 원인 #D: RAK3172 AT+JOIN 내부 동작과 MCU 타임아웃 불일치 [가능성: 중]

**문제:**

MCU 측 타임아웃과 RAK3172 모듈 내부 동작 시간이 경합:

```c
// RAK3172_Join() Step 1: AT+JOIN 전송
sprintf(Commned, "AT+JOIN=1:0:%d:%d", RAK3172Dev.Config.JoinInterval, RAK3172Dev.Config.JoinAttempts);
// AT+JOIN=1:0:10:3 → 10초 간격으로 3번 시도

// Step 2: EVT 대기 타임아웃
oTMR_Elapsed(&JoinTimer, SECOND_TO_MS(10*3+1), TICKBASE_SYSTICK)
// = 31초 타임아웃
```

**RAK3172 내부 동작:**
- Join Request 1회 → RX1 Window (1~2초) → RX2 Window (2~3초) → 결과 대기
- 3번 시도 × 10초 간격 = **최대 30초 + RX 윈도우 대기**
- 마지막 시도의 RX2 완료까지 실제 **~32-34초** 소요 가능

**시나리오:**
1. MCU: AT+JOIN 전송 → 31초 타이머 시작
2. RAK3172: 3번째 Join의 RX2 대기 중 (32초 경과)
3. MCU: 31초 타임아웃 → `JoinRetryCount++`
4. MCU: 다시 Step 0 → AT+NJS=? 전송
5. RAK3172: 아직 이전 Join 처리 중 → NJS 응답이 불안정
6. 또는: 이전 Join이 32초째 성공 → `+EVT:JOINED` 전송
7. MCU: 이미 다음 AT+NJS 전송 중 → JOINED EVT가 AT 응답과 혼선

**영향:**
- 타이밍 경합으로 Join 성공 EVT를 놓칠 수 있음
- 반복될수록 RAK3172 내부 상태와 MCU 인식이 불일치
- 최악의 경우: 실제로는 Join 되었는데 MCU가 인식 못함 → Close → 다시 Open

**심각도:** MEDIUM - 단독으로 23시간 중단은 어렵지만, IsBusy 문제와 결합 시 악화 요인

**개선안:** 타임아웃을 `JoinInterval * JoinAttempts + 5`로 여유분 확대 (31초 → 35초)

---

### 추가 원인 #E: MiLoRa_Control의 IsOpen + !IsJoined → Close 즉시 실행 [가능성: 중]

**문제:**

절전모드에서 Open 성공 후, 네트워크 이벤트로 `IsJoined=0`이 되면 즉시 Close:

```c
// Mi_LoRa.c:765-771
else if(MiIoT_IsPowerSaveMode){
    if(MiLoRa_IsOpen){
        if(!RAK3172Dev.Status.IsJoined){    // ← Join 해제 시 즉시 Close
            MiLoRa_Close();
        }
```

**시나리오 (LoRaWAN 세션 만료):**
1. Join 성공 → IsOpen=1, IsJoined=1
2. 메일 전송 → 완료 → AutoOffDelay 1초 → Close (정상 절전)
3. 나중에 다시 메일 발생 → Open → AT+NJS=? → 이전 세션 유효 (NJS=1) → Join Skip → Open 성공
4. 메일 전송 시도 → `+EVT:AT_NO_NETWORK_JOINED` 수신 (세션 만료) → IsJoined=0
5. MiLoRa_Control: IsOpen=1 && !IsJoined → **즉시 Close**
6. 전송 중인 메일이 중단됨 → SendMailboxStep 리셋

**특이 케이스: NJS 세션 유효성 착각**
- RAK3172_Join() Step 0에서 AT+NJS=1을 받으면 Join을 건너뜀
- 하지만 서버 측에서 세션이 이미 만료됨 (예: 게이트웨이 재시작)
- 전송 시 서버가 No Network Joined 응답 → 실패
- Close → Open → 다시 NJS 확인 → RAK3172가 아직 NJS=1 보고 (모듈은 세션 유효하다고 인식)
- **Join 없이 Open → 전송 → 실패 → Close → 반복 (무한 루프)**

이 무한 루프에서 빠져나오려면 RAK3172의 NJS가 0이 되어야 하는데, `+EVT:AT_NO_NETWORK_JOINED` 수신 시:
```c
// LoRa_RAK3172.c:38
else if(RAK3172_COMPARE_COMMEND(pStr, "AT_NO_NETWORK_JOINED")){
    RAK3172Dev.Status.IsJoined = 0;
    RAK3172Dev.Status.IsJoinFail = 1;
}
```

IsJoined=0이 되므로 MiLoRa_Control에서 Close. 그런데 RAK3172 모듈의 NJS 레지스터는 변경되지 않음 (MCU가 IsJoined 플래그만 변경, 모듈 내부는 여전히 NJS=1).

다음 Open에서 RAK3172_Open → RAK3172_Join Step 0: AT+NJS=? → **여전히 1** → Join Skip → Open 성공. 같은 패턴 반복.

**이것은 잠재적 무한 루프:**
```
Open → NJS=1 → Skip Join → 전송 → AT_NO_NETWORK_JOINED → Close
→ Open → NJS=1 → Skip Join → 전송 → AT_NO_NETWORK_JOINED → Close
→ ... (반복)
```

이 루프가 빠르게 반복되면 재시도는 멈추지 않지만, **실제 Join을 하지 않으므로** ChirpStack에서는 Join 요청이 보이지 않고, 모든 전송이 실패함.

**하지만:** MiLoRa_Close()에서 `RAK3172_Close()` → `memset(&RAK3172Dev.Status, 0, ...)` → IsJoined=0. 그리고 MiLoRa_Open()에서 전원 OFF→ON (RAK3172 리부트). 리부트 후 NJS는 0이 됨. 따라서 RAK3172_Join() Step 0에서 NJS=0 → Step 1(AT+JOIN) → 실제 Join 시도.

**결론:** 전원 cycle이 정상이면 이 시나리오는 발생하지 않음. 하지만 전원 OFF 시간(500ms)이 부족하여 모듈이 완전 리셋되지 않으면 NJS 잔류 가능.

**심각도:** MEDIUM - 전원 cycle 불완전 시 Join 없는 무한 루프 가능

---

### 추가 원인 #F: RAK3172_Open() InitStep static 변수 잔류 [가능성: 중하]

**문제:**

`RAK3172_Open()`의 `InitStep`은 정상 완료(RESULT_OK) 또는 실패(RESULT_ERROR) 시에만 0으로 리셋됨. `MiLoRa_Close()` 호출이 Open 진행 중에 발생하면:

```c
// MiLoRa_Open() Step 4 → RAK3172_Open() 실행 중
// 외부에서 MiLoRa_Close() 호출 시:
MiLoRa_Close() → MiLoRa_OpenStep = 0
             → RAK3172_Close() → UART DeInit, Status 초기화
// RAK3172_Open()의 InitStep은 그대로 유지!
```

**그러나:** `MiLoRa_Close()`는 Open 진행 중에는 호출되지 않는 구조:
- `MiLoRa_Control()`에서 Close 조건은 `MiLoRa_IsOpen` 기반
- Open 진행 중에는 `MiLoRa_IsOpen=0`이므로 Close 조건에 해당 안 함
- 유일한 예외: `MiLoRa_RadioFailCount > 5 && MiLoRa_IsOpen` → Open 진행 중에는 IsOpen=0이므로 해당 없음

**예외 경로:** `OperatingMode` 변경 시 무조건 Close (line 762-764)
```c
if(MiIoT_Parameter.Operating.OperatingMode != IoTOperatingMode_Operating && ... ){
    MiLoRa_Close();
}
```

이 경우 Open 진행 중에도 Close 가능. InitStep이 잔류하면 다음 Open에서 UART 미초기화 상태로 AT 명령 시도 → 실패 → InitStep=0 리셋 → 다음 시도 정상.

**심각도:** LOW - 1회 Open 실패로 자가 복구됨

---

### 추가 원인 #G: oAT_ReadLine에서 \r\n 파싱 오차 [가능성: 하]

**문제:**

```c
// ONE_ATCommend.c:120-131
pStr = strtok(pBuffer, "\r\n");
if(pStr != NULL && AT_RECEIVE_BUFFER_COUNT(pAT) > 0){
    oAT_ReceiveResponse(pAT, pStr);
    pAT->ReceiveCallback(pStr, strlen(pStr));
    AT_RECEIVE_BUFFER_REMOVE(pAT, strlen(pStr)+2);  // +2 = \r\n 가정
}
```

`strlen(pStr)+2`로 제거하는데, `strtok`은 `\r`, `\n`, `\r\n` 모두를 구분자로 처리. 만약 RAK3172가 `\r\n` 대신 `\n`만 보내면 1바이트만 필요한데 2바이트 제거 → 다음 메시지의 첫 바이트가 유실됨.

**RAK3172 실제 동작:** RUI3 AT 응답은 `\r\n`을 사용. 하지만 `+EVT:` 메시지는 환경에 따라 다를 수 있음.

**심각도:** LOW - RAK3172가 표준 `\r\n` 사용 시 문제 없음. 비표준 줄바꿈 시 간헐적 파싱 오류

---

### 추가 원인 #H: MiLoRa_MailBoxTransmitScheduler Step 1에서 무한 대기 [가능성: 하]

**문제:**

```c
// Mi_LoRa.c:535-538
case 1: // LoRa Open 완료 대기
    if(MiLoRa_IsOpen){
        MiLoRa_ScheduleStep++;
    }
    break;
```

Step 1에서 `MiLoRa_IsOpen`이 1이 될 때까지 무한 대기. 하지만:
- `!MiLoRa_IsOpen` 시 Step 0으로 리셋 (line 517-519)
- Open 성공 시 IsOpen=1 → Step 2 진행
- Open 실패 시 IsOpen=0 유지 → Step 0 리셋

**따라서** 무한 대기는 발생하지 않음. 정상 동작 확인.

**심각도:** NONE

---

## 4. 종합 평가

### 수정 완료된 근본 원인 (기존 분석)

```
[근본 원인] MiLoRa_IsBusy가 MiIoT_IsBusy에 미반영  ← ✅ 수정 완료
     ↓
[연쇄] Sleep 중 LoRa 프로세스 파괴 → 23시간 중단
```

### 추가 원인 위험도 매트릭스

| # | 원인 | 가능성 | 심각도 | 23시간 중단 가능? | 조치 필요? |
|---|------|--------|--------|-----------------|-----------|
| A | RAK3172_Join() static 변수 잔류 | 중 | LOW | ✗ (자가 복구) | 권장 |
| B | SysTick/RTC 보정 오차 누적 | 중하 | LOW | ✗ (누적 오차) | 확인 |
| C | DMA 250ms 수신 대기 지연 | 중 | LOW | ✗ (지연만) | - |
| D | AT+JOIN 타임아웃 불일치 | 중 | MEDIUM | △ (EVT 유실) | 권장 |
| E | NJS 세션 유효성 착각 루프 | 중 | MEDIUM | △ (전원cycle 불완전 시) | 관찰 |
| F | RAK3172_Open InitStep 잔류 | 중하 | LOW | ✗ (자가 복구) | - |
| G | oAT_ReadLine 파싱 오차 | 하 | LOW | ✗ | - |
| H | MailBoxScheduler Step 1 대기 | 하 | NONE | ✗ | - |

### 결론

**기존 IsBusy 수정이 23시간 중단의 근본 원인을 해결함.** 추가 분석에서 발견된 원인들은 대부분 경미하며, 자가 복구되는 수준.

다만, 다음 2가지는 방어적 개선이 권장됨:

1. **원인 #D**: `RAK3172_Join()` 타임아웃을 31초 → 35초로 여유분 확대
2. **원인 #A**: `RAK3172_Close()`에서 Join 관련 static 변수 리셋 방안 검토 (외부 리셋 함수 추가 또는 Close 시 flag으로 리셋)

## 5. 검증 우선순위

1. **[최우선]** IsBusy 수정 후 실기 테스트: 게이트웨이 전원 차단 → Join 실패 유발 → 재시도 패턴 관찰
2. **[관찰]** 시리얼 로그로 Open/Join/Close 사이클 타이밍 확인
3. **[확인]** RTC 클럭 소스 (LSE vs LSI) 확인 → SysTick 보정 오차 범위 결정
4. **[관찰]** NJS 세션 캐시로 인한 Join Skip → 전송 실패 패턴 유무 확인
