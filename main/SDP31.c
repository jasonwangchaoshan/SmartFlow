#include <string.h>

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/task.h"

#include "sdkconfig.h" // generated by "make menuconfig"

#include "SDP31.h"
#include "oled1306.h"
#include "ssd1366.h"
#include "PensaGpio.h"
#include "uart_echo.h"
#include "sf05.h"
#include "time_ctrl.h"
#include "AD_for_temperature_power.h"
#include "sf05.h"
#include "SDP31.h"

#define ESP_SLAVE_ADDR                     0x28             /*!< ESP32 slave address, you can set any 7bit value */
#define WRITE_BIT                          I2C_MASTER_WRITE /*!< I2C master write */
#define READ_BIT                           I2C_MASTER_READ  /*!< I2C master read */
#define ACK_CHECK_EN                       0x1              /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS                      0x0              /*!< I2C master will not check ack from slave */
#define ACK_VAL                            0x0              /*!< I2C ack value */
#define NACK_VAL                           0x1              /*!< I2C nack value */
uint8_t readData[9]={0,0,0,0,0,0,0,0,0};
#define SDA_SDP31_PIN GPIO_NUM_18
#define SCL_SDP31_PIN GPIO_NUM_5


/*
#define SDP_MEASUREMENT_COMMAND_0    0x3f
#define SDP_MEASUREMENT_COMMAND_1    0xf9 //stop*/

#define SDP_MEASUREMENT_COMMAND_0    0x36
#define SDP_MEASUREMENT_COMMAND_1    0x2f
#define tag "SDP31"
#define BIU16(data, start) (((uint16_t)(data)[start]) << 8 | ((data)[start + 1]))
//float pressure;
void i2c_SDP31_init()
{
	i2c_config_t i2c_config = {
		.mode = I2C_MODE_MASTER,
		.sda_io_num = SDA_SDP31_PIN,
		.scl_io_num = SCL_SDP31_PIN,
		.sda_pullup_en = GPIO_PULLUP_ENABLE,
		.scl_pullup_en = GPIO_PULLUP_ENABLE,
		.master.clk_speed = 50000
	};
	i2c_param_config(I2C_NUM_0, &i2c_config);
	i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);

}

/**
 * @brief test code to write esp-i2c-slave
 *
 * 1. set mode
 *  _______________________________________________________________________________________
 * | start | slave_addr + wr_bit + ack | write 0 byte + ack  | write 1 byte + ack  | stop |
 * --------|---------------------------|---------------------|----------------------------|
 * 2. wait more than 45 ms
 * 3. read data
 * _______________________________________________________________________________________________________________________________
 * | start | slave_addr + rd_bit + ack | read 1 byte + ack  | read 1 byte + nack |read 1 byte + ack  | read 1 byte + nack | stop |
 * --------|---------------------------|--------------------|--------------------|-------------------|--------------------|------|
 */

