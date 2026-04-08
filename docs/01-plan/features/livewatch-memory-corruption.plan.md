# LiveWatch Memory Corruption Analysis Plan

## Status: RESOLVED - Debugger Artifact (Cause E 확정)
## Created: 2026-03-31
## Updated: 2026-04-01

### Resolution (2026-04-01)
- **Test 3 실행 결과**: 시리얼 출력 uwTick 정상 증가, MailBox 값 정상
- **LiveWatch는 비정상 표시** → DAP/DMA AHB 버스 경합으로 인한 디버거 아티팩트 확정
- **실제 RAM 오염 없음** → 펌웨어 동작에 영향 없음, 수정 불필요
- DMA IT 비활성화(방법 1) 테스트: 첫 실행 정상 → 디버거 재연결 시 재발 → 원복

---

## 1. Problem Statement

### Symptom
- LiveWatch 디버깅 시 `uwTick`, `MiLoRa_IsMailBoxReady` 등 전역변수가 비정상적으로 표시
- `MiLoRa_TimeSync()`과 `MiLoRa_MailBoxTransmitScheduler()` 주석 처리 시 정상
- 두 함수 활성화 시 문제 발현

### Previous Occurrence
- VMX3264 프로젝트에서 동일 증상 발생 이력 (memory-optimization.plan.md)
- 원인: `RAK3172_Transmit`의 `char StrBuffer[2048]` 스택 로컬 변수 -> 스택 오버플로우
- 해결: `static char TxStringBuffer[600]`으로 변경 (스택 2,048B -> BSS 600B)

### New Finding (2026-04-01)
- **USART3 RX DMA를 삭제하면 증상 소멸**
- 그러나 RX DMA 삭제 시 `oAT_Open()` 실패 -> **LoRa 모듈 전체 비활성화**
- 따라서 DMA 직접 원인인지, LoRa 비활성화로 간접 해소인지 구분 필요

---

## 2. Memory Map Analysis (현재 빌드 기준 .map)

### Linker Configuration
| Item | Value |
|------|-------|
| FLASH | 256 KB (0x08000000) |
| RAM | 48 KB (0x20000000) |
| RAM2 | 16 KB (0x10000000, 미사용) |
| _Min_Heap_Size | 0x200 (512 B) |
| **_Min_Stack_Size** | **0x400 (1,024 B)** |

### RAM Layout (현재 .map - RX DMA 포함 빌드)
```
0x20000000 +-- .data start (_sdata)
           |   .data: ~1,028 B
0x20000404 +-- .bss start
           |
           |   0x20000498  MiIoT_Parameter     (~2,296 B)
           |   0x20000dc8  MiIoT_LoRaMailBox   (9,048 B)
           |   0x20003124  MiLoRa_IsMailBoxReady (4 B)
           |
           |   0x200054f8  hdma_usart3_tx      (72 B = DMA_HandleTypeDef)
           |   0x20005540  hdma_usart3_rx      (72 B = DMA_HandleTypeDef)
           |   0x2000558c  uwTick              (4 B)   <-- hdma_usart3_rx 바로 다음!
           |   0x200055bc  RAK3172Dev          (2,220 B = 0x8AC)
           |     +0x007B   Buffer.TxBuffer[400]  (~0x20005637)
           |     +0x020B   Buffer.RxBuffer[800]  (~0x200057C7) <-- DMA RX 대상
           |     +0x052B   Buffer.ResponseBuffer[800]
           |   0x20005E68  (RAK3172Dev end)
           |
~0x20005E6C +-- .bss end / Heap start
           |   [Heap: 512 B min]
           |   [Free: ~24,420 B]
           |   [Stack grows downward]
0x2000C000 +-- Stack top (_estack)
```

### DMA RX 버퍼 오버플로우 검증 결과
- DMA 대상: `RAK3172Dev.Buffer.RxBuffer[800]` (~0x200057C7 ~ 0x20005AE7)
- DMA 모드: CIRCULAR (800바이트 내에서 순환)
- **버퍼는 RAK3172Dev 구조체 내부에 안전하게 위치** -> 오버플로우 없음
- `uwTick`(0x2000558c)과 DMA 버퍼(~0x200057C7) 사이 거리: ~571 바이트
- **DMA 버퍼가 uwTick을 직접 덮어쓰는 것은 불가능**

---

## 3. USART3 DMA 상세 구성

### USART3 TX DMA (DMA1_Channel2)
| 항목 | 값 |
|------|------|
| Instance | DMA1_Channel2 |
| Direction | DMA_MEMORY_TO_PERIPH |
| Mode | DMA_NORMAL |
| Priority | LOW |
| 용도 | AT 명령어 전송 (oAT_TransmitBuffer) |
| ISR | DMA1_Channel2_IRQHandler |

