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
#include "dueros_service.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_mem.h"
#include "i2s_stream.h"
#include "raw_stream.h"
#include "filter_resample.h"

#include "joshvm_esp32_rec_engine.h"
#include "joshvm_esp32_media.h"
#include "joshvm.h"
#include "joshvm_esp32_ring_buff.h"


//---struct
typedef struct {
	uint32_t vad_off_ms;
	uint8_t sign;
	void(*wakeup_callback)(int);
	void(*vad_callback)(int);
}j_rec_engine_create_t;

//---variable
static const char *TAG = "JOSHVM_REC_ENGINE>>>>>>>";
static j_rec_engine_create_t *j_rec_engine_cfg;
static int8_t j_wakeup_status = 0;	//detect wakeup have created or not
static int8_t j_vad_status = 0;	//detect vad have created or not
static uint8_t vad_run = 0;//SAVE_OFF/SAVE_ON
static QueueHandle_t j_que_vad = NULL;
static QueueHandle_t j_que_wakeup  = NULL;


//---define
#define VAD_OFF_MS	800
#define REC_WAKEUP_OFF	0
#define REC_WAKEUP_ON	1
#define REC_VAD_OFF	0
#define REC_VAD_ON	1
#define	SIGN_WAKEUP	1
#define SIGN_VAD	2
#define VAD_TASK_VAD_START	1
#define VAD_TASK_VAD_STOP	2
#define QUEUE_VAD_LEN	4
#define QUEUE_VAD_SIZE	4

//---enum
enum{
	SAVE_OFF = 0,
	SAVE_ON,
}J_QUE_VAD_E;

enum{
	WAKEUP_OFF = 0,
	WAKEUP_ON,
}J_QUE_WAKEUP_E;


extern joshvm_media_t *joshvm_media;


static void vad_task(void *handle)
{
	uint32_t r_queue = 0;	
	uint8_t task_run = 1;
	uint8_t *voiceData = audio_calloc(1, REC_ONE_BLOCK_SIZE);
	if (NULL == voiceData) {
        ESP_LOGE(TAG, "Func:%s, Line:%d, Malloc failed", __func__, __LINE__);
		return;
    }
	
	while(task_run){
		xQueueReceive(j_que_vad, &r_queue, portMAX_DELAY);
		if(joshvm_media == NULL){
		 	ESP_LOGE(TAG, "joshvm_media = NULL. Func:%s, Line:%d, Malloc failed", __func__, __LINE__);
			return;
		}

		ring_buffer_t *rb = joshvm_media->joshvm_media_u.joshvm_media_audio_vad_rec.rec_rb;
		
		switch(r_queue){
			case VAD_TASK_VAD_START:
				ESP_LOGI(TAG, "VAD_TASK_VAD_START");

				while(vad_run){					
					int ret = rec_engine_data_read(voiceData, REC_ONE_BLOCK_SIZE, 110 / portTICK_PERIOD_MS);
					ESP_LOGD(TAG, "index = %d", ret);
					if ((ret == 0) || (ret == -1)) {
						continue;
					}

					if(ret > 0){	
						ring_buffer_write(voiceData,REC_ONE_BLOCK_SIZE,rb);
					}    		
				}		
				break;	
			case VAD_TASK_VAD_STOP:
				ESP_LOGI(TAG, "VAD_TASK_VAD_STOP");
				task_run = 0;
				break;	
			default :
				break;
		}
	}
	free(voiceData);
	voiceData = NULL;
	vTaskDelete(NULL);
}

static void rec_engine_cb(rec_event_type_t type, void *user_data)
{
	uint32_t r_queue;
	j_rec_engine_create_t *callback = (j_rec_engine_create_t*)user_data;
    if (REC_EVENT_WAKEUP_START == type) {
		if(callback->sign == SIGN_VAD){return;}

        ESP_LOGI(TAG, "REC_ENGINE_CB_EVENT_WAKEUP_START");
		((void(*)(int))callback->wakeup_callback)(1);
		rec_engine_vad_enable(false);
		
			
    } else if (REC_EVENT_VAD_START == type) {
		if(callback->sign == SIGN_VAD){
	        ESP_LOGI(TAG, "REC_ENGINE_CB_EVENT_VAD_START");
			vad_run = SAVE_ON;
			r_queue = VAD_TASK_VAD_START;
			xQueueSend(j_que_vad, &r_queue, 0);
			((void(*)(int))callback->vad_callback)(0);
		}
		
    } else if (REC_EVENT_VAD_STOP == type) {
    	if(callback->sign == SIGN_VAD){
	        ESP_LOGI(TAG, "REC_ENGINE_CB_EVENT_VAD_STOP");
			vad_run = SAVE_OFF;
			r_queue = VAD_TASK_VAD_STOP;
			xQueueSend(j_que_vad, &r_queue, 0);
			((void(*)(int))callback->vad_callback)(1);
    	}

    } else if (REC_EVENT_WAKEUP_END == type) {
		if(callback->sign == SIGN_VAD){return;}	
		
        ESP_LOGI(TAG, "REC_ENGINE_CB_EVENT_WAKEUP_END");
		r_queue = WAKEUP_ON;
		xQueueSend(j_que_wakeup, &r_queue, 0);
	
    } else {
    }
}

