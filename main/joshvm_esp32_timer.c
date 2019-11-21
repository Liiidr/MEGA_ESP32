#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "joshvm_esp32_media.h"
//---define
#define TAG "JOSH_TIMER"
#define TIMER_RELOAD_PERIOD	(pdMS_TO_TICKS(200UL))
//---variable
extern uint32_t vad_off_time;
extern uint16_t track_check_time_cnt;

static void joshvm_timer_callback(TimerHandle_t xtimer)
{
	
	if(vad_off_time < 0xffffffff){
		vad_off_time++;		
	}

	if(track_check_time_cnt < 0xffffffff){
		track_check_time_cnt++;
	}
}

joshvm_err_t joshvm_vad_timer(void)
{
	TimerHandle_t joshvm_timer;
	
	joshvm_timer = xTimerCreate("joshvm_timer",TIMER_RELOAD_PERIOD,pdTRUE,0,joshvm_timer_callback);
	if(joshvm_timer != NULL){
		if(xTimerStart(joshvm_timer, 0) != pdFAIL){
			vTaskStartScheduler();
			return JOSHVM_OK;			
		} 
		ESP_LOGE(TAG,"joshvm_timer create failed!");
	}
	ESP_LOGE(TAG,"joshvm_timer create failed!");
	return JOSHVM_FAIL;
}

