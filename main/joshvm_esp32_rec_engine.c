#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_log.h"
#include "board.h"
#include "audio_common.h"
#include "audio_pipeline.h"
#include "mp3_decoder.h"
#include "i2s_stream.h"
#include "raw_stream.h"
#include "filter_resample.h"
#include "esp_sr_iface.h"
#include "esp_sr_models.h"
#include "sdkconfig.h"
#include "audio_mem.h"
#include "esp_audio.h"
#include "audio_element.h"
#include "esp_vad.h"

#include "joshvm_esp32_rec_engine.h"
#include "joshvm_esp32_media.h"
#include "joshvm.h"
#include "joshvm_esp32_ring_buff.h"


#include "recorder_engine.h"

//---enum
typedef enum {
    WAKE_UP = 1,
    OPEN_THE_LIGHT,
    CLOSE_THE_LIGHT,
    VOLUME_INCREASE,
    VOLUME_DOWN,
    PLAY,
    PAUSE,
    MUTE,
    PLAY_LOCAL_MUSIC,
} asr_event_e;

typedef enum{
	EVENT_WAKEUP_START = 0,
	EVENT_VAD_START,
	EVENT_VAD_STOP,
	EVENT_WAKEUP_END,
}rec_event_tpye_e;

typedef enum{
	WAKEUP_ENABLE  = 1,
	WAKEUP_DISABLE,
	VAD_START,
	VAD_STOP,
}rec_status_e;

//---struct
typedef struct{
	const esp_sr_iface_t * model;
	model_iface_data_t *iface;
	vad_handle_t vad_inst;
	int8_t wakeup_state;
	int8_t vad_state;
	uint32_t vad_off_time;
	int16_t *voice_buff;
	void* i2s_reader;
	void* filter;
	void* raw_reader;
	void* pipeline;
	void(*rec_vad_cb)(rec_event_tpye_e,void*);
	void(*wakeup_callback)(int);//notify jvm
	void(*vad_callback)(int);//notify jvm
}rec_engine_t;

//---variable
static const char *TAG = "JOSHVM_REC_ENGINE>>>>>>>";
static rec_engine_t rec_engine;
static int8_t need_notify_vad_stop = false;
static TaskHandle_t rec_engine_task_handler = NULL;
uint32_t vad_off_time = 0;

extern joshvm_media_t *joshvm_media;

//---define
#define VAD_SAMPLE_RATE_HZ 16000
#define VAD_FRAME_LENGTH_MS 30
#define VAD_BUFFER_LENGTH (VAD_FRAME_LENGTH_MS * VAD_SAMPLE_RATE_HZ / 1000)
#define VAD_OFF_TIME 1000


static void rec_vad_cb(rec_event_tpye_e type, void *user_data)
{
	if (EVENT_VAD_START == type) {
		ESP_LOGI(TAG, "rec_engine_cb - REC_EVENT_VAD_START");
		
		
		
    } else if (EVENT_VAD_STOP == type) {
        ESP_LOGI(TAG, "rec_engine_cb - REC_EVENT_VAD_STOP");
		
    } else {
    }
}

static uint32_t recorder_pipeline_init(rec_engine_t* rec_engine)
{	
	ESP_LOGI(TAG,"recorder_pipeline_init");
#if 0
		rec_engine->model = &esp_sr_wakenet4_quantized;
#else
		rec_engine->model = &esp_sr_wakenet3_quantized;
#endif
		rec_engine->iface = rec_engine->model->create(DET_MODE_90);
		int num = rec_engine->model->get_word_num(rec_engine->iface);
		for (int i = 1; i <= num; i++) {
			char *name = rec_engine->model->get_word_name(rec_engine->iface, i);
			ESP_LOGI(TAG, "keywords: %s (index = %d)", name, i);
		}
		float threshold = rec_engine->model->get_det_threshold_by_mode(rec_engine->iface, DET_MODE_90, 1);
		int sample_rate = rec_engine->model->get_samp_rate(rec_engine->iface);
		int audio_chunksize = rec_engine->model->get_samp_chunksize(rec_engine->iface);
		ESP_LOGI(TAG, "keywords_num = %d, threshold = %f, sample_rate = %d, chunksize = %d, sizeof_uint16 = %d", num, threshold, sample_rate, audio_chunksize, sizeof(int16_t));
		rec_engine->voice_buff = (int16_t *)audio_malloc(audio_chunksize * sizeof(short));
		audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
		rec_engine->pipeline = audio_pipeline_init(&pipeline_cfg);
		mem_assert(rec_engine->pipeline);
	
		i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
		i2s_cfg.i2s_config.sample_rate = 48000;
		i2s_cfg.type = AUDIO_STREAM_READER;
#if defined CONFIG_ESP_LYRAT_MINI_V1_1_BOARD
		i2s_cfg.i2s_port = 1;
#endif
		rec_engine->i2s_reader = i2s_stream_init(&i2s_cfg);
		rsp_filter_cfg_t rsp_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
		rsp_cfg.src_rate = 48000;
		rsp_cfg.src_ch = 2;
		rsp_cfg.dest_rate = 16000;
		rsp_cfg.dest_ch = 1;
		rsp_cfg.type = AUDIO_CODEC_TYPE_ENCODER;
		rec_engine->filter = rsp_filter_init(&rsp_cfg);
	
		raw_stream_cfg_t raw_cfg = {
			.out_rb_size = 8 * 1024,
			.type = AUDIO_STREAM_READER,
		};
		rec_engine->raw_reader = raw_stream_init(&raw_cfg);
	
		audio_pipeline_register(rec_engine->pipeline, rec_engine->i2s_reader, "i2s");
		audio_pipeline_register(rec_engine->pipeline, rec_engine->filter, "filter");
		audio_pipeline_register(rec_engine->pipeline, rec_engine->raw_reader, "raw");
		audio_pipeline_link(rec_engine->pipeline, (const char *[]) {"i2s", "filter", "raw"}, 3);
		audio_pipeline_run(rec_engine->pipeline);

		rec_engine->vad_inst = vad_create(VAD_MODE_4, VAD_SAMPLE_RATE_HZ, VAD_FRAME_LENGTH_MS);
		
	return 0;
}