static  audio_element_handle_t raw_read;
#ifdef CONFIG_ESP_LYRATD_MINI_V1_1_BOARD
static esp_err_t recorder_pipeline_open_for_mini(void **handle)
{
    audio_element_handle_t i2s_stream_reader;
    audio_pipeline_handle_t recorder;
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    recorder = audio_pipeline_init(&pipeline_cfg);
    if (NULL == recorder) {
        return ESP_FAIL;
    }
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.i2s_port = 1;
    i2s_cfg.i2s_config.use_apll = 0;
    i2s_cfg.i2s_config.sample_rate = 16000;
    i2s_cfg.i2s_config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
    i2s_cfg.type = AUDIO_STREAM_READER;
    i2s_stream_reader = i2s_stream_init(&i2s_cfg);

    raw_stream_cfg_t raw_cfg = RAW_STREAM_CFG_DEFAULT();
    raw_cfg.type = AUDIO_STREAM_READER;
    raw_read = raw_stream_init(&raw_cfg);

    audio_pipeline_register(recorder, i2s_stream_reader, "i2s");
    audio_pipeline_register(recorder, raw_read, "raw");

    audio_pipeline_link(recorder, (const char *[]) {"i2s", "raw"}, 2);
    audio_pipeline_run(recorder);
    *handle = recorder;
    return ESP_OK;
}
#else
static esp_err_t recorder_pipeline_open(void **handle)
{
    audio_element_handle_t i2s_stream_reader;
    audio_pipeline_handle_t recorder;
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    recorder = audio_pipeline_init(&pipeline_cfg);
    if (NULL == recorder) {
        return ESP_FAIL;
    }
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_READER;
    i2s_stream_reader = i2s_stream_init(&i2s_cfg);
    audio_element_info_t i2s_info = {0};
    audio_element_getinfo(i2s_stream_reader, &i2s_info);
    i2s_info.bits = 16;
    i2s_info.channels = 2;
    i2s_info.sample_rates = 48000;
    audio_element_setinfo(i2s_stream_reader, &i2s_info);

    rsp_filter_cfg_t rsp_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
    rsp_cfg.src_rate = 48000;
    rsp_cfg.src_ch = 2;
    rsp_cfg.dest_rate = 16000;
    rsp_cfg.dest_ch = 1;
    rsp_cfg.type = AUDIO_CODEC_TYPE_ENCODER;
    audio_element_handle_t filter = rsp_filter_init(&rsp_cfg);

    raw_stream_cfg_t raw_cfg = RAW_STREAM_CFG_DEFAULT();
    raw_cfg.type = AUDIO_STREAM_READER;
    raw_read = raw_stream_init(&raw_cfg);

    audio_pipeline_register(recorder, i2s_stream_reader, "i2s");
    audio_pipeline_register(recorder, filter, "filter");
    audio_pipeline_register(recorder, raw_read, "raw");
    audio_pipeline_link(recorder, (const char *[]) {"i2s", "filter", "raw"}, 3);
    audio_pipeline_run(recorder);
    *handle = recorder;
    return ESP_OK;
}
#endif

static esp_err_t recorder_pipeline_read(void *handle, char *data, int data_size)
{
    raw_stream_read(raw_read, data, data_size);
    return ESP_OK;
}

static esp_err_t recorder_pipeline_close(void *handle)
{
    audio_pipeline_deinit(handle);
    return ESP_OK;
}

esp_err_t joshvm_rec_engine_create(j_rec_engine_create_t *handle)
{		
	ESP_LOGI(TAG, " joshvm_rec_engine_create");

	rec_config_t eng = DEFAULT_REC_ENGINE_CONFIG();
	eng.vad_off_delay_ms = handle->vad_off_ms;
	eng.wakeup_time_ms = 2 * 1000;
	eng.evt_cb = rec_engine_cb;
#ifdef CONFIG_ESP_LYRATD_MINI_V1_1_BOARD
	eng.open = recorder_pipeline_open_for_mini;
#else
	eng.open = recorder_pipeline_open;
#endif
	eng.close = recorder_pipeline_close; 
	eng.fetch = recorder_pipeline_read;
	eng.extension = NULL;
	eng.support_encoding = false;
	eng.user_data = (void*)handle;
	return rec_engine_create(&eng);
}

