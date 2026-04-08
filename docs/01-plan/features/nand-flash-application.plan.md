# NAND Flash Application Planning Document

> **Summary**: NAND 플래시(MT29F2G01) 활용 — 데이터 측정 → Storage FIFO → NAND 저장 → LoRa Mailbox → LoRa 전송 5단계 순차 파이프라인, Parameter 이중 저장, 시리얼 데이터 전송, 데이터 색인 기능 구현
>
> **Project**: SIA100-2A (STM32L433RBT7 IoT Tilt Sensor Node)
> **Version**: FW 1.0
> **Author**: JONE
> **Date**: 2026-02-24
> **Status**: Draft

---

## 1. Overview

### 1.1 Purpose

SIA100-2A 디바이스에 장착된 QSPI NAND 플래시(Micron MT29F2G01ABAGDWB, 2Gb)를 활용하여:
- **핵심 데이터 파이프라인** (5단계 순차 처리):
  1. **데이터 측정** — 센서(Tilt/온도)에서 `IoT_DataPacket_t` 생성
  2. **Storage FIFO 저장** — `MiStorage_FIFO_t`(최대 5개) 버퍼에 큐잉
  3. **NAND 저장** — FIFO에서 꺼내 NAND Block 3~2045에 순차 기록
  4. **LoRa Mailbox 저장** — NAND 저장 완료 후 `IoT_Mailbox_t`에 등록
  5. **LoRa 전송** — Mailbox에서 꺼내 RAK3172로 전송
  - 각 단계 완료 후 다음 단계로 진행하여 데이터 무손실 보장
- Parameter를 MCU 내부 플래시와 NAND 플래시에 동시 저장하여 데이터 안정성 확보
- USB 시리얼 요청 시 저장된 IoT 데이터를 호스트로 전송
- NAND 플래시에 저장된 IoT 데이터의 색인(Index) 기능으로 데이터 조회 지원

### 1.2 Background

현재 상태:
- **NAND 드라이버**: `NAND_MT29F2G01ABAGDWB_IT.c`에 완전 구현됨 (QSPI 4-line 모드)
- **Storage 계층**: `Mi_Storage.c`에 PageHeader 기반 읽기/쓰기 구현됨
- **Parameter 저장**: 현재 MCU 내부 플래시(마지막 2KB 페이지)만 사용 — NAND 경로 주석 처리됨
- **IoT 데이터 로깅**: NAND Block 3~2045에 쓰기 구현됨, 하지만 시작 시 색인(Indexing) 비활성화
- **시리얼 프로토콜**: JSON 기반 Get/Set 명령 구현됨 (`Mi_Serial.c`, `Mi_Serial_SIA100.c`)
- **NAND 드라이버 활성화 이슈**: `QSPI_DUALFLASH_ENABLE` 매크로 가드 확인 필요

### 1.3 Related Documents

- Hardware: SIA100-2A HW0.1 Schematic
- MCU Config: `SIA100_2A_HW0_1_FW0_1_STM32L433RBT7.ioc`
- NAND Datasheet: Micron MT29F2G01ABAGDWB-IT

---

## 2. Scope

### 2.1 In Scope

- [x] FR-01: Parameter를 MCU 내부 플래시와 NAND 플래시에 동시 저장/읽기
- [x] FR-02: IoT 센서 측정 데이터를 NAND 플래시에 저장
- [x] FR-03: USB 시리얼 요청으로 NAND에 저장된 IoT 데이터 전송
- [x] FR-04: NAND 플래시 IoT 데이터 색인(Index) — 파일 리스트업

### 2.2 Out of Scope

- LoRa 통신 프로토콜 자체 변경 (페이로드 포맷, 주파수 설정 등)
- NAND Wear-leveling 알고리즘 (현재 순차 쓰기 방식 유지)
- NAND Bad Block Management 개선 (기존 `MT29F2G_BadBlockCheck` 유지)
- OTA(Over-The-Air) 펌웨어 업데이트

> **Note**: 데이터 파이프라인 순서 변경(Storage FIFO → NAND → LoRa Mailbox → LoRa 전송)은 In Scope에 포함됨

---

## 3. Requirements

### 3.1 Functional Requirements

