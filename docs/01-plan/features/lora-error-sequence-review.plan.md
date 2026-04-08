# Plan: LoRa 통신 오류 시퀀스 검토

> Feature: lora-error-sequence-review
> Created: 2026-02-19
> Status: Plan

---

## 1. 목적

LoRa(RAK3172) 통신에서 **연결 실패** 또는 **데이터 전송 오류** 발생 시의 시퀀스를 검토하여 잠재적 문제점을 식별하고 개선안을 도출한다.

---

## 2. 검토 대상 파일

| 파일 | 역할 |
|------|------|
| `Core/Src/Mi_LoRa.c` (674줄) | LoRa 미들웨어 - Open/Join/Send/Control/TimeSync |
| `oThirdParty/Src/LoRa_RAK3172.c` (1179줄) | RAK3172 AT 드라이버 - Open/Join/Transmit/이벤트 처리 |
| `Core/Inc/Mi_LoRa.h` | 상태 변수, 매크로 정의 |
| `oThirdParty/Inc/LoRa_RAK3172.h` | RAK3172 디바이스 구조체, 상태 비트필드 |

---

## 3. 시퀀스 분석 - 연결 실패 (Join Failure)

### 3.1 정상 시퀀스

```
MiLoRa_Control() → MiLoRa_Open()
  Step 0: ProductCode 확인, 백오프 대기
  Step 1: GPIO OFF, RAK3172_Close()
  Step 2: 500ms 대기
  Step 3: GPIO ON (전원 인가), 500ms 대기
  Step 4: RAK3172_Open() → AT초기화 10단계
  Step 5: RAK3172_Join() → OTAA Join 시도
  → RESULT_OK: IsOpen=1, IsConnected=1
```

### 3.2 실패 시퀀스 흐름 분석

#### (A) RAK3172_Open() 실패 (AT 통신 불가)

```
Step 4: RAK3172_Open() → RESULT_ERROR
  ↓
MiLoRa_Open() → result = RESULT_ERROR
  ↓
OpenFailTimestamp 기록, OpenFailCount++
  ↓
GPIO OFF, RAK3172_Close()
  ↓
다음 호출 시 MILORA_OPEN_REMAINING_TIME (5초 백오프) 대기 후 재시도
```

**발견된 문제점:**

