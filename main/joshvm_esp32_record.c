#include "joshvm_esp32_record.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_mem.h"
#include "i2s_stream.h"
#include "filter_resample.h"
#include "fatfs_stream.h"
#include "http_stream.h"
#include "raw_stream.h"
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
    i2s_cfg.i2s_config.sample_rate = 16000;//handle->joshvm_media_u.joshvm_media_mediarecorder.sample_rate;
    i2s_cfg.i2s_config.channel_format =  handle->joshvm_media_u.joshvm_media_mediarecorder.channel;//I2S_CHANNEL_FMT_ONLY_LEFT;
    i2s_cfg.i2s_config.bits_per_sample = handle->joshvm_media_u.joshvm_media_mediarecorder.bit_rate;
    i2s_cfg.type = AUDIO_STREAM_READER;
    i2s_stream_reader = i2s_stream_init(&i2s_cfg);

	audio_element_info_t i2s_info = {0};
    audio_element_getinfo(i2s_stream_reader, &i2s_info);
	printf("i2s_info %d  %d  %d\r\n ",i2s_info.sample_rates,i2s_info.channels,i2s_info.bits);	

	
    rsp_filter_cfg_t rsp_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
    rsp_cfg.src_rate = 48000;
    rsp_cfg.src_ch = 2;
    rsp_cfg.dest_rate = 16000;//handle->joshvm_media_u.joshvm_media_mediarecorder.channel;
    rsp_cfg.dest_ch = 1;
    rsp_cfg.type = AUDIO_CODEC_TYPE_ENCODER;
    audio_element_handle_t filter = rsp_filter_init(&rsp_cfg);
	
	//---create encoder element
	if(joshvm_meida_format_wav == handle->joshvm_media_u.joshvm_media_mediarecorder.format){
		wav_encoder_cfg_t wav_cfg = DEFAULT_WAV_ENCODER_CONFIG();	
		audio_element_handle_t wav_encoder = wav_encoder_init(&wav_cfg);
		audio_element_info_t wav_info = {0};
    	audio_element_getinfo(wav_encoder, &wav_info);
		wav_info.sample_rates = 16000;
		wav_info.channels = 1;
		wav_info.bits = 16;
		audio_element_setinfo(wav_encoder, &wav_info);	
		
		printf("wav_info %d  %d  %d\r\n ",wav_info.sample_rates,wav_info.channels,wav_info.bits);	
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
	
	audio_element_info_t fatfs_info = {0};
    audio_element_getinfo(fatfs_writer, &fatfs_info);
    fatfs_info.bits = handle->joshvm_media_u.joshvm_media_mediarecorder.bit_rate;
    if(I2S_CHANNEL_FMT_ONLY_LEFT==handle->joshvm_media_u.joshvm_media_mediarecorder.channel){
		fatfs_info.channels = 1;
	}else{fatfs_info.channels = 2;}
    fatfs_info.sample_rates = handle->joshvm_media_u.joshvm_media_mediarecorder.sample_rate;
	printf("fatfs_info %d  %d  %d\r\n ",fatfs_info.sample_rates,fatfs_info.channels,fatfs_info.bits);
    audio_element_setinfo(fatfs_writer, &fatfs_info);
	
    audio_pipeline_register(recorder, i2s_stream_reader, "i2s");
	audio_pipeline_register(recorder, filter, "resample");
	audio_pipeline_register(recorder, fatfs_writer, "fatfs");
    audio_pipeline_link(recorder, (const char *[]) {"i2s","encode","fatfs"}, 3);
	ret = audio_element_set_uri(fatfs_writer,handle->joshvm_media_u.joshvm_media_mediarecorder.url);
	printf("set url:%s,ret=%d\r\n",handle->joshvm_media_u.joshvm_media_mediarecorder.url,ret);
	
    ESP_LOGI(TAG, "Recorder has been created");
	handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.i2s = i2s_stream_reader;
	handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.filter = filter;
	handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.stream_writer = fatfs_writer;
	handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.pipeline = recorder;
    return JOSHVM_OK;
}

