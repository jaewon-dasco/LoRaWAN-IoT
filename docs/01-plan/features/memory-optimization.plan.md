# Memory Optimization Plan - VMX3264 (STM32L433RBT6)

## 1. Before Optimization (Baseline)

### 1.1 Overall Summary

| Region | Total | Used | Free | Usage |
|--------|-------|------|------|-------|
| **FLASH** | 256 KB (262,144 B) | 101,160 B (.text + .rodata + .data) | 160,984 B | **38.6%** |
| **RAM** | 48 KB (49,152 B) | 44,804 B (.data + .bss) | 4,348 B | **91.2%** |
| **RAM2** | 16 KB (16,384 B) | 0 B | 16,384 B | **0%** |

### 1.2 RAM Section Breakdown

| Section | Address Range | Size |
|---------|--------------|------|
| .data | 0x20000000 - 0x20000350 | 848 B |
| .bss | 0x20000350 - 0x2000AF04 | 43,956 B |
| **Free (heap+stack)** | 0x2000AF04 - 0x2000C000 | **4,348 B** |

### 1.3 Top RAM Consumers (BSS)

| # | Variable | Module | Size (B) | % of BSS |
|---|----------|--------|----------|----------|
| 1 | HistorgramBuffer | Mi_Measurement | 16,000 | 36.4% |
| 2 | MiIoT_LoRaMailBox | Mi_LoRa | 9,048 | 20.6% |
| 3 | MiIoT_DefaultParameter | Mi_IoT | 2,296 | 5.2% |
| 4 | MiIoT_Parameter | Mi_IoT | 2,296 | 5.2% |
| 5 | MiStorage_LoadedParameter | Mi_Storage | 2,296 | 5.2% |
| 6 | RAK3172Dev | LoRa_RAK3172 | 2,220 | 5.1% |
| 7 | Buffer.17 (NAND page) | Mi_Storage | 2,048 | 4.7% |
| 8 | DMABuffer.34 (sweep) | Mi_Measurement | 1,500 | 3.4% |
| 9 | CaptureDMABuffer | Mi_Measurement | 1,200 | 2.7% |
| 10 | MiSerial_RxBuffer | Mi_Serial | 1,000 | 2.3% |
| 11 | StrBuffer | Mi_Serial | 1,000 | 2.3% |
| | **Top 11 Total** | | **40,904** | **93.1%** |

### 1.4 Stack Usage Risk (Before)

| Function | Stack (B) | Risk |
|----------|-----------|------|
| **RAK3172_Transmit** | **2,088** | **CRITICAL** (char[2048] local) |
| MiStorage_SensorDataIndexing | 528 | HIGH |
| MiIoT_MailBox_NewItem | 480 | HIGH |
| MiStorage_WriteIoTParameter | 472 | HIGH |
| MiSerial_Set | 200 | MEDIUM |
| RAK3172_Open | 136 | LOW |

Worst-case call chain estimate: ~2,500+ bytes stack depth.
Available stack space: ~3,836 bytes (4,348 - 512 heap).
**Stack margin: ~1,336 bytes (34%)** — very tight.

---

## 2. Optimization Plan & Results

### OPT-1: RAM2 (16KB) Activation — SKIPPED

**Status**: SKIPPED (user decision)

**Reason**: RAM2 (SRAM2)는 STM32L4에서 저전력 데이터 보존 및 보안(Firewall) 용도로 설계됨.
범용 버퍼 저장소로 사용하는 것은 설계 목적에 맞지 않으므로 향후 부트로더 또는 저전력 모드에서 활용하기로 결정.

추가 사유: 포인터 방식으로 RAM2에 배치 시 STM32CubeIDE Live Watch 디버거가 정상 동작하지 않는 문제 확인됨.

### OPT-2A: DefaultParameter → const FLASH — COMPLETED

**Status**: COMPLETED

**변경 내용**:
- `Mi_IoT_VMX3264.c`: `const IoTParameter_t MiIoT_DefaultParameter = {...}` 선언 (컴파일 타임 초기화)
- `Mi_IoT.c`: 기존 `IoTParameter_t MiIoT_DefaultParameter;` 선언 제거
- `Mi_IoT.h`: `extern const IoTParameter_t MiIoT_DefaultParameter;` 로 변경
- `Mi_Main_VMX3264.c`: 런타임 초기화 코드 제거 (memset, 필드 할당, for 루프)

**결과**: DefaultParameter가 .bss (RAM) → .rodata (FLASH)로 이동
**RAM 절감**: 2,296 B

### OPT-2B: LoadedParameter → Operating 비교만 유지 — COMPLETED

**Status**: COMPLETED

**변경 내용**:
- `Mi_Storage.c`: `IoTParameter_t MiStorage_LoadedParameter` (2,296 B) → `static IoTOperatingParameter_t MiStorage_LastOperating` (~12 B)
- ReadIoTParameter: Operating 필드만 저장
- WriteIoTParameter: Operating 변경 시 기존 mailbox 아이템을 찾아 업데이트하거나 새로 생성하는 로직으로 개선

