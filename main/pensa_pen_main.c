/*
 * Copyright 2010-2015 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * Additions Copyright 2016 Espressif Systems (Shanghai) PTE LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */
/**
 * @file subscribe_publish_sample.c
 * @brief simple MQTT publish and subscribe on the same topic
 *
 * This example takes the parameters from the build configuration and establishes a connection to the AWS IoT MQTT Platform.
 * It subscribes and publishes to the same topic - "test_topic/esp32"
 *
 * Some setup is required. See example README for details.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"

#include "nvs.h"
#include "nvs_flash.h"

#include "aws_iot_config.h"
#include "aws_iot_log.h"
#include "aws_iot_version.h"
#include "aws_iot_mqtt_client_interface.h"

#include "tcpip_adapter.h"
#include "esp_smartconfig.h"
#include "esp_ota_ops.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "mbedtls/platform.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/esp_debug.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"
#include "mbedtls/certs.h"

#include "driver/gpio.h"
#include "PensaGpio.h"
#include "oled1306.h"
#include "ssd1366.h"
#include "time_ctrl.h"
#include "AD_for_temperature_power.h"
#include "sf05.h"
#include "SDP31.h"
#include "ble_uart_app.h"

static const char *TAG = "OTA_Start";

/* The examples use simple WiFi configuration that you can set via
   'make menuconfig'.

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_WIFI_SSID CONFIG_WIFI_SSID
#define EXAMPLE_WIFI_PASS CONFIG_WIFI_PASSWORD
#define EXAMPLE_SERVER_IP   CONFIG_SERVER_IP
#define EXAMPLE_SERVER_PORT CONFIG_SERVER_PORT
#define EXAMPLE_FILENAME CONFIG_EXAMPLE_FILENAME
#define BUFFSIZE 1024
#define TEXT_BUFFSIZE 1024
//static const char *TAG = "ota";
/*an ota data write buffer ready to write to the flash*/
static char ota_write_data[BUFFSIZE + 1] = { 0 };
/*an packet receive buffer*/
//static char text[BUFFSIZE + 1] = { 0 };
/* an image total length*/
static int binary_file_length = 0;
/*socket id*/
//static int socket_id = -1;
/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT = BIT0;

/* Constants that aren't configurable in menuconfig */
#define WEB_SERVER "s3-us-west-2.amazonaws.com"
#define WEB_PORT "443"

#define WEB_URL "https://s3-us-west-2.amazonaws.com/lpzxxsp/pensapenota.bin"
                 //https://s3-us-west-2.amazonaws.com/lpzxxsp/pensapenota.bin
//#define WEB_URL "https://s3-us-west-2.amazonaws.com/lpzxxsp/task_watchdog.bin"
//#define WEB_URL "https://s3-us-west-2.amazonaws.com/lpzxxsp/ota.bin"
//https://s3-us-west-2.amazonaws.com/lpzxxsp/ota.bin

static const char *REQUEST = "GET " WEB_URL " HTTP/1.0\r\n"
    "Host: "WEB_SERVER"\r\n"
    "User-Agent: esp-idf/1.0 esp32\r\n"
    "\r\n";
/* Root cert for howsmyssl.com, taken from server_root_cert.pem

   The PEM file was extracted from the output of this command:
   openssl s_client -showcerts -connect www.howsmyssl.com:443 </dev/null

   The CA root cert is the last cert given in the chain of certs.

   To embed it in the app binary, the PEM file is named
   in the component.mk COMPONENT_EMBED_TXTFILES variable.
*/
extern const uint8_t server_root_cert_pem_start[] asm("_binary_server_root_cert_pem_start");
extern const uint8_t server_root_cert_pem_end[]   asm("_binary_server_root_cert_pem_end");
/* CA Root certificate, device ("Thing") certificate and device
 * ("Thing") key.

   Example can be configured one of two ways:

   "Embedded Certs" are loaded from files in "certs/" and embedded into the app binary.

   "Filesystem Certs" are loaded from the filesystem (SD card, etc.)

   See example README for more details.
*/
#if defined(CONFIG_EXAMPLE_EMBEDDED_CERTS)

extern const uint8_t aws_root_ca_pem_start[] asm("_binary_aws_root_ca_pem_start");
extern const uint8_t aws_root_ca_pem_end[] asm("_binary_aws_root_ca_pem_end");
extern const uint8_t certificate_pem_crt_start[] asm("_binary_certificate_pem_crt_start");
extern const uint8_t certificate_pem_crt_end[] asm("_binary_certificate_pem_crt_end");
extern const uint8_t private_pem_key_start[] asm("_binary_private_pem_key_start");
extern const uint8_t private_pem_key_end[] asm("_binary_private_pem_key_end");

#elif defined(CONFIG_EXAMPLE_FILESYSTEM_CERTS)

static const char * DEVICE_CERTIFICATE_PATH = CONFIG_EXAMPLE_CERTIFICATE_PATH;
static const char * DEVICE_PRIVATE_KEY_PATH = CONFIG_EXAMPLE_PRIVATE_KEY_PATH;
static const char * ROOT_CA_PATH = CONFIG_EXAMPLE_ROOT_CA_PATH;

#else
#error "Invalid method for loading certs"
#endif

/**
 * @brief Default MQTT HOST URL is pulled from the aws_iot_config.h
 */
char HostAddress[255] = AWS_IOT_MQTT_HOST;
char topicbuff[16] = {0};

char MAC_Address_buf[20];
       uint8_t MAC_Address[6]={0};
/**
 * @brief Default MQTT port is pulled from the aws_iot_config.h
 */
uint32_t port = AWS_IOT_MQTT_PORT;


void smartconfig_example_task(void * parm);

//wifi smartconfig
static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        xTaskCreate(smartconfig_example_task, "smartconfig_example_task", 4096, NULL, 3, NULL);
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        break;
    default:
        break;
    }
    return ESP_OK;
}

