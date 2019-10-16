#include "audio_pipeline.h"
#include "esp_audio.h"
#include "audio_mem.h"
#include "audio_common.h"
#include "audio_pipeline.h"
#include "joshvm_esp32_media.h"
#include "joshvm_esp32_record.h"
#include "joshvm_audio_wrapper.h"
#include "esp_log.h"
#include "string.h"

//---define
#define TAG  "-----------JOSHVM_ESP32_MEDIA-----------"


//---variable
static int8_t audio_status = 0;
static struct{
	int format;
	char* url;
	int sample_rate;
	int channel;
	int bit_rate;
}joshvm_meida_recorder_default_cfg = {joshvm_meida_format_wav,"/sdcard/default.wav",16000,1,16};

static joshvm_media_t *joshvm_media = NULL;

//------------------test-------------------
void *handle_player_test = NULL;

#define MP3_URI_TEST "file://sdcard/48000.wav"

static void media_player_callback_test(void*handle,int para)
{
	ESP_LOGI(TAG,"media_player_callback");
}


//------------------test-------------------
//留给播放结束的回调
void joshvm_esp32_media_callback()
{
	ESP_LOGI(TAG,"joshvm_esp32_media_callback");

	//audio_status = JOSHVM_MEDIA_RESERVE;
	((joshvm_media_t *)handle_player_test)->joshvm_media_u.joshvm_media_mediaplayer.callback(NULL,0);//need to rewrite
}


int joshvm_esp32_media_create(int type, void** handle)
{
	printf("------------------------------------------\r\n");
	printf("------*MEGA_ESP32 Version bate_v1.3*------\r\n");
	printf("------------------------------------------\r\n");

	ESP_LOGI(TAG,"joshvm_esp32_media_create");
	int ret = JOSHVM_OK;
	joshvm_media = (joshvm_media_t*)audio_calloc(1, sizeof(joshvm_media_t));
	joshvm_media->media_type = type;
	switch(type){
		case MEDIA_PLAYER: 	
			
			break;
		case MEDIA_RECORDER: 	
			joshvm_media->joshvm_media_u.joshvm_media_mediarecorder.format = joshvm_meida_recorder_default_cfg.format;
			joshvm_media->joshvm_media_u.joshvm_media_mediarecorder.url = joshvm_meida_recorder_default_cfg.url;
			joshvm_media->joshvm_media_u.joshvm_media_mediarecorder.sample_rate = joshvm_meida_recorder_default_cfg.sample_rate;
			joshvm_media->joshvm_media_u.joshvm_media_mediarecorder.channel = joshvm_meida_recorder_default_cfg.channel;
			joshvm_media->joshvm_media_u.joshvm_media_mediarecorder.bit_rate = joshvm_meida_recorder_default_cfg.bit_rate;	
			joshvm_meida_recorder_init(joshvm_media);
			break;
		case AUDIO_TRACK:
			ret = joshvm_audio_track_init(joshvm_media);
			break;
		case AUDIO_RECORDER:
			ret = joshvm_audio_recorder_init(joshvm_media);
			break;
		default :
			break;
	}	

	*handle = joshvm_media;	
	return ret;
}


int joshvm_esp32_media_close(void* handle)
{
	ESP_LOGI(TAG,"joshvm_esp32_media_close");

	audio_free(handle);
	if(handle == NULL){
		return JOSHVM_OK;
	}
	return JOSHVM_FAIL;
}