| ID | Requirement | Priority | Status |
|----|-------------|----------|--------|
| FR-01 | Parameter를 MCU 내부 플래시(2KB)와 NAND Block 2에 동시 저장. 읽기 시 MCU 플래시 우선, CRC 실패 시 NAND에서 복원 | High | Pending |
| FR-02 | **5단계 순차 파이프라인**: 센서 측정 완료 → Storage FIFO(`MiStorage_FIFO_t`, 최대 5개)에 큐잉 → NAND 데이터 영역(Block 3~2045)에 순차 저장 → LoRa Mailbox에 등록 → LoRa 전송. 블록 끝 도달 시 다음 블록으로 이동, Bad Block 자동 스킵 | High | Pending |
| FR-03 | USB 시리얼 `Get/StoredData` 명령으로 지정 범위의 IoT 데이터를 JSON 형식으로 전송. 페이지 단위 전송 지원 (메모리 제약 고려) | High | Pending |
| FR-04 | USB 시리얼 `Get/DataIndex` 명령으로 NAND에 저장된 데이터 목록(색인) 전송: 총 레코드 수, 최신/최구 타임스탬프, 블록별 요약 정보 | Medium | Pending |
| FR-05 | 시작 시 NAND 데이터 색인 스캔 활성화 (`MiStorage_SensorDataIndexing` 복원/개선) | High | Pending |
| FR-06 | USB 시리얼 `Set/EraseStoredData` 명령으로 NAND 데이터 영역 일괄 삭제 | Low | Pending |

### 3.2 Non-Functional Requirements

| Category | Criteria | Measurement Method |
|----------|----------|-------------------|
| Performance | NAND 페이지 쓰기 < 5ms (QSPI 4-line, ~8MHz clock) | 오실로스코프 / 디버그 타이머 |
| Performance | 색인 스캔 시간 < 10초 (2043 블록 스캔) | 시작 시간 측정 |
| Reliability | Parameter 이중 저장으로 단일 플래시 오류 시에도 복원 가능 | CRC32 검증 테스트 |
| Memory | RAM 사용량 증가 < 512 바이트 (색인 구조체 포함) | 컴파일러 메모리 맵 |
| Power | NAND 미사용 시 자동 전원 차단 (기존 1초 타이머 유지) | 전류 측정 |

---

## 4. Success Criteria

### 4.1 Definition of Done

- [ ] FR-01: Parameter 저장 시 MCU 플래시와 NAND 동시 기록 확인
- [ ] FR-01: MCU 플래시 CRC 실패 시 NAND에서 자동 복원 확인
- [ ] FR-02: 센서 측정 후 NAND에 데이터 저장 확인 (디버그 로그)
- [ ] FR-03: `Get/StoredData` 시리얼 명령으로 저장 데이터 수신 확인
- [ ] FR-04: `Get/DataIndex` 시리얼 명령으로 색인 정보 수신 확인
- [ ] FR-05: 전원 재시작 후 색인 자동 복원 확인
- [ ] 기존 LoRa 전송 기능 정상 동작 확인 (회귀 테스트)

### 4.2 Quality Criteria

- [ ] 빌드 경고 0건
- [ ] Watchdog 리셋 없이 72시간 연속 동작
- [ ] Bad Block 발생 시 자동 스킵 확인

---

## 5. Risks and Mitigation

| Risk | Impact | Likelihood | Mitigation |
|------|--------|------------|------------|
| NAND 드라이버 매크로 가드(`QSPI_DUALFLASH_ENABLE`) 미정의로 컴파일 실패 | High | High | 빌드 설정에 `MT29F2G_ENABLED` 매크로 추가, 불필요한 `QSPI_DUALFLASH_ENABLE` 가드 제거 또는 정의 |
| 색인 스캔 시 전원 부족으로 NAND 읽기 실패 | Medium | Low | 공급 전압 체크 후 스캔 시작, 실패 시 재시도 로직 |
| 시리얼 대량 데이터 전송 중 버퍼 오버플로우 | High | Medium | 페이지 단위 전송, Flow control 구현, DMA TX 완료 콜백 활용 |
| NAND Bad Block 누적으로 저장 영역 감소 | Medium | Low | Bad Block 카운트 모니터링, 색인에 유효 블록 수 포함 |
| 색인 스캔 시간이 Watchdog 타임아웃(~11초) 초과 | High | Medium | 스캔 루프 중 주기적 IWDG 리프레시, 또는 블록 단위 분할 스캔 |
| 동시 저장 시 NAND 쓰기 실패가 전체 저장 실패로 이어짐 | Medium | Low | MCU 플래시 저장 성공 시 결과 반환, NAND 실패는 경고만 기록 |

---

## 6. Architecture Considerations

### 6.1 Project Level

Embedded C 프로젝트 (STM32 HAL 기반), bkit 레벨 분류 해당 없음.

### 6.2 Key Architectural Decisions