static void initialise_wifi(void)   //wifi初始化
{
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}

static void wifi_sc_callback(smartconfig_status_t status, void *pdata)
{
    switch (status) {
        case SC_STATUS_WAIT:
            ESP_LOGI(TAG, "SC_STATUS_WAIT");
            break;
        case SC_STATUS_FIND_CHANNEL:
            ESP_LOGI(TAG, "SC_STATUS_FINDING_CHANNEL");
                oled_display(OLED_2_ADDRESS,5,"    FINDING     ");  //OLED显示
                oled_display(OLED_2_ADDRESS,6,"                ");  //OLED显示	
                oled_display(OLED_2_ADDRESS,7,"                ");  //OLED显示	            
            break;
        case SC_STATUS_GETTING_SSID_PSWD:
            ESP_LOGI(TAG, "SC_STATUS_GETTING_SSID_PSWD");
            break;
        case SC_STATUS_LINK:
                oled_display(OLED_2_ADDRESS,5," SC_STATUS_LINK ");  //OLED显示
                oled_display(OLED_2_ADDRESS,6,"                ");  //OLED显示	
                oled_display(OLED_2_ADDRESS,7,"                ");  //OLED显示	 
            ESP_LOGI(TAG, "SC_STATUS_LINK");
            wifi_config_t *wifi_config = pdata;
            ESP_LOGI(TAG, "SSID:%s", wifi_config->sta.ssid);
            ESP_LOGI(TAG, "PASSWORD:%s", wifi_config->sta.password);
            ESP_ERROR_CHECK( esp_wifi_disconnect() );
            ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, wifi_config) );
            ESP_ERROR_CHECK( esp_wifi_connect() );
            break;
        case SC_STATUS_LINK_OVER:
        
        
                oled_display(OLED_2_ADDRESS,5,"    LINK_OVER   ");  //OLED显示
                oled_display(OLED_2_ADDRESS,6,"                ");  //OLED显示	
                oled_display(OLED_2_ADDRESS,7,"                ");  //OLED显示	
            ESP_LOGI(TAG, "SC_STATUS_LINK_OVER");
            if (pdata != NULL) {
                uint8_t phone_ip[4] = { 0 };
                memcpy(phone_ip, (uint8_t* )pdata, 4);
                ESP_LOGI(TAG, "Phone ip: %d.%d.%d.%d\n", phone_ip[0], phone_ip[1], phone_ip[2], phone_ip[3]);
				
				oled_display(OLED_2_ADDRESS,5,"Connect SUCCESS");  //OLED显示 
            }
            xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
            break;
        default:
            break;
    }
}

void smartconfig_example_task(void * parm)
{
    EventBits_t uxBits;
    ESP_ERROR_CHECK( esp_smartconfig_set_type(SC_TYPE_ESPTOUCH) );
    ESP_ERROR_CHECK( esp_smartconfig_start(wifi_sc_callback) );
    while (1) {
       Heater_Ctrl(Heater_disable);  
        uxBits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT | CONNECTED_BIT, true, false, portMAX_DELAY); 
        if(uxBits & CONNECTED_BIT) {
            ESP_LOGI(TAG, "WiFi Connected to ap");
                oled_display(OLED_2_ADDRESS,5,"Connected to ap ");  //OLED显示
                oled_display(OLED_2_ADDRESS,6,"                ");  //OLED显示	
                oled_display(OLED_2_ADDRESS,7,"                ");  //OLED显示
        }
        if(uxBits & CONNECTED_BIT) {
                oled_display(OLED_2_ADDRESS,5,"smartconfig over");  //OLED显示
                oled_display(OLED_2_ADDRESS,6,"                ");  //OLED显示	
                oled_display(OLED_2_ADDRESS,7,"                ");  //OLED显示
            ESP_LOGI(TAG, "smartconfig over");
            esp_smartconfig_stop();
            vTaskDelete(NULL);
        }
    }
}

/*
static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      //This is a workaround as ESP32 WiFi libs don't currently  auto-reassociate. 
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        break;
    default:
        break;
    }
    return ESP_OK;
}


static void initialise_wifi(void)
{
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_WIFI_SSID,
            .password = EXAMPLE_WIFI_PASS,
        },
    };
    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}
*/
void iot_subscribe_callback_handler(AWS_IoT_Client *pClient, char *topicName, uint16_t topicNameLen,
                                    IoT_Publish_Message_Params *params, void *pData) {
    ESP_LOGI(TAG, "Subscribe callback");
    ESP_LOGI(TAG, "%.*s\t%.*s", topicNameLen, topicName, (int) params->payloadLen, (char *)params->payload);
	//sprintf(topicbuff,"%s",(char *)params->payload);
	/*--------OLED------------------*/ 
    //	oled_display(OLED_1_ADDRESS,3,topicbuff);  //OLED显示
}

void disconnectCallbackHandler(AWS_IoT_Client *pClient, void *data) {
    ESP_LOGW(TAG, "MQTT Disconnect");
    IoT_Error_t rc = FAILURE;

    if(NULL == pClient) {
        return;
    }

    if(aws_iot_is_autoreconnect_enabled(pClient)) {
        ESP_LOGI(TAG, "Auto Reconnect is enabled, Reconnecting attempt will start now");
    } else {
        ESP_LOGW(TAG, "Auto Reconnect not enabled. Starting manual reconnect...");
        rc = aws_iot_mqtt_attempt_reconnect(pClient);
        if(NETWORK_RECONNECTED == rc) {
            ESP_LOGW(TAG, "Manual Reconnect Successful");
        } else {
            ESP_LOGW(TAG, "Manual Reconnect Failed - %d", rc);
        }
    }
}

