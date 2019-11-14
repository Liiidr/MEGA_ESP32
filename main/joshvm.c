/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2018 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "sdkconfig.h"
#include "audio_mem.h"
#include "recorder_engine.h"
#include "esp_audio.h"
#include "esp_log.h"
#include "board.h"
#include "app_wifi_service.h"
#include "sd_card_init.h"
#include "joshvm_esp32_media.h"
#include "joshvm_esp32_rec_engine.h"
#include "joshvm_esp32_player.h"
#include "joshvm.h"
#include "joshvm_esp32_timer.h"

//---variate
static const char *TAG              = "JOSHVM_Audio";
audio_board_handle_t MegaBoard_handle = NULL;
UBaseType_t pvCreatedTask_vadtask;


extern esp_audio_handle_t           player;

//---define
#define MP3_STREAM_URI "file://userdata/ding.mp3"


//---fun
extern void javanotify_simplespeech_event(int, int);
extern void JavaTask();
extern int esp32_record_voicefile(unsigned char* filename, int time);
extern int esp32_playback_voice(int index);
extern void esp32_device_control(int);
extern int esp32_read_voice_buffer(unsigned char*, int);
extern int esp32_playback_voice_url(const char *url);
extern void esp32_stop_playback(void);
extern void esp32_stop_record(void);
extern void JavaNativeTest();

char *int_buff = NULL;
int esp32_read_voice_buffer(unsigned char* buffer,	int length)
{
	return 0;
}

void joshvm_app_init(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);
    ESP_LOGI(TAG, "ADF version is %s", ADF_VER);
	//MegaBoard_handle = audio_board_init();
	//audio_hal_ctrl_codec(MegaBoard_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);

	//esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
	//esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);
    //audio_board_sdcard_init(set);
    app_sdcard_init();
	ESP_LOGE(TAG,"before wifi = %d",heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
	app_wifi_service();
	ESP_LOGE(TAG,"after wifi = %d",heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
	joshvm_vad_timer();


	
	int_buff = (char*)audio_malloc(1024);

	ESP_LOGE(TAG,"heap_caps_get_free_size = %d",heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
	/*	printf("Main task Executing on core : %d\n",xPortGetCoreID());

	
	vTaskGetRunTimeStats(buff);
	printf("JOSHVM_Audio,Run Time Stats:\nTask Name   Time	  Percent\n%s\n", buf);	
	vTaskList(buff);
	printf("JOSHVM_Audio,Task List:\nTask Name	 Status   Prio	  HWM	 Task Number\n%s\n", buf);
	*/
	vTaskDelay(500);

	while (1) {
		/*
		printf("MALLOC_CAP_DEFAULT\n");
		heap_caps_print_heap_info(MALLOC_CAP_DEFAULT);
		printf("MALLOC_CAP_8BIT\n");
		heap_caps_print_heap_info(MALLOC_CAP_8BIT);
		printf("MALLOC_CAP_INTERNAL\n");
		heap_caps_print_heap_info(MALLOC_CAP_INTERNAL);
		printf("MALLOC_CAP_SPIRAM\n");
		heap_caps_print_heap_info(MALLOC_CAP_SPIRAM);
		printf("\n");
*/
		pvCreatedTask_vadtask = uxTaskGetStackHighWaterMark( NULL );
		extern void test_esp32_media(void);
		test_esp32_media();
  		//JavaTask(); 
		//JavaNativeTest();	
		//extern void fun_test();
		//fun_test();
	
		
		for (int i = 10; i >= 0; i--) {
	        printf("Restarting in %d seconds...\n", i);
	        vTaskDelay(10000 / portTICK_PERIOD_MS);
	    }
	    printf("Restarting now.\n");
	    //fflush(stdout);
	    //esp_restart();	  
	}
}

/**
 * JOSH VM interface
 **/
int esp32_playback_voice(int i) {
	//NOT implemented
	esp_err_t ret = ESP_FAIL;
	int player_volume = 0;
	switch(i){
	case 0:
		javanotify_simplespeech_event(2, 0);
		return 0;
	break;
	case 1:
		javanotify_simplespeech_event(2, 0);
		return 0;
	break;
	case 2:

        esp_audio_vol_get(player, &player_volume);
        player_volume += 10;
        if (player_volume > 100) {
            player_volume = 100;
        }
        esp_audio_vol_set(player, player_volume);
        printf("AUDIO_USER_KEY_VOL_UP [%d]\n", player_volume);

		javanotify_simplespeech_event(2, 0);
		return 0;
	case 3:

		esp_audio_vol_get(player, &player_volume);
		player_volume -= 10;
		if (player_volume < 0) {
			player_volume = 0;
		}
		esp_audio_vol_set(player, player_volume);
		printf("AUDIO_USER_KEY_VOL_DOWN [%d]\n", player_volume);

		javanotify_simplespeech_event(2, 0);
		return 0;
	case 4:
		joshvm_spiffs_audio_play_handler(MP3_STREAM_URI);		
		return 0;
	break;
	default:
	break;
	}
	
	return ret;
}

int esp32_record_voicefile(unsigned char* filename, int time) {
	//NOT implemented
	return 0;
}

int esp32_playback_voice_url(const char *url)
{
	int ret;
	ret = joshvm_audio_play_handler(url);	
	if(ret == ESP_ERR_AUDIO_NO_ERROR){

	}
	return ret;
}

void esp32_device_control(int command) {
	switch (command) {
		case 0: //PAUSE
		printf("JOSHVM_Audio:esp32_device_control: PAUSE\n");
		rec_engine_detect_suspend(REC_VOICE_SUSPEND_ON);
		break;
		case 1: //RESUME
		printf("JOSHVM_Audio:esp32_device_control: RESUME\n");
		rec_engine_detect_suspend(REC_VOICE_SUSPEND_OFF);
		break;
		case 2: //LISTEN
		printf("JOSHVM_Audio:esp32_device_control: LISTEN\n");
		rec_engine_vad_enable(true);
		break;
		case 3: //SLEEP
		printf("JOSHVM_Audio:esp32_device_control: SLEEP\n");
		rec_engine_vad_enable(false);
		break;
	}
}

void esp32_stop_playback() {
	printf("JOSHVM_Audio:esp_audio_stop\n");
	esp_audio_stop(player, TERMINATION_TYPE_NOW);
}

void esp32_stop_record() {
	printf("JOSHVM_Audio:rec_engine_trigger_stop\n");
	rec_engine_trigger_stop();
}
