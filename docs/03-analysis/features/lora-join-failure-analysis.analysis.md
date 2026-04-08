# Gap Analysis: LoRa Join 실패 시 재시도 중단

> **Feature**: lora-join-failure-analysis
> **Phase**: Check (Gap Analysis)
> **Date**: 2026-03-30
> **Match Rate**: 0%

## Overall Match Rate: 0%

Plan 문서에서 제안한 4가지 수정 방안이 **모두 미구현** 상태입니다.

## Gap Summary

| # | 수정 방안 | 우선순위 | 상태 | 위치 |
|---|----------|----------|------|------|
| 5.1 | MiLoRa_IsBusy → MiIoT_IsBusy 반영 | 필수 | NOT IMPLEMENTED | Mi_IoT.c:634 |
| 5.2 | Open 상태머신 전체 타임아웃 | 필수 | NOT IMPLEMENTED | Mi_LoRa.c:602-746 |
| 5.3 | Sleep 진입 전 LoRa 상태 확인 | 권장 | NOT IMPLEMENTED | Mi_IoT.c:583 |
| 5.4 | 절전모드 재접속 로직 추가 | 권장 | NOT IMPLEMENTED | Mi_LoRa.c:774-781 |

## Root Cause Chain (미해결)

```
[근본 원인] MiLoRa_IsBusy가 MiIoT_IsBusy에 미반영 (5.1)
     ↓
[연쇄] LoRa Open 진행 중 MCU STOP Mode 진입 (5.3)
     ↓
[연쇄] GPIO DeInit → LoRa UART/전원 단절
     ↓
[연쇄] RAK3172 EVT/AT 응답 유실
     ↓
[직접] Open 상태머신 Stuck, 전체 타임아웃 없음 (5.2)
     ↓
[현상] Join 재시도 중단 (23시간 공백)
```

## Severity

- **5.1 + 5.2**: CRITICAL - 이 두 가지 미구현이 23시간 공백의 직접 원인
- **5.3**: HIGH - 5.1 구현만으로는 IsOpen=1 && IsBusy=0 상태에서 Sleep 방지 불가
- **5.4**: MEDIUM - 메일 없을 때 재접속 지연 문제

## Recommendation

Match Rate 0% → **즉시 구현 필요** (`/pdca iterate lora-join-failure-analysis`)
