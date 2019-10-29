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


//---variable
static const char *TAG = "JOSHVM_REC_ENGINE>>>>>>>";

extern joshvm_media_t *joshvm_media;

static  audio_element_handle_t raw_read;
#ifdef CONFIG_ESP_LYRAT_MINI_V1_1_BOARD
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



esp_err_t joshvm_rec_engine_create(void *handle)
{		
	ESP_LOGI(TAG, " joshvm_rec_engine_create");

	rec_config_t eng = DEFAULT_REC_ENGINE_CONFIG();
	eng.vad_off_delay_ms =
	eng.wakeup_time_ms = 10*1000;
	eng.evt_cb = rec_engine_cb;
#ifdef CONFIG_ESP_LYRAT_MINI_V1_1_BOARD
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
	return JOSHVM_FAIL;

}

int joshvm_esp32_wakeup_disable()
{

		
	return JOSHVM_FAIL;
}

int joshvm_esp32_vad_start(void(*callback)(int))
{

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

	return JOSHVM_OK;
}

int joshvm_esp32_vad_set_timeout(int ms)
{
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

	vTaskDelay(5000);
	

}

