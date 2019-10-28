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

//---variate
static const char *TAG              = "JOSHVM_Audio";
audio_board_handle_t MegaBoard_handle = NULL;

extern esp_audio_handle_t           player;

//---define
#define MP3_2_STREAM_URI "file://userdata/bingo_2.mp3"
#define MP3_1_STREAM_URI "file://userdata/bingo_1.mp3"
#define MP3_STREAM_URI "file://userdata/ding.mp3"
#define AMR_STREAM_MP3_SD_URI "file://sdcard/44100.mp3"
#define AMR_STREAM_WAV_SD_URI "file://sdcard/48000.wav"

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


int esp32_read_voice_buffer(unsigned char* buffer,	int length)
{
	return 0;
}

void joshvm_app_init(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);
    ESP_LOGI(TAG, "ADF version is %s", ADF_VER);
	MegaBoard_handle = audio_board_init();
	audio_hal_ctrl_codec(MegaBoard_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);

	esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
	esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);
    //audio_board_sdcard_init(set);
    app_sdcard_init();
	app_wifi_service();


	

	printf("Main task Executing on core : %d\n",xPortGetCoreID());

	static char buf[1024];
	vTaskGetRunTimeStats(buf);
	printf("JOSHVM_Audio,Run Time Stats:\nTask Name   Time	  Percent\n%s\n", buf);	
	vTaskList(buf);
	printf("JOSHVM_Audio,Task List:\nTask Name	 Status   Prio	  HWM	 Task Number\n%s\n", buf);
	
	vTaskDelay(1000);

	while (1) {

		test_esp32_media();	

  		//JavaTask(); 
		//JavaNativeTest();		
		//test_rec_engine();
		//joshvm_esp32_wifi_set("JOSH","josh20177",0);
		
		for (int i = 10; i >= 0; i--) {
	        printf("Restarting in %d seconds...\n", i);
	        vTaskDelay(5000 / portTICK_PERIOD_MS);
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
		//t = joshvm_audio_play_handler(AMR_STREAM_MP3_URI);
		//display_service_set_pattern(disp_serv, DISPLAY_PATTERN_TURN_ON, 0);
		javanotify_simplespeech_event(2, 0);
		return 0;
	break;
	case 1:
		//display_service_set_pattern(disp_serv, DISPLAY_PATTERN_TURN_OFF, 0);
		javanotify_simplespeech_event(2, 0);
		//joshvm_spiffs_audio_play_handler(AMR_STREAM_MP3_URI);
		//ret = joshvm_audio_play_handler(AMR_STREAM_MP3_URI);
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
	case 5:

		joshvm_spiffs_audio_play_handler(MP3_2_STREAM_URI);
		
		return 0;
	case 6:

		joshvm_spiffs_audio_play_handler(MP3_1_STREAM_URI);
		
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
