# MiStorage Scheduler Refactoring Plan

> **Summary**: MiStorage를 직접 호출 방식에서 스케줄러 패턴으로 리팩토링 — `MiStorage_SetSchedule()`로 작업을 큐에 등록하고, `MiStorage_Scheduler()`가 순차 실행하는 구조
>
> **Project**: SIA100-2A (STM32L433RBT7 IoT Tilt Sensor Node)
> **Version**: FW 1.0
> **Author**: JONE
> **Date**: 2026-03-03
> **Status**: Draft

---

## 1. Overview

### 1.1 Purpose

현재 MiStorage의 각 함수(`MiStorage_WriteIoTData`, `MiStorage_ReadIoTData` 등)를 여러 모듈에서 직접 호출하는 구조를, **스케줄러 기반 간접 호출 구조**로 변경:

- **기존**: 각 모듈이 `MiStorage_WriteIoTData()` 등을 직접 호출 → NAND 접근 경합 가능
- **변경**: 각 모듈이 `MiStorage_SetSchedule()`로 작업 등록 → `MiStorage_Scheduler()`가 순차 처리

**장점**:
- NAND 접근 직렬화: 동시 접근 경합 제거
- 호출자 비동기화: 호출 측이 NAND I/O 완료를 기다리지 않아도 됨
- 전원 관리 통합: Scheduler가 Open/Close를 일괄 관리
- 5단계 파이프라인 통합: 측정 → Schedule 등록 → NAND 저장 → LoRa Mailbox → LoRa 전송

### 1.2 Background

현재 상태:
- **스켈레톤 구현 완료**: `Mi_Storage.h`에 `MiStorage_Schedule_t`, `MiStorage_ScheduleItem_t`, `MiStorage_ScheduleCommend_t` 정의됨
- **큐 API 구현 완료**: `MiStorage_SetSchedule()` (enqueue), `MiStorage_GetSchedule()` (dequeue) 구현됨
- **Scheduler 프레임 구현**: `MiStorage_Scheduler()` 상태머신 스켈레톤 있으나 command handler가 비어 있음
- **직접 호출 잔존**: 6개 외부 모듈에서 MiStorage 함수를 직접 호출 중

### 1.3 Related Documents

- [nand-flash-application.plan.md](nand-flash-application.plan.md) — NAND Flash 전체 Plan (5단계 파이프라인)

---

## 2. Scope

### 2.1 In Scope

- [x] FR-01: `MiStorage_Scheduler()` command handler 완성 (5개 커맨드 실행 연결)
- [x] FR-02: 기존 직접 호출을 `MiStorage_SetSchedule()` 간접 호출로 마이그레이션
- [x] FR-03: 초기화 시퀀스(ReadParam → WriteParam → Indexing) 스케줄러 기반으로 전환
- [x] FR-04: 작업 완료 상태 확인 메커니즘 (`State` 필드 기반)
- [x] FR-05: `MiStorage_GetSchedule()` 반환값 버그 수정

### 2.2 Out of Scope

- NAND 드라이버 자체 수정 (`MT29F2G_*` 함수)
- LoRa Mailbox 내부 로직 변경
- 시리얼 프로토콜 명령 추가
- NAND Wear-leveling / Bad Block Management 개선

---

## 3. Requirements

### 3.1 Functional Requirements

| ID | Requirement | Priority | Status |
|----|-------------|----------|--------|
| FR-01 | `MiStorage_Scheduler()` case 2에서 `pSchedule->Commend`에 따라 기존 함수 호출: `SCHEDULE_CMD_INDEXING` → `MiStorage_SensorDataIndexing()`, `SCHEDULE_CMD_READ_PARAMETER` → `MiStorage_ReadIoTParameter()`, `SCHEDULE_CMD_WRITE_PARAMETER` → `MiStorage_WriteIoTParameter()`, `SCHEDULE_CMD_READ_IOTDATA` → `MiStorage_ReadIoTData()`, `SCHEDULE_CMD_WRITE_IOTDATA` → `MiStorage_WriteIoTData()` | High | Pending |
| FR-02 | 외부 모듈 직접 호출 마이그레이션: `Mi_IoT.c`, `Mi_Calibration.c`, `Mi_Serial_SIA100.c`, `Mi_Serial.c`에서 `MiStorage_SetSchedule()` 사용으로 전환 | High | Pending |
| FR-03 | `void MiStorage()` 초기화 시퀀스를 스케줄러 기반으로 변경: 시작 시 ReadParam → (실패 시) WriteDefaultParam → Indexing 순서를 Schedule에 등록 | High | Pending |
| FR-04 | `MiStorage_ScheduleItem_t.State` 필드를 통한 완료/에러 상태 확인: 호출자가 등록한 아이템의 `State`를 polling하여 완료 여부 판단 | Medium | Pending |
| FR-05 | `MiStorage_GetSchedule()` 반환값 수정: 현재 값 반환(`MiStorage_Schedule.Items[...Last]`) → 포인터 반환(`&MiStorage_Schedule.Items[...Last]`) | High | Pending |

