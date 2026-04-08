# Plan: LoRa Code Optimization

## Feature: lora-optimization
## Date: 2026-03-06
## Status: In Progress
## Last Updated: 2026-03-06

---

## Overview

LoRa(RAK3172) 드라이버 및 애플리케이션 코드의 버그 수정, 성능 개선, 저전력 최적화를 수행한다.

## Priority Order

최우선순위는 **버그 수정**, 그 다음 성능, 저전력 순으로 진행한다.

| 우선순위 | 카테고리 | 상태 |
|----------|----------|------|
| 1 (최우선) | Phase 1: Bug Fix | ✅ 완료 |
| 2 | Phase 2: Performance | 1/3 대기 |
| 3 | Phase 3: Low Power | 1/6 대기 |

---

## Phase 1: Bug Fix (Critical) ✅ 완료

### 1-1. LINKCHECK 파싱 구분자 오류 ✅
- **파일**: `oThirdParty/Src/LoRa_RAK3172.c:83-107`
- **수정**: `strtok` 구분자를 `":"` → `","` 로 변경

### 1-2. GetSignalQuality break 누락 ✅
- **파일**: `oThirdParty/Src/LoRa_RAK3172.c:273-278`
- **수정**: `case AT_RESULT_OK:` 에 `break` 추가 (RSSI, SNR 모두)

### 1-3. SendMailbox 실패 시 mailbox 미제거 ✅
- **파일**: `Core/Src/Mi_LoRa.c:459-462`
- **수정**: RESULT_NULL (encode 실패) 시 mail 제거하도록 수정

---

## Phase 2: Performance (1/3 대기)

### 2-1. Transmit StrBuffer 스택 최적화
- **파일**: `oThirdParty/Src/LoRa_RAK3172.c:345`
- **문제**: `char StrBuffer[2048] = {0,}` 매 호출마다 스택에 2KB + memset
- **수정**: `static` 변경 또는 TxBuffer 재활용
- **상태**: ⏳ 대기

### 2-2. AT+CFM 매 전송마다 설정 ✅
- **파일**: `oThirdParty/Src/LoRa_RAK3172.c:369-383`
- **문제**: RAK3172가 전송 후 CFM을 자동 리셋하므로 매번 AT+CFM 전송 필요
- **수정**: Config.CFM 필드 제거, 매 전송 시 AT+CFM 전송 유지 (모듈 동작 특성상 필수)

### 2-3. IsChangedConfig() 활성화 ✅
- **파일**: `oThirdParty/Src/LoRa_RAK3172.c:742-755`
- **문제**: 주석처리되어 매 Open마다 10개 AT 명령 무조건 전송
- **수정**: 주석 해제, 변경된 설정만 전송 + skip/start 로그 추가

---

## Phase 3: Low Power (1/6 대기)

### 3-1. PowerSave 모드: LPM → 3초 대기 → PowerOff ✅
- **파일**: `oThirdParty/Src/LoRa_RAK3172.c`, `Core/Src/Mi_LoRa.c`
- **문제**: mailbox 비면 즉시 전원 OFF → 재 join 필요 (30초+)
- **수정**:
  - `RAK3172_Sleep()` / `RAK3172_Wakeup()` 함수 추가
  - AT+LPM=1로 Low Power Mode 진입 (Join 유지, UART wakeup 가능)
  - AT+LPMLVL=1 (Stop1) 설정으로 UART wakeup 지원
  - AT+LPM=0으로 Wakeup
  - Sleep 후 **3초** 경과 + !MiIoT_IsBusy 시 PowerOff, mailbox 들어오면 즉시 Wakeup
- **참고**: AT+SLEEP은 시간 기반으로만 동작하며 UART wakeup 불가 → AT+LPM 방식 채택
- **참고**: Class A에서 LPM 중 downlink 수신 불가 (RX window는 TX 후에만 열림)

### 3-2. 전원 ON 후 대기 시간 증가
- **파일**: `Core/Src/Mi_LoRa.c:547`
- **문제**: 500ms로 RAK3172 부팅에 부족할 수 있음
- **수정**: 1000~1500ms로 증가
- **상태**: ⏳ 대기

### 3-3. RAM 버퍼 크기 축소 ✅
- **파일**: `oThirdParty/Inc/LoRa_RAK3172.h:29-30`
- **문제**: TX/RX/Response 각 2048바이트 = 6KB
- **수정**: TX=400B, RX=800B, Response=800B (BSS -4,288B 절약)

### 3-4. MiLoRa_IsBusy 플래그 관리 ✅
- **파일**: `Core/Src/Mi_LoRa.c`, `Core/Src/Mi_IoT.c`
- **문제1**: MiLoRa_Open() 성공 후에도 IsBusy=0 → 전송 중 MCU STOP 진입 가능
- **문제2**: Mi_IoT.c에서 MiLoRa_IsPowerOn으로 MCU busy 판단 → LPM 중 MCU sleep 불가
- **문제3**: SendMailbox에 IsSleeping 체크 없음 → Wakeup 전 AT 명령 충돌 가능
- **수정**:
  - MiIoT_IsBusy 조건에서 MiLoRa_IsBusy 제거 (서브시스템 전용으로 분리)
  - Open 성공 시 IsBusy=1 유지, 실패 시에만 IsBusy=0
  - Sleep 시 IsBusy=0, Wakeup 시 IsBusy=1, Close 시 IsBusy=0
  - SendMailbox에 IsSleeping 체크 추가 (RESULT_PAUSE 반환)
- **IsBusy 전이 규칙**:
  - =1: Open 시작 / Wakeup 성공
  - =0: Sleep 진입 / 전원 OFF / Open 실패