1. **[P1] 백오프 시간이 고정 5초로 변경됨**
   - [Mi_LoRa.h:18](Core/Inc/Mi_LoRa.h#L18): 기존 점진적 백오프 `5s * (count-1), cap 15s`가 주석처리되고 고정 5초로 변경
   - OpenFailCount가 계속 증가해도 백오프가 5초로 동일 → 네트워크 없는 환경에서 불필요한 전력 소비
   - **위험도: 중간** - 배터리 소모 가점

2. **[P2] RAK3172_Open() ATR 리셋 실패 시 RESULT_ERROR 반환**
   - [LoRa_RAK3172.c:1070](oThirdParty/Src/LoRa_RAK3172.c#L1070): ATR 명령 실패 시 바로 ERROR로 빠짐
   - 리셋 명령은 모듈이 재부팅하면서 응답이 불안정할 수 있음
   - ATR 실패를 무시하고 진행하는 것이 더 안전할 수 있음

3. **[P3] RAK3172_Open()에서 InitStep이 static인데, 실패 시 0으로 리셋됨 → 매번 전체 초기화 반복**
   - 이미 성공한 단계(DevEUI 읽기 등)를 캐시하고 있어 일부 최적화는 있으나, Config 설정은 매번 재실행

#### (B) RAK3172_Join() 실패

```
Step 5: RAK3172_Join()
  ↓
  AT+JOIN=1:0:10:6 전송 (10초 간격, 6회 시도)
  ↓
  1초 간격으로 AT+NJS=? 폴링
  ↓
  +EVT:JOIN_FAILED 수신 → IsJoinFail=1
  ↓
RESULT_ERROR 반환 → MiLoRa_Open()
  ↓
JoinTryCount++ → OpenFailCount++
  ↓
OpenFailCount > 10 → IsConnected = 0
  ↓
PowerSaveMode에서 IsConnected=0이면 → 더 이상 Open 시도 안 함!
```

**발견된 문제점:**

4. **[P4] IsConnected=0 이후 복구 경로 부재**
   - [Mi_LoRa.c:649](Core/Src/Mi_LoRa.c#L649): `!MiLoRa_IsConnected` 일 때 `MiLoRa_Open()` 호출하지만...
   - [Mi_LoRa.c:613](Core/Src/Mi_LoRa.c#L613): OpenFailCount > 10이면 IsConnected = 0
   - 코드를 다시 보면, PowerSaveMode에서 `!MiLoRa_IsOpen` 일 때:
     - `!MiLoRa_IsConnected` → `MiLoRa_Open()` 호출 (재시도함)
     - `MiLoRa_IsMailBoxReady` → `MiLoRa_Open()` 호출
   - **실제로는 IsConnected=0이어도 Open을 계속 시도하므로 문제없음** ✅
   - 다만, OpenFailCount가 계속 누적되어 매번 IsConnected=0이 반복되는 구조

5. **[P5] Join 타임아웃 계산의 정확성**
   - [LoRa_RAK3172.c:522](oThirdParty/Src/LoRa_RAK3172.c#L522): `JoinInterval * JoinAttempts + 1`초 = 10*6+1 = 61초
   - RAK3172 모듈 내부에서 10초 간격으로 6회 시도 → 실제 소요시간 ~60초
   - 타임아웃이 충분하므로 정상적 ✅

6. **[P6] JoinTryCount와 OpenFailCount의 이중 카운트**
   - [Mi_LoRa.c:580-585](Core/Src/Mi_LoRa.c#L580-L585): Join 실패 시 JoinTryCount++
   - [Mi_LoRa.c:611](Core/Src/Mi_LoRa.c#L611): OpenFailCount++
   - JoinTryCount는 지역변수(static)인데 실질적으로 사용되지 않음 → 불필요한 변수
   - **위험도: 낮음** - 기능에 영향 없으나 코드 정리 필요

---

## 4. 시퀀스 분석 - 데이터 전송 오류 (Transmit Failure)

### 4.1 정상 전송 시퀀스

```
MiLoRa_MailBoxTransmitScheduler()
  → IoT_MailBox_Sort() → 우선순위 정렬
  → MiIoT_MailBox_GetItem() → 다음 메일 가져오기
  → MiLoRa_SendMailbox(pMail)
    Step 0: 플래그 초기화
    Step 1: MiLoRa_Encode() → 51바이트 이하로 분할
    Step 2: 5초 간격 대기 (시퀀스 간)
    Step 3: RAK3172_Transmit()
      Step 0: IsBusy 확인
      Step 1: IsJoined 확인
      Step 2: AT+CFM 설정
      Step 3: AT+TIMEREQ (옵션)
      Step 4: AT+SEND 전송
      Step 5: TX_DONE/SEND_CONFIRMED_OK 대기
    → RESULT_OK: 다음 시퀀스로
  → 모든 시퀀스 완료 → RESULT_OK
```

### 4.2 실패 시퀀스 흐름 분석

#### (C) Confirmed 전송 실패 (SEND_CONFIRMED_FAILED)

```
RAK3172_Transmit() Step 5:
  +EVT:SEND_CONFIRMED_FAILED → IsSendFailed=1
  ↓
RESULT_ERROR → MiLoRa_SendMailbox() Step 3
  ↓
RadioFailCount++ (누적)
SendErrorCount++ → (SendErrorCount % 5) == 0 이면 다음 시퀀스로 강제 이동
  ↓
RadioFailCount > 5 → MiLoRa_Control()에서 MiLoRa_Close() 호출
  ↓
모듈 전원 OFF → 다시 Open 시도
```

**발견된 문제점:**

7. **[P7] 주석과 코드 불일치 - "3번" vs 실제 5번**
   - [Mi_LoRa.c:269-271](Core/Src/Mi_LoRa.c#L269-L271): 주석은 "3번 전송했으면" 이지만 실제 코드는 `(++SendErrorCount % 5) == 0` → 5회
   - **위험도: 낮음** - 동작은 정상이나 주석 수정 필요

8. **[P8] 전송 실패 시 메일박스에서 메일이 제거되지 않음**
   - [Mi_LoRa.c:442-456](Core/Src/Mi_LoRa.c#L442-L456): MailBoxTransmitScheduler에서:
     - `RESULT_OK` → 메일 제거 ✅
     - `RESULT_FAULT` → Close() 호출하여 재부팅
     - `RESULT_ERROR/TIMEOUT` → **default에 빠져서 아무 처리 없음!**
   - SendMailbox 자체가 RESULT_ERROR를 직접 반환하지 않음 (내부에서 에러를 5회까지 자체 재시도)
   - **하지만** RESULT_PAUSE가 계속 반환되면 무한 대기 상태 가능
   - `pMail->IsError=1`로 설정되지만, Scheduler가 이를 체크하지 않음
   - **위험도: 중간** - 영구적으로 전송 불가 메일이 큐에 남을 수 있음

9. **[P9] SendMailbox에서 RESULT_FAULT 경로 분석**
   - [Mi_LoRa.c:276](Core/Src/Mi_LoRa.c#L276): `default` 케이스에서 SendFaultCount >= 10이면 RESULT_FAULT
   - RAK3172_Transmit()에서 RESULT_FAULT를 반환하는 경우:
     - IsJoined가 아닐 때 (Step 1)
     - AT+CFM 명령 실패 (Step 2)
     - AT+TIMEREQ 명령 실패 (Step 3)
     - AT+SEND 명령 실패 (Step 4)
     - HexString 변환 실패 (Step 4)
   - **10회 연속 FAULT 후 Close() → 정상적인 복구 경로** ✅

10. **[P10] AT+TIMEREQ 실패 시 IsJoined=0으로 강제 설정**
    - [LoRa_RAK3172.c:406](oThirdParty/Src/LoRa_RAK3172.c#L406): `RAK3172Dev.Status.IsJoined = 0`
    - TimeSync 요청 실패로 인해 Join 상태가 리셋됨 → 이후 Transmit에서 RESULT_FAULT 반환
    - **위험도: 높음** - TimeSync 실패만으로 전체 연결이 끊어지는 부적절한 동작
    - 서버에서 TIMEREQ를 지원하지 않는 경우 연속적으로 발생 가능

#### (D) 전송 타임아웃 (10초 대기 후 응답 없음)

```
RAK3172_Transmit() Step 5:
  10초 대기 → IsTxDone=0, IsSendComformed=0, IsSendFailed=0
  ↓
RESULT_TIMEOUT → MiLoRa_SendMailbox() Step 3
  ↓
RadioFailCount++, SendErrorCount++
  ↓
동일 시퀀스 재시도 (5초 후)
  ↓
5회 실패 → 다음 시퀀스로 강제 이동
  ↓
RadioFailCount > 5 → 모듈 리셋
```

**발견된 문제점:**

11. **[P11] 전송 타임아웃 후 모듈 상태 불일치 가능성**
    - 타임아웃 발생 시 RAK3172 모듈은 아직 송신 중일 수 있음
    - 다음 AT+SEND 명령 전송 시 모듈이 busy 상태 → "AT_BUSY_ERROR" 응답 가능
    - AT 레이어에서 이 응답을 별도 처리하지 않음
    - **위험도: 중간** - 연속 전송 실패의 원인이 될 수 있음

#### (E) MiLoRa_SendMailbox()에서 IsJoined 해제 시

```
MiLoRa_SendMailbox() 진입:
  !pLoRaDevice->Status.IsJoined || !MiLoRa_IsOpen
  ↓
RESULT_PAUSE 반환
  ↓
MailBoxTransmitScheduler의 case 2에서:
  RESULT_PAUSE → break (아무 처리 없음)
  ↓
다음 루프에서 다시 SendMailbox 호출 → 다시 PAUSE
  ↓
무한 반복 (메일이 큐에서 영원히 빠지지 않음)
```

**발견된 문제점:**

12. **[P12] RESULT_PAUSE 무한 루프**
    - [Mi_LoRa.c:443-444](Core/Src/Mi_LoRa.c#L443-L444): Scheduler에서 PAUSE를 받으면 계속 재시도
    - MiLoRa_Control()에서 Close()가 호출되면 IsOpen=0이 되어 Scheduler Step이 0으로 리셋됨
    - 하지만 **Join은 되어있으나 일시적으로 IsJoined가 풀린 경우** PAUSE가 지속됨
    - 실제로 P10에서 TimeSync 실패로 IsJoined=0이 되면 이 상황 발생
    - **위험도: 높음** - P10과 결합하면 전송 완전 중단 가능

---

## 5. 시퀀스 분석 - RadioFault 리셋 경로

```
MiLoRa_Control():
  RadioFailCount > 5 → MiLoRa_Close()
  ↓
MiLoRa_Close():
  RAK3172_Close() → AT 종료, Status 초기화
  GPIO OFF → 전원 차단
  RadioFailCount = 0, IsOpen = 0
  ↓
다음 루프에서 MiLoRa_Open() → 전체 재초기화
```

**추가 발견사항:**

13. **[P13] RadioFailCount 리셋 시점**
    - [Mi_LoRa.c:478](Core/Src/Mi_LoRa.c#L478): Close()에서 RadioFailCount = 0
    - [Mi_LoRa.c:262](Core/Src/Mi_LoRa.c#L262): 전송 성공 시에도 RadioFailCount = 0
    - RadioFailCount가 5를 초과할 때까지 성공이 한 번도 없어야 Close 발생 → 적절한 설계 ✅

---

## 6. 시퀀스 분석 - 전원 관리와 오류의 상호작용

### PowerSave 모드에서의 오류 시퀀스

```
MiLoRa_Control() [PowerSaveMode]:

케이스 1: IsOpen=1, !IsJoined
  → MiLoRa_Close() (즉시 종료)
  → 다음 루프에서 IsConnected에 따라 재시도 결정

케이스 2: IsOpen=1, IsJoined, 메일박스 비어있음, 1초 경과
  → MiLoRa_Close() (절전)
  → 메일 도착 시 다시 Open

케이스 3: IsOpen=0, IsConnected=0
  → MiLoRa_Open() (재시도)
  → 연속 실패 시 5초 백오프로 계속 시도

케이스 4: IsOpen=0, IsConnected=1, 메일 있음
  → MiLoRa_Open() (전송을 위해)

케이스 5: IsOpen=0, IsConnected=1, 메일 없음
  → 아무 것도 안 함 (절전 유지) ✅
```

**발견된 문제점:**

14. **[P14] PowerSaveMode에서 Join 실패 → 즉시 Close → 재Open → Join 재시도의 빠른 사이클**
    - Join은 61초 소요 → 실패 → Close → 5초 백오프 → Open(~3초) → Join(61초) → 실패 → ...
    - 약 69초 주기로 계속 반복 → 배터리 소모
    - IsConnected=0이 되어도 계속 시도 (케이스 3)
    - **위험도: 중간** - 장기간 네트워크 부재 시 배터리 고갈

---

## 7. 발견된 문제점 요약 및 우선순위

| ID | 문제 | 위험도 | 영향 | 개선 필요 |
|----|------|--------|------|-----------|
| **P10** | AT+TIMEREQ 실패 시 IsJoined=0 강제 설정 | **높음** | 연결 끊김 | **즉시** |
| **P12** | RESULT_PAUSE 무한 루프 (P10 결합) | **높음** | 전송 중단 | **즉시** |
| **P8** | 전송 실패 메일이 큐에 영구 잔류 가능 | 중간 | 큐 점유 | 검토 |
| **P11** | 타임아웃 후 모듈 busy 상태 미처리 | 중간 | 연속 실패 | 검토 |
| **P1** | 백오프 고정 5초 (점진적 백오프 비활성) | 중간 | 전력 낭비 | 선택 |
| **P14** | PowerSave에서 Join 실패 반복 사이클 | 중간 | 배터리 | 선택 |
| **P2** | ATR 리셋 실패 시 즉시 ERROR | 낮음 | 초기화 | 선택 |
| **P7** | 주석 "3번" vs 코드 5번 불일치 | 낮음 | 가독성 | 선택 |
| **P6** | JoinTryCount 미사용 변수 | 낮음 | 정리 | 선택 |

---

## 8. 개선 제안

### 8.1 [즉시] P10 수정 - AT+TIMEREQ 실패 시 IsJoined 유지

**파일:** `oThirdParty/Src/LoRa_RAK3172.c` 406행

```c
// Before (문제)
RAK3172Dev.Status.IsJoined = 0;
result = RESULT_FAULT;

// After (개선)
// TimeSync 실패는 Join 상태와 무관하므로 IsJoined 유지
RAK3172Dev.Status.IsTimeSyncEnabled = 0;
result = RESULT_FAULT;  // 또는 RESULT_ERROR로 변경하여 재시도 허용
```

### 8.2 [즉시] P12 수정 - PAUSE 상태 타임아웃 추가

**파일:** `Core/Src/Mi_LoRa.c` MiLoRa_MailBoxTransmitScheduler()

```c
// Scheduler case 2에서 PAUSE 타임아웃 처리 추가
case RESULT_PAUSE:
    // 일정 시간 PAUSE 지속 시 메일 스킵 또는 에러 처리
    break;
```

### 8.3 [검토] P8 수정 - 전송 실패 메일 처리 정책

- SendMailbox가 모든 시퀀스를 시도한 후 에러로 종료하면 → Scheduler에서 메일 제거
- 또는 재시도 카운터 추가하여 N회 실패 시 메일 폐기

### 8.4 [선택] P1 수정 - 점진적 백오프 복원

**파일:** `Core/Inc/Mi_LoRa.h` 17-18행

```c
// 기존 점진적 백오프 복원 (5초 ~ 15초)
#define MILORA_OPEN_REMAINING_TIME \
    ((MiLoRa_OpenFailTimestamp == 0 || MiLoRa_OpenFailCount == 0) ? 0 : \
    oTMR_CountDown(&MiLoRa_OpenFailTimestamp, \
    MATH_LIMIT(SECOND_TO_MS(5) * (MiLoRa_OpenFailCount-1), 0, SECOND_TO_MS(15)), \
    TICKBASE_SYSTICK))
```

---

## 9. 전체 오류 시퀀스 다이어그램

```
┌─────────────────────────────────────────────────────────────────────┐
│                    MiLoRa() Main Loop                               │
│  ┌──────────────┐  ┌──────────────┐  ┌────────────────────────┐    │
│  │MiLoRa_Control│→│MiLoRa_TimeSync│→│MiLoRa_MailBoxScheduler │    │
│  └──────┬───────┘  └──────────────┘  └───────────┬────────────┘    │
│         │                                         │                 │
│    ┌────▼────┐                              ┌─────▼─────┐          │
│    │Open/Close│                              │SendMailbox│          │
│    └────┬────┘                              └─────┬─────┘          │
│         │                                         │                 │
│  ┌──────▼──────┐                          ┌───────▼───────┐        │
│  │RAK3172_Open │                          │RAK3172_Transmit│        │
│  │ (10 Steps)  │                          │  (6 Steps)     │        │
│  └──────┬──────┘                          └───────┬───────┘        │
│         │                                         │                 │
│  ┌──────▼──────┐                                  │                 │
│  │RAK3172_Join │                                  │                 │
│  │ (3 Steps)   │                                  │                 │
│  └─────────────┘                                  │                 │
│                                                   │                 │
│  ═══════ 오류 복구 경로 ═══════                    │                 │
│                                                   │                 │
│  OpenFail→5s백오프→재Open     RadioFail>5→Close→재Open              │
│  JoinFail→OpenFailCount++     SendError→5회후 Skip                  │
│  FailCount>10→IsConnected=0   Fault×10→Close                       │
│  ★P10:TIMEREQ실패→IsJoined=0  ★P12:PAUSE무한루프                    │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 10. 다음 단계

1. **P10, P12 수정 코드 작성** → Design 문서로 상세 설계
2. **P8, P11 추가 분석** → AT 레이어 응답 처리 확인
3. **수정 후 테스트 시나리오 작성** → 각 오류 경로 재현 테스트
