#include "audio_pipeline.h"
#include "esp_audio.h"
#include "audio_mem.h"
#include "audio_common.h"
#include "audio_pipeline.h"
#include "joshvm_esp32_ring_buff.h"
#include "joshvm_esp32_media.h"
#include "joshvm_esp32_record.h"
#include "joshvm_esp32_player.h"
#include "joshvm.h"
#include "esp_log.h"
#include "string.h"
//---test
#include "joshvm_esp32_rec_engine.h"


//---define
#define TAG  "JOSHVM_ESP32_MEDIA>>>"
//5*48*2*1000    48K*2CHA*5S = 16K*1CHA*60S 
#define A_RECORD_RB_SIZE 48*10000
#define A_TRACK_RB_SIZE 48*10000
//---variable
static int8_t audio_status = 0;
static struct{
	int format;
	char* url;
	int sample_rate;
	int channel;
	int bit_rate;
}j_meida_rec_default_cfg = {joshvm_meida_format_wav,"/sdcard/default.wav",16000,1,16};

static struct{
	int sample_rate;
	int channel;
	int bit_rate;
}j_audio_track_default_cfg = {16000,1,16};

static struct{
	int sample_rate;
	int channel;
	int bit_rate;
}j_audio_rec_default_cfg = {16000,1,16};


joshvm_media_t *joshvm_media = NULL;
static TaskHandle_t audio_recorder_handler = NULL;
static TaskHandle_t audio_track_handler = NULL;


//------------------test--start-----------------
void *handle_media_rec_test = NULL;
void *handle_media_player_test = NULL;
void *handle_player_test = NULL;
void *handle_recorder_test = NULL;


#define MP3_URI_TEST "file://sdcard/48000.wav"

static void media_player_callback_test(void*handle,int para)
{
	ESP_LOGI(TAG,"media_player_callback");
}


//------------------test--end-----------------
//留给播放结束的回调
void joshvm_esp32_media_callback()
{
	ESP_LOGI(TAG,"joshvm_esp32_media_callback");

	//audio_status = JOSHVM_MEDIA_RESERVE;
	joshvm_media->joshvm_media_u.joshvm_media_mediaplayer.callback(joshvm_media,0);//need to rewrite
}

static ring_buffer_t audio_recorder_rb;
static ring_buffer_t audio_track_rb;
static ring_buffer_t audio_vad_rb;

int joshvm_esp32_media_create(int type, void** handle)
{
	printf(">>>-----------------------------------<<<\r\n");
	printf("--->>>MEGA_ESP32 Version beta_v1.13*<<<---\r\n");
	printf(">>>-----------------------------------<<<\r\n");

	int ret = JOSHVM_OK;	
	joshvm_media = (joshvm_media_t*)audio_calloc(1, sizeof(joshvm_media_t));
	joshvm_media->media_type = type;
	joshvm_media->evt_que = xQueueCreate(2, sizeof(esp_audio_state_t));	
	switch(type){
		case MEDIA_PLAYER: 	
			joshvm_audio_wrapper_init();
			ESP_LOGI(TAG,"MediaPlayer created!");
			break;
		case MEDIA_RECORDER: 	
			joshvm_media->joshvm_media_u.joshvm_media_mediarecorder.format = j_meida_rec_default_cfg.format;
			joshvm_media->joshvm_media_u.joshvm_media_mediarecorder.url = j_meida_rec_default_cfg.url;
			joshvm_media->joshvm_media_u.joshvm_media_mediarecorder.sample_rate = j_meida_rec_default_cfg.sample_rate;
			joshvm_media->joshvm_media_u.joshvm_media_mediarecorder.channel = j_meida_rec_default_cfg.channel;
			joshvm_media->joshvm_media_u.joshvm_media_mediarecorder.bit_rate = j_meida_rec_default_cfg.bit_rate;	
			ret = joshvm_meida_recorder_init(joshvm_media);
			ESP_LOGI(TAG,"MediaRecorder created!");
			break;
		case AUDIO_TRACK:
			ring_buffer_init(&audio_track_rb,A_TRACK_RB_SIZE);
			joshvm_media->joshvm_media_u.joshvm_media_audiotrack.sample_rate = j_audio_track_default_cfg.sample_rate;
			joshvm_media->joshvm_media_u.joshvm_media_audiotrack.channel = j_audio_track_default_cfg.channel;
			joshvm_media->joshvm_media_u.joshvm_media_audiotrack.bit_rate = j_audio_track_default_cfg.bit_rate;
			joshvm_media->joshvm_media_u.joshvm_media_audiotrack.track_rb = &audio_track_rb;
			ESP_LOGI(TAG,"AudioTrack created!");
			break;
		case AUDIO_RECORDER:			
			ring_buffer_init(&audio_recorder_rb,A_RECORD_RB_SIZE);
			joshvm_media->joshvm_media_u.joshvm_media_audiorecorder.sample_rate = j_audio_rec_default_cfg.sample_rate;
			joshvm_media->joshvm_media_u.joshvm_media_audiorecorder.channel = j_audio_rec_default_cfg.channel;
			joshvm_media->joshvm_media_u.joshvm_media_audiorecorder.bit_rate = j_audio_rec_default_cfg.bit_rate;
			joshvm_media->joshvm_media_u.joshvm_media_audiorecorder.rec_rb = &audio_recorder_rb;
			ESP_LOGI(TAG,"AudioRecorder created!");
			break;
		case AUDIO_VAD_REC:
			ring_buffer_init(&audio_vad_rb,JOSHVM_CYCLEBUF_BUFFER_SIZE);
			joshvm_media->joshvm_media_u.joshvm_media_audio_vad_rec.rec_rb = &audio_vad_rb;
			ESP_LOGI(TAG,"VAD AudioRecorder created!");
			break;
		default :
			return JOSHVM_INVALID_ARGUMENT;
			break;
	}	

	*handle = joshvm_media;	
	return ret;
}