| Decision | Options | Selected | Rationale |
|----------|---------|----------|-----------|
| Parameter 이중 저장 전략 | A) MCU 우선+NAND 백업 / B) NAND 우선+MCU 백업 | A) MCU 우선 | MCU 내부 플래시가 읽기 속도 빠르고, NAND 전원 ON 불필요 |
| 색인 저장 위치 | A) RAM only / B) NAND 별도 블록 / C) NAND Block 1 | A) RAM only | 시작 시 스캔으로 재구성, 비휘발 색인은 복잡도 높음 |
| 시리얼 데이터 전송 방식 | A) 전체 한번에 / B) 페이지 단위 분할 / C) 스트리밍 | B) 페이지 단위 | STM32L433 RAM 제한(64KB), DMA TX 버퍼 재사용 |
| 색인 스캔 방식 | A) 전체 페이지 읽기 / B) 헤더만 읽기 / C) 블록 첫 페이지만 | B) 헤더만 읽기 | 13바이트 헤더만으로 유효성 판단 가능, 스캔 시간 최소화 |
| 측정→NAND 버퍼링 | A) 직접 NAND 쓰기 / B) Storage FIFO 경유 / C) LoRa Mailbox 공유 | B) Storage FIFO | 측정과 NAND I/O 디커플링, NAND 전원 ON/쓰기 지연 시 데이터 유실 방지 (최대 5개 버퍼) |
| LoRa Mailbox 등록 시점 | A) 측정 직후 / B) NAND 저장 완료 후 | B) NAND 저장 후 | NAND 저장 확인 후 전송하여 데이터 무손실 파이프라인 보장 |

### 6.3 NAND 메모리 맵

```
NAND MT29F2G01 (2Gb = 256MB)
2048 Blocks x 64 Pages x 2048 Bytes/Page

┌──────────────────────────────────────────────┐
│ Block 0-1    : Reserved (Factory Bad Block)  │
├──────────────────────────────────────────────┤
│ Block 2      : Parameter Storage             │
│               Page 0: IoTParameter_t         │
│               (13B header + payload + CRC)   │
├──────────────────────────────────────────────┤
│ Block 3-2045 : IoT Data Storage Area         │
│               2043 Blocks x 64 Pages         │
│               = 최대 130,752 레코드           │
│               (각 페이지에 1개 IoT_DataPacket)│
├──────────────────────────────────────────────┤
│ Block 2046-2047 : Reserved                   │
└──────────────────────────────────────────────┘
```

### 6.4 데이터 흐름 (5단계 순차 파이프라인)

```
[1. 데이터 측정]
    │  Measurement_Sensor() → IoT_DataPacket_t (14B Tilt Frame)
    │
    ▼
[2. Storage FIFO 저장]
    │  MiStorage_FIFO_AddItem()
    │  MiStorage_FIFO_t (최대 5개 아이템 순환 버퍼)
    │  ┌─────────────────────────────────────┐
    │  │ [0] [1] [2] [3] [4]                │
    │  │  ▲First          ▲Last  Count=N    │
    │  └─────────────────────────────────────┘
    │
    ▼
┌──────────────────── MiStorage() 상태머신 ────────────────────┐
│                                                               │
│  [3. NAND 저장]                                               │
│    │  FIFO에서 아이템 꺼냄 → MiStorage_WriteIoTData()         │
│    │  Block 3~2045 순차 기록 (NewerAddress++ → 다음 페이지)   │
│    │  저장 완료 시 FIFO 아이템 제거                            │
│    │                                                          │
│    ▼  (저장 완료 후)                                          │
│                                                               │
│  [4. LoRa Mailbox 저장]                                       │
│    │  MiIoT_MailBox_NewItem(&MiIoT_LoRaMailBox, ...)          │
│    │  IoT_Mailbox_t (최대 5개 아이템)                          │
│                                                               │
└───────────────────────────────────────────────────────────────┘
    │
    ▼
┌──────────────────── MiIoT_MailBoxProcess() ──────────────────┐
│                                                               │
│  [5. LoRa 전송]                                               │
│    │  MiLoRa_SendMailbox() ──▶ RAK3172 전송                   │
│    │  전송 완료 시 Mailbox 아이템 제거                         │
│                                                               │
└───────────────────────────────────────────────────────────────┘

※ Storage FIFO: 측정과 NAND I/O를 디커플링, NAND 전원 ON 지연 시 데이터 보호
※ NAND 저장 실패 시에도 LoRa Mailbox에 등록하여 전송은 진행 (데이터 유실 방지)
※ 모든 단계는 비동기 상태머신으로 메인 루프 블로킹 없음

[USB Serial Request]   ◀── MiStorage_ReadIoTData()
  Get/StoredData               │
  Get/DataIndex        ◀── 색인 조회 (RAM Index)
```