void aws_iot_task(void *param) {
    char cPayload[100];


    int32_t i = 0;

    IoT_Error_t rc = FAILURE;

    AWS_IoT_Client client;
    IoT_Client_Init_Params mqttInitParams = iotClientInitParamsDefault;
    IoT_Client_Connect_Params connectParams = iotClientConnectParamsDefault;

    IoT_Publish_Message_Params paramsQOS0;
    IoT_Publish_Message_Params paramsQOS1;

    ESP_LOGI(TAG, "AWS IoT SDK Version %d.%d.%d-%s", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, VERSION_TAG);

    mqttInitParams.enableAutoReconnect = false; // We enable this later below
    mqttInitParams.pHostURL = HostAddress;
    mqttInitParams.port = port;

#if defined(CONFIG_EXAMPLE_EMBEDDED_CERTS)
    mqttInitParams.pRootCALocation = (const char *)aws_root_ca_pem_start;
    mqttInitParams.pDeviceCertLocation = (const char *)certificate_pem_crt_start;
    mqttInitParams.pDevicePrivateKeyLocation = (const char *)private_pem_key_start;

#elif defined(CONFIG_EXAMPLE_FILESYSTEM_CERTS)
    mqttInitParams.pRootCALocation = ROOT_CA_PATH;
    mqttInitParams.pDeviceCertLocation = DEVICE_CERTIFICATE_PATH;
    mqttInitParams.pDevicePrivateKeyLocation = DEVICE_PRIVATE_KEY_PATH;
#endif

    mqttInitParams.mqttCommandTimeout_ms = 20000;
    mqttInitParams.tlsHandshakeTimeout_ms = 5000;
    mqttInitParams.isSSLHostnameVerify = true;
    mqttInitParams.disconnectHandler = disconnectCallbackHandler;
    mqttInitParams.disconnectHandlerData = NULL;

#ifdef CONFIG_EXAMPLE_SDCARD_CERTS
    ESP_LOGI(TAG, "Mounting SD card...");
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 3,
    };
    sdmmc_card_t* card;
    esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SD card VFAT filesystem.");
        abort();
    }
#endif

    rc = aws_iot_mqtt_init(&client, &mqttInitParams);
    if(SUCCESS != rc) {
        ESP_LOGE(TAG, "aws_iot_mqtt_init returned error : %d ", rc);
        abort();
    }

    /* Wait for WiFI to show as connected */
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                        false, true, portMAX_DELAY);

    connectParams.keepAliveIntervalInSec = 10;
    connectParams.isCleanSession = true;
    connectParams.MQTTVersion = MQTT_3_1_1;
    /* Client ID is set in the menuconfig of the example */
    connectParams.pClientID = CONFIG_AWS_EXAMPLE_CLIENT_ID;
    connectParams.clientIDLen = (uint16_t) strlen(CONFIG_AWS_EXAMPLE_CLIENT_ID);
    connectParams.isWillMsgPresent = false;

    ESP_LOGI(TAG, "Connecting to AWS...");
/*--------OLED------------------*/ 
   	oled_display(OLED_1_ADDRESS,2,"Connect To MQTT");  //OLED显示
    do {
        rc = aws_iot_mqtt_connect(&client, &connectParams);
        if(SUCCESS != rc) {
            ESP_LOGE(TAG, "Error(%d) connecting to %s:%d", rc, mqttInitParams.pHostURL, mqttInitParams.port);
            vTaskDelay(1000 / portTICK_RATE_MS);
        }
    } while(SUCCESS != rc);

    /*
     * Enable Auto Reconnect functionality. Minimum and Maximum time of Exponential backoff are set in aws_iot_config.h
     *  #AWS_IOT_MQTT_MIN_RECONNECT_WAIT_INTERVAL
     *  #AWS_IOT_MQTT_MAX_RECONNECT_WAIT_INTERVAL
     */
    rc = aws_iot_mqtt_autoreconnect_set_status(&client, true);
    if(SUCCESS != rc) {
        ESP_LOGE(TAG, "Unable to set Auto Reconnect to true - %d", rc);
        abort();
    }


        esp_efuse_read_mac(MAC_Address);//获取MAC地址
        sprintf(MAC_Address_buf,"%02x%02x%02x%02x%02x%02x",MAC_Address[0],MAC_Address[1],MAC_Address[2],MAC_Address[3],MAC_Address[4],MAC_Address[5]);//打印MAC地址
        //ESP_LOGE("%02x:%02x:%02x:%02x:%02x:%02x",MAC_Address[0],MAC_Address[1],MAC_Address[2],MAC_Address[3],MAC_Address[4],MAC_Address[5]);//打印MAC地址
        oled_display(OLED_1_ADDRESS,1,MAC_Address_buf);  //OLED显示 


	
    const char *TOPIC = MAC_Address_buf;
    const int TOPIC_LEN = strlen(TOPIC);
	/*--------OLED------------------*/ 
    ESP_LOGI(TAG, "Subscribing...");
    rc = aws_iot_mqtt_subscribe(&client, TOPIC, TOPIC_LEN, QOS0, iot_subscribe_callback_handler, NULL);//订阅来自AWS IOT的消息
	
    if(SUCCESS != rc) {
        ESP_LOGE(TAG, "Error subscribing : %d ", rc);
        abort();
    }

    sprintf(cPayload, "%s : %d ", "hello from SDK", i);

    paramsQOS0.qos = QOS0;
    paramsQOS0.payload = (void *) cPayload;
    paramsQOS0.isRetained = 0;

    paramsQOS1.qos = QOS0;