int joshvm_esp32_media_close(joshvm_media_t* handle)
{
	ESP_LOGI(TAG,"joshvm_esp32_media_close");
	vQueueDelete(((joshvm_media_t*)handle)->evt_que);
	
	switch(handle->media_type)
	{
		case MEDIA_PLAYER:
			joshvm_audio_player_destroy();
			ESP_LOGI(TAG,"MediaPlayer closed!");
			break;
		case MEDIA_RECORDER:
			joshvm_media_recorder_release(handle);
			ESP_LOGI(TAG,"MediaRecorder closed!");
			break;
		case AUDIO_TRACK:
			//joshvm_audio_track_release(handle);
			ring_buffer_deinit(handle->joshvm_media_u.joshvm_media_audiotrack.track_rb);
			break;
		case AUDIO_RECORDER:
			//joshvm_audio_rcorder_release(handle);
			ring_buffer_deinit(handle->joshvm_media_u.joshvm_media_audiorecorder.rec_rb);
			break;
		case AUDIO_VAD_REC:
			//joshvm_audio_rcorder_release(handle);
			ring_buffer_deinit(handle->joshvm_media_u.joshvm_media_audio_vad_rec.rec_rb);
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
		default : 
			ret = JOSHVM_NOT_SUPPORTED;
			break;
	}

	return ret;
}

