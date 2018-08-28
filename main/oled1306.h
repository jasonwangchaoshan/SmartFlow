#ifndef _OLED1306_H_
#define _OLED1306_H_


#define OLED_1_ADDRESS   0x3C
#define OLED_2_ADDRESS   0x3D

#define Clock_LINE    	  	0
#define PENSA_ID_LINE    	1
#define FLOW_VALUE_LINE   	2
#define EK_P4_LINE    		3
#define DEVICE_ID_LINE 		4
#define BLUETOOTH_LINE    	5
#define OTA_STATUS_LINE    	6
#define AWS_IOT_LINE   		7











void i2c_oled_init();
void ssd1306_init(uint8_t oled_address);
void task_ssd1306_display_pattern(uint8_t oled_address) ;
void task_ssd1306_display_clear(uint8_t oled_address);
void task_ssd1306_contrast(uint8_t oled_address);
void task_ssd1306_scroll(uint8_t oled_address);
void task_ssd1306_display_text(uint8_t oled_address,const void *arg_text);
void oled_APP(void);
void oled_display(uint8_t oled_address,uint8_t line, char * text ); //显示字符(行，字符)


#endif 