//    paramsQOS1.qos = QOS1;

	paramsQOS1.payload = (void *) cPayload;
    paramsQOS1.isRetained = 0;

    while((NETWORK_ATTEMPTING_RECONNECT == rc || NETWORK_RECONNECTED == rc || SUCCESS == rc)) {

        //Max time the yield function will wait for read messages
        rc = aws_iot_mqtt_yield(&client, 100);
        if(NETWORK_ATTEMPTING_RECONNECT == rc) {
            // If the client is attempting to reconnect we will skip the rest of the loop.
            continue;
        }


        vTaskDelay(1000 / portTICK_RATE_MS);
        sprintf(cPayload, "%s : %d ", "hello from ESP32 (QOS0)", i++);
        paramsQOS0.payloadLen = strlen(cPayload);
        rc = aws_iot_mqtt_publish(&client, TOPIC, TOPIC_LEN, &paramsQOS0);

        sprintf(cPayload, "%s : %d ", "hello from ESP32 (QOS0)", i++);
        paramsQOS1.payloadLen = strlen(cPayload);
        rc = aws_iot_mqtt_publish(&client, TOPIC, TOPIC_LEN, &paramsQOS1);
        if (rc == MQTT_REQUEST_TIMEOUT_ERROR) {
            ESP_LOGW(TAG, "QOS1 publish ack not received.");
            rc = SUCCESS;
        }
    }

    ESP_LOGE(TAG, "An error occurred in the main loop.");
    abort();
}


static void __attribute__((noreturn)) task_fatal_error()
{
 /*   ESP_LOGE(TAG, "Exiting task due to fatal error...");
    close(socket_id);
    (void)vTaskDelete(NULL);

    while (1) {
        ;
    }*/
}
/*来自OTA-----------------------------read buffer by byte still delim ,return read bytes counts*/
static int read_until(char *buffer, char delim, int len)
{
//  /*TODO: delim check,buffer check,further: do an buffer length limited*/
    int i = 0;
    while (buffer[i] != delim && i < len) {
        ++i;
    }
    return i + 1;
}


/* 来自OTA-----------------------------resolve a packet from http socket
 * return true if packet including \r\n\r\n that means http packet header finished,start to receive packet body
 * otherwise return false
 * */
static bool read_past_http_header(char text[], int total_len, esp_ota_handle_t update_handle)
{
    /* i means current position 解析接收到的数据，如果接收到的数据为回车换行，则认为已经完成*/
    int i = 0, i_read_len = 0;
    while (text[i] != 0 && i < total_len) {
        i_read_len = read_until(&text[i], '\n', total_len);
        // if we resolve \r\n line,we think packet header is finished
        if (i_read_len == 2) {
            int i_write_len = total_len - (i + 2);
            memset(ota_write_data, 0, BUFFSIZE);
            /*copy first http packet body to write buffer*/
            memcpy(ota_write_data, &(text[i + 2]), i_write_len);

            esp_err_t err = esp_ota_write( update_handle, (const void *)ota_write_data, i_write_len);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Error: esp_ota_write failed! err=0x%x", err);
                return false;
            } else {
                ESP_LOGI(TAG, "esp_ota_write header OK");
                binary_file_length += i_write_len;
            }
            return true;
        }
        i += i_read_len;
    }
    return false;
}
static char buf[BUFFSIZE + 1] = { 0 };


/* 来自OTA----------------------------*/
static void aws_OTA_Task(void *pvParameters)
{
   
	int ret, flags,len_cnt=0,bar_cnt=0;//len;
	char write_buf[16]={0};
	char progress_bar[20]={0};
	int j;	
    esp_err_t err;
    /* update handle : set by esp_ota_begin(), must be freed via esp_ota_end() */
    esp_ota_handle_t update_handle = 0 ;
    const esp_partition_t *update_partition = NULL;

    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ssl_context ssl;
    mbedtls_x509_crt cacert;
    mbedtls_ssl_config conf;
    mbedtls_net_context server_fd;

    mbedtls_ssl_init(&ssl);
    mbedtls_x509_crt_init(&cacert);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    ESP_LOGI(TAG, "Seeding the random number generator");

    mbedtls_ssl_config_init(&conf);

    mbedtls_entropy_init(&entropy);
    if((ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                    NULL, 0)) != 0)
    {
        ESP_LOGE(TAG, "mbedtls_ctr_drbg_seed returned %d", ret);
        abort();
    }

    ESP_LOGI(TAG, "Loading the CA root certificate...");

    ret = mbedtls_x509_crt_parse(&cacert, server_root_cert_pem_start,
                                 server_root_cert_pem_end-server_root_cert_pem_start);
	//连接到证书网络
    if(ret < 0)
    {
        ESP_LOGE(TAG, "mbedtls_x509_crt_parse returned -0x%x\n\n", -ret);
        abort();
    }

    ESP_LOGI(TAG, "Setting hostname for TLS session...");

     /* Hostname set here should match CN in server certificate */
    if((ret = mbedtls_ssl_set_hostname(&ssl, WEB_SERVER)) != 0)
    {//证书跟服务器验证
        ESP_LOGE(TAG, "mbedtls_ssl_set_hostname returned -0x%x", -ret);
        abort();
    }

    ESP_LOGI(TAG, "Setting up the SSL/TLS structure...");

    if((ret = mbedtls_ssl_config_defaults(&conf,
                                          MBEDTLS_SSL_IS_CLIENT,
                                          MBEDTLS_SSL_TRANSPORT_STREAM,
                                          MBEDTLS_SSL_PRESET_DEFAULT)) != 0)
    {
        ESP_LOGE(TAG, "mbedtls_ssl_config_defaults returned %d", ret);
        goto exit;
    }

    /* MBEDTLS_SSL_VERIFY_OPTIONAL is bad for security, in this example it will print
       a warning if CA verification fails but it will continue to connect.

       You should consider using MBEDTLS_SSL_VERIFY_REQUIRED in your own code.
    */
    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_OPTIONAL);
    mbedtls_ssl_conf_ca_chain(&conf, &cacert, NULL);
    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);
#ifdef CONFIG_MBEDTLS_DEBUG
    mbedtls_esp_enable_debug_log(&conf, 4);