int joshvm_esp32_media_start(joshvm_media_t* handle, void(*callback)(void*, int))
{
	ESP_LOGI(TAG,"joshvm_esp32_media_start");
	QueueHandle_t que = handle->evt_que;
	uint16_t que_val = 0;
	int ret = 0;
	if(audio_status != JOSHVM_MEDIA_PAUSED){	//start		
		switch(handle->media_type){
			case MEDIA_PLAYER:			
				handle->joshvm_media_u.joshvm_media_mediaplayer.callback = callback;
				ret = joshvm_audio_play_handler(handle->joshvm_media_u.joshvm_media_mediaplayer.url);
				//joshvm_spiffs_audio_play_handler(handle->joshvm_media_u.joshvm_media_mediaplayer.url);				
				break;
			case MEDIA_RECORDER:
				ret = audio_pipeline_run(handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.pipeline);
				break;
			case AUDIO_TRACK:
				handle->joshvm_media_u.joshvm_media_audiotrack.callback = callback;
				ret = joshvm_audio_track_init(handle);
				que_val = QUE_TRACK_START;
				xQueueSend(que, &que_val, (portTickType)0);
				xTaskCreate(joshvm_audio_track_task,"joshvm_audio_track_task",2*1024,(void*)handle,JOSHVM_AUDIO_TRACK_TASK_PRI,&audio_track_handler);
				ESP_LOGI(TAG,"AudioTrack start!");
				break;
			case AUDIO_RECORDER:
				ret = joshvm_audio_recorder_init(handle);				
				xTaskCreate(joshvm_audio_recorder_task, "joshvm_audio_recorder_task", 2 * 1024, (void*)handle, JOSHVM_AUDIO_RECORDER_TASK_PRI, &audio_recorder_handler);
				ESP_LOGI(TAG,"AudioRecorder start!");
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
	QueueHandle_t que = handle->evt_que;
	uint16_t que_val = 0;
	int ret = JOSHVM_FAIL;
	switch(handle->media_type){
		case MEDIA_PLAYER:			
			if((ret = joshvm_audio_stop_handler()) != ESP_OK){
				return JOSHVM_FAIL;
			}	
			ret = JOSHVM_OK;
			ESP_LOGI(TAG,"MediaPlayer stop!");
			break;
		case MEDIA_RECORDER:
			ret = audio_pipeline_terminate(handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.pipeline);	
			ESP_LOGI(TAG,"MediaRecorder stop!");
			break;
		case AUDIO_TRACK:	
			que_val = QUE_TRACK_STOP;
			xQueueSend(que, &que_val, (portTickType)0);
			ESP_LOGI(TAG,"AudioTrack stop!");
			ret = JOSHVM_OK;
			break;
		case AUDIO_RECORDER:
			que_val = QUE_RECORD_STOP;
			xQueueSend(que, &que_val, (portTickType)0);
			//vTaskSuspend(audio_recorder_handler);//test
			ret = audio_pipeline_terminate(handle->joshvm_media_u.joshvm_media_audiorecorder.audiorecorder_t.pipeline);
			joshvm_audio_rcorder_release(handle);
			ESP_LOGI(TAG,"AudioRedorder stop!");
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
			handle->joshvm_media_u.joshvm_media_mediarecorder.format = j_meida_rec_default_cfg.format;
			handle->joshvm_media_u.joshvm_media_mediarecorder.url = j_meida_rec_default_cfg.url;
			handle->joshvm_media_u.joshvm_media_mediarecorder.sample_rate = j_meida_rec_default_cfg.sample_rate;
			handle->joshvm_media_u.joshvm_media_mediarecorder.channel = j_meida_rec_default_cfg.channel;
			handle->joshvm_media_u.joshvm_media_mediarecorder.bit_rate = j_meida_rec_default_cfg.bit_rate;	
			ret = JOSHVM_OK;
			break;
		default :
			ret = JOSHVM_NOT_SUPPORTED;
			break;
	}
	return ret;
}

int joshvm_esp32_media_get_state(joshvm_media_t* handle, int* state)
{
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
			joshvm_media_get_state(handle,state);
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
	int ret = JOSHVM_OK;
	switch(handle->media_type){
		case AUDIO_RECORDER:
			joshvm_audio_recorder_read(handle->joshvm_media_u.joshvm_media_audiorecorder.rec_rb,buffer,size,bytesRead);
			break;
		case AUDIO_VAD_REC:
			joshvm_audio_recorder_read(handle->joshvm_media_u.joshvm_media_audio_vad_rec.rec_rb,buffer,size,bytesRead);
			break;			
		default:
			ret = JOSHVM_INVALID_ARGUMENT;
			break;		
	}

	//joshvm_audio_recorder_read(handle,buffer,size,bytesRead);
	return ret;
}

int joshvm_esp32_media_write(joshvm_media_t* handle, unsigned char* buffer, int size, int* bytesWritten, void(*callback)(void*, int))
{
	return joshvm_audio_track_write(handle->joshvm_media_u.joshvm_media_audiotrack.track_rb,buffer,size,bytesWritten);
}

int joshvm_esp32_media_flush(joshvm_media_t* handle)
{
	ring_buffer_flush(handle->joshvm_media_u.joshvm_media_audiotrack.track_rb);
	return 0;
}

int joshvm_esp32_media_get_buffsize(joshvm_media_t* handle, int* size)
{	
	switch(handle->media_type){
		case AUDIO_TRACK:
			*size = A_TRACK_RB_SIZE;
			break;
		case AUDIO_RECORDER:
			*size = A_RECORD_RB_SIZE;
			break;
		default :
			return JOSHVM_NOT_SUPPORTED;
			break;
	}
	return 0;
}

int joshvm_esp32_media_set_audio_sample_rate(joshvm_media_t* handle, uint32_t value)
{
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
			handle->joshvm_media_u.joshvm_media_audiorecorder.sample_rate = value;
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
			handle->joshvm_media_u.joshvm_media_audiorecorder.channel = value;
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
			handle->joshvm_media_u.joshvm_media_audiorecorder.bit_rate = value;
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
	int ret;
	switch(handle->media_type){
		case MEDIA_PLAYER:
			if(strstr(source,source_http_head) == NULL){	
				strcat(source_head,source);
				handle->joshvm_media_u.joshvm_media_mediaplayer.url = source_head;
				ESP_LOGI(TAG,"Set MediaPlayer Source:%s",source_head);
			}else{
				handle->joshvm_media_u.joshvm_media_mediaplayer.url = source;
				ESP_LOGI(TAG,"Set MediaPlayer Source:%s",source);
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
	*volume = 100;
	return JOSHVM_OK;
}

int joshvm_esp32_media_get_volume(int* volume)
{
	ESP_LOGI(TAG,"joshvm_esp32_media_get_volume");
	return joshvm_volume_get_handler(volume);
}

int joshvm_esp32_media_set_volume(int volume)
{
	ESP_LOGI(TAG,"joshvm_esp32_media_set_volume");
	//adjust
	volume++;
	return joshvm_volume_set_handler(volume);
}

int joshvm_esp32_media_add_volume()
{
	ESP_LOGI(TAG,"joshvm_esp32_media_add_volume");
	joshvm_volume_adjust_handler(11);
	return 0;
}

int joshvm_esp32_media_sub_volume()
{
	ESP_LOGI(TAG,"joshvm_esp32_media_sub_volume");
	joshvm_volume_adjust_handler(-9);
	return 0;
}

int joshvm_esp32_media_release(void* handle)
{
	return 0;
}

int volume;
void test_esp32_media(void)
{

/*//---wakeup test
	joshvm_esp32_wakeup_enable(media_player_callback_test);

	vTaskDelay(20000 / portTICK_PERIOD_MS); 

	
	joshvm_esp32_wakeup_enable(media_player_callback_test);

*/

	


	//joshvm_esp32_media_create(0,&handle_media_player_test);
	//joshvm_esp32_media_create(1,&handle_media_rec_test);
	joshvm_esp32_media_create(2,&handle_player_test);
	joshvm_esp32_media_create(3,&handle_recorder_test);
	//joshvm_esp32_vad_start(test_vad_callback);



	

	joshvm_esp32_media_start(handle_recorder_test,media_player_callback_test);
	vTaskDelay(10000 / portTICK_PERIOD_MS); 
	joshvm_esp32_media_stop(handle_recorder_test);

	//joshvm_esp32_vad_stop();


	((joshvm_media_t*)handle_player_test)->joshvm_media_u.joshvm_media_audiotrack.track_rb = ((joshvm_media_t*)handle_recorder_test)->joshvm_media_u.joshvm_media_audiorecorder.rec_rb;

	joshvm_esp32_media_start(handle_player_test,media_player_callback_test);
	vTaskDelay(2000 / portTICK_PERIOD_MS); 
	printf("track  %d\n",audio_element_get_state(((joshvm_media_t*)handle_player_test)->joshvm_media_u.joshvm_media_audiotrack.audiotrack_t.i2s));
	joshvm_esp32_media_stop(handle_player_test);


/*
	vTaskDelay(2000 / portTICK_PERIOD_MS); 
	joshvm_esp32_vad_start(test_vad_callback);
	vTaskDelay(10000 / portTICK_PERIOD_MS); 


	joshvm_esp32_media_start(handle_player_test,media_player_callback_test);
	vTaskDelay(10000 / portTICK_PERIOD_MS); 
	printf("track  %d\n",audio_element_get_state(((joshvm_media_t*)handle_player_test)->joshvm_media_u.joshvm_media_audiotrack.audiotrack_t.i2s));
	joshvm_esp32_media_stop(handle_player_test);

*/	
	
/*	//joshvm_esp32_media_set_source(handle_player_test,MP3_URI_TEST);
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
	*/
}


