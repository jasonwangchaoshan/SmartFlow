/*
 * AD_for_ temperature_power.h
 *
 *  Created on: 2017Äê12ÔÂ16ÈÕ
 *      Author: DVT-FW
 */

//#ifndef EXAMPLES_GET_STARTED_HELLO_WORLD_MAIN_AD_FOR__TEMPERATURE_POWER_H_
//#define EXAMPLES_GET_STARTED_HELLO_WORLD_MAIN_AD_FOR__TEMPERATURE_POWER_H_

#ifndef _AD_FOR_TEMPERATURE_POWER_H_
#define _AD_FOR_TEMPERATURE_POWER_H_
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#define TEMPERATURE_TAB_NUM  310

extern const unsigned int  K_temperature_table[TEMPERATURE_TAB_NUM];



void adc1_task(void *pvParameter);
void adc1_temperature_power();
uint32_t Temperature_test(void);
uint32_t get_temperture(uint32_t temperature_ad);

void ad2_app(void);

#endif /* EXAMPLES_GET_STARTED_HELLO_WORLD_MAIN_AD_FOR__TEMPERATURE_POWER_H_ */



