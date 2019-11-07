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


#include "joshvm_esp32_rec_engine.h"
#include "joshvm_esp32_media.h"
#include "joshvm_esp32_record.h"
#include "joshvm_esp32_ring_buff.h"
#include "joshvm_esp32_player.h"
#include "joshvm.h"

#define test_url "http://mirror-advertising.oss-cn-beijing.aliyuncs.com/20191104/c9817b80-aa09-4237-bbe6-7a61d038823c.mp3"
#define MP3_URI "/userdata/ding.mp3"
#define MP3_URI2 "/sdcard/16000.mp3"






//static void *handle_media_rec= NULL;
static void *handle_media_player = NULL;
//static void *handle_track = NULL;
//static void *handle_audio_rec = NULL;
static void *handle_vad_rec = NULL;

extern int esp32_playback_voice(int index);



static void player_callback(void* para, int para2)
{
	printf("finish play  index = %d\n",para2);
	ESP_LOGE("TEST>>>>>>>>","player start heap_caps_get_free_size = %d",heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
}

static void vad_callback(int index)
{
	printf("***vad*** callback  index = %d\n",index);

	if(index){
	/*	printf("\n");
		heap_caps_print_heap_info(MALLOC_CAP_DEFAULT);
		printf("\n");
		heap_caps_print_heap_info(MALLOC_CAP_8BIT);
		printf("\n");
		heap_caps_print_heap_info(MALLOC_CAP_INTERNAL);
		printf("\n");
		heap_caps_print_heap_info(MALLOC_CAP_SPIRAM);
		printf("\n");*/
		joshvm_esp32_vad_stop();
		//joshvm_esp32_media_close(handle_vad_rec);
		vTaskDelay(3000);
		joshvm_esp32_media_set_source(handle_media_player,MP3_URI2);
		joshvm_esp32_media_start(handle_media_player,player_callback);
	}
}


static void wake_callback(int index)
{
	printf("wakeup callback  index = %d\n",index);
	//esp32_playback_voice(4);
	joshvm_esp32_media_create(4,&handle_vad_rec);
	joshvm_esp32_vad_start(vad_callback);
	
}


void fun_test()
{	
	//joshvm_esp32_wakeup_enable(wake_callback);
	/*
	joshvm_esp32_media_create(0,&handle_media_player);
	
	
	
	joshvm_esp32_media_set_source(handle_media_player,MP3_URI2);
	joshvm_esp32_media_start(handle_media_player,player_callback);
*/

	
	while(1){


	

		vTaskDelay(1000);
	}	
}