### 3.2 Non-Functional Requirements

| Category | Criteria | Measurement Method |
|----------|----------|-------------------|
| Performance | 스케줄러 오버헤드 < 1ms (큐 등록/조회) | 디버그 타이머 |
| Reliability | Schedule 큐 오버플로우 시 안전한 에러 반환 (NULL) | 코드 리뷰 |
| Memory | Schedule 큐 RAM 사용량 고정 (`10 x sizeof(ScheduleItem_t)`) | 컴파일러 메모리 맵 |
| Compatibility | 기존 기능 100% 유지 (동작 변경 없이 호출 경로만 변경) | 회귀 테스트 |

---

## 4. Success Criteria

### 4.1 Definition of Done

- [ ] FR-01: Scheduler에서 5개 command 모두 정상 실행 확인
- [ ] FR-02: 외부 모듈에서 MiStorage 함수 직접 호출 0건 (Scheduler/SetSchedule 경유만)
- [ ] FR-03: 전원 재시작 후 Parameter 읽기 → Indexing 자동 완료 확인
- [ ] FR-04: 호출자가 Schedule Item의 State로 완료 확인 가능
- [ ] FR-05: `MiStorage_GetSchedule()` 포인터 반환 확인
- [ ] 기존 LoRa 전송 정상 동작 (회귀 테스트)
- [ ] 빌드 경고 0건

---

## 5. Risks and Mitigation

| Risk | Impact | Likelihood | Mitigation |
|------|--------|------------|------------|
| `pData` 포인터 수명 문제: Schedule 등록 시점과 실행 시점 사이에 원본 데이터가 변경/해제될 수 있음 | High | High | `pData`에 전달하는 데이터는 static 또는 global 버퍼 사용, 또는 Schedule 아이템 내부에 데이터 복사 검토 |
| 동기식 호출자 마이그레이션: `Mi_Serial.c:285`에서 `while(result == RESULT_RUN)` 블로킹 루프 사용 중 | Medium | High | Schedule 등록 후 State polling 방식으로 전환, 또는 Serial 처리 상태머신에서 비동기 대기 |
| Schedule 큐 오버플로우: 측정 주기가 NAND 쓰기 속도보다 빠를 때 | Medium | Low | `MiStorage_SetSchedule()` NULL 반환 시 재시도 로직, 큐 사이즈 10개로 충분한 여유 |
| 초기화 순서 의존성: ReadParam 완료 전에 다른 모듈이 Schedule 등록할 수 있음 | Medium | Medium | 초기화 완료 플래그 추가, 또는 `MiStorage()` 초기화 단계에서만 직접 호출 유지 |
| Scheduler 상태 리셋: NAND FAULT 발생 시 진행 중인 Schedule 아이템 처리 | Medium | Low | FAULT 시 현재 아이템 State를 RESULT_FAULT로 설정하고 다음 아이템으로 진행 |

---

## 6. Architecture Considerations

### 6.1 Key Architectural Decisions

| Decision | Options | Selected | Rationale |
|----------|---------|----------|-----------|
| 데이터 전달 방식 | A) 포인터만 저장 / B) 아이템 내부에 데이터 복사 | A) 포인터 | 이미 구현된 구조 유지, RAM 절약. 호출자가 static/global 데이터 보장 |
| 초기화 시퀀스 처리 | A) Schedule에 등록 / B) 기존 직접 호출 유지 | A) Schedule 등록 | 통일된 실행 경로, Scheduler가 모든 NAND 접근 관리 |
| 완료 통지 방식 | A) State 필드 polling / B) 콜백 함수 / C) 이벤트 플래그 | A) State polling | 기존 상태머신 패턴과 일관, 추가 구조체 불필요 |
| 동기식 호출자 처리 | A) 비동기로 전환 / B) 별도 동기 API 유지 | A) 비동기 전환 | 모든 NAND 접근을 Scheduler로 통일, 블로킹 호출 제거 |

### 6.2 현재 vs 변경 후 아키텍처