int joshvm_esp32_media_prepare(joshvm_media_t* handle, void(*callback)(void*, int))
{
	ESP_LOGI(TAG,"joshvm_esp32_media_prepare");

	int ret = 0;
	switch(handle->media_type){
		case MEDIA_PLAYER:
			ret = JOSHVM_OK;
			break;
		case MEDIA_RECORDER:	
			ESP_LOGE(TAG,"heap_caps_get_free_size = %d",heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
			joshvm_meida_recorder_cfg(handle);

			break;
		/*case AUDIO_TRACK:
			break;
		case AUDIO_RECORDER:
			break;*/
		default :
			ret = JOSHVM_NOT_SUPPORTED;
			break;
	}

	return ret;
}

int joshvm_esp32_media_start(joshvm_media_t* handle, void(*callback)(void*, int))
{
	ESP_LOGI(TAG,"joshvm_esp32_media_start");

	int ret = 0;
	if(audio_status != JOSHVM_MEDIA_PAUSED){	//start		
		switch(handle->media_type){
			case MEDIA_PLAYER:				
				ret = joshvm_audio_play_handler(handle->joshvm_media_u.joshvm_media_mediaplayer.url);
				//joshvm_spiffs_audio_play_handler(handle->joshvm_media_u.joshvm_media_mediaplayer.url);
				handle->joshvm_media_u.joshvm_media_mediaplayer.callback = callback;

				break;
			case MEDIA_RECORDER:
				ret = audio_pipeline_run(handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.pipeline);
				break;
			case AUDIO_TRACK:
				ret = audio_pipeline_run(handle->joshvm_media_u.joshvm_media_audiotrack.audiotrack_t.pipeline);
				break;
			case AUDIO_RECORDER:
				ret = audio_pipeline_run(handle->joshvm_media_u.joshvm_media_audiorecorder.audiorecorder_t.pipeline);
				
				break;
			default :
				ret = JOSHVM_NOT_SUPPORTED;
				break;
		}
	}else{		//resume
		audio_status = JOSHVM_MEDIA_PLAYING;
		switch(handle->media_type){
			case MEDIA_PLAYER:			
				ret = joshvm_audio_resume_handler(handle->joshvm_media_u.joshvm_media_mediaplayer.url);
				break;
			case MEDIA_RECORDER:				
				ret = JOSHVM_NOT_SUPPORTED;
				break;
			case AUDIO_TRACK:				
				ret = audio_pipeline_resume(handle->joshvm_media_u.joshvm_media_audiotrack.audiotrack_t.pipeline);
				break;
			case AUDIO_RECORDER:
				ret = JOSHVM_NOT_SUPPORTED;
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
	ESP_LOGI(TAG,"joshvm_esp32_media_pause");

	int ret;
	audio_status = JOSHVM_MEDIA_PAUSED;
	switch(handle->media_type){
		case MEDIA_PLAYER:			
			ret = joshvm_audio_pause();	
			break;
		case MEDIA_RECORDER:
			ret = JOSHVM_NOT_SUPPORTED;
			break;
		case AUDIO_TRACK:			
			ret = audio_pipeline_pause(handle->joshvm_media_u.joshvm_media_audiotrack.audiotrack_t.pipeline);
			break;
		case AUDIO_RECORDER:
			ret = JOSHVM_NOT_SUPPORTED;
			break;
		default :
			ret = JOSHVM_NOT_SUPPORTED;
			break;
	}
	return ret;
}

int joshvm_esp32_media_stop(joshvm_media_t* handle)
{
	ESP_LOGI(TAG,"joshvm_esp32_media_stop");

	int ret;
	switch(handle->media_type){
		case MEDIA_PLAYER:			
			if((ret = joshvm_audio_stop_handler()) != ESP_OK){
				return JOSHVM_FAIL;
			}	
			ret = JOSHVM_OK;
			break;
		case MEDIA_RECORDER:
			ret = audio_pipeline_terminate(handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.pipeline);		
			break;
		case AUDIO_TRACK:			
			ret = audio_pipeline_terminate(handle->joshvm_media_u.joshvm_media_audiotrack.audiotrack_t.pipeline);
			break;
		case AUDIO_RECORDER:			
			ret = audio_pipeline_terminate(handle->joshvm_media_u.joshvm_media_audiorecorder.audiorecorder_t.pipeline);
			break;
		default :
			ret = JOSHVM_NOT_SUPPORTED;
			break;
	}
	return ret;
}


int joshvm_esp32_media_reset(joshvm_media_t* handle)
{
	ESP_LOGI(TAG,"joshvm_esp32_media_reset");

	int ret;
	switch(handle->media_type){
		case MEDIA_PLAYER:			

			ret = JOSHVM_OK;
			break;
		case MEDIA_RECORDER:
			handle->joshvm_media_u.joshvm_media_mediarecorder.format = joshvm_meida_recorder_default_cfg.format;
			handle->joshvm_media_u.joshvm_media_mediarecorder.url = joshvm_meida_recorder_default_cfg.url;
			handle->joshvm_media_u.joshvm_media_mediarecorder.sample_rate = joshvm_meida_recorder_default_cfg.sample_rate;
			handle->joshvm_media_u.joshvm_media_mediarecorder.channel = joshvm_meida_recorder_default_cfg.channel;
			handle->joshvm_media_u.joshvm_media_mediarecorder.bit_rate = joshvm_meida_recorder_default_cfg.bit_rate;	
			ret = JOSHVM_OK;
			break;
		default :
			ret = JOSHVM_NOT_SUPPORTED;
			break;
	}
	return ret;
}

int joshvm_esp32_media_release(joshvm_media_t* handle)
{
	ESP_LOGI(TAG,"joshvm_esp32_media_release");
	switch(handle->media_type)
	{
		case MEDIA_PLAYER:
			break;
		case MEDIA_RECORDER:
			joshvm_media_recorder_release(handle);
			break;
		case AUDIO_TRACK:
			joshvm_audio_track_release(handle);
			break;
		case AUDIO_RECORDER:
			joshvm_audio_rcorder_release(handle);
			break;
		default:
			break;
	}

	if(handle){
		audio_free(handle);		
		handle = NULL;
	}
	return JOSHVM_OK;
}

int joshvm_esp32_media_get_state(joshvm_media_t* handle, int* state)
{
	ESP_LOGI(TAG,"joshvm_esp32_media_get_state");

	int ret;
	switch(handle->media_type){
		case MEDIA_PLAYER:
			ret = joshvm_audio_get_state();
			switch(ret){
				case 1:
					*state = JOSHVM_MEDIA_PLAYING;//
					ret = JOSHVM_OK;
					break;
				case 2:
					*state = JOSHVM_MEDIA_PAUSED;
					ret = JOSHVM_OK;
					break;
				case 3:
					*state = JOSHVM_MEDIA_STOPPED;
					ret = JOSHVM_OK;
					break;
				default:
					ret = JOSHVM_FAIL;
					break;
			}
			return ret;
			break;
		case MEDIA_RECORDER:
			joshvm_media_get_state(handle,state);
			ret = JOSHVM_OK;
			break;
		case AUDIO_TRACK:
			joshvm_media_get_state(handle,state);
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

int joshvm_esp32_media_read(joshvm_media_t* handle, unsigned char* buffer, int size, int* bytesRead, void(*callback)(void*, int))
{
	return 0;
}

int joshvm_esp32_media_write(joshvm_media_t* handle, unsigned char* buffer, int size, int* bytesWritten, void(*callback)(void*, int))
{

	return 0;
}

int joshvm_esp32_media_flush(joshvm_media_t* handle)
{
	return 0;
}

int joshvm_esp32_media_get_buffsize(joshvm_media_t* handle, int* size)
{
	return 0;
}

int joshvm_esp32_media_set_audio_sample_rate(joshvm_media_t* handle, uint32_t value)
{
	ESP_LOGI(TAG,"joshvm_esp32_media_set_audio_sample_rate");

	int ret;
	switch(handle->media_type){
		case MEDIA_PLAYER:					
			ret = JOSHVM_NOT_SUPPORTED;
			break;
		case MEDIA_RECORDER:
			handle->joshvm_media_u.joshvm_media_mediarecorder.sample_rate = value;
			ret = JOSHVM_OK;
			break;
		case AUDIO_TRACK:
			handle->joshvm_media_u.joshvm_media_audiotrack.sample_rate = value;
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

int joshvm_esp32_media_set_channel_config(joshvm_media_t* handle, uint8_t value)
{
	ESP_LOGI(TAG,"joshvm_esp32_media_set_channel_config");

	int ret;
	switch(handle->media_type){
		case MEDIA_PLAYER:					
			ret = JOSHVM_NOT_SUPPORTED;
			break;
		case MEDIA_RECORDER:
			handle->joshvm_media_u.joshvm_media_mediarecorder.channel = value;
			ret = JOSHVM_OK;
			break;
		case AUDIO_TRACK:
			handle->joshvm_media_u.joshvm_media_audiotrack.channel = value;
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

int joshvm_esp32_media_set_audio_bit_rate(joshvm_media_t* handle, uint8_t value)
{
	ESP_LOGI(TAG,"joshvm_esp32_media_set_audio_bit_rate");

	int ret;
	switch(handle->media_type){
		case MEDIA_PLAYER:					
			ret = JOSHVM_NOT_SUPPORTED;
			break;
		case MEDIA_RECORDER:
			handle->joshvm_media_u.joshvm_media_mediarecorder.bit_rate = value;
			ret = JOSHVM_OK;
			break;
		case AUDIO_TRACK:
			handle->joshvm_media_u.joshvm_media_audiotrack.bit_rate = value;
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

static char source_head[100] = "file:/";
static char source_http_head[5] = "http";
int joshvm_esp32_media_set_source(joshvm_media_t* handle, char* source)
{
	ESP_LOGI(TAG,"joshvm_esp32_media_set_source");

	int ret;
	switch(handle->media_type){
		case MEDIA_PLAYER:
			if(strstr(source,source_http_head) == NULL){	
				strcat(source_head,source);
				handle->joshvm_media_u.joshvm_media_mediaplayer.url = source_head;
			}else{
				handle->joshvm_media_u.joshvm_media_mediaplayer.url = source;
			}				
			ret = JOSHVM_OK;
			break;
		default :
			ret = JOSHVM_NOT_SUPPORTED;
			break;
	}
	return ret;

}

int joshvm_esp32_media_set_output_file(joshvm_media_t* handle, char* file)
{
	ESP_LOGI(TAG,"joshvm_esp32_media_set_output_file");

	int ret;
	switch(handle->media_type){
		case MEDIA_RECORDER:
			handle->joshvm_media_u.joshvm_media_mediarecorder.url = file;
			ret = JOSHVM_OK;
			break;
		default :
			ret = JOSHVM_NOT_SUPPORTED;
			break;
	}
	return ret;

}

int joshvm_esp32_media_set_output_format(joshvm_media_t* handle, int format)
{
	ESP_LOGI(TAG,"joshvm_esp32_media_set_output_format");

	int ret;
	switch(handle->media_type){
		case MEDIA_RECORDER:
			handle->joshvm_media_u.joshvm_media_mediarecorder.format = format;
			ret = JOSHVM_OK;
			break;
		default :
			ret = JOSHVM_NOT_SUPPORTED;
			break;
	}
	return ret;

}

int joshvm_esp32_media_get_position(joshvm_media_t* handle, int* pos)
{
	ESP_LOGI(TAG,"joshvm_esp32_media_get_position");

	int ret;
	switch(handle->media_type){
		case MEDIA_PLAYER:
			joshvm_audio_time_get(pos);
			ret = JOSHVM_OK;
			break;
		default :
			ret = JOSHVM_NOT_SUPPORTED;
			break;
	}
	return ret;	
}

int joshvm_esp32_media_set_position(joshvm_media_t* handle, int pos, void(*callback)(void*, int))
{
	ESP_LOGI(TAG,"joshvm_esp32_media_set_position");

	int ret;
	switch(handle->media_type){
		case MEDIA_PLAYER:
			//handle->joshvm_media_u.joshvm_media_mediaplayer.positon = pos;
			ret = JOSHVM_FAIL;
			break;
		default :
			ret = JOSHVM_NOT_SUPPORTED;
			break;
	}
	return ret;	
}

int joshvm_esp32_media_get_duration(joshvm_media_t* handle, int* duration)
{
	ESP_LOGI(TAG,"joshvm_esp32_media_get_duration");

	int ret;
	switch(handle->media_type){
		case MEDIA_PLAYER:
			//*duration = handle->joshvm_media_u.joshvm_media_mediaplayer.duration;
			ret = JOSHVM_FAIL;
			break;
		default :
			ret = JOSHVM_NOT_SUPPORTED;
			break;
	}
	return ret;	

}

int joshvm_esp32_media_get_max_volume(int* volume)
{
	ESP_LOGI(TAG,"joshvm_esp32_media_get_max_volume");

	return *volume = 100;
}

int joshvm_esp32_media_get_volume(int* volume)
{
	ESP_LOGI(TAG,"joshvm_esp32_media_get_volume");

	int ret;
	ret = joshvm_volume_get_handler(volume);
	return ret;
}

int joshvm_esp32_media_set_volume(int volume)
{
	ESP_LOGI(TAG,"joshvm_esp32_media_set_volume");

	int ret;
	ret = joshvm_volume_set_handler(volume);
	return ret;
}

int joshvm_esp32_media_add_volume()
{
	ESP_LOGI(TAG,"joshvm_esp32_media_add_volume");

	joshvm_volume_adjust_handler(5);
	return 0;
}

int joshvm_esp32_media_sub_volume()
{
	ESP_LOGI(TAG,"joshvm_esp32_media_sub_volume");

	joshvm_volume_adjust_handler(-10);
	return 0;
}


void test_esp32_media(void)
{	

	joshvm_esp32_media_create(1,&handle_player_test);
	//joshvm_esp32_media_set_source(handle_player_test,MP3_URI_TEST);
	joshvm_esp32_media_start(handle_player_test,media_player_callback_test);
	vTaskDelay(10000 / portTICK_PERIOD_MS);
	joshvm_esp32_media_stop(handle_player_test);
	vTaskDelay(1000 / portTICK_PERIOD_MS); 

	joshvm_esp32_media_set_output_file(handle_player_test,"/sdcard/default9.wav");
	joshvm_esp32_media_set_audio_sample_rate(handle_player_test,16000);
	joshvm_esp32_media_set_channel_config(handle_player_test,1);
	joshvm_esp32_media_prepare(handle_player_test,NULL);
	joshvm_esp32_media_start(handle_player_test,media_player_callback_test);

	vTaskDelay(10000 / portTICK_PERIOD_MS); 

	joshvm_esp32_media_stop(handle_player_test);
	
	joshvm_esp32_media_release(handle_player_test);
	
}