### 3-5. MiIoT_IsIORun 분리 (MCU Sleep 조건 개선) ✅
- **파일**: `Core/Src/Mi_IoT.c`, `Core/Inc/Mi_IoT.h`
- **문제**: GPIOControl이 LoRaEnable=1이면 RESULT_RUN 반환 → MiIoT_IsBusy=1 → LoRa LPM 중 MCU sleep 불가 (약 30초 지연)
- **원인**: MiMain_GPIOControl은 모든 DO가 0이 아니면 RESULT_RUN 반환하며, LoRaEnable도 DO에 포함
- **수정**:
  - `MiIoT_IsIORun` 플래그 추가 (GPIO/IO 활성 상태 전용)
  - `MiIoT_IsBusy` = Storage + Measurement + Sampling + Status (서브시스템 전용, LoRa/GPIO 제외)
  - MCU STOP 진입 조건: `!MiIoT_IsIORun && !MiIoT_IsBusy && MiIoT_IsPowerSaveMode && !MiIoT_IsPause`
  - LoRa Close 조건에 `!MiIoT_IsBusy` 추가 (서브시스템 busy 중 전원 OFF 방지)
- **효과**: LoRa LPM 중에도 MCU가 즉시 STOP 모드 진입 가능

### 3-6. LoRa Close 시 서브시스템 연동 ✅
- **파일**: `Core/Src/Mi_LoRa.c:656`
- **문제**: LoRa Sleep 후 타이머만으로 Close → 다른 서브시스템이 데이터 전송 대기 중일 수 있음
- **수정**: `!MiIoT_IsBusy && SleepTimer && oTMR_Elapsed(&SleepTimer, SECOND_TO_MS(3), ...)` 조건으로 서브시스템 idle 확인 후 Close

---

## Implementation Order

| 순서 | 항목 | 난이도 | 우선순위 | 상태 |
|------|------|--------|----------|------|
| 1 | 1-1. LINKCHECK 파싱 구분자 | Low | P1-Bug | ✅ 완료 |
| 2 | 1-2. GetSignalQuality break | Low | P1-Bug | ✅ 완료 |
| 3 | 1-3. SendMailbox 미제거 | Medium | P1-Bug | ✅ 완료 |
| 4 | 2-2. AT+CFM Config.CFM 갱신 | Low | P2-Perf | ✅ 완료 |
| 5 | 2-3. IsChangedConfig 활성화 | Medium | P2-Perf | ✅ 완료 |
| 6 | 3-3. RAM 버퍼 축소 | Low | P3-LP | ✅ 완료 |
| 7 | 3-1. LPM → 3초 → PowerOff | High | P3-LP | ✅ 완료 |
| 8 | 3-4. MiLoRa_IsBusy 관리 | Medium | P1-Bug* | ✅ 완료 |
| 9 | 3-5. MiIoT_IsIORun 분리 | Medium | P1-Bug* | ✅ 완료 |
| 10 | 3-6. LoRa Close 서브시스템 연동 | Low | P3-LP | ✅ 완료 |
| 11 | 2-1. StrBuffer static | Low | P2-Perf | ⏳ 대기 |
| 12 | 3-2. 전원 ON 대기 시간 | Low | P3-LP | ⏳ 대기 |

> *P1-Bug: 3-4, 3-5는 Phase 3에 분류되어 있으나 실질적으로 버그 수정 (MCU STOP 진입 불가/전송 중 sleep 진입)

---

## PowerSave 동작 시퀀스 (최종)

```
Mailbox 데이터 도착
  → MiLoRa_Open() [IsBusy=1]
  → Join + Transmit
  → Mailbox 비움 → 1초 delay
  → RAK3172_Sleep() (AT+LPM=1) [IsBusy=0]
  → MCU STOP 모드 진입 (MiIoT_IsIORun=0 && MiIoT_IsBusy=0)
  → [RTC wakeup 또는 mailbox 도착]
  → (mailbox 있으면) RAK3172_Wakeup() [IsBusy=1] → Transmit 반복
  → (mailbox 없으면) 3초 대기 + !MiIoT_IsBusy 확인
  → MiLoRa_Close() (전원 OFF) [GPIO.DO.LoRaEnable=0]
```

---

## Affected Files

- `oThirdParty/Src/LoRa_RAK3172.c`
- `oThirdParty/Inc/LoRa_RAK3172.h`
- `Core/Src/Mi_LoRa.c`
- `Core/Src/Mi_IoT.c`
- `Core/Inc/Mi_IoT.h`
- `Core/Src/Mi_Main_SIV100.c` (참조, MiMain_GPIOControl)

## Risks

- AT+LPM 설정 후 실제 소모전류 측정 검증 필요
- RAM 버퍼 축소 시 긴 AT 응답 잘릴 수 있음 (실제 최대 응답 길이 확인 필요)
- IsChangedConfig 활성화 시 기존 주석처리 이유 확인 필요
- MiLoRa_TimeSync가 LPM Sleep 중 AT 명령 시도 가능 (LPM 자동 wake로 처리됨)
- Class A에서 LPM/PowerOff 중 downlink 수신 불가 (서버 큐잉 후 다음 uplink 시 전달)
- MiIoT_IsPause가 현재 코드에서 1로 설정되는 곳 없음 (향후 사용 시 확인 필요)

## Build Status

- **Latest**: text=91,936, data=792, bss=35,720, 0 errors, 0 warnings
- **테스트 대기**: 펌웨어 플래시 후 PowerSave 시퀀스 실기 검증 필요
