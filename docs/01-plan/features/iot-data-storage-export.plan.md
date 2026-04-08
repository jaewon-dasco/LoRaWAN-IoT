# Plan: IoT 센서데이터 NAND 저장 및 시리얼 내보내기

## 1. 개요

IoT 센서 데이터를 NAND 플래시에 저장하고, 저장된 데이터를 색인한 뒤, 시리얼(USB UART)로 내보내는 기능의 구현 점검 및 보완.

**대상 장비**: SIA100-2A (STM32L433RBT7 + MT29F2G01ABAGDWB-IT)
**시리얼**: USART1 115200bps (USB 연결 시 활성화)

---

## 2. 현재 구현 상태 분석

### 2.1 이미 구현된 기능 (검증 필요)

| 기능 | 파일 | 함수 | 상태 |
|------|------|------|------|
| 센서 데이터 저장 | Mi_Storage.c:476 | `MiStorage_WriteIoTData()` | 구현 완료 - 검증 필요 |
| 데이터 색인 | Mi_Storage.c:501 | `MiStorage_SensorDataIndexing()` | 구현 완료 - 검증 필요 |
| 단건 데이터 읽기 | Mi_Storage.c:454 | `MiStorage_ReadIoTData()` | 구현 완료 |
| 색인 정보 조회 | Mi_Serial.c:405 | `Get/DataIndex` 시리얼 명령 | 구현 완료 |
| 단건 저장 데이터 조회 | Mi_Serial.c:428 | `Get/StoredData` 시리얼 명령 | 구현 완료 (블로킹 문제 있음) |
| 측정 후 자동 저장 | Mi_IoT.c:619 | `MiStorage_WriteIoTData(&DataPacket)` | 호출부 구현 완료 |

### 2.2 미구현/문제점

| 항목 | 설명 |
|------|------|
| 벌크 데이터 내보내기 | 여러 건의 데이터를 순차 내보내는 프로토콜 없음 |
| `Get/StoredData` 블로킹 | `while((r = MiStorage_ReadIoTData(...)) == RESULT_RUN);` 블로킹 루프 사용 |
| `MiSerial_Specific()` 빈 스텁 | Mi_Serial_SIA100.c:116 - 커스텀 명령 확장 포인트 미활용 |
| TX 버퍼 50바이트 | 대량 전송 시 DMA 완료 대기 필요 |
| 전송 진행 상태 피드백 | 대량 내보내기 시 진행률 표시 없음 |

---

## 3. 작업 순서

### Step 1: 데이터 저장 검증 (MiStorage_WriteIoTData)

**목표**: 측정 데이터가 NAND에 정상 저장되는지 확인

**검증 방법**:
1. 디바이스 전원 ON → NAND 초기화 로그 확인
2. 센서 측정 트리거 → `MiStorage | WriteData OK Adr=X Cnt=Y` 로그 확인
3. `Get/DataIndex` 명령으로 NewerAddr, Count 갱신 확인

**확인 포인트**:
- `MiStorage()` 상태머신이 정상 동작하는지 (Step 0→1→2→3)
- NAND Open → ReadParam → Indexing → 대기(AutoOff) 흐름
- WriteIoTData 호출 시 NAND AutoOpen → Write → AutoClose 동작

### Step 2: 저장 데이터 색인 검증 (MiStorage_SensorDataIndexing)

**목표**: 전원 재투입 후 색인이 정확히 수행되는지 확인

**검증 방법**:
1. 데이터 저장 후 전원 재투입
2. `MiStorage | Indexing start` → `MiStorage | Indexing OK Cnt=X ...` 로그 확인
3. `Get/DataIndex` 명령으로 OlderAddr/NewerAddr/Count 정합성 확인
4. `Get/StoredData` 명령으로 OlderAddr, NewerAddr 위치의 데이터 내용 확인

**확인 포인트**:
- CountOfData가 실제 저장 건수와 일치하는지
- OlderAddress/NewerAddress가 시간순 정렬과 일치하는지
- Empty NAND 상태에서도 정상 초기화되는지

### Step 3: 색인된 데이터 시리얼 내보내기 (프로토콜 설계 + 구현)

**목표**: 색인된 전체 데이터를 시리얼로 순차 내보내기

#### 3.1 프로토콜 설계

