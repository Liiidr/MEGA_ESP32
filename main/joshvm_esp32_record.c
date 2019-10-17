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

//---define
#define TAG  "------JSOHVM_ESP32_RECORDER--------"
//---creater cfg
#define RECORD_RATE         16000
#define RECORD_CHANNEL      2
#define RECORD_BITS         16

#define SAVE_FILE_RATE      handle->joshvm_media_u.joshvm_media_mediarecorder.sample_rate	
#define SAVE_FILE_CHANNEL   handle->joshvm_media_u.joshvm_media_mediarecorder.channel
#define SAVE_FILE_BITS      handle->joshvm_media_u.joshvm_media_mediarecorder.bit_rate

#define PLAYBACK_RATE       48000
#define PLAYBACK_CHANNEL    1
#define PLAYBACK_BITS       16

//---element cfg
#define RECORDER_CFG_FORMAT	handle->joshvm_media_u.joshvm_media_mediarecorder.format
#define RECORDER_CFG_URL	handle->joshvm_media_u.joshvm_media_mediarecorder.url
#define RECORDER_CFG_SAMPLE_RATE handle->joshvm_media_u.joshvm_media_mediarecorder.sample_rate
#define RECORDER_CFG_CHANNEL handle->joshvm_media_u.joshvm_media_mediarecorder.channel
#define RECORDER_CFG_BITS	handle->joshvm_media_u.joshvm_media_mediarecorder.bit_rate

typedef struct rsp_filter {
    resample_info_t *resample_info;
    unsigned char *out_buf;
    unsigned char *in_buf;
    void *rsp_hd;
    int in_offset;
} rsp_filter_t;

