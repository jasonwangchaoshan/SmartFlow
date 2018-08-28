#ifndef _SDP31_H_
#define _SDP31_H_



#define SDP31_I2C_ADDRESS 0x21
void SDP31_APP(void);
void i2c_SDP3X_init();
float getPressureDiff(void);
float getTemperature(void);
uint8_t *readSensor(void);//Gets RAW sensor data
//extern float pressure;
float getpressure(void);
//void task_SDP31_contrast(void *pvParameters) ;
#endif 
