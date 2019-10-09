#include "audio_pipeline.h"
#include "esp_audio.h"
#include "audio_mem.h"
#include "audio_common.h"
#include "audio_pipeline.h"
#include "joshvm_esp32_media.h"
#include "joshvm_esp32_record.h"
#include "joshvm_audio_wrapper.h"
#include "esp_log.h"

//---define
#define TAG  "-----------JOSHVM_ESP32_MEDIA-----------"


//---variable
static int8_t audio_status = 0;


//------------------test-------------------
void *handle_player_test;
#define MP3_URI_TEST "file://userdata/ding.mp3"

static void media_player_callback_test(void*handle,int para)
{
	ESP_LOGI(TAG,"media_player_callback");
}


//------------------test-------------------
//留给播放结束的回调
void joshvm_esp32_media_callback()
{
	audio_status = JOSHVM_MEDIA_RESERVE;
	((joshvm_media_t *)handle_player_test)->joshvm_media_u.joshvm_media_mediaplayer.callback(NULL,0);//need to rewrite
}

/*

static void joshvm_media_mediaplayer_start(const char *url)
{	
	//joshvm_spiffs_audio_play_handler(url);//spiffs
	joshvm_audio_play_handler(url);//fatfs/http
}


static void joshvm_media_mediarecorder_start(joshvm_media_t* handle)
{
	ESP_LOGI(TAG,"joshvm_media_mediarecorder_start");
	audio_pipeline_run(handle->joshvm_media_u.joshvm_media_mediarecorder.recorder);

}
*/
int joshvm_esp32_media_create(int type, void** handle)
{
	ESP_LOGI(TAG,"joshvm_esp32_media_create");
	joshvm_media_t *joshvm_media = (joshvm_media_t*)audio_calloc(1, sizeof(joshvm_media_t));
	joshvm_media->media_type = type;
	switch(type){
		case MEDIA_PLAYER: 	
			//joshvm_media->joshvm_media_u.joshvm_media_mediaplayer.start = joshvm_media_mediaplayer_start;
			break;
		case MEDIA_RECORDER: 	
			//joshvm_media->joshvm_media_u.joshvm_media_mediarecorder.start = joshvm_media_mediarecorder_start;
			joshvm_media->joshvm_media_u.joshvm_media_mediarecorder.format = joshvm_meida_format_wav;
			joshvm_media->joshvm_media_u.joshvm_media_mediarecorder.url = "/sdcard/media.wav";
			joshvm_media->joshvm_media_u.joshvm_media_mediarecorder.sample_rate = 48000;
			joshvm_media->joshvm_media_u.joshvm_media_mediarecorder.channel = I2S_CHANNEL_FMT_ONLY_LEFT;
			joshvm_media->joshvm_media_u.joshvm_media_mediarecorder.bit_rate = 16;
			
			break;
		default :
			break;
	}
	

	*handle = joshvm_media;
	return 0;
}


int joshvm_esp32_media_close(void* handle)
{
	audio_free(handle);
	if(handle == NULL){
		return JOSHVM_OK;
	}
	return JOSHVM_FAIL;
}

int joshvm_esp32_media_prepare(joshvm_media_t* handle, void(*callback)(void*, int))
{
	int ret;
	switch(handle->media_type){
		case MEDIA_PLAYER:
			
			break;
		case MEDIA_RECORDER:	
			joshvm_meida_recorder_init(handle);

			break;
		case AUDIO_TRACK:

			break;
		case AUDIO_RECORDER:

			break;
		default :
			break;


	}

	return 0;
}

