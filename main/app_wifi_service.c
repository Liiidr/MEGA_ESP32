#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "audio_mem.h"
#include "app_wifi_service.h"
#include "wifi_service.h"
#include "airkiss_config.h"
#include "smart_config.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "joshvm_esp32_media.h"
#include "esp_spiffs.h"

static const char *TAG              = "APP_WIFI_SERVICE>>>>>>>";
static periph_service_handle_t wifi_serv = NULL;
QueueHandle_t app_wifi_serv_queue = NULL;


#define	PROPERTY_CFG	1
#define LAST_AIRKISS_CFG 2
#define	AIRKISS_CFG	3
#define LAST_CONNECT_cfg 4

typedef struct{
	char *last_connect_ssid;
	char *last_connect_password;
	char *property_ssid;
	char *property_password;
}app_wifi_config_t;
app_wifi_config_t *app_wifi_config = NULL;

void app_wifi_airkissprofile_write(char *ssid,char *passwd)
{/*
	size_t total = 0, used = 0;
    int ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
*/
	ESP_LOGI(TAG, "Opening airkiss_profile");
    FILE* f = fopen("/appdb/unsecure/airkiss_profile.txt", "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }

    fprintf(f,"SSID:%s\n", ssid);
	fprintf(f,"PASSWD:%s\n", passwd);	
    fclose(f);
    ESP_LOGI(TAG, "airkiss_profile written");
}

void app_wifi_airkissprofile_read(char *ssid,char *passwd)
{
	// Open renamed file for reading
	ESP_LOGI(TAG, "Reading airkiss profile");
	FILE* f = fopen("/appdb/unsecure/airkiss_profile.txt", "r");
	if (f == NULL) {
		ESP_LOGE(TAG, "Failed to open airkiss profile for reading");
		return;
	}
	
	fgets(ssid, 64, f);	
	fgets(passwd, 64, f);
	fclose(f);

	// strip newline
	char* pos = strchr(ssid, ':');
	ssid = pos+1;
	pos = strchr(ssid, '\n');
	if (pos) {
		*pos = '\0';
	}
	
	// strip newline
	pos = strchr(passwd, ':');
	passwd = pos+1;
	pos = strchr(passwd, '\n');
	if (pos) {
		*pos = '\0';
	}
	printf("Read from airkiss profile,ssid:'%s',passwd:'%s'\n",ssid, passwd);
}



void app_wifi_get_airkisscfg(char *ssid,char *pwd)
{
	app_wifi_airkissprofile_write(ssid,pwd);	
}

void app_wifi_get_last_connectcfg(uint8_t ssid[],uint8_t ssid_len,uint8_t pwd[],uint8_t pwd_len)
{
	memcpy(app_wifi_config->last_connect_ssid, ssid, ssid_len);
    memcpy(app_wifi_config->last_connect_password, pwd, pwd_len);
}

void app_wifi_airkiss_cfg_connect()
{
	int ret;
	char ssid[32],passwd[64];
	app_wifi_airkissprofile_read(ssid,passwd);
	wifi_config_t sta_cfg = {0};
	strncpy((char *)&sta_cfg.sta.ssid,ssid, strlen(ssid));
	strncpy((char *)&sta_cfg.sta.password,passwd, strlen(passwd));
	ret = wifi_service_set_sta_info(wifi_serv, &sta_cfg);
}

void app_wifi_lastconnect_cfg_connect()
{
	int ret;
	wifi_config_t sta_cfg = {0};
	strncpy((char *)&sta_cfg.sta.ssid,app_wifi_config->last_connect_ssid, strlen(app_wifi_config->last_connect_ssid));
	strncpy((char *)&sta_cfg.sta.password,app_wifi_config->last_connect_password, strlen(app_wifi_config->last_connect_password));
	ret = wifi_service_set_sta_info(wifi_serv, &sta_cfg);
}

void app_wifi_property_cfg_connect()
{
	int ret;
	wifi_config_t sta_cfg = {0};
	strncpy((char *)&sta_cfg.sta.ssid,app_wifi_config->property_ssid, strlen(app_wifi_config->property_ssid));
	strncpy((char *)&sta_cfg.sta.password,app_wifi_config->property_password, strlen(app_wifi_config->property_password));
	ret = wifi_service_set_sta_info(wifi_serv, &sta_cfg);
}


static esp_err_t app_wifi_service_cb(periph_service_handle_t handle, periph_service_event_t *evt, void *ctx)
{
    ESP_LOGD(TAG, "event type:%d,source:%p, data:%p,len:%d,ctx:%p",
             evt->type, evt->source, evt->data, evt->len, ctx);
			 
    if (evt->type == WIFI_SERV_EVENT_CONNECTED) {		
        ESP_LOGI(TAG, "PERIPH_WIFI_CONNECTED [%d]", __LINE__);


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
	int8_t cnt = 0;

	char ssid[64],passwd[64];//test
	

	while(1){
		xQueueReceive(app_wifi_serv_queue, &r_queue, portMAX_DELAY);		

		switch(r_queue){
			case	APP_WIFI_SERV_DISCONNECTED:
				wifi_service_connect(wifi_serv);
				ESP_LOGI(TAG,"APP_WIFI_SERV_DISCONNECTED");
		
				break;
			case	APP_WIFI_SERV_RECONNECTEDFAILED:				
				ESP_LOGI(TAG,"APP_WIFI_SERV_RECONNECTEDFAILED");
				cnt++;				
				if(cnt > 4) cnt = 1;
				switch(cnt){
					case 	LAST_CONNECT_cfg:
						printf("wifi connecting with last SSID and PASSWD.\n");
						app_wifi_lastconnect_cfg_connect();
					break;
					case	PROPERTY_CFG:
						printf("wifi connecting with property SSID and PASSWD.\n");
						app_wifi_property_cfg_connect();
					break;
					case	LAST_AIRKISS_CFG:
						printf("wifi connecting with airkiss profile SSID and PASSWD.\n");
						app_wifi_airkiss_cfg_connect();
						
					break;
					case 	AIRKISS_CFG:
						printf("enable airkiss.\n");
						wifi_service_setting_start(wifi_serv, 0);
					default:

					break;
				}	
				break;


			default:

				break;
		}


	}
}




void app_wifi_service(void)
{
	app_wifi_config = (app_wifi_config_t*)audio_malloc(sizeof(app_wifi_config_t));


	wifi_config_t sta_cfg = {0};
	strncpy((char *)&sta_cfg.sta.ssid,"JOSH", strlen("JOSH"));
	strncpy((char *)&sta_cfg.sta.password,"josh20177", strlen("josh20177"));

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
	wifi_service_set_sta_info(wifi_serv, &sta_cfg);
	
	xTaskCreate(app_wifi_task,"app_wifi_task",3*1024,NULL,5,NULL);
	
}

int joshvm_esp32_wifi_set(char* ssid, char* password, int force)
{
	int ret = JOSHVM_FAIL;
	app_wifi_config->property_ssid = ssid;
	app_wifi_config->property_password = password;
	if(force == false){
		//ret = wifi_service_state_get(wifi_serv);
		//if(ret == PERIPH_SERVICE_STATE_RUNNING){
		ESP_LOGI(TAG,"if saved ssid pwd can't connect,property cfg will be set!");
		return JOSHVM_NOTIFY_LATER; 
		//} 
	}
	ESP_LOGI(TAG,"joshvm_esp32_wifi_set!");
	wifi_config_t sta_cfg = {0};
	strncpy((char *)&sta_cfg.sta.ssid,ssid, strlen(ssid));
	strncpy((char *)&sta_cfg.sta.password,password, strlen(password));
	ret = wifi_service_set_sta_info(wifi_serv, &sta_cfg);
		
	if(ret == ESP_OK){
		return JOSHVM_OK;
	}
	return JOSHVM_FAIL;
}



