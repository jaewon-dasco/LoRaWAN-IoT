/*
 * ONE_IoControl.c
 *
 *  Created on: Dec 5, 2024
 *      Author: JONE
 */
#include "ONE_Common.h"
#include "ONE_Time.h"
#include "ONE_Math.h"
#include "ONE_Signal.h"

uint8_t oThreashold(double Signal, oThreshold_t *pThreashold)
{
    if(pThreashold == NULL){
        return 0;
    }
    if(pThreashold->LowLevel == pThreashold->HighLevel){
        pThreashold->State = 0;
        return 0;
    }

    if(pThreashold->State){
        if(Signal < pThreashold->LowLevel){
            if(!pThreashold->DelayTime){
                pThreashold->State = 0;
            }
            else if(oTMR_Elapsed(&pThreashold->Timer, pThreashold->DelayTime, TICKBASE_SYSTICK)){
                pThreashold->State = 0;
                pThreashold->Timer = oTMR_GetTick(TICKBASE_SYSTICK);
            }
        }
        else{
            pThreashold->Timer = oTMR_GetTick(TICKBASE_SYSTICK);
        }
    }
    else{
        if(Signal > pThreashold->HighLevel){
            if(!pThreashold->DelayTime){
                pThreashold->State = 1;
            }
            else if(oTMR_Elapsed(&pThreashold->Timer, pThreashold->DelayTime, TICKBASE_SYSTICK)){
                pThreashold->State = 1;
                pThreashold->Timer = oTMR_GetTick(TICKBASE_SYSTICK);
            }
        }
        else{
            pThreashold->Timer = oTMR_GetTick(TICKBASE_SYSTICK);
        }
    }

    return pThreashold->State;
}

double oLinear(oLinear_t *pLin, double Input, uint8_t RangeLimit)
{
	double Gain = 0;
	double Offset = 0;

	if(LINEAR_IS_EMPTY(pLin)){
		return Input;
	}

	if(pLin->InMax == pLin->InMin){
		return Input;
	}

	Gain = (pLin->OutMax-pLin->OutMin) / (pLin->InMax-pLin->InMin);
	Offset = pLin->OutMin - (pLin->InMin*Gain);

	if(RangeLimit){
		Input = MATH_LIMIT(Input, pLin->InMin, pLin->InMax);
	}

	return Input * Gain + Offset;
}

void oBlink(oBlinker_t *Blinker)
{
    if(Blinker == NULL){
        return;
    }

    if(!Blinker->Enable || Blinker->Pulse.Frequency == 0 || Blinker->Pulse.Pattern == 0 || Blinker->Pulse.Length == 0){
        Blinker->Output = 0;
        Blinker->IndexOfPulse = 0;
        Blinker->Timer = oTMR_GetTick(TICKBASE_SYSTICK); // oTMR_ResetNow(&Blinker->Timer); → 대입으로 변경
    }
    else if(Blinker->Pulse.Frequency > 1000){
        Blinker->Output = 1;
    }
    else{
        Blinker->Pulse.Length = MATH_MIN(Blinker->Pulse.Length, BLINK_MAXLENGTH);

        if(Blinker->IndexOfPulse < SIGNAL_PATTERN_MAXSIZE){
            Blinker->Output = (uint8_t)(Blinker->Pulse.Pattern >> Blinker->IndexOfPulse) & 0x01;
        }
        else{
            Blinker->Output = 0;
        }

        if(oTMR_Trigger(&Blinker->Timer, (uint32_t)(1000.0/Blinker->Pulse.Frequency), True, TICKBASE_SYSTICK)){
            Blinker->IndexOfPulse += 1;
        }
    }

    if(Blinker->Reverse){
        Blinker->Output = !Blinker->Output;
    }

    Blinker->IsEndOfIndex = Blinker->IndexOfPulse >= Blinker->Pulse.Length;

    if(Blinker->IsEndOfIndex){
        Blinker->IndexOfPulse = 0;
        Blinker->IsEndOfIndex = 1;
    }
}

/*
 * Edge | 0=falling | 1=rising
 */
uint8_t oTrigger(oTrig_t *pTrig, uint8_t Signal)
{
	if(pTrig == NULL){
		return 0;
	}

	pTrig->Input = Signal;

	if(pTrig->PastSignal != pTrig->Input){
		if((pTrig->Edge == 0 && pTrig->Input == 0) || (pTrig->Edge != 0 && pTrig->Input != 0)){
			pTrig->Output = 1;
			pTrig->Count++;
		}
	}
	else{
		pTrig->Output = 0;
	}

	pTrig->PastSignal = pTrig->Input;

	return pTrig->Output;
}