int joshvm_esp32_media_start(joshvm_media_t* handle, void(*callback)(void*, int))
{
	int ret = 0;
	if(1){//audio_status != JOSHVM_MEDIA_PAUSED){	//start		
		switch(handle->media_type){
			case MEDIA_PLAYER:
				//handle->joshvm_media_u.joshvm_media_mediaplayer.start(handle->joshvm_media_u.joshvm_media_mediaplayer.url);
				//joshvm_audio_play_handler(handle->joshvm_media_u.joshvm_media_mediaplayer.url);
				joshvm_spiffs_audio_play_handler(handle->joshvm_media_u.joshvm_media_mediaplayer.url);
				handle->joshvm_media_u.joshvm_media_mediaplayer.callback = callback;
				ret = JOSHVM_OK;
				break;
			case MEDIA_RECORDER:
				//handle->joshvm_media_u.joshvm_media_mediarecorder.start(handle); 
				audio_pipeline_run(handle->joshvm_media_u.joshvm_media_mediarecorder.recorder);
				ret = JOSHVM_OK;
				break;
			case AUDIO_TRACK:
				
				ret = JOSHVM_OK;
				break;
			case AUDIO_RECORDER:

				ret = JOSHVM_OK;
				break;
			default :
				ret = JOSHVM_NOT_SUPPORTED;
				break;
		}
	}else{		//resume
		audio_status = JOSHVM_MEDIA_PLAYING;
		switch(handle->media_type){
			case MEDIA_PLAYER:			
				joshvm_audio_resume_handler(handle->joshvm_media_u.joshvm_media_mediaplayer.url);
		
				break;
			case MEDIA_RECORDER:
				audio_pipeline_resume(handle->joshvm_media_u.joshvm_media_mediarecorder.recorder);
				ret = JOSHVM_OK;
				break;
			case AUDIO_TRACK:
				
				ret = JOSHVM_OK;
				break;
			case AUDIO_RECORDER:

				ret = JOSHVM_OK;
				break;
			default :
				ret = JOSHVM_NOT_SUPPORTED;
				break;
		}
	}	
	return ret;
}

int joshvm_esp32_media_pause(joshvm_media_t* handle)
{
	int ret;
	audio_status = JOSHVM_MEDIA_PAUSED;
	switch(handle->media_type){
		case MEDIA_PLAYER:			
			ret = joshvm_audio_pause();
	
			break;
		case MEDIA_RECORDER:
			audio_pipeline_pause(handle->joshvm_media_u.joshvm_media_mediarecorder.recorder);
			ret = JOSHVM_OK;
			break;
		case AUDIO_TRACK:
			
			ret = JOSHVM_OK;
			break;
		case AUDIO_RECORDER:

			ret = JOSHVM_OK;
			break;
		default :
			ret = JOSHVM_NOT_SUPPORTED;
			break;
	}
	return ret;
}

int joshvm_esp32_media_stop(joshvm_media_t* handle)
{
	int ret;
	switch(handle->media_type){
		case MEDIA_PLAYER:			
			if((ret = joshvm_audio_stop_handler()) != ESP_OK){
				return JOSHVM_FAIL;
			}
	
			ret = JOSHVM_OK;
			break;
		case MEDIA_RECORDER:
			audio_pipeline_terminate(handle->joshvm_media_u.joshvm_media_mediarecorder.recorder);
			audio_status = JOSHVM_MEDIA_RESERVE;
			ret = JOSHVM_OK;
			break;
		case AUDIO_TRACK:
			
			ret = JOSHVM_OK;
			break;
		case AUDIO_RECORDER:

			ret = JOSHVM_OK;
			break;
		default :
			ret = JOSHVM_NOT_SUPPORTED;
			break;
	}
	return ret;
}


int joshvm_esp32_media_reset(joshvm_media_t* handle)
{
	int ret;
	switch(handle->media_type){
		case MEDIA_PLAYER:			

			ret = JOSHVM_OK;
			break;
		case MEDIA_RECORDER:
			handle->joshvm_media_u.joshvm_media_mediarecorder.format = joshvm_meida_format_wav;
			handle->joshvm_media_u.joshvm_media_mediarecorder.url = "/sdcard/media.wav";
			handle->joshvm_media_u.joshvm_media_mediarecorder.sample_rate = 48000;
			handle->joshvm_media_u.joshvm_media_mediarecorder.channel = I2S_CHANNEL_FMT_ONLY_LEFT;
			handle->joshvm_media_u.joshvm_media_mediarecorder.bit_rate = 16;
			ret = JOSHVM_OK;
			break;
		case AUDIO_TRACK:
			
			ret = JOSHVM_OK;
			break;
		case AUDIO_RECORDER:

			ret = JOSHVM_OK;
			break;
		default :
			ret = JOSHVM_NOT_SUPPORTED;
			break;
	}
	return 0;
}

