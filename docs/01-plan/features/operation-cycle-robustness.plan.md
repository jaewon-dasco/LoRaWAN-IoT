# Plan: 동작 사이클 전체 견고성 검토

## 개요

| 항목 | 값 |
|------|-----|
| Feature | operation-cycle-robustness |
| 작성일 | 2026-03-11 |
| 대상 | SIC100 HW2.2 FW3.0 (STM32L433RBT6 + RAK3172) |
| 범위 | 측정 → LoRa 전송 → Sleep → Wakeup 전체 사이클 |

## 동작 사이클 흐름

```
MiMain() → MiStorage() → MiIoT() → MiSerial()
                           │
                           ├─ MiLoRa()          [LoRa 통신]
                           ├─ UpdateMeasurement  [주기 측정]
                           ├─ UpdateSampling     [수동 샘플링]
                           ├─ UpdateStatus       [상태 업데이트]
                           ├─ Sleep()            [저전력 모드]
                           └─ LEDIndicator       [표시]
```

**Sleep 진입 조건** (Mi_IoT.c:681):
```c
!MiIoT_IsIORun && !MiIoT_IsBusy && MiIoT_IsPowerSaveMode && !MiIoT_IsPause && oTMR_GetTick() > 3000
```

---

## 1단계: 측정 (Measurement)

### 검토 결과: SAFE

| 항목 | 분석 | 상태 |
|------|------|:----:|
| CAN 통신 타임아웃 | 콜백 기반, MiIoT_DT 타이머로 상위 제어 | OK |
| 에러 카운트 제한 | MEASURE_CAN_ERROR_MAXCOUNT=3, RETRY_MAXCOUNT=5 | OK |
| 무한루프 가능성 | 모든 상태머신 bounded, 에러시 step 리셋 | OK |
| IWDG 충돌 | 최대 블로킹: ADC PollForConversion 100ms (IWDG 11s 이내) | OK |

**안전 근거**: Measurement 상태머신은 에러 카운트로 탈출 보장. CAN 응답 실패시 최대 3×5=15회 시도 후 종료.

---

## 2단계: LoRa 전송 (Transmission)

### 2-1. RAK3172_Transmit

| 항목 | 분석 | 상태 |
|------|------|:----:|
| AT 커맨드 타임아웃 | AT+SEND 10초, AT+CFM 3초, AT+TIMEREQ 3초 | OK |
| TX_DONE 대기 | Step 5: 10초 타임아웃 | OK |
| 스택 사용 | StrBuffer[2048] 로컬 (26KB 중 2KB) | OK |
| 무한루프 가능성 | 모든 case에 타임아웃/에러 탈출 존재 | OK |

### 2-2. MiLoRa_SendMailbox

| 항목 | 분석 | 상태 |
|------|------|:----:|
| 시퀀스 전송 | Encode 결과로 루프 제어, SqcCount 증가 | OK |
| 에러 재시도 | QoS 횟수만큼 재시도 후 다음 시퀀스 | OK |
| Fault 처리 | SendFaultCount ≥ 10 → RESULT_FAULT | OK |

### 2-3. MailBox 스케줄러 — **P3 미적용 (CRITICAL)**

| 항목 | 분석 | 상태 |
|------|------|:----:|
| RESULT_OK | 메일 제거 → step 0 복귀 | OK |
| RESULT_FAULT | MiLoRa_Close() → 재부팅 | OK |
| RESULT_PAUSE/RUN | 대기 (정상) | OK |
| **RESULT_ERROR** | **default: break → 메일 미제거!** | **FAIL** |

**P3 문제 상세** (Mi_LoRa.c:492-493):
```c
default:
    break;  // ← RESULT_ERROR/RESULT_NULL 시 메일이 영구히 남음
```

**영향**:
- Encode 실패시 해당 메일이 Mailbox에 영구 잔류
- 매 스케줄 사이클마다 동일 메일 재시도 (무한 반복)
- 다른 정상 메일 전송 차단 (GetItem이 항상 같은 메일 반환)
- **LoRa 전송 중단의 직접적 원인 가능**

**수정안**:
```c
default:
    MiLoRa_ScheduleStep = 0;
    MiIoT_MailBox_Remove(&MiIoT_LoRaMailBox, pMail);
    oSerial_Log("MiLoRa | Mailbox removed (error)");
    break;
```

### 2-4. MiLoRa_Open/Close

| 항목 | 분석 | 상태 |
|------|------|:----:|
| Open 상태머신 | LoRaEnable GPIO + 500ms 대기, 단계별 진행 | OK |
| Close 시 step 리셋 | OpenStep=0, IsOpen=false | OK |
| RadioFailCount | 누적 실패시 AutoOffDelay 후 Close→재Open | OK |
| Join 재시도 | JoinMaxRetry 제한, 타임아웃 존재 | OK |

---

## 3단계: Sleep 모드

### 3-1. Sleep 진입