### USART3 RX DMA (DMA1_Channel3) - 삭제 시 증상 해소
| 항목 | 값 |
|------|------|
| Instance | DMA1_Channel3 |
| Direction | DMA_PERIPH_TO_MEMORY |
| Mode | DMA_CIRCULAR |
| Priority | LOW |
| 용도 | AT 응답 수신 (oAT_Open -> HAL_UART_Receive_DMA) |
| ISR | DMA1_Channel3_IRQHandler |
| Buffer | RAK3172Dev.Buffer.RxBuffer[800] |

### RX DMA 삭제 시 부작용 분석
```
oAT_Open() (ONE_ATCommend.c:603):
    if(pAT->pUART->hdmarx == NULL){
        return AT_RESULT_ERROR;     // <-- RX DMA 없으면 여기서 실패
    }

RAK3172_Open() (LoRa_RAK3172.c:1037):
    case 1:
        if(oAT_Open(&RAK3172Dev.AT) == AT_RESULT_OK){
            // ... 성공 시 LoRa 초기화 진행
        }
        else{
            result = RESULT_ERROR;  // <-- LoRa 모듈 전체 비활성화
        }
```
**결론: RX DMA 삭제 = LoRa 전체 비활성화 = TimeSync/MailBox 미실행 = 증상 소멸**
-> RX DMA 직접 원인 확정 불가, LoRa 비활성화로 인한 간접 해소일 가능성 높음

---

## 4. Root Cause Analysis (우선순위 순)

### Cause B: AT 명령 상태머신 동시접근 충돌 (HIGH - 1순위 용의자)

**재분석 결과 여전히 가장 유력한 원인**

```c
void MiLoRa(UART_HandleTypeDef *pUART) {
    MiLoRa_Control();                  // -> oAT_TransmitCommend() -> ProcessStep 사용
    MiLoRa_TimeSync();                 // -> oAT_TransmitCommend() -> ProcessStep 사용
    MiLoRa_MailBoxTransmitScheduler(); // -> oAT_TransmitCommend() -> ProcessStep 사용
    RAK3172();                         // -> oAT_Proc()
}
```

**공유되는 static 변수 (ONE_ATCommend.c):**
1. `RAK3172Dev.AT.ProcessStep` - 모든 oAT_TransmitCommend 호출자가 공유
2. `static ATTransmitBufferStep` (oAT_TransmitBuffer:277) - 모든 호출자 공유
3. `static ATResponseStep` (oAT_WaitForResponse:327) - 모든 호출자 공유

**충돌 시나리오:**
1. MailBoxTransmit -> `oAT_TransmitCommend("AT+SEND=...")` -> ProcessStep=2
2. 같은 루프에서 TimeSync -> `oAT_TransmitCommend("AT+LTIME=?")` 진입
3. ProcessStep이 이미 2이므로 case 2로 진입 -> TX 버퍼에 다른 명령 추가
4. ResponseFilter 덮어쓰기 -> 타임아웃 발생 -> 상태 혼란

**보호 메커니즘 분석:**
| 함수 | IsBusy 체크 | 보호 상태 |
|------|------------|----------|
| RAK3172_Transmit | step 0에서 `!IsBusy` 체크 | **보호됨** |
| RAK3172_GetLocalTime | IsBusy 체크 **없음** | **취약** |
| RAK3172_Join | IsBusy 체크 **없음** | **취약** |

### Cause E: DMA CIRCULAR + 디버거 DAP 버스 경합 (MEDIUM)

- USART3 RX DMA가 CIRCULAR 모드로 AHB 버스 지속 점유
- LiveWatch는 DAP(Debug Access Port) 통해 SRAM 읽기
- DMA 활동 시 DAP 읽기 지연 -> LiveWatch에 잘못된 값 표시
- **실제 메모리 오염이 아닌 디버거 표시 문제일 가능성**
- 모든 인터럽트 우선순위 (0,0) -> SysTick과 DMA 인터럽트 간 경쟁

### Cause C: oSerial_Log 스택 사용 (MEDIUM)
- `oSerial_vPrint`에서 `char Buffer[100]` (스택 100B)
- `char fmt[24]` (스택 24B)
- TimeSync + MailBox 동시 활성 시 로그 빈도 증가

### Cause D: calloc/free 힙 단편화 (LOW)
- `oAT_ReadLine`에서 `calloc(length+1)` -> `free()` 반복
- 힙 512B, DMA 수신 버퍼 최대 800B -> 할당 실패 가능
- NULL 체크 있으므로 크래시는 아님, 데이터 유실 가능

---

## 5. 진단 테스트 계획

### Test 1: DMA 직접 원인 vs LoRa 비활성화 구분 (최우선)
**목적:** RX DMA 삭제 시 증상 해소가 DMA 자체 문제인지 LoRa 비활성화 때문인지 구분

