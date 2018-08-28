#include "esp_log.h"
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


#include <time.h>
#include <sys/time.h>
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "apps/sntp/sntp.h"
#include "oled1306.h"
#include "ssd1366.h"


//using namespace std;

#define TAG "Clock"
//const int CONNECTED_BIT = BIT0;

/* FreeRTOS event group to signal when we are connected & ready to make a request */
//static EventGroupHandle_t wifi_event_group;

static void obtain_time(void);
static void initialize_sntp(void);
//static void initialise_wifi(void);
static esp_err_t event_handler(void *ctx, system_event_t *event);

//OLED oled = OLED(GPIO_NUM_2, GPIO_NUM_15, SSD1306_128x64);

void myTask(void *pvParameters) {
	time_t now;

	struct tm timeinfo;//时间信息
	char strftime_buf[16];//时间存储buff
	char strftime_buf2[16];//时间存储buff
	//oled.select_font(4);
	//oled.set_contrast(0);
	while (1) {
	time(&now);
	localtime_r(&now, &timeinfo);
	strftime(strftime_buf, sizeof(strftime_buf), "%H:%M:%S", &timeinfo);
	sprintf(strftime_buf2,"%s       ",strftime_buf); //OLED显示
	oled_display(OLED_1_ADDRESS,Clock_LINE,"                ");  //OLED显示	
	oled_display(OLED_1_ADDRESS,Clock_LINE,strftime_buf2);  //OLED显示
	vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
}

static void obtain_time(void)
{
   // ESP_ERROR_CHECK( nvs_flash_init() );
  //  initialise_wifi();
//	ESP_LOGI(TAG, "Waiting for WiFi to be ready...");
   // xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
  //                      false, true, portMAX_DELAY);
    initialize_sntp();

    // wait for time to be set
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 10;

	oled_display(OLED_1_ADDRESS,Clock_LINE,"            ");  //OLED显示	
	oled_display(OLED_1_ADDRESS,Clock_LINE,"Waiting for");  //OLED显示
    while(timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
       // ESP_LOGI(TAG, "Time:%s", now);
    }

    // ESP_ERROR_CHECK( esp_wifi_stop() );
}

static void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, (char*)"pool.ntp.org");
    sntp_init();
}


void app_time_display()
 {

		oled_display(OLED_1_ADDRESS,Clock_LINE,"                ");  //OLED显示	
		oled_display(OLED_1_ADDRESS,Clock_LINE,"Connecting...");  //OLED显示
		time_t now;
		struct tm timeinfo;
		time(&now);
		localtime_r(&now, &timeinfo);
		// Is time set? If not, tm_year will be (1970 - 1900).
		if (timeinfo.tm_year < (2016 - 1900)) {
				ESP_LOGI(TAG, "Time is not set yet. Connecting to WiFi and getting time over NTP.");
				obtain_time();
				// update 'now' variable with current time
				time(&now);
		}
		setenv("TZ", "TZ=CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
		tzset();
		xTaskCreatePinnedToCore(&myTask, "clocktask", 2048, NULL, 5, NULL, 1);

}




