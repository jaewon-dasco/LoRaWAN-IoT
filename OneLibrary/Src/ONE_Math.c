/*
 * ONE_Math.c
 *
 *  Created on: Feb 7, 2025
 *      Author: JONE
 */
#include "ONE_Math.h"

oRPY_t oMath_AccelationToRPY(oVector3_t Acc)
{
	oRPY_t RPY = {0, 0, 0};

	if(Acc.Y != 0 || Acc.Z != 0){
		RPY.Pitch = RADIAN_TO_DEGREE(atan(Acc.X / sqrtf(pow(Acc.Y,2) + pow(Acc.Z,2))));
	}

	if(Acc.X != 0 || Acc.Z != 0){
		RPY.Roll = RADIAN_TO_DEGREE(atan(Acc.Y / sqrtf(pow(Acc.X,2) + pow(Acc.Z,2))));
	}

	return RPY;
}

double oMath_Polynomial(double InData, double Polynomial[], uint8_t NumberOfPolynomial)
{
	uint8_t IsEmpty = 1;
	double OutData = 0;

	if(Polynomial == NULL || NumberOfPolynomial > 20){
		return InData;
	}

	for(int i=0; i<NumberOfPolynomial; i++)
	{
		OutData +=  Polynomial[i] * pow(InData, (double)i);

		if(Polynomial[i] != 0){
			IsEmpty = 0;
		}
	}

	if(IsEmpty){
		return InData;
	}

	return OutData;
}

double oMath_Median3(double a, double b, double c)
{
    if (a > b){ double t=a; a=b; b=t; }
    if (b > c){ double t=b; b=c; c=t; }
    if (a > b){ double t=a; a=b; b=t; }

	return b;
}
