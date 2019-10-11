#include "joshvm_esp32_record.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_mem.h"
#include "i2s_stream.h"
#include "filter_resample.h"
#include "fatfs_stream.h"
#include "http_stream.h"
#include "wav_encoder.h"
#include "amrnb_encoder.h"
#include "amrwb_encoder.h"
#include "opus_encoder.h"
#include "esp_log.h"


#define TAG  "------JSOHVM_ESP32_RECORDER--------"

int joshvm_meida_recorder_init(joshvm_media_t  * handle)
{	
    ESP_LOGI(TAG, "joshvm_meida_recorder_init");

	int ret;
	audio_element_handle_t i2s_stream_reader = NULL;
    audio_pipeline_handle_t recorder = NULL;
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    recorder = audio_pipeline_init(&pipeline_cfg);
    if (NULL == recorder) {
        return JOSHVM_FAIL;
    }
	
	//---create i2s element
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.i2s_port = 1;
    i2s_cfg.i2s_config.use_apll = 0;
    i2s_cfg.i2s_config.sample_rate = handle->joshvm_media_u.joshvm_media_mediarecorder.sample_rate;
    i2s_cfg.i2s_config.channel_format =  handle->joshvm_media_u.joshvm_media_mediarecorder.channel;//I2S_CHANNEL_FMT_ONLY_LEFT;
    i2s_cfg.i2s_config.bits_per_sample = handle->joshvm_media_u.joshvm_media_mediarecorder.bit_rate;
    i2s_cfg.type = AUDIO_STREAM_READER;
    i2s_stream_reader = i2s_stream_init(&i2s_cfg);

	//---create encoder element
	if(joshvm_meida_format_wav == handle->joshvm_media_u.joshvm_media_mediarecorder.format){
		wav_encoder_cfg_t wav_cfg = DEFAULT_WAV_ENCODER_CONFIG();	
		audio_element_handle_t wav_encoder = wav_encoder_init(&wav_cfg);
		audio_pipeline_register(recorder, wav_encoder, "encode");
		handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.encoder = wav_encoder;
	}else if(joshvm_meida_format_amrnb == handle->joshvm_media_u.joshvm_media_mediarecorder.format){
		amrnb_encoder_cfg_t amrnb_cfg = DEFAULT_AMRNB_ENCODER_CONFIG();	
		audio_element_handle_t amrnb_encoder = amrnb_encoder_init(&amrnb_cfg);
		audio_pipeline_register(recorder, amrnb_encoder, "encode");
		handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.encoder = amrnb_encoder;
	}else if(joshvm_meida_format_amrwb == handle->joshvm_media_u.joshvm_media_mediarecorder.format){
		amrwb_encoder_cfg_t amrwb_cfg = DEFAULT_AMRWB_ENCODER_CONFIG();	
		audio_element_handle_t amrwb_encoder = amrwb_encoder_init(&amrwb_cfg);
		audio_pipeline_register(recorder, amrwb_encoder, "encode");
		handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.encoder = amrwb_encoder;
	}else if(joshvm_meida_format_opus == handle->joshvm_media_u.joshvm_media_mediarecorder.format){
		opus_encoder_cfg_t opus_cfg = DEFAULT_OPUS_ENCODER_CONFIG();	
		audio_element_handle_t opus_encoder = encoder_opus_init(&opus_cfg);
		audio_pipeline_register(recorder, opus_encoder, "encode");
		handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.encoder = opus_encoder;
	}

	//---create fatfs element
	fatfs_stream_cfg_t fatfs_cfg = FATFS_STREAM_CFG_DEFAULT();
	fatfs_cfg.type = AUDIO_STREAM_WRITER;
	audio_element_handle_t fatfs_writer = fatfs_stream_init(&fatfs_cfg);
    audio_pipeline_register(recorder, i2s_stream_reader, "i2s");
	audio_pipeline_register(recorder, fatfs_writer, "fatfs");
    audio_pipeline_link(recorder, (const char *[]) {"i2s", "encode","fatfs"}, 3);
	ret = audio_element_set_uri(fatfs_writer,handle->joshvm_media_u.joshvm_media_mediarecorder.url);
	printf("set url:%s,ret=%d\r\n",handle->joshvm_media_u.joshvm_media_mediarecorder.url,ret);
	
    ESP_LOGI(TAG, "Recorder has been created");
	handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.i2s = i2s_stream_reader;	
	handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.stream_writer = fatfs_writer;
	handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.pipeline = recorder;

	audio_element_info_t *info = audio_calloc(1,sizeof(audio_element_info_t));
	audio_element_getinfo(i2s_stream_reader,info);
	printf("sample_rate = %d,bits = %d,channel = %d\r\n",info->sample_rates,info->bits,info->channels);

	
    return JOSHVM_OK;
}

int joshvm_media_recorde_get_state(joshvm_media_t* handle,int* state)
{
	ESP_LOGI(TAG, "joshvm_media_recorde_get_state");

	int ret;
	ret = audio_element_get_state(handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.i2s);
	
	switch(ret){
		case AEL_STATE_FINISHED:
		case AEL_STATE_STOPPED:
			*state = JOSHVM_MEDIA_STOPPED;
			ret = JOSHVM_OK;
			break;
		case AEL_STATE_PAUSED:
			*state = JOSHVM_MEDIA_PAUSED;			
			ret = JOSHVM_OK;
			break;
		case AEL_STATE_RUNNING:
			*state = JOSHVM_MEDIA_RECORDING;
			ret = JOSHVM_OK;
			break;
		default:
			ret = JOSHVM_FAIL;
			break;		
	}

	return ret;
	
}


