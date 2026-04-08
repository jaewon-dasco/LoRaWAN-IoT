# Plan: MT29F2G01A NAND Driver Bug Fix

## Overview
MT29F2G01ABAGDWB SPI NAND driver (`NAND_MT29F2G01ABAGDWB_IT.c`) 전체 코드 리뷰 결과 발견된 버그 수정

## Current Symptom
- `Init[3] SetFeature FAIL` - NAND Init 단계 3에서 Protection register 설정 실패
- NAND Open 실패로 전체 NAND 기능 비활성화

## QSPI Hardware Configuration (main.c)
- MCU: STM32L433RBT7
- ClockPrescaler: 2 (~26.67MHz @ 80MHz HCLK)
- ClockMode: QSPI_CLOCK_MODE_3 (CPOL=1, CPHA=1)
- DualFlash: QSPI_DUALFLASH_DISABLE (Single flash mode)
- FlashID: QSPI_FLASH_ID_1
- QSPI Pins: PA3(CLK), PA6(IO3), PA7(IO2), PB0(IO1), PB1(IO0), PB11(NCS)
- NAND Enable: PC12 (Open Drain, initially HIGH)

## Discovered Bugs

### BUG-01: WriteMemory returns wrong result type [Critical]
- **File**: NAND_MT29F2G01ABAGDWB_IT.c:308
- **Issue**: `MT29F2G_Result_t` function returns `RESULT_ERROR` (oResult_t = -1)
- **Should be**: `MT29F2G_RESULT_PARAMETER_ERROR` (MT29F2G_Result_t = -1)
- **Impact**: Works by coincidence (same value -1), but violates type safety

### BUG-02: ReadByte/WriteByte boundary check off-by-one [Critical]
- **File**: NAND_MT29F2G01ABAGDWB_IT.c:687, 717
- **Issue**: `Block > MT29F2G_BLOCK_LENGTH` allows Block=2048 (invalid)
- **Should be**: `Block >= MT29F2G_BLOCK_LENGTH`
- **Same for**: `Page > MT29F2G_PAGE_LENGTH` allows Page=64 (invalid)
- **Impact**: Out-of-bounds NAND access, potential data corruption

### BUG-03: ReadByte/WriteByte Size clamping ignores ByteIndex [Medium]
- **File**: NAND_MT29F2G01ABAGDWB_IT.c:699, 729
- **Issue**: `MATH_MIN(Size, MT29F2G_MAINDATA_LENGTH)` doesn't consider ByteIndex offset
- **Should be**: `MATH_MIN(Size, MT29F2G_MAINDATA_LENGTH - ByteIndex)`
- **Impact**: Page boundary overflow when ByteIndex > 0

### BUG-04: Init sequence missing Configuration register (0xB0) setup [Critical]
- **File**: NAND_MT29F2G01ABAGDWB_IT.c:487-562
- **Issue**: Init only sets Protection register (0xA0), NOT Configuration register (0xB0)
- **Missing**: QE (Quad Enable) bit for Quad I/O mode
- **Missing**: ECC_EN bit explicit verification
- **Impact**: READ_MODE=4, WRITE_MODE=4 use Quad commands but NAND may not be in Quad mode
- **Note**: Some MT29F2G variants support Quad commands without QE - needs datasheet verification

### BUG-05: GetInfo writes to 0xB0 without WriteEnable [Medium]
- **File**: NAND_MT29F2G01ABAGDWB_IT.c:436-456
- **Issue**: SET FEATURES (0x1F) to Configuration register (0xB0) called without prior WriteEnable
- **Impact**: GetInfo function may silently fail to modify configuration

### BUG-06: Init SetFeature failure - root cause TBD [Blocking]
- **Current log**: `Init[3] SetFeature FAIL` (error code format already added, awaiting test)
- **Possible causes**:
  - WriteEnable AUTOPOLLING_ERROR (-9): NAND WEL bit not setting
  - WriteEnable WREN_FAIL (-13): WEL set then cleared
  - SetFeature TX_ERROR (-6): QSPI HAL command/transmit error
- **/WP pin concern**: PA7 (QUADSPI_BK1_IO2) doubles as NAND /WP pin.
  In non-Quad mode, this pin's state is undefined by QSPI peripheral.
  If LOW, NAND hardware write protect may be active.

## Fix Priority & Order

### Phase 1 - Immediate code fixes (no hardware dependency)
1. BUG-01: Fix WriteMemory return type
2. BUG-02: Fix boundary check off-by-one
3. BUG-03: Fix Size clamping with ByteIndex
4. BUG-05: Fix GetInfo WriteEnable

### Phase 2 - Init sequence improvement
5. BUG-04: Add Configuration register (0xB0) setup step in Init
   - Read current 0xB0 value after Reset
   - Enable ECC_EN (bit 4)
   - Enable QE (bit 0) if needed for Quad mode
   - Add as new Init step between current step 3 and step 4

### Phase 3 - Debug & verify SetFeature failure
6. BUG-06: Test with error code log, analyze exact failure point
   - If AUTOPOLLING_ERROR: investigate /WP pin state
   - If TX_ERROR: investigate QSPI state after Reset

## oResult_t Values Reference
```
RESULT_FAULT = -9, RESULT_OK = -4, RESULT_RUN = -3, RESULT_ERROR = -1, RESULT_NULL = 0
```

## MT29F2G_Result_t Values Reference
```
FATAL_FAULT=-14, WREN_FAIL=-13, ECC_ERROR=-12, ERASE_FAIL=-11,
PROGRAM_FAIL=-10, AUTOPOLLING_ERROR=-9, STATE_FAIL=-8,
RX_ERROR=-7, TX_ERROR=-6, COM_ERROR=-5, TIMEOUT=-4,
BADBLOCK=-3, BUSY=-2, PARAMETER_ERROR=-1, NULL=0, RUN=1, OK=2
```