uint8_t oDebounce(oDebounce_t *pDebounce, uint8_t Signal)
{
    if(pDebounce == NULL){
        return 0;
    }

    pDebounce->Input = Signal;

    if(pDebounce->Reset){
        pDebounce->Input = 0;
        pDebounce->Output = 0;
        pDebounce->Reset = 0;
        pDebounce->Timestamp = oTMR_GetTick(TICKBASE_SYSTICK); // oTMR_ResetNow(&pDebounce->Timestamp); → 대입으로 변경
    }
    else if(pDebounce->Input && !pDebounce->Output){
        if(oTMR_Elapsed(&pDebounce->Timestamp, pDebounce->RisingDelayTime, TICKBASE_SYSTICK)){
            pDebounce->Output = 1;
        }
    }
    else if(!pDebounce->Input && pDebounce->Output){
        if(oTMR_Elapsed(&pDebounce->Timestamp, pDebounce->FallingDelayTime, TICKBASE_SYSTICK)){
            pDebounce->Output = 0;
        }
    }
    else{
        pDebounce->Timestamp = oTMR_GetTick(TICKBASE_SYSTICK); // oTMR_ResetNow(&pDebounce->Timestamp); → 대입으로 변경
    }

    return pDebounce->Output;
}



#ifdef GPIOA
uint8_t oTrigger_GPIO(oIO_t IO, oTrig_t *pTrig, oDebounce_t* pDebounce)
{
	if(pDebounce == NULL){
		return 0;
	}

	oDebounce_GPIO(IO, pDebounce);

	return oTrigger(pTrig, pDebounce->Output);
}
uint8_t oDebounce_GPIO(oIO_t IO, oDebounce_t *pDebounce)
{
	if(IO.Port == NULL || pDebounce == NULL){
		return 0;
	}

	pDebounce->Input = IO_READ(IO);

	oDebounce(pDebounce, pDebounce->Input);

	return pDebounce->Output;
}
void oBlink_GPIO(oIO_t IO, oBlinker_t *Blinker)
{
	if(IO.Port == NULL || Blinker == NULL){
		return;
	}

	oBlink(Blinker);

	IO_WRITE(IO, Blinker->Output);
}

#endif


uint8_t oDelay_Rising(oDelay_t *pDelay)
{
    if(pDelay == NULL){
        return 0;
    }
    if(!pDelay->Enable){
        pDelay->Timestamp = oTMR_GetTick(TICKBASE_SYSTICK);
        return 0;
    }

    if(!pDelay->Input){
        pDelay->Output = 0;
        pDelay->Timestamp = oTMR_GetTick(TICKBASE_SYSTICK); // oTMR_ResetNow(&pDelay->Timestamp); → 대입으로 변경
    }
    else if(!pDelay->Output){
        pDelay->Output = oTMR_Elapsed(&pDelay->Timestamp, pDelay->DelayTime, TICKBASE_SYSTICK) != 0 ? 1 : 0;
    }

    return pDelay->Output;
}

uint8_t oDelay_Falling(oDelay_t *pDelay)
{
    if(pDelay == NULL){
        return 0;
    }
    if(!pDelay->Enable){
        pDelay->Timestamp = oTMR_GetTick(TICKBASE_SYSTICK);
        return 0;
    }

    if(pDelay->Input){
        pDelay->Output = 1;
        pDelay->Timestamp = oTMR_GetTick(TICKBASE_SYSTICK); // oTMR_ResetNow(&pDelay->Timestamp); → 대입으로 변경
    }
    else if(pDelay->Output){
        pDelay->Output = oTMR_Elapsed(&pDelay->Timestamp, pDelay->DelayTime, TICKBASE_SYSTICK) != 0 ? 0 : 1;
    }

    return pDelay->Output;
}

uint8_t oDelay_AutoOff(oDelay_t *pDelay)
{
    if(pDelay == NULL){
        return 0;
    }
    if(!pDelay->Enable){
        pDelay->Timestamp = oTMR_GetTick(TICKBASE_SYSTICK);
        return 0;
    }

    pDelay->Output = pDelay->Input;

    if(!pDelay->Input){
        pDelay->Timestamp = oTMR_GetTick(TICKBASE_SYSTICK); // oTMR_ResetNow(&pDelay->Timestamp); → 대입으로 변경
    }
    else if(oTMR_Elapsed(&pDelay->Timestamp, pDelay->DelayTime, TICKBASE_SYSTICK)){
        pDelay->Output = 0;
    }

    return pDelay->Output;
}

/* History

2026-06-26 | v0.1
	- baseline (ONE_Signal.c)
*/