### 6.5 시리얼 프로토콜 확장

```
기존 명령:
  Get/DeviceInfo, Get/DeviceState, Get/ChannelConfig,
  Get/LoRaState, Get/SystemConfig, Get/SensorData, Get/SamplingADCs
  Set/DeviceInfo, Set/ChannelConfig, Set/SystemConfig,
  Set/LoRaConfig, Set/Save, Set/Reset

추가 명령:
  Get/DataIndex     → NAND 데이터 색인 정보 응답
  Get/StoredData    → NAND 저장 데이터 전송 (범위 지정)
  Set/EraseStoredData → NAND 데이터 영역 삭제
```

---

## 7. Implementation Strategy

### 7.1 수정 대상 파일

| File | Changes |
|------|---------|
| `Core/Src/Mi_Storage.c` | FR-02: Storage FIFO 큐잉/디큐잉 처리, FIFO→NAND 쓰기 상태머신, NAND 완료 후 LoRa Mailbox 등록. FR-01: Parameter 이중 저장 로직. FR-05: 색인 스캔 활성화 |
| `Core/Inc/Mi_Storage.h` | Storage FIFO API 선언 (`MiStorage_FIFO_AddItem` 등), 색인 구조체 정의 |
| `Core/Src/Mi_IoT.c` | FR-02: 측정 완료 후 Storage FIFO에 데이터 추가 (기존 LoRa Mailbox 직접 등록 제거), MailboxProcess에서 LoRa 전송만 담당 |
| `Core/Src/Mi_Serial_SIA100.c` | FR-03: `Get/StoredData` 핸들러, FR-04: `Get/DataIndex` 핸들러, FR-06: `Set/EraseStoredData` 핸들러 |
| `Core/Src/Mi_Main_Node_SIA100.c` | FR-05: 초기화 시 색인 스캔 호출 |
| `oThirdParty/Src/NAND_MT29F2G01ABAGDWB_IT.c` | 매크로 가드 검토/수정 |

### 7.2 구현 순서

> **핵심 파이프라인 우선**: 측정 → Storage FIFO → NAND → LoRa Mailbox → LoRa 전송

1. **NAND 드라이버 활성화**: 매크로 가드 해결, NAND Init/Read/Write 기본 동작 확인
2. **Storage FIFO 구현** (FR-02): `MiStorage_FIFO_t` 큐잉/디큐잉 API 구현, 측정 완료 시 FIFO에 데이터 추가
3. **NAND 저장 연동** (FR-02): `MiStorage()` 상태머신에서 FIFO → NAND 쓰기 처리
4. **LoRa Mailbox 연동** (FR-02): NAND 저장 완료 후 LoRa Mailbox에 등록하도록 변경
5. **5단계 파이프라인 검증**: 측정 → FIFO → NAND → Mailbox → LoRa 전체 순차 흐름 확인
6. **색인 스캔 활성화** (FR-05): `MiStorage_SensorDataIndexing` 복원/개선
7. **Parameter 이중 저장** (FR-01): `MiStorage_WriteIoTParameter` / `ReadIoTParameter` 수정
8. **시리얼 색인 조회** (FR-04): `Get/DataIndex` 명령 구현
9. **시리얼 데이터 전송** (FR-03): `Get/StoredData` 명령 구현
10. **데이터 삭제** (FR-06): `Set/EraseStoredData` 명령 구현
11. **통합 테스트**: 전체 파이프라인 + 시리얼 명령 종합 검증

---

## 8. Next Steps

1. [ ] Design 문서 작성 (`nand-flash-application.design.md`) — 상세 함수 시그니처, 데이터 구조, 시퀀스 다이어그램
2. [ ] 리뷰 및 승인
3. [ ] 구현 시작

---

## Version History

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 0.1 | 2026-02-24 | Initial draft | JONE |
| 0.2 | 2026-02-25 | 데이터 흐름을 순차 처리로 변경 (센서→NAND→LoRa), FR-02 순차 의존성 추가, 구현 순서 재정렬 | JONE |
| 0.3 | 2026-02-25 | 5단계 파이프라인으로 확장 (측정→Storage FIFO→NAND→LoRa Mailbox→LoRa 전송), FIFO 버퍼링 아키텍처 결정 추가, 수정 대상 파일 역할 재정의 | JONE |
