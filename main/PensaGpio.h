
#ifndef __PensaGpio_H__
#define __PensaGpio_H__

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"



#define LED_ON      0
#define LED_OFF     1

#define Heater_enable      1
#define Heater_disable     0



#define LED_enable      0
#define LED_disable     1


#define GPIO_Green_LED   25 //green_led
#define GPIO_Blue_LED    33 //blue_led
#define GPIO_Red_LED     32 //red_led
#define GPIO_Charging    19 //Chaarging

#define GPIO_LED_EN     27 //LED enable

#define GPIO_Heater_Ctrl     26 //Temperature enable

#define GPIO_OUTPUT_PIN_SEL  ((1ULL<<GPIO_Green_LED) | (1ULL<<GPIO_Blue_LED)| (1ULL<<GPIO_Red_LED) | (1ULL<<GPIO_LED_EN) | (1ULL<<GPIO_Heater_Ctrl))
#define GPIO_INPUT_IO_0     0
#define GPIO_INPUT_IO_22     23
#define GPIO_Microphone     4
#define GPIO_INPUT_PIN_SEL  ((1ULL<<GPIO_INPUT_IO_0) | (1ULL<<GPIO_INPUT_IO_22)| (1ULL<<GPIO_Microphone)| (1ULL<<GPIO_Charging))
#define ESP_INTR_FLAG_DEFAULT 0

void gpio_init(void);

void Green_LED(uint8_t LED_Status);
void Red_LED(uint8_t LED_Status);
void Blue_LED(uint8_t LED_Status);
void LED_EN(uint8_t LED_Status);
void Heater_Ctrl(uint8_t Heater_Status);
void remove_Microphone_intr(void);







#endif /* __ESP_ASSERT_H__ */