static uint32_t recorder_pipeline_deinit(rec_engine_t* rec_engine)
{
	ESP_LOGI(TAG,"recorder_pipeline_deinit");
	ESP_LOGI(TAG, "[ 7 ] Destroy model");
	rec_engine->model->destroy(rec_engine->iface);
	rec_engine->model = NULL;

	vad_destroy(rec_engine->vad_inst);
	audio_free(rec_engine->voice_buff);
	rec_engine->voice_buff = NULL;
	
	audio_pipeline_terminate(rec_engine->pipeline);	
	/* Terminate the pipeline before removing the listener */
	audio_pipeline_remove_listener(rec_engine->pipeline);	
	audio_pipeline_unregister(rec_engine->pipeline, rec_engine->raw_reader);
	audio_pipeline_unregister(rec_engine->pipeline, rec_engine->i2s_reader);
	audio_pipeline_unregister(rec_engine->pipeline, rec_engine->filter);	
	/* Release all resources */
	audio_pipeline_deinit(rec_engine->pipeline);
	audio_element_deinit(rec_engine->raw_reader);
	audio_element_deinit(rec_engine->i2s_reader);
	audio_element_deinit(rec_engine->filter);	

	return 0;
}

static void rec_engine_task(void* handle)
{
	ESP_LOGI(TAG,"rec_engine_task");
	vad_state_t last_vad_state = 0;
	vad_state_t vad_state = 0;
	rec_engine_t* rec_engine = (rec_engine_t*)handle;
	int audio_chunksize = rec_engine->model->get_samp_chunksize(rec_engine->iface);
	audio_chunksize = audio_chunksize * sizeof(short);
	//int16_t *voice_buff = (int16_t *)audio_malloc(audio_chunksize * sizeof(short));
	if (NULL == rec_engine->voice_buff) {
		ESP_LOGE(TAG, "Memory allocation failed!");
		rec_engine->model->destroy(rec_engine->iface);
		rec_engine->model = NULL;
		return;
	}

	int8_t vad_writer_buff_flag = 0;
	while (1) {
		raw_stream_read(rec_engine->raw_reader, (char *)rec_engine->voice_buff, audio_chunksize);
		
		if(rec_engine->vad_state == VAD_START){	
			vad_state = vad_process(rec_engine->vad_inst, rec_engine->voice_buff);
			
			if(vad_state == VAD_SPEECH){
				//ESP_LOGI(TAG, "Speech detected");
				//clear timer,vad_off_time increase 1 per 200ms
				vad_off_time = 0;
			}			
			
			//vad stop		
			if(((vad_off_time * 200) >= rec_engine->vad_off_time) && (need_notify_vad_stop == true)){
				ESP_LOGI(TAG,"VAD_STOP");
				need_notify_vad_stop = false;
				rec_engine->vad_callback(1);
				vad_writer_buff_flag = 0;
			}
				
			//detect voice 
			if((vad_state != last_vad_state) && (vad_state == VAD_SPEECH) && (vad_writer_buff_flag == 0)){
				ESP_LOGI(TAG,"VAD_START");
				last_vad_state = vad_state;
				rec_engine->vad_callback(0);
				vad_writer_buff_flag = 1;				
			}else if((vad_state != last_vad_state) && (vad_state == VAD_SILENCE)){
				last_vad_state = vad_state;
				need_notify_vad_stop = true;
			}

			if(vad_writer_buff_flag){
				ring_buffer_write(rec_engine->voice_buff,audio_chunksize,joshvm_media->joshvm_media_u.joshvm_media_audio_vad_rec.rec_rb);
			}
		}

		if(rec_engine->wakeup_state == WAKEUP_ENABLE){
			int keyword = rec_engine->model->detect(rec_engine->iface, (int16_t *)rec_engine->voice_buff);
			switch (keyword) {
				case WAKE_UP:
					ESP_LOGI(TAG, "Wake up");
					rec_engine->wakeup_callback(0);
					break;
				case OPEN_THE_LIGHT:
					ESP_LOGI(TAG, "Turn on the light");
					break;
				case CLOSE_THE_LIGHT:
					ESP_LOGI(TAG, "Turn off the light");
					break;
				case VOLUME_INCREASE:
					ESP_LOGI(TAG, "volume increase");
					break;
				case VOLUME_DOWN:
					ESP_LOGI(TAG, "volume down");
					break;
				case PLAY:
					ESP_LOGI(TAG, "play");
					break;
				case PAUSE:
					ESP_LOGI(TAG, "pause");
					break;
				case MUTE:
					ESP_LOGI(TAG, "mute");
					break;
				case PLAY_LOCAL_MUSIC:
					ESP_LOGI(TAG, "play local music");
					break;
				default:
					ESP_LOGD(TAG, "Not supported keyword");
					break;
			}
		}
/*
		if((WAKEUP_DISABLE == rec_engine->wakeup_state) && (VAD_STOP == rec_engine->vad_state)){
			break;
		}		*/
	}
}