#endif

    if ((ret = mbedtls_ssl_setup(&ssl, &conf)) != 0)
    {
        ESP_LOGE(TAG, "mbedtls_ssl_setup returned -0x%x\n\n", -ret);
        goto exit;
    }

    while(1) 
	{
        /* Wait for the callback to set the CONNECTED_BIT in the
           event group.
        */
        xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                            false, true, portMAX_DELAY);
        ESP_LOGI(TAG, "Connected to AP");

        mbedtls_net_init(&server_fd);

        ESP_LOGI(TAG, "Connecting to %s:%s...", WEB_SERVER, WEB_PORT);
		//连接到服务器
        if ((ret = mbedtls_net_connect(&server_fd, WEB_SERVER,
                                      WEB_PORT, MBEDTLS_NET_PROTO_TCP)) != 0)
        {
            ESP_LOGE(TAG, "mbedtls_net_connect returned -%x", -ret);
            goto exit;
        }

        ESP_LOGI(TAG, "Connected.");

        mbedtls_ssl_set_bio(&ssl, &server_fd, mbedtls_net_send, mbedtls_net_recv, NULL);

        ESP_LOGI(TAG, "Performing the SSL/TLS handshake...");

        while ((ret = mbedtls_ssl_handshake(&ssl)) != 0)
        {
            if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE)
            {
                ESP_LOGE(TAG, "mbedtls_ssl_handshake returned -0x%x", -ret);
 
         	oled_display(OLED_1_ADDRESS,0,"Current Version ");  //OLED显示	
	        oled_display(OLED_1_ADDRESS,1,"     V1.0       ");  //OLED显示		

	        oled_display(OLED_1_ADDRESS,2,"----------------");  //OLED显示	
	        oled_display(OLED_1_ADDRESS,3,"    OTA fail    ");  //OLED显示
	        oled_display(OLED_1_ADDRESS,4,"----------------");  //OLED显示		        	        
	        oled_display(OLED_1_ADDRESS,5," pls Press 'EN' ");  //OLED显示	
	        oled_display(OLED_1_ADDRESS,6," to retry again ");  //OLED显示

	        oled_display(OLED_1_ADDRESS,7,"--------------- ");  //OLED显示	
 
                //----connect fail----
                goto exit;
            }
        }

        ESP_LOGI(TAG, "Verifying peer X.509 certificate...");

        if ((flags = mbedtls_ssl_get_verify_result(&ssl)) != 0)
        {
            /* In real life, we probably want to close connection if ret != 0 */
            ESP_LOGW(TAG, "Failed to verify peer certificate!");
            bzero(buf, sizeof(buf));
            mbedtls_x509_crt_verify_info(buf, sizeof(buf), "  ! ", flags);
            ESP_LOGW(TAG, "verification info: %s", buf);
        }
        else {
            ESP_LOGI(TAG, "Certificate verified.");
        }

        ESP_LOGI(TAG, "Cipher suite is %s", mbedtls_ssl_get_ciphersuite(&ssl));
         	oled_display(OLED_1_ADDRESS,0,"current version ");  //OLED显示	
	        oled_display(OLED_1_ADDRESS,1,"     V1.0       ");  //OLED显示	
	        oled_display(OLED_1_ADDRESS,2,"   CA verified  ");  //OLED显示
	        oled_display(OLED_1_ADDRESS,3,"----------------");  //OLED显示	
	        oled_display(OLED_1_ADDRESS,4,"Ready to update ");  //OLED显示	
	        oled_display(OLED_1_ADDRESS,5,"   Don't Move ! ");  //OLED显示
	        oled_display(OLED_1_ADDRESS,6,"----------------");  //OLED显示	
	        oled_display(OLED_1_ADDRESS,7,"--------------- ");  //OLED显示	
 
		
        size_t written_bytes = 0;
        do {
            ret = mbedtls_ssl_write(&ssl,
                                    (const unsigned char *)REQUEST + written_bytes,
                                    strlen(REQUEST) - written_bytes);
            if (ret >= 0) {
                ESP_LOGI(TAG, "%d bytes written", ret);
                written_bytes += ret;
            } else if (ret != MBEDTLS_ERR_SSL_WANT_WRITE && ret != MBEDTLS_ERR_SSL_WANT_READ) {
                ESP_LOGE(TAG, "mbedtls_ssl_write returned -0x%x", -ret);
                goto exit;
            }
        } while(written_bytes < strlen(REQUEST));
	const esp_partition_t *configured = esp_ota_get_boot_partition();
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (configured != running) {
        ESP_LOGW(TAG, "Configured OTA boot partition at offset 0x%08x, but running from offset 0x%08x",
                 configured->address, running->address);
        ESP_LOGW(TAG, "(This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)");
    }
    ESP_LOGI(TAG, "Running partition type %d subtype %d (offset 0x%08x)",
             running->type, running->subtype, running->address);
        ESP_LOGI(TAG, "Reading HTTP response...");
       ESP_LOGI(TAG, "Writing HTTP request...");
    update_partition = esp_ota_get_next_update_partition(NULL);
    ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%x",
             update_partition->subtype, update_partition->address);
    assert(update_partition != NULL);

    err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed, error=%d", err);
        task_fatal_error();
    }
    ESP_LOGI(TAG, "esp_ota_begin succeeded");
         	oled_display(OLED_1_ADDRESS,0,"current version ");  //OLED显示	
	        oled_display(OLED_1_ADDRESS,1,"     V1.0       ");  //OLED显示	
	        oled_display(OLED_1_ADDRESS,2,"                ");  //OLED显示
	        oled_display(OLED_1_ADDRESS,3,"  Downloading   ");  //OLED显示	
	        oled_display(OLED_1_ADDRESS,4,"                ");  //OLED显示	
	        oled_display(OLED_1_ADDRESS,5,"                ");  //OLED显示
	        oled_display(OLED_1_ADDRESS,6,"   Don't Move ! ");  //OLED显示	
	        oled_display(OLED_1_ADDRESS,7,"                ");  //OLED显示	    

    bool resp_body_start = false;// flag = true;
        do//读网页BUFF
        {
		memset(buf, 0, TEXT_BUFFSIZE);
       		memset(ota_write_data, 0, BUFFSIZE);
       		Heater_Ctrl(Heater_disable);  
            //len = sizeof(buf) - 1;
            bzero(buf, sizeof(buf));
            ret = mbedtls_ssl_read(&ssl, (unsigned char *)buf, TEXT_BUFFSIZE);//ret 读取的字节数
			//if(ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE)
	        if(ret>0)
	        {
	                len_cnt++;
		        if(!resp_body_start)//辨别帧头
		        {
			        memcpy(ota_write_data, buf, ret);
			        resp_body_start = read_past_http_header(buf, ret, update_handle);
			        ESP_LOGI(TAG, "read_past_http_header");
		        }
		        else
		        {         
		                memcpy(ota_write_data, buf,ret);
                		err = esp_ota_write( update_handle, (unsigned char *)ota_write_data, ret);
                                bar_cnt=len_cnt%16;
                                for(j=0;j<=bar_cnt;j++)
                                {
                                        progress_bar[j]='-';
                                        progress_bar[1+j]='>';
                                        if(j==16){progress_bar[20]=" ";}
                                       // oled_display(OLED_2_ADDRESS,7,"                 "); break; //OLED显示
                                        oled_display(OLED_2_ADDRESS,7,progress_bar); 
                                
                                }
                                bar_cnt=len_cnt/100;
                                
                                switch(bar_cnt)
                                {
                                case 0:oled_display(OLED_1_ADDRESS,7,"->               "); break; //OLED显示
                                case 1:oled_display(OLED_1_ADDRESS,7,"-->              "); break; //OLED显示
                                case 2:oled_display(OLED_1_ADDRESS,7,"--->             "); break; //OLED显示
                                case 3:oled_display(OLED_1_ADDRESS,7,"---->            "); break; //OLED显示
                                case 4:oled_display(OLED_1_ADDRESS,7,"----->           "); break; //OLED显示
                                case 5:oled_display(OLED_1_ADDRESS,7,"------>          "); break; //OLED显示
                                case 6:oled_display(OLED_1_ADDRESS,7,"------->         "); break; //OLED显示
                                case 7:oled_display(OLED_1_ADDRESS,7,"-------->        "); break; //OLED显示
                                case 8:oled_display(OLED_1_ADDRESS,7,"--------->       "); break; //OLED显示
                                case 9:oled_display(OLED_1_ADDRESS,7,"---------->      "); break; //OLED显示
                                case 10:oled_display(OLED_1_ADDRESS,7,"---------->     "); break; //OLED显示
                                case 11:oled_display(OLED_1_ADDRESS,7,"----------->    "); break; //OLED显示
                                case 12:oled_display(OLED_1_ADDRESS,7,"------------>   "); break; //OLED显示
                                case 13:oled_display(OLED_1_ADDRESS,7,"------------->  "); break; //OLED显示
                                case 14:oled_display(OLED_1_ADDRESS,7,"--------------> "); break; //OLED显示
                                case 15:oled_display(OLED_1_ADDRESS,7,"--------------->"); break; //OLED显示
                                default:oled_display(OLED_1_ADDRESS,7,"--------------->"); break; //OLED显示
                                
                                }

                		sprintf(write_buf,"   %d kb",len_cnt);
                		//sprintf(progress_bar,"%s",progress_bar);

	                        oled_display(OLED_1_ADDRESS,4,"                ");  //OLED显示	                		
                		oled_display(OLED_1_ADDRESS,4,write_buf);  //OLED显示	
	                     //   oled_display(OLED_1_ADDRESS,7,"1              >");  //OLED显示	                		
                		//oled_display(OLED_1_ADDRESS,7,progress_bar);  //OLED显示	                		
		                ESP_LOGI(TAG, "ota_write_data");
		        }
		        continue;
	         
	        }
            if(ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
                ret = 0;
                break;
            }

            if(ret < 0)
            {
                ESP_LOGE(TAG, "mbedtls_ssl_read returned -0x%x", -ret);
		ESP_LOGE(TAG, "Error: receive data error! errno=%d", errno);
                break;
            }

            if(ret == 0)
            {
                ESP_LOGI(TAG, "connection closed");
			//	flag = false;
                ESP_LOGI(TAG, "Connection closed, all packets received");
                break;
            }
		static int request_count1;
        ESP_LOGI(TAG, "Have written %d  times", ++request_count1);
        //    len = ret;
            binary_file_length += ret;
            ESP_LOGI(TAG, "Have written image length %d", binary_file_length);
			
           // ESP_LOGD(TAG, "%d bytes read", len);
            /* Print response directly to stdout as it is read */
 		//       for(int i = 0; i < len; i++) {
		//	 for(int i = 0; i < 50; i++) {
			//	putchar(buf[i]);
              //  putchar((buf[i] >>4) +'0');
			//	putchar((buf[i] & 0X0F) +'0');
				
        //    }
        } while(1);
		if (esp_ota_end(update_handle) != ESP_OK) {
			ESP_LOGE(TAG, "esp_ota_end failed!");
				//task_fatal_error();
			
			}
			err = esp_ota_set_boot_partition(update_partition);
			if (err != ESP_OK) {
				ESP_LOGE(TAG, "esp_ota_set_boot_partition failed! err=0x%x", err);
				task_fatal_error();
			}
			
		ESP_LOGI(TAG, "Prepare to restart system!");
         	oled_display(OLED_1_ADDRESS,0,"current version ");  //OLED显示	
	        oled_display(OLED_1_ADDRESS,1,"     V1.0       ");  //OLED显示	
	        oled_display(OLED_1_ADDRESS,2,"                ");  //OLED显示
	        oled_display(OLED_1_ADDRESS,3," OTA Success   !");  //OLED显示	
	        oled_display(OLED_1_ADDRESS,4,"                ");  //OLED显示	
	        oled_display(OLED_1_ADDRESS,5,"  Restart NOW!  ");  //OLED显示
	        oled_display(OLED_1_ADDRESS,6,"       5s       ");  //OLED显示	
	        oled_display(OLED_1_ADDRESS,7,"--------------->");  //OLED显示	        
	        
         	oled_display(OLED_1_ADDRESS,0,"----------------");  //OLED显示	
	        oled_display(OLED_1_ADDRESS,1,"----------------");  //OLED显示	
	        oled_display(OLED_1_ADDRESS,2,"It will Restart!");  //OLED显示
	        oled_display(OLED_1_ADDRESS,3,"----------------");  //OLED显示	
	        oled_display(OLED_1_ADDRESS,4,"pls Press Normal");  //OLED显示	
	        oled_display(OLED_1_ADDRESS,5,"Or It will OTA  ");  //OLED显示
	        oled_display(OLED_1_ADDRESS,6,"-----again !----");  //OLED显示	
	        oled_display(OLED_1_ADDRESS,7,"----------------");  //OLED显示		        
	        oled_display(OLED_2_ADDRESS,0,"----------------");  //OLED显示
	        oled_display(OLED_2_ADDRESS,1,"----------------");  //OLED显示	
	        oled_display(OLED_2_ADDRESS,2,"----------------");  //OLED显示	
	        oled_display(OLED_2_ADDRESS,3,"----------------");  //OLED显示	
	        oled_display(OLED_2_ADDRESS,4,"Press to Normal");  //OLED显示	
	        oled_display(OLED_2_ADDRESS,5,"Or It will OTA  ");  //OLED显示
	        oled_display(OLED_2_ADDRESS,6,"-----again !----");  //OLED显示	
	        oled_display(OLED_2_ADDRESS,7,"----------------");  //OLED显示	 


                vTaskDelay(1000 / portTICK_PERIOD_MS);
	        oled_display(OLED_1_ADDRESS,7,"       4s       ");  //OLED显示	
                vTaskDelay(1000 / portTICK_PERIOD_MS);
	        oled_display(OLED_1_ADDRESS,7,"       3s       ");  //OLED显示		        		
                vTaskDelay(1000 / portTICK_PERIOD_MS);
	        oled_display(OLED_1_ADDRESS,7,"       2s       ");  //OLED显示	
                vTaskDelay(1000 / portTICK_PERIOD_MS);
	        oled_display(OLED_1_ADDRESS,7,"       1s       ");  //OLED显示		        
                vTaskDelay(1000 / portTICK_PERIOD_MS);
	        oled_display(OLED_1_ADDRESS,7,"       0s       ");  //OLED显示		        
                vTaskDelay(1000 / portTICK_PERIOD_MS);		
 	while(0==gpio_get_level(GPIO_INPUT_IO_22)){//高点平就不亮灯
 	        bar_cnt++;
	        oled_display(OLED_1_ADDRESS,4,"Press to Normal");  //OLED显示	
	        oled_display(OLED_1_ADDRESS,5,"Or It will OTA  ");  //OLED显示
	        oled_display(OLED_1_ADDRESS,6,"-----again !----");  //OLED显示	                
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                Red_LED(bar_cnt%2);// Flashing red LED
                Green_LED(LED_OFF);
		Blue_LED(LED_OFF);
	}               
	
                
                               
	esp_restart();
        mbedtls_ssl_close_notify(&ssl);

    exit:
        mbedtls_ssl_session_reset(&ssl);
        mbedtls_net_free(&server_fd);
		
        if(ret != 0)
        {
            mbedtls_strerror(ret, buf, 100);
            ESP_LOGE(TAG, "Last error was: -0x%x - %s", -ret, buf);
        }

        putchar('\n'); // JSON output doesn't have a newline at end

        static int request_count;
        ESP_LOGI(TAG, "Completed %d requests", ++request_count);

        for(int countdown = 10; countdown >= 0; countdown--) {
            ESP_LOGI(TAG, "%d...", countdown);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
        ESP_LOGI(TAG, "Starting again!");
    }
}