**요청 (PC → 디바이스)**:
```json
{"Type" : "Get/ExportData", "StartAddr" : "192", "Count" : "10"};\r\n
```
- `StartAddr`: 시작 주소 (생략 시 OlderAddress부터)
- `Count`: 내보낼 건수 (생략 시 전체, 최대 100건/요청)

**응답 (디바이스 → PC)**: 건별 JSON 전송
```json
{"Type":"ExportData","Seq":"1","Addr":"192","Time":"2026-02-25 10:30:00","Temp":"23.45","TiltX":"1.234","TiltY":"-0.567"};\r\n
{"Type":"ExportData","Seq":"2","Addr":"193","Time":"2026-02-25 10:35:00","Temp":"23.50","TiltX":"1.230","TiltY":"-0.560"};\r\n
...
{"Type":"ExportDone","Total":"10","Errors":"0"};\r\n
```

**오류 응답**:
```json
{"Type":"ExportData","Seq":"5","Addr":"196","Error":"ReadFail"};\r\n
```

#### 3.2 구현 위치

| 구현 내용 | 파일 | 위치 |
|-----------|------|------|
| Export 명령 파싱 | Mi_Serial.c | `MiSerial_Get()` 함수에 `Get/ExportData` 분기 추가 |
| Export 상태머신 | Mi_Serial_SIA100.c | `MiSerial_Process()`에 비블로킹 Export 루프 추가 |
| Export 완료 콜백 | Mi_Serial_SIA100.c | 전송 완료 시 ExportDone 메시지 전송 |

#### 3.3 핵심 설계 원칙

1. **비블로킹**: `while()` 블로킹 루프 금지 → 상태머신 패턴 사용
2. **건별 전송**: DMA TX 완료 후 다음 건 전송 (TX 버퍼 50바이트 제약)
3. **주소 순회**: StartAddr → StartAddr+Count (bad block skip, 순환 버퍼 고려)
4. **에러 건 스킵**: 읽기 실패 시 Error 레코드 전송 후 다음 건으로 진행
5. **중단 지원**: 전송 중 새 명령 수신 시 Export 중단

---

## 4. 제약 조건

| 항목 | 값 | 비고 |
|------|-----|------|
| UART Baud Rate | 115200 bps | ~11.5 KB/s |
| TX Buffer | 50 bytes | DMA 기반, 완료 대기 필요 |
| 데이터 1건 JSON 크기 | ~120 bytes | ExportData 한 줄 |
| 1건 전송 시간 | ~10ms | 120B / 11.5KB/s |
| 100건 전송 예상 시간 | ~1~2초 | NAND 읽기 포함 |
| NAND 페이지 읽기 시간 | ~0.5ms | QSPI 4-line mode |
| 데이터 영역 | Block 3~2045 (Page 0~63) | 약 130,752 페이지 |

---

## 5. 파일 영향 범위

| 파일 | 변경 유형 |
|------|-----------|
| `Core/Src/Mi_Serial.c` | `MiSerial_Get()`에 `Get/ExportData` 분기 추가 |
| `Core/Src/Mi_Serial_SIA100.c` | `MiSerial_Process()`에 Export 상태머신 추가 |
| `Core/Inc/Mi_Serial.h` | Export 관련 extern 변수 선언 (필요 시) |

**변경하지 않는 파일**:
- `Mi_Storage.c` - 기존 ReadIoTData 재활용
- `NAND_MT29F2G01ABAGDWB_IT.c` - 드라이버 변경 없음

---

## 6. 테스트 시나리오

| # | 테스트 | 예상 결과 |
|---|--------|-----------|
| T1 | 전원 ON 후 NAND 초기화 로그 | `MT29F2G \| Init OK SR=0x...` + `MiStorage \| Storage[2] Indexing done` |
| T2 | 센서 측정 후 Write 로그 | `MiStorage \| WriteData OK Adr=X` |
| T3 | `Get/DataIndex` 시리얼 명령 | Count, NewerAddr 정상 응답 |
| T4 | `Get/StoredData {"Address":"X"}` | 저장된 Tilt 데이터 JSON 응답 |
| T5 | `Get/ExportData {"Count":"5"}` | 5건 순차 JSON 전송 + ExportDone |
| T6 | Empty NAND에서 Export | `ExportDone Total=0` |
| T7 | Export 중 다른 명령 수신 | Export 중단, 새 명령 처리 |
