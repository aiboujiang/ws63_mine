/*
 * UI.h
 *
 *  Created on: Jul 1, 2024
 *      Author: W
 */

#ifndef UI_UI_H_
#define UI_UI_H_

#include "cmsis_os2.h"
#include "../lvgl_light_demo.h"

#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include "ctype.h"
#include <stdbool.h>
enum LightState{
	LIGHTOFF,
	LIGHTON,
};

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

#define WifiUsrname  "3863"       //WiFi名称
#define WifiPassword "HQYJ12345678"        //WiFi密码

void UI_Init(void);
#endif /* UI_UI_H_ */