uint8_t *readSensor() {


	esp_err_t espRc;
    //   uint8_t txData[2] = {SDP_MEASUREMENT_COMMAND_0, SDP_MEASUREMENT_COMMAND_1};
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();

	i2c_master_start(cmd);
	espRc =i2c_master_write_byte(cmd, (SDP31_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);//写地址


	i2c_master_write_byte(cmd, SDP_MEASUREMENT_COMMAND_0, true);//写命令
	i2c_master_write_byte(cmd, SDP_MEASUREMENT_COMMAND_1, true);

  	i2c_master_stop(cmd);
   	espRc = i2c_master_cmd_begin(I2C_NUM_0, cmd, 10 / portTICK_RATE_MS);
   	i2c_cmd_link_delete(cmd);



    	vTaskDelay(50 / portTICK_RATE_MS);

    	cmd = i2c_cmd_link_create();
     	i2c_master_start(cmd);

     	espRc = i2c_master_write_byte(cmd, (SDP31_I2C_ADDRESS << 1) |I2C_MASTER_READ, true);//写地址

	espRc=i2c_master_read(cmd, readData, (uint8_t)9, ACK_VAL);//读数据

	i2c_master_stop(cmd);
	espRc = i2c_master_cmd_begin(I2C_NUM_0, cmd, 10/portTICK_PERIOD_MS);
	i2c_cmd_link_delete(cmd);
	/*printf("readData[0]: %d\n", readData[0]);
	printf("readData[1]: %d\n", readData[1]);
	printf("readData[2]: %d\n", readData[2]);
	printf("readData[3]: %d\n", readData[3]);
	printf("readData[4]: %d\n", readData[4]);
	printf("readData[5]: %d\n", readData[5]);
	printf("readData[6]: %d\n", readData[6]);
	printf("readData[7]: %d\n", readData[7]);
	printf("readData[8]: %d\n", readData[8]);*/
	return readData; 
}

/**********************************************************
 * getPressureDiff
 *  Gets the current Pressure Differential from the sensor.
 *
 * @return float - The Pressure in Pascal
 **********************************************************/
float getPressureDiff(void)
{
  int16_t dp_ticks;
  // merge chars to one int
  dp_ticks = BIU16(readSensor(), 0);
//	printf("readData[0]: %d\n", readData[0]);
//	printf("readData[1]: %d\n", readData[1]);
  dp_ticks =(readData[0]<<8 | readData[1]);
  // adapt scale factor according to datasheet (here for version SDP31)
  float dp_scale = 60.0;//dp_scale 是比例因子
  return dp_ticks/dp_scale;

}
/**********************************************************
 * getTemparature
 *  Gets the current Temparature from the sensor.
 *
 * @return float - The Temparature
 **********************************************************/
float getTemperature(void)
{
  int16_t  temperature_ticks;
  // merge chars to one int
  temperature_ticks = BIU16(readSensor(), 3);
  float t_scale = 200.0;//t_scale 是比例因子
  return temperature_ticks/t_scale;
}

float flowcalculate(float flow)
{
	if(flow==0) return 0;
	else if(flow<=0.15) return 1.1;
	else if(flow<=0.32) return 2.1;
	else if(flow<=0.54) return 3.2;
	else if(flow<=0.79) return 4.3;
	else if(flow<=1.07) return 5.4;
	else if(flow<=1.39) return 6.4;
	else if(flow<=1.75) return 7.5;
	else if(flow<=2.13) return 8.6;
	else if(flow<=2.52) return 9.7;
	else if(flow<=2.93) return 10.7;
	else if(flow<=4.08) return 12.9;
	else if(flow<=5.32) return 15.0;
        else return 17.2;

}
float getpressure(void)
{
  float pressure;
 // i2c_SDP31_init();
  pressure=getPressureDiff();//获取气压值
  return  pressure;
}
void task_SDP31_contrast(void *pvParameters) {

	//i2c_SDP31_init();
	float temp=0;
	float flow=0;float pressure;
	float FlowAll=0;
	float Flow2=0;
	char buf1[50];//用于打印数据
	char buf2[50];//用于打印数据
	char buf3[50];//用于打印数据
	char buf4[50];//用于打印数据
	char buf5[50];//用于打印数据
     while(1)
	{
	temp=getTemperature();//获取温度值
	pressure=getPressureDiff();//获取气压值
	flow=flowcalculate(pressure);//查表得出气流值
        Flow2= SF05_GetFlow2();
        sprintf(buf4,"   Flow2:%05.2f    ",Flow2);

	if(pressure>=0.01){FlowAll=FlowAll+flow;}
	else if(pressure<=0.001)FlowAll=0;
	//printf("getPressureDiff: %f\n", pressure);
	//printf("getTemperature: %f\n", temp);
	//printf("FlowAll: %f\n", FlowAll);
	sprintf(buf1," Tempera:%03.2f   ",temp);      //OLED显示
//	sprintf(buf2,"Pressure:%03.2f",pressure); //OLED显示

	sprintf(buf2,"   Flow:%03.2f      ",pressure); //OLED显示

        if(gpio_get_level(GPIO_Microphone))  Heater_Ctrl(Heater_enable);
        else    Heater_Ctrl(Heater_disable);
      //  if(gpio_get_level(GPIO_Charging))  
      //          oled_display(OLED_1_ADDRESS,0,"Charging   ---> ");
      //  else    oled_display(OLED_1_ADDRESS,0,"                ");     
        
	//sprintf(buf3,"FlowAll:%-03.2f",4.3*FlowAll); //OLED显示

	printf("SF05_GetFlow2:%05.2f \n", Flow2);
	printf("getPressureDiff: %03.2f\n", pressure);
	printf("Tempera: %03.2f\n", temp);
	//uart_my_printf(buf3,strlen(buf3));
	//uart_my_printf(buf5,strlen(buf5));
//	oled_display(OLED_1_ADDRESS,0,"                ");  //OLED显示
	oled_display(OLED_1_ADDRESS,1,buf1);  //OLED显示
	oled_display(OLED_1_ADDRESS,3,buf2);  //OLED显示
	oled_display(OLED_1_ADDRESS,5,buf4);  //OLED显示
	oled_display(OLED_1_ADDRESS,0,"                ");  //OLED显示	
	oled_display(OLED_1_ADDRESS,2,"                ");  //OLED显示
	oled_display(OLED_1_ADDRESS,4,"                ");  //OLED显示
	oled_display(OLED_1_ADDRESS,6,"                ");  //OLED显示

        vTaskDelay(1 / portTICK_RATE_MS);
	}
}

void SDP31_APP(void)
{

	xTaskCreate(&task_SDP31_contrast, "ssid1306_contrast", 2048, NULL, 6, NULL);

}