int joshvm_media_get_state(joshvm_media_t* handle,int* state)
{
	ESP_LOGI(TAG, "joshvm_media_recorde_get_state");

	int ret;

	switch(handle->media_type)
	{
		case MEDIA_RECORDER:
			ret = audio_element_get_state(handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.i2s);
			break;
		case AUDIO_TRACK:
			ret = audio_element_get_state(handle->joshvm_media_u.joshvm_media_audiotrack.audiotrack_t.i2s);
			break;
		case AUDIO_RECORDER:

			ret = JOSHVM_OK;
			break;
		default :
			ret = JOSHVM_NOT_SUPPORTED;
			break;
	}
	
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

int joshvm_audio_track_init(joshvm_media_t* handle)
{	
    ESP_LOGI(TAG, "joshvm_audio_track_init");
/*
	int ret;
	audio_element_handle_t i2s_stream_writer = NULL;
	audio_element_handle_t raw_writer = NULL;
    audio_pipeline_handle_t audiotrack = NULL;
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    audiotrack = audio_pipeline_init(&pipeline_cfg);
    if (NULL == audiotrack) {
        return JOSHVM_FAIL;
    }	
	//---create i2s element
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.i2s_port = 1;
    i2s_cfg.i2s_config.use_apll = 0;
    i2s_cfg.i2s_config.sample_rate = handle->joshvm_media_u.joshvm_media_audiotrack.sample_rate;
    i2s_cfg.i2s_config.channel_format =  handle->joshvm_media_u.joshvm_media_audiotrack.channel;//I2S_CHANNEL_FMT_ONLY_LEFT;
    i2s_cfg.i2s_config.bits_per_sample = handle->joshvm_media_u.joshvm_media_audiotrack.bit_rate;
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    i2s_stream_writer = i2s_stream_init(&i2s_cfg);

	raw_stream_cfg_t raw_cfg = RAW_STREAM_CFG_DEFAULT();
    raw_cfg.type = AUDIO_STREAM_WRITER;
    raw_writer = raw_stream_init(&raw_cfg);

    audio_pipeline_register(audiotrack, i2s_stream_writer, "i2s");
    audio_pipeline_register(audiotrack, raw_writer, "raw");
    audio_pipeline_link(audiotrack, (const char *[]) {"raw", "i2s"}, 2);
	
    ESP_LOGI(TAG, "track has been created");
	handle->joshvm_media_u.joshvm_media_audiotrack.audiotrack_t.i2s = i2s_stream_writer;	
	handle->joshvm_media_u.joshvm_media_audiotrack.audiotrack_t.raw_writer = raw_writer;
	handle->joshvm_media_u.joshvm_media_audiotrack.audiotrack_t.pipeline = audiotrack;
*/
    return JOSHVM_OK;
}

int joshvm_audio_recorder_init(joshvm_media_t* handle)
{	
    ESP_LOGI(TAG, "joshvm_audio_recorder_init");
/*
	int ret;
	audio_element_handle_t i2s_stream_reader = NULL;
	audio_element_handle_t raw_reader = NULL;
    audio_pipeline_handle_t audiorecorder = NULL;
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    audiorecorder = audio_pipeline_init(&pipeline_cfg);
    if (NULL == audiorecorder) {
        return JOSHVM_FAIL;
    }	
	//---create i2s element
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.i2s_port = 1;
    i2s_cfg.i2s_config.use_apll = 0;
    i2s_cfg.i2s_config.sample_rate = handle->joshvm_media_u.joshvm_media_audiorecorder.sample_rate;
    i2s_cfg.i2s_config.channel_format =  handle->joshvm_media_u.joshvm_media_audiorecorder.channel;//I2S_CHANNEL_FMT_ONLY_LEFT;
    i2s_cfg.i2s_config.bits_per_sample = handle->joshvm_media_u.joshvm_media_audiorecorder.bit_rate;
    i2s_cfg.type = AUDIO_STREAM_READER;
    i2s_stream_reader = i2s_stream_init(&i2s_cfg);

	raw_stream_cfg_t raw_cfg = RAW_STREAM_CFG_DEFAULT();
    raw_cfg.type = AUDIO_STREAM_READER;
    raw_reader = raw_stream_init(&raw_cfg);

    audio_pipeline_register(audiorecorder, i2s_stream_reader, "i2s");
    audio_pipeline_register(audiorecorder, raw_reader, "raw");
    audio_pipeline_link(audiorecorder, (const char *[]) {"i2s", "raw"}, 2);
	
    ESP_LOGI(TAG, "Recorder has been created");
	handle->joshvm_media_u.joshvm_media_audiorecorder.audiorecorder_t.i2s = i2s_stream_reader;	
	handle->joshvm_media_u.joshvm_media_audiorecorder.audiorecorder_t.raw_reader = raw_reader;
	handle->joshvm_media_u.joshvm_media_audiorecorder.audiorecorder_t.pipeline = audiorecorder;
	*/

    return JOSHVM_OK;
}


void joshvm_media_recorder_release(joshvm_media_t* handle)
{
	audio_pipeline_terminate(handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.pipeline);
    audio_pipeline_unregister(handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.pipeline, handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.stream_writer);
    audio_pipeline_unregister(handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.pipeline, handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.i2s);
    audio_pipeline_unregister(handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.pipeline, handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.encoder);
    audio_pipeline_unregister(handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.pipeline, handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.filter);
	audio_pipeline_deinit(handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.pipeline);
    audio_element_deinit(handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.stream_writer);
    audio_element_deinit(handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.i2s);
	audio_element_deinit(handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.filter);
    audio_element_deinit(handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.encoder);
}

void joshvm_audio_track_release(joshvm_media_t* handle)
{
	audio_pipeline_terminate(handle->joshvm_media_u.joshvm_media_audiotrack.audiotrack_t.pipeline);
    audio_pipeline_unregister(handle->joshvm_media_u.joshvm_media_audiotrack.audiotrack_t.pipeline, handle->joshvm_media_u.joshvm_media_audiotrack.audiotrack_t.raw_writer);
    audio_pipeline_unregister(handle->joshvm_media_u.joshvm_media_audiotrack.audiotrack_t.pipeline, handle->joshvm_media_u.joshvm_media_audiotrack.audiotrack_t.i2s);
    audio_pipeline_deinit(handle->joshvm_media_u.joshvm_media_audiotrack.audiotrack_t.pipeline);
    audio_element_deinit(handle->joshvm_media_u.joshvm_media_audiotrack.audiotrack_t.raw_writer);
    audio_element_deinit(handle->joshvm_media_u.joshvm_media_audiotrack.audiotrack_t.i2s);
}

void joshvm_audio_rcorder_release(joshvm_media_t* handle)
{
	audio_pipeline_terminate(handle->joshvm_media_u.joshvm_media_audiorecorder.audiorecorder_t.pipeline);	
    audio_pipeline_unregister(handle->joshvm_media_u.joshvm_media_audiorecorder.audiorecorder_t.pipeline, handle->joshvm_media_u.joshvm_media_audiorecorder.audiorecorder_t.raw_reader);
    audio_pipeline_unregister(handle->joshvm_media_u.joshvm_media_audiorecorder.audiorecorder_t.pipeline, handle->joshvm_media_u.joshvm_media_audiorecorder.audiorecorder_t.i2s);
    audio_pipeline_deinit(handle->joshvm_media_u.joshvm_media_audiorecorder.audiorecorder_t.pipeline);
    audio_element_deinit(handle->joshvm_media_u.joshvm_media_audiorecorder.audiorecorder_t.raw_reader);
    audio_element_deinit(handle->joshvm_media_u.joshvm_media_audiorecorder.audiorecorder_t.i2s);
}


