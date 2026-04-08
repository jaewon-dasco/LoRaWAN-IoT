# LoRa Join 실패 시 재시도 중단 원인 분석

> **Feature**: lora-join-failure-analysis
> **Phase**: Completed
> **Date**: 2026-03-30
> **Status**: Done

## 1. 현상

- ChirpStack 로그에서 Join 성공 기록 간 **8시간~23시간** 공백 발생
- 마지막 Join: 2026-03-29 10:55:00 → 다음 Join: 2026-03-30 09:32:40 (**약 23시간 공백**)
- 외부 인터럽트로 수동 재시도하면 **즉시 Join 성공**
- ChirpStack 서버 측 차단/밴 메커니즘은 없음 (서버 문제 배제)
- **디바이스 측에서 Join 시도 자체를 중단**한 것으로 확인

## 2. 관련 코드 구조

### 2.1 호출 체인
```
MiIoT() 메인루프 매 사이클:
  ├─ MiLoRa(pUART)              [Mi_LoRa.c:796]
  │   ├─ MiLoRa_Control()       → Open/Close 결정
  │   ├─ MiLoRa_TimeSync()      → 시간 동기화
  │   ├─ MiLoRa_MailBoxTransmitScheduler()
  │   └─ RAK3172()              → AT 명령 처리
  │
  ├─ MiIoT_IsBusy 계산          [Mi_IoT.c:634]
  │   = MiStorage_IsOpen || MiLoRa_IsBusy  ← ✅ 수정 완료
  │   | MiIoT_UpdateMeasurement()
  │   | MiIoT_UpdateSampling()
  │   | MiIoT_UpdateStatus()
  │
  └─ MiIoT_SleepMode()          [Mi_IoT.c:575]
      → !IsBusy && PowerSaveMode → STOP Mode (10초)
```

### 2.2 핵심 플래그
| 플래그 | 의미 | 설정 위치 |
|--------|------|-----------|
| `MiLoRa_IsOpen` | LoRa 모듈 Open 상태 | MiLoRa_Open() 성공 시 |
| `MiLoRa_IsReachable` | 서버 접속 이력 (1회 이상 Join 성공) | Join 성공 시 1, 11회 실패 시 0 |
| `MiLoRa_IsBusy` | LoRa Open/전송 진행 중 | Open Step 0~완료, SendMailbox 전송 중 |
| `MiIoT_IsBusy` | 시스템 Busy (Sleep 방지) | Storage/LoRa/Measurement/Sampling/Status |
| `IsJoined` | 현재 LoRaWAN 연결 상태 | RAK3172 EVT 수신 시 |
| `MiLoRa_IsMailBoxReady` | 전송할 메일 존재 | 메일박스에 데이터 있으면 1 |

## 3. 원인 분석 (가능성 높은 순)

---

### 원인 #1: MiLoRa_IsBusy가 MiIoT_IsBusy에 미반영 → Sleep 중 LoRa 프로세스 파괴 [가능성: 최상]

**근거:**

`MiIoT_IsBusy` 계산에 `MiLoRa_IsBusy`가 누락되어, LoRa Open 상태머신 진행 중에도 MCU가 STOP Mode(10초)에 진입. GPIO DeInit으로 RAK3172 UART/전원이 단절되어 AT 응답과 Join EVT가 유실됨.

---

### 원인 #2: Sleep 진입 시 GPIO DeInit으로 LoRa 전원/UART 단절 [가능성: 상]

**근거:**

Sleep Step 1에서 `MiIoT_GPIODeInitCallback()`이 무조건 실행되어 LoRaEnable GPIO와 UART 핀이 DeInit됨. RAK3172가 예상치 못한 상태에 빠지고, AT 명령 실패 또는 EVT 유실.

---

### 원인 #3: Join EVT 수신 실패 → RAK3172_Join() Step 2 타임아웃 대기 반복 [가능성: 상]

**근거:**

`AT+JOIN` 후 MCU가 STOP mode에 진입하면 RAK3172의 `+EVT:JOIN_FAILED` 수신 불가. 31초 타임아웃까지 대기 반복으로 사이클이 크게 늘어남.

