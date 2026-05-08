/*
 * ONE_Math.h
 *
 *  Created on: Feb 7, 2025
 *      Author: JONE
 */

#ifndef INC_ONE_MATH_H_
#define INC_ONE_MATH_H_

#include "ONE_Common.h"

#define MATH_PI 						3.14159265358979323846

#define MATH_ABS(x)							((x) < 0 ? -(x) : (x))
#define MATH_MAX(a, b)						((a) > (b) ? (a) : (b))
#define MATH_MIN(a, b)						((a) < (b) ? (a) : (b))
#define MATH_LIMIT(x, low, high)			(low==high ? x : MATH_MIN(high, MATH_MAX(x, low)))
#define MATH_INTERVAL(a, b)					(MATH_MAX(a,b)-MATH_MIN(a,b))

//Round define
#define MATH_ROUND(x, n)  (round((x) * pow(10, n)) / pow(10, n))
#define MATH_ROUNDUP(x, n)  (ceil((x) * pow(10, n)) / pow(10, n))
#define MATH_ROUNDDOWN(x, n)   (floor((x) * pow(10, n)) / pow(10, n))

//Conversion define
#define RADIAN_TO_DEGREE(x)  ((x) * (180.0 / MATH_PI))
#define DEGREE_TO_RADIAN(x)  ((x) * (MATH_PI / 180.0))

#define VOLT_DIV_RATIO(R1, R2) ((double)(R2)/((double)(R1) + (double)(R2)))
#define VOLT_TO_LOW_RESISTANCE(Vin, Vout, PullUp) (Vout == Vin ? 0 : ((double)PullUp * (double)Vout) / ((double)Vin - (double)Vout))
#define VOLT_TO_HIGH_RESISTANCE(Vin, Vout, PullDown) (Vout == 0 ? 0 : (((double)Vin / (double)Vout -1)) * (double)PullDown)

#define RPM_TO_FREQUENCY(Rpm) ((double)Rpm / 60)
#define COUNT_TO_FREQUENCY(Period, Count) ((double)Count/((double)Period/1000))

#define NTC_3K_TO_CELSIUS(R)		MATH_MAX(((1.0 / ( (1.0 / 298.15) + (1.0 / 3950.0) * log((double)(R) / 3000.0) )) - 273.15), -39.9)
#define NTC_10K_TO_CELSIUS(R)		MATH_MAX(((1.0 / ( (1.0 / 298.15) + (1.0 / 3950.0) * log((double)(R) / 10000.0) )) - 273.15), -39.9)
#define RTD_PT1000_TO_CELSIUS(R)	(((double)(R) - 1000.0) / (1000.0 * 0.00385))
#define RTD_PT100_TO_CELSIUS(R)		(((double)(R) - 100.0) / 0.385)

#define RANGE_INITIALIZER()	{(double)(1.7976931348623157e+308), (double)(-1.7976931348623157e+308)}

typedef struct{
	int32_t X;
	int32_t Y;
}oPoint2D_t;

typedef struct{
	int32_t X;
	int32_t Y;
	int32_t Z;
}oPoint3D_t;

typedef struct{
	double Min;
	double Max;
}oRange_t;

typedef struct{
	double X;
	double Y;
}oVector2_t;

typedef struct{
	double X;
	double Y;
	double Z;
}oVector3_t;

typedef struct{
	double Pitch;
	double Roll;
	double Yaw;
}oRPY_t;

extern oRPY_t oMath_AccelationToRPY(oVector3_t Acc);
extern double oMath_Polynomial(double InData, double Polynomial[], uint8_t NumberOfPolynomial);
extern double oMath_Median3(double a, double b, double c);

#endif /* INC_ONE_MATH_H_ */