| 항목 | 분석 | 상태 |
|------|------|:----:|
| P4 부팅 가드 | oTMR_GetTick() > 3000 (3초) | OK |
| IsBusy 판정 | MiStorage_IsOpen 초기값 + 측정/샘플링/상태 | OK |
| IsIORun 판정 | GPIO.DO 비트 OR USB 연결 | OK |
| IsPause | MiIoT() 끝에서 0으로 리셋 | OK |
| Blink 애니메이션 | case 3: ~6.4초 후 다음 슬립 (bounded) | OK |
| SleepCallback | NULL → 즉시 step 0 복귀 (현재 NULL) | OK |

### 3-2. Sleep 진입 차단 시나리오

| 시나리오 | 가능성 | 영향 |
|----------|--------|------|
| MiStorage_IsOpen 미클리어 | 낮음 (close시 0 설정 확인) | Sleep 차단 |
| GPIO.DO 핀 고착 | HW 결함시만 | Sleep 차단 |
| USB 상시 연결 | 정상 동작 (충전 중 깨어있음) | 정상 |
| MiIoT_IsPowerSaveMode=0 | 설정값, OperatingMode에 따라 | 정상 |

### 3-3. Native_SleepMode (STOP 모드)

| 항목 | 분석 | 상태 |
|------|------|:----:|
| P2 최소 Sleep | Millisecond=0 → 10초 강제 (IWDG 보호) | OK |
| RTC 깨우기 | LPTIM/RTC 타이머 기반, 2048Hz ÷ DIV16 | OK |
| EXTI 깨우기 | USB 핀 EXTI Rising 설정 | OK |
| GPIO DeInit | Sleep 전 Analog 모드 전환 (누설 방지) | OK |
| GPIO ReInit | Wakeup 후 GPIO 복원 콜백 | OK |

### 3-4. Wakeup 실패 가능성

| 시나리오 | 가능성 | 대응 |
|----------|--------|------|
| RTC 클럭 정지 | 극히 낮음 (LSE 크리스탈) | IWDG가 LSI로 리셋 |
| IWDG 리셋 루프 | P2 수정으로 차단 (10초 wakeup) | OK |
| GPIO 미복원 | 콜백 NULL 방어 (line 692, 701) | OK |

---

## 4단계: 워치독 (IWDG)

### 검토 결과: SAFE

| 항목 | 분석 | 상태 |
|------|------|:----:|
| 타임아웃 | Prescaler 256, ~11초 | OK |
| 리프레시 | MiMain 메인루프 매 반복 | OK |
| STOP 모드 | LSI 클럭 유지, STOP 중에도 동작 | OK |
| HardFault | while(1) → IWDG 리셋 → P4로 정상 부팅 | OK |

**최대 블로킹 구간**: Native_SleepMode(10000ms) = 10초 → IWDG 11초 이내

---

## 5단계: 타이머 시스템

### 검토 결과: SAFE

| 항목 | 분석 | 상태 |
|------|------|:----:|
| 32비트 롤오버 | oTMR_Interval()에서 정상 처리 | OK |
| TMR_TICK_MAXVALUE | 0xFFFFFFFF 스킵 (센티넬 값) | OK |
| DWT 카운터 | CoreClock 기반 μs/ms, 오버플로 처리 | OK |
| 49.7일 롤오버 | oTMR_Interval이 언더플로 보정 | OK |

---

## 종합 판정

### 발견된 문제

| # | 심각도 | 항목 | 파일 | 상태 |
|---|--------|------|------|:----:|
| **P3** | **CRITICAL** | Mailbox 실패 메일 미제거 | Mi_LoRa.c:492 | **미적용** |

### 안전 확인 항목

| # | 항목 | 근거 |
|---|------|------|
| 1 | 측정 루틴 | 에러 카운트 제한, 상태머신 bounded |
| 2 | LoRa 전송 | AT 타임아웃, Fault 카운트, QoS 재시도 제한 |
| 3 | Sleep 진입 | P4 부팅 가드, 5조건 AND, Blink bounded |
| 4 | Wakeup | P2 최소 10초, RTC+EXTI, IWDG 백업 |
| 5 | 워치독 | 11초 타임아웃, STOP 중 동작, 모든 블로킹 < 11초 |
| 6 | 타이머 | 롤오버 안전, 센티넬 스킵 |

---

## 수정 계획

### P3 수정 (즉시 적용)

**파일**: `Core/Src/Mi_LoRa.c` line 492-493

**Before**:
```c
default:
    break;
```

**After**:
```c
default:
    MiLoRa_ScheduleStep = 0;
    MiIoT_MailBox_Remove(&MiIoT_LoRaMailBox, pMail);
    oSerial_Log("MiLoRa | Mailbox removed (error)");
    break;
```

**효과**: Encode 실패/NULL 메일 즉시 제거 → Mailbox 정체 방지 → 정상 메일 전송 보장

---

## 결론

- **P3 수정이 유일한 미적용 Critical Fix** — 즉시 적용 필요
- P1(정적 버퍼), P2(최소 Sleep), P4(부팅 가드)는 정상 적용됨
- 측정/전송/Sleep/Wakeup 전체 경로에서 **P3 외 추가 위험 없음**
- 48시간 이상 연속 동작 검증 테스트 권장