void app_main()
{
    // Initialize NVS.
    
    uint32_t temperature_ad_value=0;
    uint32_t temperature=0;
    char temperature_AD_buff[16]={0};
    char temperature_buff[16]={0};
                esp_err_t err = nvs_flash_init();
                if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
                ESP_ERROR_CHECK(nvs_flash_erase());
                err = nvs_flash_init();
                }    
            ESP_ERROR_CHECK( err );
    
    
    gpio_init();//
    LED_EN(LED_enable);
 		Green_LED(LED_OFF);
		Red_LED(LED_OFF);
		Blue_LED(LED_OFF); 
		
		Heater_Ctrl(Heater_disable);  
	i2c_oled_init();
	ssd1306_init(OLED_1_ADDRESS);
	ssd1306_init(OLED_2_ADDRESS);
                oled_display(OLED_1_ADDRESS,0,"                ");  //OLED显示	
                oled_display(OLED_1_ADDRESS,1,"                ");  //OLED显示	
                oled_display(OLED_1_ADDRESS,2,"                ");  //OLED显示
                oled_display(OLED_1_ADDRESS,3,"                ");  //OLED显示	
                oled_display(OLED_1_ADDRESS,4,"                ");  //OLED显示	
                oled_display(OLED_1_ADDRESS,5,"                ");  //OLED显示
                oled_display(OLED_1_ADDRESS,6,"                ");  //OLED显示	
                oled_display(OLED_1_ADDRESS,7,"                ");  //OLED显示	
                oled_display(OLED_2_ADDRESS,0,"                ");  //OLED显示
                oled_display(OLED_2_ADDRESS,1,"                ");  //OLED显示	
                oled_display(OLED_2_ADDRESS,2,"                ");  //OLED显示	
                oled_display(OLED_2_ADDRESS,3,"                ");  //OLED显示	
                oled_display(OLED_2_ADDRESS,4,"                ");  //OLED显示	
                oled_display(OLED_2_ADDRESS,5,"                ");  //OLED显示
                oled_display(OLED_2_ADDRESS,6,"                ");  //OLED显示	
                oled_display(OLED_2_ADDRESS,7,"                ");  //OLED显示

         vTaskDelay(1000 / portTICK_PERIOD_MS);
	oled_display(OLED_1_ADDRESS,0,"1.Press 'Update'");  //OLED显示	
	oled_display(OLED_1_ADDRESS,1,"to update code  ");  //OLED显示	
	oled_display(OLED_1_ADDRESS,2,"2.Open the app  ");  //OLED显示
	oled_display(OLED_1_ADDRESS,3,"3.enter password");  //OLED显示	
	oled_display(OLED_1_ADDRESS,4,"4.It will update");  //OLED显示	
	oled_display(OLED_1_ADDRESS,5,"Press 'Normal'  ");  //OLED显示
	oled_display(OLED_1_ADDRESS,6,"if don't Update ");  //OLED显示	
	oled_display(OLED_1_ADDRESS,7,"It works normal ");  //OLED显示	
	oled_display(OLED_2_ADDRESS,0,"Press 'Normal'  ");  //OLED显示
	oled_display(OLED_2_ADDRESS,1,"if don't Update ");  //OLED显示	
	oled_display(OLED_2_ADDRESS,2,"It works normal ");  //OLED显示	
	oled_display(OLED_2_ADDRESS,3,"                ");  //OLED显示	
	oled_display(OLED_2_ADDRESS,4,"                ");  //OLED显示	
	oled_display(OLED_2_ADDRESS,5,"                ");  //OLED显示
	oled_display(OLED_2_ADDRESS,6,"                ");  //OLED显示	
	oled_display(OLED_2_ADDRESS,7,"                ");  //OLED显示

 //   app_time_display();