int joshvm_esp32_media_release(void* handle)
{
	audio_free(handle);
	if(handle == NULL){
		return JOSHVM_OK;
	}
	return JOSHVM_FAIL;
}


int joshvm_esp32_media_get_state(void* handle, int* state)
{
	int ret;
	ret = joshvm_audio_get_state();
	switch(ret){
		case 1:
			ret = JOSHVM_MEDIA_PLAYING;//
			break;
		case 2:
			ret = JOSHVM_MEDIA_PAUSED;
			break;
		case 3:
			ret = JOSHVM_MEDIA_STOPPED;
			break;
		default:
			ret = JOSHVM_FAIL;
			break;
	}
	return ret;
}
/*
joshvm_esp32_media_read(void* handle, unsigned char* buffer, int size, int* bytesRead, void(*callback)(void*, int))
{

}

joshvm_esp32_media_write(void* handle, unsigned char* buffer, int size, int* bytesWritten, void(*callback)(void*, int))
{

}

int joshvm_esp32_media_flush(void* handle)
{
}

int joshvm_esp32_media_get_buffsize(void* handle, int* size)
{

}
*/
int joshvm_esp32_media_set_audio_sample_rate(joshvm_media_t* handle, uint32_t value)
{
	handle->joshvm_media_u.joshvm_media_mediaplayer.sample_rate = value;
	return 0;
}

int joshvm_esp32_media_set_channel_config(joshvm_media_t* handle, uint8_t value)
{
	handle->joshvm_media_u.joshvm_media_mediaplayer.channel = value;
	return 0;
}

int joshvm_esp32_media_set_audio_bit_rate(joshvm_media_t* handle, uint8_t value)
{
	handle->joshvm_media_u.joshvm_media_mediaplayer.bit_rate = value;
	return 0;
}

int joshvm_esp32_media_set_source(joshvm_media_t* handle, char* source)
{

	handle->joshvm_media_u.joshvm_media_mediaplayer.url = source;
	return 0;
}
/*
int joshvm_esp32_media_set_output_file(void* handle, char* file)
{

}

int joshvm_esp32_media_set_output_format(void* handle, int format)
{

}

int joshvm_esp32_media_get_position(void* handle, int* pos)
{
	
}

int joshvm_esp32_media_set_position(void* handle, int pos, void(*callback)(void*, int))
{
}

int joshvm_esp32_media_get_duration(void* handle, int* duration)
{

}

int joshvm_esp32_media_get_duration(void* handle, int* duration)
{
}

int joshvm_esp32_media_get_max_volume(int* volume)
{

}
*/
int joshvm_esp32_media_get_volume(int* volume)
{
	int ret;
	ret = joshvm_volume_get_handler(volume);
	return ret;
}

int joshvm_esp32_media_set_volume(int volume)
{
	int ret;
	ret = joshvm_volume_set_handler(volume);
	return ret;
}

int joshvm_esp32_media_add_volume()
{
	joshvm_volume_adjust_handler(5);
	return 0;
}

int joshvm_esp32_media_sub_volume()
{
	joshvm_volume_adjust_handler(-10);
	return 0;
}


void test_esp32_media(void)
{
	joshvm_esp32_media_create(0,&handle_player_test);
	joshvm_esp32_media_set_source(handle_player_test,MP3_URI_TEST);
	//joshvm_esp32_media_prepare(handle_player_test,NULL);
	joshvm_esp32_media_start(handle_player_test,media_player_callback_test);

	//vTaskDelay(15000 / portTICK_PERIOD_MS);	

	//joshvm_esp32_media_pause(handle_player_test);

	//vTaskDelay(5000 / portTICK_PERIOD_MS);	
	
}


