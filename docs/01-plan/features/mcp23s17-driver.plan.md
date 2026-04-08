# Plan: MCP23S17TE SPI GPIO Expander Driver

## 1. Overview

MCP23S17TE 16-bit SPI GPIO Expander를 `oThirdParty/` 에 드라이버로 구현한다.
기존 NAND(MT29F2G) 드라이버의 코딩 스타일을 따르며, HAL SPI 핸들을 입력받아 초기화하고
사용하기 편한 함수 인터페이스를 제공한다.

## 2. Hardware Context

### 2.1 기존 핀 정의 (main.h)
| 핀 | GPIO | 용도 |
|----|------|------|
| `SPI_SCK_IOEXT_Pin` | PA1 | SPI Clock |
| `SPI_MISO_IOEXT_Pin` | PA11 | SPI MISO |
| `SPI_MOSI_IOEXT_Pin` | PA12 | SPI MOSI |
| `SPI_CS_IOEXT_Pin` | PC7 | Chip Select |
| `DO_IOEXT_RESET_Pin` | PC6 | Hardware Reset |

### 2.2 MCP23S17 Chip 특성
- 16-bit I/O expander (PORTA 8bit + PORTB 8bit)
- SPI 인터페이스 (최대 10MHz)
- 핀별 입/출력 방향 설정
- 핀별 풀업 저항
- 인터럽트 지원 (핀 변화 감지)
- Hardware Address (A2, A1, A0) 핀으로 최대 8개 디바이스 버스 공유

### 2.3 SPI 프로토콜
```
[Control Byte] [Register Address] [Data...]
Control Byte = 0100 A2 A1 A0 R/W
- Write: 0x40 | (addr << 1)
- Read:  0x41 | (addr << 1)
```

### 2.4 주요 레지스터 (IOCON.BANK=0, Sequential mode)
| Register | Addr | 설명 |
|----------|------|------|
| IODIRA   | 0x00 | Port A 방향 (1=Input, 0=Output) |
| IODIRB   | 0x01 | Port B 방향 |
| IPOLA    | 0x02 | Port A 입력 극성 반전 |
| IPOLB    | 0x03 | Port B 입력 극성 반전 |
| GPINTENA | 0x04 | Port A 인터럽트 활성 |
| GPINTENB | 0x05 | Port B 인터럽트 활성 |
| DEFVALA  | 0x06 | Port A 인터럽트 기본값 |
| DEFVALB  | 0x07 | Port B 인터럽트 기본값 |
| INTCONA  | 0x08 | Port A 인터럽트 비교 모드 |
| INTCONB  | 0x09 | Port B 인터럽트 비교 모드 |
| IOCON    | 0x0A | Configuration |
| GPPUA    | 0x0C | Port A 풀업 저항 |
| GPPUB    | 0x0D | Port B 풀업 저항 |
| INTFA    | 0x0E | Port A 인터럽트 플래그 (R) |
| INTFB    | 0x0F | Port B 인터럽트 플래그 (R) |
| INTCAPA  | 0x10 | Port A 인터럽트 캡처 (R) |
| INTCAPB  | 0x11 | Port B 인터럽트 캡처 (R) |
| GPIOA    | 0x12 | Port A 값 |
| GPIOB    | 0x13 | Port B 값 |
| OLATA    | 0x14 | Port A 출력 래치 |
| OLATB    | 0x15 | Port B 출력 래치 |

## 3. Implementation Plan

### 3.1 파일 구조
```
oThirdParty/
├── Inc/
│   └── MCP23S17.h              ← 헤더
└── Src/
    └── MCP23S17.c              ← 구현
```

### 3.2 네이밍 규칙 (NAND 스타일 따름)
- Prefix: `MCP23S17_`
- 타입: `MCP23S17_t` (디바이스 구조체), `MCP23S17_Result_t` (결과 enum)
- 함수: `MCP23S17_Init()`, `MCP23S17_ReadPort()` 등
- 매크로: `MCP23S17_REG_IODIRA`, `MCP23S17_IOCON_MIRROR` 등

### 3.3 디바이스 구조체 (oIO_t 활용)
`ONE_Signal.h`의 `oIO_t` (Port + Pin + ActiveLevel) 를 CS/RST 핀에 재활용:
```c
typedef struct {
    SPI_HandleTypeDef *pSPI;
    oIO_t *pCS;                // CS 핀 포인터 (ActiveLevel: GPIO_PIN_RESET)
    oIO_t *pRST;               // Reset 핀 포인터 (ActiveLevel: GPIO_PIN_RESET)
    uint8_t HWAddress;         // 0~7 (A2,A1,A0)
    uint8_t IsOpen;
} MCP23S17_t;
```
호출부에서 oIO_t를 선언하고 포인터로 전달:
```c
oIO_t csPin  = { SPI_CS_IOEXT_GPIO_Port,    SPI_CS_IOEXT_Pin,    0 };
oIO_t rstPin = { DO_IOEXT_RESET_GPIO_Port,  DO_IOEXT_RESET_Pin,  0 };
MCP23S17_Init(&dev, &hspi, &csPin, &rstPin, 0);
```

