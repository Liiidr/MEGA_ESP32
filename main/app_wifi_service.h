/*
 *
 */

#ifndef _APP_WIFI_SERVICE_H_
#define _APP_WIFI_SERVICE_H_




typedef enum{
	APP_WIFI_SERV_UNKNOWN = 0,
	APP_WIFI_SERV_CONNECTED,
	APP_WIFI_SERV_DISCONNECTED,
	APP_WIFI_SERV_RECONNECTEDFAILED,
	APP_WIFI_SERV_AIRKISS_CONNECTED

}app_wifi_serv_queue_t;

			   										
/**
 * @brief 
 *
 * @note 
 *
 * @param 
 *
 * @return
 *
 */
void app_wifi_service(void);

/**
 * @brief 
 *
 * @note 
 *
 * @param 
 *
 * @return
 *
 */
int joshvm_esp32_wifi_set(char* ssid, char* password, int force);



#endif