---

### 원인 #4: Open 상태머신 Stuck (RESULT_RUN 무한 반환) [가능성: 중]

**근거:**

UART 응답 유실로 Step 4/5에서 `RESULT_RUN`을 무한 반환할 수 있음. 전체 타임아웃이 없어 외부 인터럽트 없이는 복구 불가. 이것이 23시간 공백의 직접 원인으로 추정.

---

### 원인 #5: SysTick 보정 오류로 타이머 동작 이상 [가능성: 하]

**근거:** RTC 기반이 아닌 고정값 보정 → 누적 오차 가능. 하지만 시간~일 단위 중단 설명하기 어려움.

---

## 4. 종합 결론 (Root Cause Chain)

```
[근본 원인] MiLoRa_IsBusy가 MiIoT_IsBusy에 미반영
     ↓
[연쇄 반응] LoRa Open 진행 중 MCU가 STOP Mode 진입
     ↓
[연쇄 반응] GPIO DeInit → LoRa UART/전원 단절
     ↓
[연쇄 반응] RAK3172 EVT 유실 → AT 응답 유실
     ↓
[직접 원인] Open 상태머신이 특정 Step에서 Stuck (RESULT_RUN 무한)
     ↓
[현상] Join 재시도가 멈추고, 외부 인터럽트 전까지 복구 불가
```

## 5. 수정 방안 및 적용 현황

### 5.1 [필수] MiLoRa_IsBusy를 MiIoT_IsBusy에 반영 ✅ 완료

```c
// Mi_IoT.c:634
MiIoT_IsBusy = MiStorage_IsOpen || MiLoRa_IsBusy;
```

### 5.2 [필수] MiLoRa_IsBusy 설정 시점 개선 ✅ 완료

- Open Step 0 진입 시 `MiLoRa_IsBusy = 1` (기존 Step 1 → Step 0으로 앞당김)
- 메일 전송 시 `MiLoRa_IsBusy = pMail->IsBusy` 반영 추가

```
MiLoRa_IsBusy 생명주기:
  = 1  : MiLoRa_Open() Step 0 진입 시          [Mi_LoRa.c:624]
  = 0  : MiLoRa_Open() 완료/실패 시            [Mi_LoRa.c:709]
  = 0  : MiLoRa_Close() 시                    [Mi_LoRa.c:584]
  = pMail->IsBusy : MiLoRa_SendMailbox() 시   [Mi_LoRa.c:394]
```

### 5.3 Open 상태머신에 전체 타임아웃 → 불필요 (적용 안 함)
- 각 AT 명령에 개별 타임아웃/재시도가 이미 구현되어 있음
- 5.1 수정으로 Sleep 중 UART 유실 문제 해결 → Stuck 발생 원인 제거

### 5.4 절전 모드 재접속 로직 → 설계 의도와 충돌 (적용 안 함)
- 절전 모드 설계: 메일 없으면 접속 안 함 (전력 절약)
- 현재 동작이 설계 의도대로 정상

## 6. 시퀀스 검증 결과 (수정 후)

| 시나리오 | 상태 | 비고 |
|----------|------|------|
| Open 진행 중 Sleep | **해결됨** ✅ | IsBusy=1로 Sleep 차단 |
| Open 실패 후 재시도 | **정상** ✅ | 1회 Sleep(10초) 후 재시도 |
| 메일 전송 중 Sleep | **해결됨** ✅ | pMail->IsBusy 반영 |
| Open 상태 + 메일 없음 → Sleep | **경미한 위험** ⚠️ | Close(1초) 전 Sleep(5ms) 가능, 치명적이진 않음 |

## 7. 검증 방법

1. 시리얼 로그에 Sleep 진입/Wake-up 로그 추가하여 Sleep 타이밍 확인
2. `MiLoRa_OpenStep` 값을 주기적으로 로그 출력하여 Stuck 여부 확인
3. 수정 적용 후 의도적으로 게이트웨이 전원 차단 → Join 실패 유발 → 재시도 패턴 관찰