### 3.4 구현할 함수 목록

#### Core (Low-level SPI)
| 함수 | 설명 |
|------|------|
| `MCP23S17_WriteRegister(dev, reg, data)` | 단일 레지스터 쓰기 |
| `MCP23S17_ReadRegister(dev, reg, *data)` | 단일 레지스터 읽기 |

#### Init / DeInit
| 함수 | 설명 |
|------|------|
| `MCP23S17_Init(dev, pSPI, pCS(oIO_t*), pRST(oIO_t*), HWAddr)` | 초기화 (Reset→IOCON 설정→전체 레지스터 디폴트) |
| `MCP23S17_DeInit(dev)` | 안전 종료 |
| `MCP23S17_Reset(dev)` | 하드웨어 리셋 |

#### Port Direction
| 함수 | 설명 |
|------|------|
| `MCP23S17_SetPortDirection(dev, port, direction)` | 포트 전체 방향 설정 (8bit) |
| `MCP23S17_SetPinDirection(dev, port, pin, dir)` | 개별 핀 방향 설정 |

#### GPIO Read/Write
| 함수 | 설명 |
|------|------|
| `MCP23S17_ReadPort(dev, port, *data)` | 포트 8bit 읽기 |
| `MCP23S17_WritePort(dev, port, data)` | 포트 8bit 쓰기 |
| `MCP23S17_ReadPin(dev, port, pin, *state)` | 개별 핀 읽기 |
| `MCP23S17_WritePin(dev, port, pin, state)` | 개별 핀 쓰기 |
| `MCP23S17_TogglePin(dev, port, pin)` | 개별 핀 토글 |

#### Pull-up
| 함수 | 설명 |
|------|------|
| `MCP23S17_SetPullUp(dev, port, pullup)` | 포트 풀업 설정 (8bit mask) |

#### Interrupt (Optional)
| 함수 | 설명 |
|------|------|
| `MCP23S17_SetInterrupt(dev, port, mask, mode)` | 인터럽트 설정 |
| `MCP23S17_GetInterruptFlag(dev, port, *flag)` | 인터럽트 플래그 읽기 |
| `MCP23S17_GetInterruptCapture(dev, port, *cap)` | 인터럽트 캡처 읽기 |

### 3.5 Init 구현 방식
NAND 드라이버와 동일한 **static step 기반 비동기 상태머신** 패턴:
```c
oResult_t MCP23S17_Init(MCP23S17_t *pDev, SPI_HandleTypeDef *pSPI,
                        oIO_t *pCS, oIO_t *pRST, uint8_t HWAddr)
{
    static uint8_t InitStep = 0;
    oResult_t result = RESULT_RUN;

    switch(InitStep) {
        case 0: // 핸들 저장, CS/RST 핀 포인터 저장
        case 1: // Hardware Reset (RST Low → 딜레이)
        case 2: // RST High → 안정화 딜레이
        case 3: // IOCON 설정 (HAEN=1 for HW address, SEQOP, MIRROR 등)
        case 4: // 전체 포트 Input 디폴트 설정
        case 5: // 초기화 완료, IsOpen=1
    }

    if(result != RESULT_RUN) InitStep = 0;
    return result;
}
```

### 3.6 CS 핀 제어 패턴 (oIO_t 포인터 경유)
```c
// CS Low (active)
HAL_GPIO_WritePin(pDev->pCS->Port, pDev->pCS->Pin, GPIO_PIN_RESET);
// SPI Transmit/Receive
HAL_SPI_TransmitReceive(pDev->pSPI, txBuf, rxBuf, len, timeout);
// CS High (inactive)
HAL_GPIO_WritePin(pDev->pCS->Port, pDev->pCS->Pin, GPIO_PIN_SET);
```

### 3.7 Port 열거형
```c
typedef enum {
    MCP23S17_PORTA = 0,
    MCP23S17_PORTB = 1,
} MCP23S17_Port_t;
```

## 4. Dependencies

- `main.h` (GPIO 핀 정의)
- `ONE_Common.h` (oResult_t)
- `ONE_Signal.h` (oIO_t)
- `ONE_Serial.h` (oSerial_Log)
- `ONE_Time.h` (oTMR_GetTick, oTMR_Elapsed)
- STM32 HAL SPI (`stm32l4xx_hal_spi.h`)

## 5. Success Criteria

- [x] NAND 드라이버와 동일한 코딩 스타일
- [x] HAL SPI 핸들 + CS/RST GPIO를 Init에서 입력받아 구조체에 저장
- [x] 비동기 상태머신 Init 패턴
- [x] 포트/핀 단위 Read/Write 편의 함수
- [x] NULL 체크, 에러 핸들링
- [x] oSerial_Log 기반 로깅

## 6. Estimated Scope

- 헤더 파일: ~120 lines
- 소스 파일: ~400 lines
- 총 2개 파일 추가