static esp_err_t joshvm_rec_engine_create(rec_engine_t* rec_engine,rec_status_e type)
{
	int8_t wakeup_state,vad_state;
	wakeup_state = rec_engine->wakeup_state;
	vad_state = rec_engine->vad_state;

	switch(type){
		case WAKEUP_ENABLE:
			rec_engine->wakeup_state = WAKEUP_ENABLE;
		break;
		case VAD_START:
			rec_engine->vad_state = VAD_START;
		break;
		default :

		break;
	}

	if((wakeup_state == WAKEUP_ENABLE) || (vad_state == VAD_START)){
		ESP_LOGI(TAG,"rec_engine have create!");
		return JOSHVM_OK;
	}
	
	recorder_pipeline_init(rec_engine);
	xTaskCreate(rec_engine_task, "rec_engine_task",2*1024, rec_engine, 10, &rec_engine_task_handler);

	return 0;
}

esp_err_t joshvm_rec_engine_destroy(rec_engine_t* rec_engine,rec_status_e type)
{
	switch(type){
		case WAKEUP_DISABLE:
			rec_engine->wakeup_state = WAKEUP_DISABLE;
		break;
		case VAD_STOP:
			rec_engine->vad_state = VAD_STOP;
		break;
		default:

		break;
	}

	if((rec_engine->wakeup_state == WAKEUP_DISABLE) && (rec_engine->vad_state == VAD_STOP)){
		vTaskDelete(rec_engine_task_handler);
		
		//recorder_pipeline_deinit(&rec_engine);
	}
	
	return 0;
}



//---------------------------------------------
int joshvm_esp32_wakeup_get_word_count(void)
{	
	return 1;
}

int joshvm_esp32_wakeup_get_word(int pos, int* index, char* wordbuf, int wordlen, char* descbuf, int desclen)
{
	*index = 0;
	memcpy(wordbuf,"Hi LeXin",sizeof("Hi LeXin"));	
	memcpy(descbuf,"enjoy happy",sizeof("enjoy happy"));

	return 0;
}

int joshvm_esp32_wakeup_enable(void(*callback)(int))
{
	ESP_LOGI(TAG,"joshvm_esp32_wakeup_enable");
	rec_engine.wakeup_callback = callback;
	joshvm_rec_engine_create(&rec_engine,WAKEUP_ENABLE);
	
	

	return JOSHVM_FAIL;

}

int joshvm_esp32_wakeup_disable()
{
	ESP_LOGI(TAG,"joshvm_esp32_wakeup_disable");
	joshvm_rec_engine_destroy(&rec_engine,WAKEUP_DISABLE);


		
	return JOSHVM_FAIL;
}

int joshvm_esp32_vad_start(void(*callback)(int))
{	
	ESP_LOGI(TAG,"joshvm_esp32_vad_start");
	rec_engine.vad_off_time = VAD_OFF_TIME;
	rec_engine.vad_callback = callback;
	joshvm_rec_engine_create(&rec_engine,VAD_START);
	


	return JOSHVM_FAIL;
}

int joshvm_esp32_vad_pause()
{
	

	return 0;
}

int joshvm_esp32_vad_resume()
{

	return 0;
}

int joshvm_esp32_vad_stop()
{
	ESP_LOGI(TAG, " joshvm_esp32_vad_stop"); 	
	joshvm_rec_engine_destroy(&rec_engine,VAD_STOP);
	return JOSHVM_OK;
}

int joshvm_esp32_vad_set_timeout(int ms)
{
	rec_engine.vad_off_time = ms;
	return JOSHVM_OK;
}

//--test-------------------

void test_callback(int index)
{
	printf("wakeup   callback  index = %d\n",index);
	
}

void test_vad_callback(int index)
{
	printf("vad   callback  ---------------- index = %d\n",index);
	
}

void test_rec_engine(void)
{

	joshvm_esp32_wakeup_enable(test_callback);

	joshvm_esp32_vad_start(test_vad_callback);



	
	

}