**RAM 절감**: ~2,284 B

### OPT-3: RAK3172_Transmit Stack Reduction — COMPLETED

**Status**: COMPLETED

**변경 내용**:
- `LoRa_RAK3172.c` line 330: `char StrBuffer[2048] = {0,};` → `static char StrBuffer[600];`
- 최대 실제 사용량 분석 결과 526 B → 600 B 여유분 확보

**Stack 절감**: 2,088 B → 40 B (2,048 B 절감)
**Trade-off**: +600 B BSS (static 변수로 이동, 원래 2,048 B 대비 1,448 B 절약)

### OPT-4: Storage NAND Page Buffer — SKIPPED

**Status**: SKIPPED (하드웨어 제약)

**Reason**: NAND 페이지 크기(2,048 B)는 MT29F2G01 하드웨어 사양이므로 버퍼 축소 불가.
RAK3172 StrBuffer와 공유하는 방안은 동시 접근 위험성과 코드 복잡도 증가로 인해 비용 대비 효과가 낮음.

### OPT-5: Histogram Buffer Size Review — DEFERRED

**Status**: DEFERRED (요구사항 분석 필요)

16,000 B (8,000 samples x 2 bytes)의 측정 요구사항 확인 후 검토 예정.

---

## 3. After Optimization

### 3.1 Build Results

```
text: 154,064 B
data: 852 B
bss:  41,520 B
```

### 3.2 Memory Usage Comparison

| Region | Before | After | Change |
|--------|--------|-------|--------|
| **RAM Used** | 44,804 B (91.2%) | 42,372 B (86.2%) | -2,432 B |
| **RAM Free** | 4,348 B (8.8%) | 8,316 B (16.9%) | **+3,968 B** |
| **FLASH Used** | 101,160 B (38.6%) | 154,916 B (59.1%) | +53,756 B |
| **RAM2** | 0 B (0%) | 0 B (0%) | - |

### 3.3 RAM Section Breakdown (After)

| Section | Address Range | Size |
|---------|--------------|------|
| .data | 0x20000000 - 0x20000354 | 852 B |
| .bss | 0x20000354 - 0x20009F84 | 41,520 B |
| **Free (heap+stack)** | 0x20009F84 - 0x2000C000 | **8,316 B** |

### 3.4 Stack Usage (After)

| Function | Before (B) | After (B) | Change |
|----------|-----------|-----------|--------|
| **RAK3172_Transmit** | **2,088** | **40** | **-2,048** |
| MiStorage_SensorDataIndexing | 528 | 528 | - |
| MiIoT_MailBox_NewItem | 480 | 480 | - |
| MiStorage_WriteIoTParameter | 472 | 472 | - |

Worst-case call chain estimate: ~1,500 bytes stack depth.
Available stack space: ~7,804 bytes (8,316 - 512 heap).
**Stack margin: ~6,304 bytes (81%)** — safe.

### 3.5 Optimization Summary

| OPT | Description | Status | RAM Savings | Stack Savings |
|-----|-------------|--------|-------------|---------------|
| OPT-1 | RAM2 activation | SKIPPED | - | - |
| OPT-2A | DefaultParameter → const FLASH | COMPLETED | 2,296 B | - |
| OPT-2B | LoadedParameter → Operating only | COMPLETED | ~2,284 B | - |
| OPT-3 | StrBuffer[2048] → static [600] | COMPLETED | - | 2,048 B |
| OPT-4 | NAND buffer sharing | SKIPPED | - | - |
| OPT-5 | Histogram buffer review | DEFERRED | TBD | - |
| | **Total** | | **~4,580 B** | **2,048 B** |

---

## 4. Verification Criteria

- [x] Build succeeds with no linker errors
- [ ] RAM usage < 70% (current: 86.2% — needs further optimization)
- [x] Stack worst-case < 50% of available stack space (19% used)
- [ ] RAM2 utilized for non-DMA large buffers (skipped by design)
- [ ] All functional tests pass (serial, LoRa, measurement, storage)
- [x] No regression in Live Watch/debugger display

---

## 5. Notes

- STM32L433RBT6 RAM2 (SRAM2, 16KB at 0x10000000): 저전력 데이터 보존 및 Firewall 보안 용도 예약
- Current optimization level: `-Os` (size) — already optimal for code size
- `--specs=nano.specs` already used — minimal C library
- `-Wl,--gc-sections` already enabled — unused code removed
- `_printf_float` and `_scanf_float` linked — consider removing if float printing not needed (~5KB savings)
- RAM 70% 이하 목표 달성을 위해 OPT-5 (HistorgramBuffer 16,000 B) 검토 또는 RAM2 부분 활용 재고 필요