/* while(1)
     { 
     temperature_ad_value=Temperature_test();
    temperature= get_temperture(temperature_ad_value);
 
    sprintf(temperature_buff,"temperature=%d",temperature);
    sprintf(temperature_AD_buff,"AD=%d   ",temperature_ad_value);  
    oled_display(OLED_2_ADDRESS,6,"                ");  //OLED显示	      
    oled_display(OLED_2_ADDRESS,7,"                ");  //OLED显示
    oled_display(OLED_2_ADDRESS,6,temperature_AD_buff);  //OLED显示	
    oled_display(OLED_2_ADDRESS,7,temperature_buff);  //OLED显示
    vTaskDelay(300 / portTICK_PERIOD_MS);
    }
*/
//_______________________________________________________________________________
	if(gpio_get_level(GPIO_INPUT_IO_22)){//高点平就不亮灯
		Green_LED(LED_ON);

		esp_efuse_read_mac(MAC_Address);//获取MAC地址
                sprintf(MAC_Address_buf,"ID: %02x%02x%02x%02x%02x%02x",MAC_Address[0],MAC_Address[1],MAC_Address[2],MAC_Address[3],MAC_Address[4],MAC_Address[5]);//打印MAC地址
                //ESP_LOGE("%02x:%02x:%02x:%02x:%02x:%02x",MAC_Address[0],MAC_Address[1],MAC_Address[2],MAC_Address[3],MAC_Address[4],MAC_Address[5]);//打印MAC地址
               oled_display(OLED_1_ADDRESS,5,MAC_Address_buf);  //OLED显示 
         //	oled_display(OLED_1_ADDRESS,0,"current version ");  //OLED显示	
                oled_display(OLED_1_ADDRESS,0,"     V1.0       ");  //OLED显示		
	        oled_display(OLED_1_ADDRESS,2,"                ");  //OLED显示
	        oled_display(OLED_1_ADDRESS,3,"Blue tooth Start");  //OLED显示	
	        oled_display(OLED_1_ADDRESS,4,"                ");  //OLED显示	
	      //  oled_display(OLED_1_ADDRESS,5,"                ");  //OLED显示
	        oled_display(OLED_1_ADDRESS,6,"                ");  //OLED显示	
	        oled_display(OLED_1_ADDRESS,7,"                ");  //OLED显示	
	        oled_display(OLED_2_ADDRESS,0,"Received data:  ");  //OLED显示
	        oled_display(OLED_2_ADDRESS,1,"                ");  //OLED显示	
	        oled_display(OLED_2_ADDRESS,2,"                ");  //OLED显示	
	        oled_display(OLED_2_ADDRESS,3,"                ");  //OLED显示	
	        oled_display(OLED_2_ADDRESS,4,"Pls link to Ble ");  //OLED显示	
	        oled_display(OLED_2_ADDRESS,5,"                ");  //OLED显示
	        oled_display(OLED_2_ADDRESS,6,"                ");  //OLED显示	
	        oled_display(OLED_2_ADDRESS,7,"                ");  //OLED显示
//______________________________________ble_uart程序___________________________________________	        
                ble_uart_main();
                

          //      ad2_app();

		//SF05_APP();
		SDP31_APP();
                
		//xTaskCreatePinnedToCore(&aws_iot_task, "aws_iot_task", 36*1024, NULL, 5, NULL, 1);	//连接到亚马逊服务器


	}
	else {
	        remove_Microphone_intr();
               	Heater_Ctrl(Heater_disable);  
 /*          	oled_display(OLED_1_ADDRESS,0,"1111111111111111");  //OLED显示	
                oled_display(OLED_1_ADDRESS,1,"     00000      ");  //OLED显示		
	        oled_display(OLED_1_ADDRESS,2,"                ");  //OLED显示
	        oled_display(OLED_1_ADDRESS,3,"0000000000000000");  //OLED显示	      */       
                vTaskDelay(100 / portTICK_PERIOD_MS);
                initialise_wifi();
                
		Blue_LED(LED_ON);	
//______________________________________通过OTA更新程序___________________________________________
		xTaskCreate(&aws_OTA_Task, "aws_OTA_Task", 8192, NULL, 5, NULL);//通过OTA更新程序

		
		
		
	}


}
