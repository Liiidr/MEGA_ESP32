#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "app_wifi_service.h"
#include "wifi_service.h"
#include "airkiss_config.h"
#include "smart_config.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "joshvm_esp32_media.h"

static const char *TAG              = "APP_WIFI_SERVICE";
static periph_service_handle_t wifi_serv = NULL;
QueueHandle_t app_wifi_serv_queue = NULL;

static esp_err_t app_wifi_service_cb(periph_service_handle_t handle, periph_service_event_t *evt, void *ctx)
{
    ESP_LOGD(TAG, "event type:%d,source:%p, data:%p,len:%d,ctx:%p",
             evt->type, evt->source, evt->data, evt->len, ctx);
    if (evt->type == WIFI_SERV_EVENT_CONNECTED) {		
        ESP_LOGI(TAG, "PERIPH_WIFI_CONNECTED [%d]", __LINE__);
		//ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));

    } else if (evt->type == WIFI_SERV_EVENT_DISCONNECTED) {
        ESP_LOGI(TAG, "PERIPH_WIFI_DISCONNECTED [%d]", __LINE__);
		uint32_t senddata = APP_WIFI_SERV_DISCONNECTED;
		xQueueSend(app_wifi_serv_queue,&senddata,0);


    } else if (evt->type == WIFI_SERV_EVENT_SETTING_TIMEOUT) {
		ESP_LOGI(TAG, "WIFI_SERV_EVENT_SETTING_TIMEOUT [%d]", __LINE__);
    }

    return ESP_OK;
}

static void app_wifi_task(void *parameter)
{
	uint32_t r_queue = 0;	

	while(1){
		xQueueReceive(app_wifi_serv_queue, &r_queue, portMAX_DELAY);		

		switch(r_queue){
			case	APP_WIFI_SERV_DISCONNECTED:
				wifi_service_connect(wifi_serv);
		
				break;
			case	APP_WIFI_SERV_RECONNECTEDFAILED:
				wifi_service_setting_start(wifi_serv, 0);
		
				break;


			default:

				break;
		}


	}
}


void app_wifi_service(void)
{
	//wifi_config_t sta_cfg = {0};
	//strncpy((char *)&sta_cfg.sta.ssid,"JOSH", strlen("JOSH"));
	//strncpy((char *)&sta_cfg.sta.password,"josh2017", strlen("josh2017"));

	app_wifi_serv_queue = xQueueCreate(3, sizeof(uint32_t));
	if(NULL == app_wifi_serv_queue){
		ESP_LOGE(TAG,"app_wifi_ser_queue created failed");
	}

	wifi_service_config_t cfg = WIFI_SERVICE_DEFAULT_CONFIG();
	cfg.evt_cb = app_wifi_service_cb;
	cfg.cb_ctx = NULL;
	cfg.setting_timeout_s = 0x7FFFFFFF;
	wifi_serv = wifi_service_create(&cfg);

	int reg_idx = 0;
	esp_wifi_setting_handle_t h = NULL;
	airkiss_config_info_t air_info = AIRKISS_CONFIG_INFO_DEFAULT();
	air_info.lan_pack.appid = CONFIG_AIRKISS_APPID;
	air_info.lan_pack.deviceid = CONFIG_AIRKISS_DEVICEID;
	air_info.aes_key = CONFIG_DUER_AIRKISS_KEY;
	h = airkiss_config_create(&air_info);
	esp_wifi_setting_regitster_notify_handle(h, (void *)wifi_serv);
	wifi_service_register_setting_handle(wifi_serv, h, &reg_idx);
	//wifi_service_set_sta_info(wifi_serv, &sta_cfg);
	
	xTaskCreate(app_wifi_task,"app_wifi_task",2*1024,NULL,5,NULL);
	
}

int joshvm_esp32_wifi_set(char* ssid, char* password, int force)
{
	int ret;
	if(force == false){
		ret = wifi_service_state_get(wifi_serv);
		if(ret == PERIPH_SERVICE_STATE_RUNNING){
			return JOSHVM_INVALID_STATE; 
		}
	}
	
	wifi_config_t sta_cfg = {0};
	strncpy((char *)&sta_cfg.sta.ssid,ssid, strlen(ssid));
	strncpy((char *)&sta_cfg.sta.password,password, strlen(password));
	ret = wifi_service_set_sta_info(wifi_serv, &sta_cfg);
	
	
	if(ret == ESP_OK){
		return JOSHVM_OK;
	}
	return JOSHVM_FAIL;
}


