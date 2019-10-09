#include "joshvm_esp32_record.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "i2s_stream.h"
#include "filter_resample.h"
#include "fatfs_stream.h"
#include "http_stream.h"
#include "wav_encoder.h"

//#include "joshvm_esp32_media.h"


#include "esp_log.h"


#define TAG  "------JSOHVM_ESP32_RECORDER--------"

int joshvm_meida_recorder_init(joshvm_media_t  * handle)
{
	
	audio_element_handle_t i2s_stream_reader;
    audio_pipeline_handle_t recorder;
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
	wav_encoder_cfg_t wav_cfg = DEFAULT_WAV_ENCODER_CONFIG();	;
	audio_element_handle_t wav_encoder;
	switch(handle->joshvm_media_u.joshvm_media_mediarecorder.format){
		case joshvm_meida_format_wav:
			
			wav_encoder = wav_encoder_init(&wav_cfg);



			audio_pipeline_register(recorder, wav_encoder, "wav");


	
			break;


		default:
			break;
	}
	//---create fatfs element
	fatfs_stream_cfg_t fatfs_cfg = FATFS_STREAM_CFG_DEFAULT();
	fatfs_cfg.type = AUDIO_STREAM_WRITER;
	audio_element_handle_t fatfs_writer = fatfs_stream_init(&fatfs_cfg);

    audio_pipeline_register(recorder, i2s_stream_reader, "i2s");
	audio_pipeline_register(recorder, fatfs_writer, "fatfs");
	


    audio_pipeline_link(recorder, (const char *[]) {"i2s", "wav","fatfs"}, 3);

	audio_element_set_uri(fatfs_writer,handle->joshvm_media_u.joshvm_media_mediarecorder.url);
	
    //audio_pipeline_run(recorder);
    ESP_LOGI(TAG, "Recorder has been created");
    handle->joshvm_media_u.joshvm_media_mediarecorder.recorder = recorder;
    return JOSHVM_OK;


}