**방법:** USART3 RX를 DMA 대신 인터럽트 방식으로 변경
1. `oAT_Open()`에서 `HAL_UART_Receive_DMA()` 대신 `HAL_UART_Receive_IT()` 사용
2. RX 완료 콜백에서 링버퍼 직접 관리
3. LoRa 기능은 정상 유지, DMA만 제거

**예상 결과:**
- 증상 해소 -> DMA CIRCULAR 모드가 원인 (Cause E 확정)
- 증상 지속 -> AT 명령 동시접근이 원인 (Cause B 확정)

### Test 2: AT 명령 직렬화 보호 (Cause B 검증)
**목적:** AT 명령 동시접근이 원인인지 확인

**방법:** `RAK3172_GetLocalTime()`과 `RAK3172_Join()`에 IsBusy 체크 추가
```c
// RAK3172_GetLocalTime 시작부
if(RAK3172Dev.Status.IsBusy) {
    return RESULT_RUN;  // 다른 AT 명령 진행 중이면 대기
}
```

**예상 결과:**
- 증상 해소 -> Cause B 확정
- 증상 지속 -> DMA 또는 다른 원인

### Test 3: 디버거 아티팩트 vs 실제 오염 구분
**목적:** LiveWatch 표시 문제인지 실제 RAM 오염인지 확인

**방법:**
1. 메인루프에서 `uwTick`을 주기적으로 시리얼 출력 (oSerial_Log)
2. LiveWatch 값과 시리얼 출력 값 비교
3. 실제 기능(타이머, 타임아웃)이 정상 동작하는지 확인

**예상 결과:**
- 시리얼 정상 + LiveWatch 비정상 -> Cause E (디버거 아티팩트)
- 시리얼도 비정상 -> 실제 RAM 오염 (Cause B 또는 다른 원인)

---

## 6. 수정 방안

### Phase 1: Cause B 수정 (HIGH - 즉시 적용 권장)

#### 1-1. IsBusy 보호 추가
```c
// LoRa_RAK3172.c - RAK3172_GetLocalTime
oResult_t RAK3172_GetLocalTime(oDateAndTime_t *pDT, oUtcOffsetMinute_t UtcOffset)
{
    if(RAK3172Dev.Status.IsBusy) {
        return RESULT_RUN;
    }
    // ... 기존 코드 ...
}
```

#### 1-2. static 변수 재진입 문제 해결 (ONE_ATCommend.c)
- `oAT_TransmitBuffer()`의 `static ATTransmitBufferStep`
- `oAT_WaitForResponse()`의 `static ATResponseStep`
- 이들을 `oATCommend_t` 구조체 멤버로 이동 (인스턴스별 독립 상태)

### Phase 2: DMA 인터럽트 우선순위 조정 (MEDIUM)
- SysTick 우선순위를 DMA보다 높게 설정 (숫자 낮을수록 높은 우선순위)
```c
// MX_DMA_Init() 또는 SystemInit 이후
HAL_NVIC_SetPriority(DMA1_Channel3_IRQn, 1, 0);  // RX DMA 우선순위 낮춤
HAL_NVIC_SetPriority(DMA1_Channel2_IRQn, 1, 0);  // TX DMA 우선순위 낮춤
// SysTick은 기본 (0,0) 유지 -> SysTick이 DMA보다 우선
```

### Phase 3: calloc 제거 (MEDIUM)
- `oAT_ReadLine()`의 `calloc(length+1)` -> static 버퍼 사용
- 512B 힙에서 최대 800B 할당 시도는 항상 실패
```c
// oAT_ReadLine 수정
static char ReadBuffer[256];  // 적절한 크기의 static 버퍼
// calloc/free 제거
```

---

## 7. 권장 실행 순서

1. **Test 3** 먼저 수행 (5분) -> 디버거 아티팩트 여부 즉시 확인
2. **Phase 1** (Cause B 수정) 적용 -> 가장 유력한 원인 해결
3. **Phase 2** (DMA 우선순위) 적용 -> 디버거 안정성 향상
4. Test 1/Test 2로 추가 검증 (필요시)

---

## 8. Previous Fixes Applied (변경 없음)

### Cause A: 스택 로컬 대형 버퍼 (COMPLETED)
- `RAK3172_ReceiveCallback`의 `char payload[601]` + `uint8_t Buffer[300]` -> static 전환
- 적용 완료, 하지만 증상 미해결

---

## 9. References
- VMX3264 memory-optimization.plan.md: 동일 MCU/아키텍처의 메모리 최적화 이력
- SIV100 lora-optimization.plan.md: LoRa 최적화 이력
- lora-join-failure.plan.md: Join 실패 분석 (Cause E - NJS 세션 문제)
- memory-safety-fix.plan.md: 21개 파일 정적 분석 결과 (161건 발견, 19건 수정 완료)
