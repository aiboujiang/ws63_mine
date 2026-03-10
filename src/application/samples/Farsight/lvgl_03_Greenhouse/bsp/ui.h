/*
 * UI.h
 *
 *  Created on: Jul 1, 2024
 *      Author: W
 */

#ifndef UI_UI_H_
#define UI_UI_H_

#include "cmsis_os2.h"
#include "../lvgl_greenhouse_demo.h"

#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include "ctype.h"
#include <stdbool.h>

#define PWM_DUTY1		500			/* 一档强度 */
#define PWM_DUTY2		700
#define PWM_DUTY3		999

enum SensorType{
	IO,
	AD
};

enum ModeType{
	USR,
	SMART,
};

enum MQTTState{
	DISCONNECTED,
	CONNECTED,
	NOT
};

struct Threshold_t
{
    char Temp;
    char Humid;
};

enum FANSTATE{
	FANSPREEDOFF,
	FANSPREED1 ,
	FANSPREED2,
	FANSPREED3,

};
#define WifiUsrname  "1001"       //WiFi名称
#define WifiPassword "HQYJ12345678"        //WiFi密码

void UI_Init(void);
#endif /* UI_UI_H_ */