static esp_err_t is_valid_rsp_filter_samplerate(int samplerate)
{
    if (samplerate < 8000
        || samplerate > 48000) {
        ESP_LOGE(TAG, "The sample rate should be within range [8000,48000], here is %d Hz", samplerate);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t is_valid_rsp_filter_channel(int channel)
{
    if (channel != 1 && channel != 2) {
        ESP_LOGE(TAG, "The number of channels should be either 1 or 2, here is %d", channel);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t rsp_filter_set_dest_info(audio_element_handle_t self, int dest_rate, int dest_ch)
{
    rsp_filter_t *filter = (rsp_filter_t *)audio_element_getdata(self);
    if (filter->resample_info->dest_rate == dest_rate
        && filter->resample_info->dest_ch == dest_ch) {
        return ESP_OK;
    }
    if (is_valid_rsp_filter_samplerate(dest_rate) != ESP_OK
        || is_valid_rsp_filter_channel(dest_ch) != ESP_OK) {
        return ESP_FAIL;
    } else {
        filter->resample_info->dest_rate = dest_rate;
        filter->resample_info->dest_ch = dest_ch;
        ESP_LOGI(TAG, "reset sample rate of destination data : %d, reset channel of destination data : %d",
                 filter->resample_info->dest_rate, filter->resample_info->dest_ch);
    }
    return ESP_OK;
}

static audio_element_handle_t create_i2s_stream(int sample_rates, int bits, int channels, audio_stream_type_t type)
{
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
	#ifdef CONFIG_ESP_LYRAT_MINI_V1_1_BOARD
		i2s_cfg.i2s_port = 1;
	#endif
    i2s_cfg.i2s_config.use_apll = 0;
    i2s_cfg.type = type;
	i2s_cfg.i2s_config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    audio_element_handle_t i2s_stream = i2s_stream_init(&i2s_cfg);
    mem_assert(i2s_stream);
    audio_element_info_t i2s_info = {0};
    audio_element_getinfo(i2s_stream, &i2s_info);
    i2s_info.bits = bits;
    i2s_info.channels = channels;
    i2s_info.sample_rates = sample_rates;
    audio_element_setinfo(i2s_stream, &i2s_info);
	printf("i2s_info %d  %d  %d\r\n ",i2s_info.sample_rates,i2s_info.channels,i2s_info.bits);
    return i2s_stream;
}

static audio_element_handle_t create_filter(int source_rate, int source_channel, int dest_rate, int dest_channel, audio_codec_type_t type)
{
    rsp_filter_cfg_t rsp_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
    rsp_cfg.src_rate = source_rate;
    rsp_cfg.src_ch = source_channel;
    rsp_cfg.dest_rate = dest_rate;
    rsp_cfg.dest_ch = dest_channel;
    rsp_cfg.type = type;
    return rsp_filter_init(&rsp_cfg);
}

static audio_element_handle_t create_wav_encoder(int sample_rate,int bits,int channels)
{
	wav_encoder_cfg_t wav_cfg = DEFAULT_WAV_ENCODER_CONFIG();	
	audio_element_handle_t wav_encoder = wav_encoder_init(&wav_cfg);
	mem_assert(wav_encoder);
	audio_element_info_t wav_info = {0};
	audio_element_getinfo(wav_encoder, &wav_info);
	wav_info.sample_rates = sample_rate;
	wav_info.channels = channels;
	wav_info.bits = bits;
	audio_element_setinfo(wav_encoder, &wav_info);		
	printf("wav_info %d  %d  %d\r\n ",wav_info.sample_rates,wav_info.channels,wav_info.bits);	
	return wav_encoder;
}

static audio_element_handle_t create_amrnb_encoder()
{
    amrnb_encoder_cfg_t amrnb_cfg = DEFAULT_AMRNB_ENCODER_CONFIG();
    return amrnb_encoder_init(&amrnb_cfg);
}

static audio_element_handle_t create_amrwb_encoder()
{
    amrwb_encoder_cfg_t amrwb_cfg = DEFAULT_AMRWB_ENCODER_CONFIG();
    return amrwb_encoder_init(&amrwb_cfg);
}

static audio_element_handle_t create_opus_encoder()
{
	opus_encoder_cfg_t opus_cfg = DEFAULT_OPUS_ENCODER_CONFIG();	
	return encoder_opus_init(&opus_cfg);
}

static audio_element_handle_t create_fatfs_stream(int sample_rates, int bits, int channels, audio_stream_type_t type)
{
    fatfs_stream_cfg_t fatfs_cfg = FATFS_STREAM_CFG_DEFAULT();
    fatfs_cfg.type = type;
    audio_element_handle_t fatfs_stream = fatfs_stream_init(&fatfs_cfg);
    mem_assert(fatfs_stream);
    audio_element_info_t writer_info = {0};
    audio_element_getinfo(fatfs_stream, &writer_info);
    writer_info.bits = bits;
    writer_info.channels = channels;
    writer_info.sample_rates = sample_rates;
    audio_element_setinfo(fatfs_stream, &writer_info);	
	printf("fatfs_info %d  %d  %d\r\n ",writer_info.sample_rates,writer_info.channels,writer_info.bits);
    return fatfs_stream;
}
 
int joshvm_meida_recorder_init(joshvm_media_t  * handle)
{	 
    ESP_LOGI(TAG, "joshvm_meida_recorder_init");

    audio_pipeline_handle_t recorder = NULL;
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    recorder = audio_pipeline_init(&pipeline_cfg);
    if (NULL == recorder) {
        return JOSHVM_FAIL;
    }	
	//---create i2s element
	audio_element_handle_t i2s_stream_reader = create_i2s_stream(RECORD_RATE,RECORD_BITS,RECORD_CHANNEL,AUDIO_STREAM_READER);
	//---create resample_filter
	audio_element_handle_t filter = create_filter(RECORD_RATE,RECORD_CHANNEL,SAVE_FILE_RATE,SAVE_FILE_CHANNEL,AUDIO_CODEC_TYPE_ENCODER);
	
	//---create encoder element
	if(joshvm_meida_format_wav == handle->joshvm_media_u.joshvm_media_mediarecorder.format){
		audio_element_handle_t wav_encoder = create_wav_encoder(SAVE_FILE_RATE,SAVE_FILE_BITS,SAVE_FILE_CHANNEL);
		audio_pipeline_register(recorder, wav_encoder, "encode");
		handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.encoder = wav_encoder;
	}else if(joshvm_meida_format_amrnb == handle->joshvm_media_u.joshvm_media_mediarecorder.format){		
		audio_element_handle_t amrnb_encoder = create_amrnb_encoder();		
		audio_pipeline_register(recorder, amrnb_encoder, "encode");
		handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.encoder = amrnb_encoder;
	}else if(joshvm_meida_format_amrwb == handle->joshvm_media_u.joshvm_media_mediarecorder.format){
		audio_element_handle_t amrwb_encoder = create_amrwb_encoder();		
		audio_pipeline_register(recorder, amrwb_encoder, "encode");
		handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.encoder = amrwb_encoder;
	}else if(joshvm_meida_format_opus == handle->joshvm_media_u.joshvm_media_mediarecorder.format){
		audio_element_handle_t opus_encoder = create_opus_encoder();
		audio_pipeline_register(recorder, opus_encoder, "encode");
		handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.encoder = opus_encoder; 
	}

	//---create fatfs element
	audio_element_handle_t fatfs_writer = create_fatfs_stream(SAVE_FILE_RATE,SAVE_FILE_BITS,SAVE_FILE_CHANNEL,AUDIO_STREAM_WRITER);	
	//---register
    audio_pipeline_register(recorder, i2s_stream_reader, "i2s");
	audio_pipeline_register(recorder, filter, "resample");
	audio_pipeline_register(recorder, fatfs_writer, "fatfs");
    audio_pipeline_link(recorder, (const char *[]) {"i2s","resample","encode","fatfs"}, 4);
	audio_element_set_uri(fatfs_writer,handle->joshvm_media_u.joshvm_media_mediarecorder.url);
	ESP_LOGI(TAG,"Set default url:%s",handle->joshvm_media_u.joshvm_media_mediarecorder.url);	
    ESP_LOGI(TAG, "Recorder has been created");
	handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.i2s = i2s_stream_reader;
	handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.filter = filter;
	handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.stream_writer = fatfs_writer;
	handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.pipeline = recorder;
    return JOSHVM_OK;
}

int joshvm_meida_recorder_cfg(joshvm_media_t *handle)
{
	ESP_LOGI(TAG, "joshvm_meida_recorder_cfg");
	//---filter_destination cfg
	rsp_filter_set_dest_info(handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.filter,RECORDER_CFG_SAMPLE_RATE,RECORDER_CFG_CHANNEL);
	
	//---encoder cfg 
	audio_element_info_t wav_info = {0};
	audio_element_getinfo(handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.encoder, &wav_info);
	wav_info.sample_rates = RECORDER_CFG_SAMPLE_RATE;
	wav_info.channels = RECORDER_CFG_CHANNEL;
	wav_info.bits = RECORDER_CFG_BITS;
	audio_element_setinfo(handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.encoder, &wav_info);		
	ESP_LOGI(TAG,"Prepare encoder_info %d  %d  %d",wav_info.sample_rates,wav_info.channels,wav_info.bits);

	//---fatfs cfg
	audio_element_info_t writer_info = {0};
    audio_element_getinfo(handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.stream_writer, &writer_info);
    writer_info.bits = RECORDER_CFG_BITS;
    writer_info.channels = RECORDER_CFG_CHANNEL;
    writer_info.sample_rates = RECORDER_CFG_SAMPLE_RATE;
    audio_element_setinfo(handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.stream_writer, &writer_info);	
	ESP_LOGI(TAG,"Prepare fatfs_info %d  %d  %d",writer_info.sample_rates,writer_info.channels,writer_info.bits);
	int ret = audio_element_set_uri(handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.stream_writer,\
									handle->joshvm_media_u.joshvm_media_mediarecorder.url);
	audio_pipeline_breakup_elements(handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.pipeline,\
									handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.stream_writer);
	audio_pipeline_relink(handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.pipeline, \
		(const char *[]) {"i2s","resample","encode","fatfs"}, 4);
	ESP_LOGI(TAG,"Prepare url:%s,ret=%d",handle->joshvm_media_u.joshvm_media_mediarecorder.url,ret);
	
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
    audio_pipeline_unregister(handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.pipeline,\
							  handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.stream_writer);
    audio_pipeline_unregister(handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.pipeline,\
							  handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.encoder);
    audio_pipeline_unregister(handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.pipeline,\
							  handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.filter);
    audio_pipeline_unregister(handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.pipeline, \
							  handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.i2s);
	audio_pipeline_deinit(handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.pipeline);
    audio_element_deinit(handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.stream_writer);
    audio_element_deinit(handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.encoder);
	audio_element_deinit(handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.filter);
    audio_element_deinit(handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.i2s);
}

void joshvm_audio_track_release(joshvm_media_t* handle)
{
	audio_pipeline_terminate(handle->joshvm_media_u.joshvm_media_audiotrack.audiotrack_t.pipeline);
    audio_pipeline_unregister(handle->joshvm_media_u.joshvm_media_audiotrack.audiotrack_t.pipeline,\
		   					  handle->joshvm_media_u.joshvm_media_audiotrack.audiotrack_t.raw_writer);
    audio_pipeline_unregister(handle->joshvm_media_u.joshvm_media_audiotrack.audiotrack_t.pipeline,\
		                      handle->joshvm_media_u.joshvm_media_audiotrack.audiotrack_t.i2s);
    audio_pipeline_deinit(handle->joshvm_media_u.joshvm_media_audiotrack.audiotrack_t.pipeline);
    audio_element_deinit(handle->joshvm_media_u.joshvm_media_audiotrack.audiotrack_t.raw_writer);
    audio_element_deinit(handle->joshvm_media_u.joshvm_media_audiotrack.audiotrack_t.i2s);
}

void joshvm_audio_rcorder_release(joshvm_media_t* handle)
{
	audio_pipeline_terminate(handle->joshvm_media_u.joshvm_media_audiorecorder.audiorecorder_t.pipeline);	
    audio_pipeline_unregister(handle->joshvm_media_u.joshvm_media_audiorecorder.audiorecorder_t.pipeline, \
		                      handle->joshvm_media_u.joshvm_media_audiorecorder.audiorecorder_t.raw_reader);
    audio_pipeline_unregister(handle->joshvm_media_u.joshvm_media_audiorecorder.audiorecorder_t.pipeline, \
		                      handle->joshvm_media_u.joshvm_media_audiorecorder.audiorecorder_t.i2s);
    audio_pipeline_deinit(handle->joshvm_media_u.joshvm_media_audiorecorder.audiorecorder_t.pipeline);
    audio_element_deinit(handle->joshvm_media_u.joshvm_media_audiorecorder.audiorecorder_t.raw_reader);
    audio_element_deinit(handle->joshvm_media_u.joshvm_media_audiorecorder.audiorecorder_t.i2s);
}