```
[현재 구조 — 직접 호출]
┌──────────────┐   ┌──────────────┐   ┌──────────────┐
│  Mi_IoT.c    │   │Mi_Serial*.c  │   │Mi_Calibra*.c │
│  WriteIoTData│   │WriteIoTParam │   │WriteIoTParam │
└──────┬───────┘   └──────┬───────┘   └──────┬───────┘
       │                  │                   │
       ▼                  ▼                   ▼
┌─────────────────────────────────────────────────────┐
│              MiStorage_Write() / Read()              │
│                  (직접 NAND 접근)                     │
└─────────────────────────────────────────────────────┘


[변경 후 구조 — 스케줄러 패턴]
┌──────────────┐   ┌──────────────┐   ┌──────────────┐
│  Mi_IoT.c    │   │Mi_Serial*.c  │   │Mi_Calibra*.c │
│ SetSchedule()│   │ SetSchedule()│   │ SetSchedule()│
└──────┬───────┘   └──────┬───────┘   └──────┬───────┘
       │                  │                   │
       ▼                  ▼                   ▼
┌─────────────────────────────────────────────────────┐
│            MiStorage_Schedule_t (큐, 최대 10개)      │
│  ┌────┬────┬────┬────┬────┬────┬────┬────┬────┬────┐│
│  │ [0]│ [1]│ [2]│ [3]│ [4]│ [5]│ [6]│ [7]│ [8]│ [9]││
│  └────┴────┴────┴────┴────┴────┴────┴────┴────┴────┘│
│       ▲Last                          ▲First          │
└──────────────────────┬──────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────┐
│            MiStorage_Scheduler()                     │
│  case 0: GetSchedule() — 큐에서 아이템 꺼냄         │
│  case 1: MiStorage_Open() — NAND 전원 ON            │
│  case 2: Command 실행 (Indexing/Read/Write/...)      │
│  case 3: 완료 처리, State 업데이트, 다음 아이템      │
│  (큐 비면 → MiStorage_Close())                       │
└─────────────────────────────────────────────────────┘
```

### 6.3 Schedule Item 생명주기

```
[호출자]                         [Scheduler]
   │                                │
   ├─ SetSchedule(cmd, data) ──▶    │
   │  State = RESULT_WAIT           │
   │                                │
   │                          GetSchedule()
   │                          State = RESULT_RUN
   │                                │
   │                          Open() → Execute cmd
   │                                │
   │  ◀── State = RESULT_OK ───────┤  (성공)
   │  ◀── State = RESULT_ERROR ────┤  (실패)
   │  ◀── State = RESULT_FAULT ────┤  (NAND 장애)
   │                                │
   │  polling: pItem->State         │
   │  != RESULT_WAIT/RUN 이면 완료  │
   │                                │
   │                          Commend = NULL (슬롯 해제)
   │                          다음 아이템으로 이동
```

### 6.4 Command ↔ 기존 함수 매핑

| ScheduleCommend | 기존 함수 | pData 타입 | Address 사용 |
|-----------------|-----------|-----------|-------------|
| `SCHEDULE_CMD_INDEXING` | `MiStorage_SensorDataIndexing()` | 미사용 (NULL) | 미사용 (0) |
| `SCHEDULE_CMD_READ_PARAMETER` | `MiStorage_ReadIoTParameter()` | `IoTParameter_t*` | `MISTORAGE_PARAMETER_STARTADR` |
| `SCHEDULE_CMD_WRITE_PARAMETER` | `MiStorage_WriteIoTParameter()` | `IoTParameter_t*` | `MISTORAGE_PARAMETER_STARTADR` |
| `SCHEDULE_CMD_READ_IOTDATA` | `MiStorage_ReadIoTData()` | `IoT_DataPacket_t*` | 지정 주소 |
| `SCHEDULE_CMD_WRITE_IOTDATA` | `MiStorage_WriteIoTData()` | `IoT_DataPacket_t*` | 자동 계산 (NewerAddress+1) |

---

## 7. Implementation Strategy

### 7.1 수정 대상 파일