esp_err_t joshvm_rec_engine_destroy(void)
{
	return rec_engine_destroy();
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
	ESP_LOGI(TAG, " joshvm_esp32_wakeup_enable");

	int ret = 0;
	if(REC_WAKEUP_ON == j_wakeup_status){
		ESP_LOGW(TAG,"rec_engine wakeup has been enable!");
		return JOSHVM_INVALID_STATE;
	}
	
	if((REC_VAD_OFF == j_vad_status) && (REC_WAKEUP_OFF == j_wakeup_status)){
		j_rec_engine_cfg = (j_rec_engine_create_t*)audio_malloc(sizeof(j_rec_engine_create_t));
		j_rec_engine_cfg->vad_off_ms = VAD_OFF_MS;
		ret = joshvm_rec_engine_create(j_rec_engine_cfg);
	}
	j_rec_engine_cfg->wakeup_callback = callback;
	j_wakeup_status = REC_WAKEUP_ON;	

	j_que_wakeup = xQueueCreate(QUEUE_VAD_LEN, QUEUE_VAD_SIZE);
	if(ESP_OK == ret){
		return JOSHVM_OK;
	}
	return JOSHVM_FAIL;
}

int joshvm_esp32_wakeup_disable()
{
	ESP_LOGI(TAG, " joshvm_esp32_wakeup_disable");

	if(	j_wakeup_status == REC_WAKEUP_OFF){
		ESP_LOGW(TAG,"there is no rec_engine wakeup running!");
		return JOSHVM_INVALID_STATE;
	}

	if((j_vad_status == REC_VAD_ON) && (j_wakeup_status == REC_WAKEUP_ON)){
		return JOSHVM_OK;
	}	

	ESP_LOGI(TAG,"Waitting for wakeup end!");
	uint32_t r_queue;
	xQueueReceive(j_que_wakeup, &r_queue,portMAX_DELAY);
	if(ESP_OK == joshvm_rec_engine_destroy()){
		j_wakeup_status = REC_WAKEUP_OFF;
		vQueueDelete(j_que_wakeup);
		audio_free(j_rec_engine_cfg);
		return JOSHVM_OK;
	}
		
	return JOSHVM_FAIL;
}

int joshvm_esp32_vad_start(void(*callback)(int))
{
	ESP_LOGI(TAG, " joshvm_esp32_vad_start");

	int ret = 0;
	if(j_vad_status == REC_VAD_ON){
		ESP_LOGW(TAG,"rec_engine vad has started!");
		return	JOSHVM_INVALID_STATE;
	}

	if((j_wakeup_status == REC_WAKEUP_OFF) && (j_vad_status == REC_VAD_OFF)){	
		j_rec_engine_cfg = (j_rec_engine_create_t*)audio_malloc(sizeof(j_rec_engine_create_t));
		j_rec_engine_cfg->vad_off_ms = VAD_OFF_MS;
		ret = joshvm_rec_engine_create(j_rec_engine_cfg);
	}	
	j_rec_engine_cfg->vad_callback = callback;
	j_vad_status = REC_VAD_ON;
	
	//---create que for rec_engine_cb to notify vad_task
	j_que_vad = xQueueCreate(QUEUE_VAD_LEN, QUEUE_VAD_SIZE);
	if(NULL == j_que_vad){
		ESP_LOGE(TAG,"j_rec_eng_que_vad created failed");
	}	

	xTaskCreate(vad_task, "vad_task", 4096, NULL, VAD_TASK_PRI,NULL);
	vTaskDelay(200);
	j_rec_engine_cfg->sign = SIGN_VAD;
	rec_engine_trigger_start();
	
	if(ESP_OK == ret){
		return JOSHVM_OK;
	}
	return JOSHVM_FAIL;
}

int joshvm_esp32_vad_pause()
{
	rec_engine_detect_suspend(REC_VOICE_SUSPEND_ON);
	return 0;
}

int joshvm_esp32_vad_resume()
{
	rec_engine_detect_suspend(REC_VOICE_SUSPEND_OFF);
	return 0;
}

int joshvm_esp32_vad_stop()
{
	ESP_LOGI(TAG, " joshvm_esp32_vad_stop"); 

	if(j_vad_status == REC_VAD_OFF){
		ESP_LOGW(TAG,"there is no rec_engine vad running!");
		return JOSHVM_INVALID_STATE;
	}

	if((j_wakeup_status == REC_WAKEUP_ON) && (j_vad_status == REC_VAD_ON)){
		//rec_engine_vad_enable(false);
		rec_engine_trigger_stop();
		return JOSHVM_OK;
	}	
	j_vad_status = REC_VAD_OFF;
	//---delete que
	vQueueDelete(j_que_vad);
	
	joshvm_rec_engine_destroy();
	audio_free(j_rec_engine_cfg);
	return JOSHVM_OK;
}

int joshvm_esp32_vad_set_timeout(int ms)
{
	j_rec_engine_cfg->vad_off_ms = ms;
	return JOSHVM_OK;
}


//--test-------------------

void test_callback(int index)
{
	printf("wakeup   callback---------------- index = %d\n",index);
	
}

void test_vad_callback(int index)
{
	printf("vad   callback---------------- index = %d\n",index);
	
}

void test_rec_engine()
{
	vTaskDelay(200);
	joshvm_esp32_vad_start(test_vad_callback);
	//joshvm_esp32_wakeup_enable(test_callback);
	
	vTaskDelay(5000);
	
	//joshvm_esp32_wakeup_disable();
	//joshvm_esp32_vad_stop();
}

