# Gap Analysis: LoRa 전송 중단 원인 분석 및 수정

## 분석 개요

| 항목 | 값 |
|------|-----|
| Feature | lora-transmission-stop |
| Plan 문서 | docs/01-plan/features/lora-transmission-stop.plan.md |
| 분석 일자 | 2026-03-11 |
| **Critical Match Rate** | **100%** |
| **Overall Match Rate** | **75% (가중치 적용)** |

---

## Critical Fixes (100% 구현 완료)

| # | 수정 | 파일 | 상태 |
|---|------|------|:----:|
| P1 | calloc/free → static 버퍼 | ONE_ATCommend.c | PASS |
| P2 | Native_SleepMode(0) → 최소 10초 | Mi_Native_L433RBT7.c | PASS |
| P3 | Mailbox 실패 메일 제거 | Mi_LoRa.c | PASS |
| P4 | 3초 부팅 가드 (추가) | Mi_IoT.c | PASS |

### P1 상세

- `AT_ParseBuffer[1024]` 정적 버퍼로 교체
- `MATH_MIN(length, AT_PARSE_BUFFER_SIZE)` 버퍼 오버플로 방지
- calloc/free 완전 제거 → 힙 파편화 원인 소멸

### P2 상세

- `if(Millisecond == 0) Millisecond = SECOND_TO_MS(10);`
- 무한 STOP 모드 방지, 10초마다 wakeup 보장
- IWDG 리셋 루프 탈출 경로 확보

### P3 상세

- `default` 케이스에서 `MiIoT_MailBox_Remove()` 호출
- Plan은 재시도 카운트 방식 제안 → 구현은 즉시 제거 방식
- `MiLoRa_SendMailbox` 내부에 이미 QoS 재시도 존재하므로 즉시 제거가 더 적합

### P4 상세 (Plan 문서에 없음 - 분석 중 추가)

- `oTMR_GetTick(TICKBASE_SYSTICK) > 3000` 조건 추가
- 부팅 후 3초간 Sleep 진입 차단
- MiStorage 파라미터 로드 시간 확보 (NAND 200ms + 읽기)

---

## 미구현 항목

| # | 항목 | 우선순위 | 미구현 사유 |
|---|------|---------|------------|
| Plan P2 | UART DMA 에러 클리어 | 중간 | 직접 원인 아님, LoRa UART는 Close/Open으로 복구 |
| Plan P4 | OperatingMode 유효성 검증 | 중간 | P2+P4 수정으로 간접 커버 |
| Plan P5 | SysTick STOP 보정 | 낮음 | 타이밍 오차만, 전송 중단과 무관 |
| Plan P6 | DWT 카운터 동기화 | 낮음 | CORECYCLE 사용처 제한적 |

---

## 결론

- **모든 Critical 수정 (P1~P4) 100% 구현 완료**
- 2단계 방어: P4(부팅 가드) + P2(Sleep 안전장치)
- 미구현 항목은 보조적 안정성 개선으로, 핵심 장애와 직접 관련 없음
- **48시간 이상 연속 동작 검증 테스트 권장**