| File | Changes |
|------|---------|
| `Core/Inc/Mi_Storage.h` | 이미 완료: Schedule 구조체/열거형 정의됨. 필요 시 `MiStorage_SetSchedule` 반환 타입을 `MiStorage_ScheduleItem_t*`로 수정 |
| `Core/Src/Mi_Storage.c` | FR-01: Scheduler command handler 연결, FR-03: `MiStorage()` 초기화→Schedule 등록, FR-05: `GetSchedule()` 버그 수정 |
| `Core/Src/Mi_IoT.c` | FR-02: `MiStorage_WriteIoTData()` 직접 호출 → `MiStorage_SetSchedule(WRITE_IOTDATA)` |
| `Core/Src/Mi_Calibration.c` | FR-02: `MiStorage_WriteIoTParameter()` 직접 호출 → `MiStorage_SetSchedule(WRITE_PARAMETER)` |
| `Core/Src/Mi_Serial_SIA100.c` | FR-02: `WriteIoTParameter`, `ReadIoTData` 직접 호출 → Schedule 등록 + State polling |
| `Core/Src/Mi_Serial.c` | FR-02: `EraseDataArea`, `ReadIoTData` 블로킹 호출 → Schedule 등록 + 비동기 대기 |

### 7.2 외부 호출 마이그레이션 목록

| 파일 | 라인 | 기존 호출 | 변경 후 |
|------|------|----------|---------|
| `Mi_IoT.c` | 484 | `MiStorage_ReadIoTData(SyncAddress, ...)` | `MiStorage_SetSchedule(SyncAddress, ..., READ_IOTDATA)` |
| `Mi_Calibration.c` | 147 | `MiStorage_WriteIoTParameter(&MiIoT_Parameter)` | `MiStorage_SetSchedule(..., WRITE_PARAMETER)` |
| `Mi_Serial_SIA100.c` | 144 | `MiStorage_WriteIoTParameter(&MiIoT_Parameter)` | `MiStorage_SetSchedule(..., WRITE_PARAMETER)` |
| `Mi_Serial_SIA100.c` | 199 | `MiStorage_ReadIoTData(ExportAddr, ...)` | `MiStorage_SetSchedule(..., READ_IOTDATA)` |
| `Mi_Serial.c` | 285 | `while(MiStorage_EraseDataArea() == RUN)` | Schedule 등록 + 비동기 대기 (또는 별도 `SCHEDULE_CMD_ERASE` 추가) |
| `Mi_Serial.c` | 487 | `while(MiStorage_ReadIoTData() == RUN)` | Schedule 등록 + 비동기 대기 |

### 7.3 구현 순서

1. **FR-05: `MiStorage_GetSchedule()` 버그 수정** — 값 반환 → 포인터 반환 (`&` 추가)
2. **FR-01: Scheduler command handler 연결** — case 2의 각 커맨드에 기존 함수 호출 연결
3. **FR-04: State 관리 로직** — 실행 전 `RESULT_RUN`, 완료 시 `RESULT_OK/ERROR/FAULT` 설정
4. **FR-03: 초기화 시퀀스 전환** — `MiStorage()` 시작 시 ReadParam → Indexing을 Schedule에 등록
5. **FR-02: 외부 모듈 마이그레이션** — Mi_IoT.c → Mi_Calibration.c → Mi_Serial_SIA100.c → Mi_Serial.c 순서
6. **`SCHEDULE_CMD_ERASE` 추가 검토** — `Mi_Serial.c`의 EraseDataArea 블로킹 호출 해결
7. **통합 테스트** — 전체 파이프라인 검증

---

## 8. Known Issues in Current Code

| Issue | Location | Description | Fix |
|-------|----------|-------------|-----|
| `MiStorage_GetSchedule()` 반환값 오류 | Mi_Storage.c:749 | `return MiStorage_Schedule.Items[...]` — 구조체 값 반환, 반환 타입은 포인터 | `return &MiStorage_Schedule.Items[...]` |
| `MiStorage_SetSchedule()` 반환값 항상 NULL | Mi_Storage.c:709-729 | `pResult`가 초기 NULL에서 갱신되지 않음 | 등록된 아이템 포인터 반환하도록 수정 |
| `EraseDataArea` 커맨드 누락 | Mi_Storage.h:63-70 | `MiStorage_ScheduleCommend_t`에 Erase 커맨드 없음 | `SCHEDULE_CMD_ERASE_DATA = 6` 추가 |
| `Mi_Serial.c` 블로킹 루프 | Mi_Serial.c:285,487 | `while(result == RESULT_RUN)` 동기 대기 — Scheduler 전환 시 메인 루프 블로킹 | 비동기 상태머신으로 전환 필요 |

---

## 9. Next Steps

1. [ ] Design 문서 작성 (`mistorage-scheduler.design.md`) — Scheduler 상세 시퀀스 다이어그램, State 전이도
2. [ ] 리뷰 및 승인
3. [ ] 구현 시작

---

## Version History

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 0.1 | 2026-03-03 | Initial draft | JONE |
