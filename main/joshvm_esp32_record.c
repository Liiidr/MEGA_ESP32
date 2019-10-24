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
//#include "joshvm_esp32_raw_buff.h"

//test
#include "esp_sr_iface.h"
#include "esp_sr_models.h"
#include "esp_vad.h"


//---define
#define TAG  "JSOHVM_ESP32_RECORDER>>>"
//16000/1000*30ms
#define VOICEBUFF_SIZE 480


//---creater cfg
#define RECORD_RATE         48000
#define RECORD_CHANNEL      2
#define RECORD_BITS         16

#define SAVE_FILE_RATE      handle->joshvm_media_u.joshvm_media_mediarecorder.sample_rate	
#define SAVE_FILE_CHANNEL   handle->joshvm_media_u.joshvm_media_mediarecorder.channel
#define SAVE_FILE_BITS      handle->joshvm_media_u.joshvm_media_mediarecorder.bit_rate

#define PLAYBACK_RATE       48000
#define PLAYBACK_CHANNEL    2
#define PLAYBACK_BITS       16

//---element cfg
#define M_REC_CFG_FORMAT	handle->joshvm_media_u.joshvm_media_mediarecorder.format
#define M_REC_CFG_URL		handle->joshvm_media_u.joshvm_media_mediarecorder.url
#define M_REC_CFG_RATE 		handle->joshvm_media_u.joshvm_media_mediarecorder.sample_rate
#define M_REC_CFG_CHANNEL 	handle->joshvm_media_u.joshvm_media_mediarecorder.channel
#define M_REC_CFG_BITS		handle->joshvm_media_u.joshvm_media_mediarecorder.bit_rate

#define A_RECORD_RATE 		handle->joshvm_media_u.joshvm_media_audiorecorder.sample_rate
#define A_RECORD_CHA 		handle->joshvm_media_u.joshvm_media_audiorecorder.channel
#define A_RECORD_BITS		handle->joshvm_media_u.joshvm_media_audiorecorder.bit_rate

#define	A_TRACK_RATE		handle->joshvm_media_u.joshvm_media_audiotrack.sample_rate
#define	A_TRACK_CHA			handle->joshvm_media_u.joshvm_media_audiotrack.channel
#define A_TRACK_BITS		handle->joshvm_media_u.joshvm_media_audiotrack.bit_rate


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
	#ifdef CONFIG_ESP_LYRATD_MINI_V1_1_BOARD
		if(AUDIO_STREAM_READER == type)i2s_cfg.i2s_port = 1;		
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

static audio_element_handle_t create_amrnb_encoder(int sample_rate,int bits,int channels)
{
    amrnb_encoder_cfg_t amrnb_cfg = DEFAULT_AMRNB_ENCODER_CONFIG();
    audio_element_handle_t amrnb_en = amrnb_encoder_init(&amrnb_cfg);
	mem_assert(amrnb_en);
	audio_element_info_t amrnb_info = {0};
	audio_element_getinfo(amrnb_en, &amrnb_info);
	amrnb_info.sample_rates = sample_rate;
	amrnb_info.channels = channels;
	amrnb_info.bits = bits;
	audio_element_setinfo(amrnb_en, &amrnb_info);		
	printf("amrnb_info %d  %d  %d\r\n ",amrnb_info.sample_rates,amrnb_info.channels,amrnb_info.bits);	
	return amrnb_en;
}

static audio_element_handle_t create_amrwb_encoder(int sample_rate,int bits,int channels)
{
    amrwb_encoder_cfg_t amrwb_cfg = DEFAULT_AMRWB_ENCODER_CONFIG();
    audio_element_handle_t amrwb_en = amrwb_encoder_init(&amrwb_cfg);
	mem_assert(amrwb_en);
	audio_element_info_t amrwb_info = {0};
	audio_element_getinfo(amrwb_en, &amrwb_info);
	amrwb_info.sample_rates = sample_rate;
	amrwb_info.channels = channels;
	amrwb_info.bits = bits;
	audio_element_setinfo(amrwb_en, &amrwb_info);		
	printf("amrwb_info %d  %d  %d\r\n ",amrwb_info.sample_rates,amrwb_info.channels,amrwb_info.bits);	
	return amrwb_en;
}

static audio_element_handle_t create_opus_encoder(int sample_rate,int bits,int channels)
{
	opus_encoder_cfg_t opus_cfg = DEFAULT_OPUS_ENCODER_CONFIG();	
	audio_element_handle_t opus_en = encoder_opus_init(&opus_cfg);
	mem_assert(opus_en);
	audio_element_info_t opus_info = {0};
	audio_element_getinfo(opus_en, &opus_info);
	opus_info.sample_rates = sample_rate;
	opus_info.channels = channels;
	opus_info.bits = bits;
	audio_element_setinfo(opus_en, &opus_info);		
	printf("opus_info %d  %d  %d\r\n ",opus_info.sample_rates,opus_info.channels,opus_info.bits);	
	return opus_en;
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
		audio_element_handle_t amrnb_encoder = create_amrnb_encoder(SAVE_FILE_RATE,SAVE_FILE_BITS,SAVE_FILE_CHANNEL);		
		audio_pipeline_register(recorder, amrnb_encoder, "encode");
		handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.encoder = amrnb_encoder;
	}else if(joshvm_meida_format_amrwb == handle->joshvm_media_u.joshvm_media_mediarecorder.format){
		audio_element_handle_t amrwb_encoder = create_amrwb_encoder(SAVE_FILE_RATE,SAVE_FILE_BITS,SAVE_FILE_CHANNEL);		
		audio_pipeline_register(recorder, amrwb_encoder, "encode");
		handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.encoder = amrwb_encoder;
	}else if(joshvm_meida_format_opus == handle->joshvm_media_u.joshvm_media_mediarecorder.format){
		audio_element_handle_t opus_encoder = create_opus_encoder(SAVE_FILE_RATE,SAVE_FILE_BITS,SAVE_FILE_CHANNEL);
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
	rsp_filter_set_dest_info(handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.filter,M_REC_CFG_RATE,M_REC_CFG_CHANNEL);
	
	//---encoder cfg 
	audio_element_info_t encoder_info = {0};
	audio_element_getinfo(handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.encoder, &encoder_info);
	encoder_info.sample_rates = M_REC_CFG_RATE;
	encoder_info.channels = M_REC_CFG_CHANNEL;
	encoder_info.bits = M_REC_CFG_BITS;
	audio_element_setinfo(handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.encoder, &encoder_info);		
	ESP_LOGI(TAG,"Prepare encoder_info %d  %d  %d",encoder_info.sample_rates,encoder_info.channels,encoder_info.bits);

	//---fatfs cfg
	audio_element_info_t writer_info = {0};
    audio_element_getinfo(handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.stream_writer, &writer_info);
    writer_info.bits = M_REC_CFG_BITS;
    writer_info.channels = M_REC_CFG_CHANNEL;
    writer_info.sample_rates = M_REC_CFG_RATE;
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
			ret = audio_element_get_state(handle->joshvm_media_u.joshvm_media_audiorecorder.audiorecorder_t.i2s);
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

	audio_element_handle_t raw_writer = NULL;
    audio_pipeline_handle_t audio_track = NULL;
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    audio_track = audio_pipeline_init(&pipeline_cfg);
    if (NULL == audio_track) {
        return JOSHVM_FAIL;
    }	

	//---create i2s element
	audio_element_handle_t i2s_stream_writer = create_i2s_stream(PLAYBACK_RATE,PLAYBACK_BITS,PLAYBACK_CHANNEL,AUDIO_STREAM_WRITER);
	//---create resample element
	audio_element_handle_t filter_sample_el = create_filter(A_TRACK_RATE,A_TRACK_CHA, PLAYBACK_RATE, PLAYBACK_CHANNEL, RESAMPLE_DECODE_MODE);
	//---create raw element
	raw_stream_cfg_t raw_cfg = RAW_STREAM_CFG_DEFAULT();
    raw_cfg.type = AUDIO_STREAM_WRITER;
    raw_writer = raw_stream_init(&raw_cfg);

    audio_pipeline_register(audio_track, i2s_stream_writer, "i2s");
	audio_pipeline_register(audio_track, filter_sample_el, "upsample");
    audio_pipeline_register(audio_track, raw_writer, "raw");
    audio_pipeline_link(audio_track, (const char *[]) {"raw","upsample", "i2s"}, 3);
	
    ESP_LOGI(TAG, "track has been created");
	handle->joshvm_media_u.joshvm_media_audiotrack.audiotrack_t.i2s = i2s_stream_writer;	
	handle->joshvm_media_u.joshvm_media_audiotrack.audiotrack_t.filter = filter_sample_el;	
	handle->joshvm_media_u.joshvm_media_audiotrack.audiotrack_t.raw_writer = raw_writer;
	handle->joshvm_media_u.joshvm_media_audiotrack.audiotrack_t.pipeline = audio_track;

    return JOSHVM_OK;
}

void joshvm_audio_track_task(void* handle)
{
	QueueHandle_t que =((joshvm_media_t*)handle)->evt_que;
	uint16_t que_val = 0;
	int16_t *voicebuff = (int16_t *)audio_malloc(VOICEBUFF_SIZE * sizeof(short));
	audio_element_handle_t raw_writer = ((joshvm_media_t*)handle)->joshvm_media_u.joshvm_media_audiotrack.audiotrack_t.raw_writer;
	ring_buffer_t* audio_track_rb = ((joshvm_media_t*)handle)->joshvm_media_u.joshvm_media_audiotrack.track_rb;
	while(1){
		xQueueReceive(que, &que_val, portMAX_DELAY);
		if(que_val == QUE_TRACK_START){
			while(ring_buffer_read(voicebuff,VOICEBUFF_SIZE * sizeof(short),audio_track_rb)){
				raw_stream_write(raw_writer,(char*)voicebuff,VOICEBUFF_SIZE * sizeof(short));
			}
			((joshvm_media_t*)handle)->joshvm_media_u.joshvm_media_audiotrack.callback(handle,0);		
			//printf("track valid_size = %d\n",audio_track_rb->valid_size);
		}
		//vTaskDelay(1000);
	}		
	audio_free(voicebuff);
}

int joshvm_audio_track_write(joshvm_media_t* handle, unsigned char* buffer, int size, int* bytesWritten)
{
	*bytesWritten = ring_buffer_write((int8_t*)buffer,size,handle->joshvm_media_u.joshvm_media_audiotrack.track_rb);
	if(*bytesWritten >=0){
		return JOSHVM_OK;
	}else{
		return JOSHVM_FAIL;
	}
}

int joshvm_audio_recorder_init(joshvm_media_t* handle)
{	
    ESP_LOGI(TAG, "joshvm_audio_recorder_init");

	audio_element_handle_t raw_reader = NULL;
    audio_pipeline_handle_t audio_recorder = NULL;
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    audio_recorder = audio_pipeline_init(&pipeline_cfg);
    if (NULL == audio_recorder) {
        return JOSHVM_FAIL;
    }	
	//---create i2s element
	audio_element_handle_t i2s_stream_reader = create_i2s_stream(RECORD_RATE,RECORD_BITS,RECORD_CHANNEL,AUDIO_STREAM_READER);
	//---create resample_filter
	audio_element_handle_t filter = create_filter(RECORD_RATE,RECORD_CHANNEL,A_RECORD_RATE,A_RECORD_CHA,AUDIO_CODEC_TYPE_ENCODER);
	//---create raw element
	raw_stream_cfg_t raw_cfg = RAW_STREAM_CFG_DEFAULT();
    raw_cfg.type = AUDIO_STREAM_READER;
    raw_reader = raw_stream_init(&raw_cfg);

    audio_pipeline_register(audio_recorder, i2s_stream_reader, "i2s");
	audio_pipeline_register(audio_recorder, filter, "filter");
    audio_pipeline_register(audio_recorder, raw_reader, "raw");
    audio_pipeline_link(audio_recorder, (const char *[]) {"i2s", "filter","raw"}, 3);
	
    ESP_LOGI(TAG, "Recorder has been created");
	handle->joshvm_media_u.joshvm_media_audiorecorder.audiorecorder_t.i2s = i2s_stream_reader;	
	handle->joshvm_media_u.joshvm_media_audiorecorder.audiorecorder_t.filter = filter;	
	handle->joshvm_media_u.joshvm_media_audiorecorder.audiorecorder_t.raw_reader = raw_reader;
	handle->joshvm_media_u.joshvm_media_audiorecorder.audiorecorder_t.pipeline = audio_recorder;	

    return JOSHVM_OK;
}

void joshvm_audio_recorder_task(void* handle)
{
	QueueHandle_t que = ((joshvm_media_t*)handle)->evt_que;
	uint16_t que_val = 0;			
	ring_buffer_t* audio_recorder_rb = ((joshvm_media_t*)handle)->joshvm_media_u.joshvm_media_audiorecorder.rec_rb;
	int16_t *voicebuff = (int16_t *)audio_malloc(VOICEBUFF_SIZE * sizeof(short));
	audio_element_handle_t raw_rec = ((joshvm_media_t*)handle)->joshvm_media_u.joshvm_media_audiorecorder.audiorecorder_t.raw_reader;    

	//ring_buffer_init(&audio_recorder_rb,A_RECORD_RB_SIZE);
	while(1){	
		raw_stream_read(raw_rec,(char*)voicebuff,VOICEBUFF_SIZE * sizeof(short));
		ring_buffer_write(voicebuff,VOICEBUFF_SIZE * sizeof(short),audio_recorder_rb);
		xQueueSend(que, &que_val, (portTickType)0);
		if(que_val == QUE_RECORD_STOP){
			break;
		}
			
		//printf("recorder valid_size = %d\n",audio_recorder_rb.valid_size);
	}
	audio_free(voicebuff);
	//ring_buffer_deinit(&audio_recorder_rb);	
	vTaskDelete(NULL);
}

int joshvm_audio_recorder_read(joshvm_media_t* handle,unsigned char* buffer, int size, int* bytesRead)
{
	*bytesRead = ring_buffer_read(buffer,size,handle->joshvm_media_u.joshvm_media_audiorecorder.rec_rb);
	if(*bytesRead >= 0){
		return JOSHVM_OK;
	}else{
		return JOSHVM_FAIL;
	}
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
    audio_pipeline_unregister(handle->joshvm_media_u.joshvm_media_mediarecorder.recorder_t.pipeline,\
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
		                      handle->joshvm_media_u.joshvm_media_audiotrack.audiotrack_t.filter);
    audio_pipeline_unregister(handle->joshvm_media_u.joshvm_media_audiotrack.audiotrack_t.pipeline,\
		                      handle->joshvm_media_u.joshvm_media_audiotrack.audiotrack_t.i2s);
    audio_pipeline_deinit(handle->joshvm_media_u.joshvm_media_audiotrack.audiotrack_t.pipeline);
    audio_element_deinit(handle->joshvm_media_u.joshvm_media_audiotrack.audiotrack_t.raw_writer);	
    audio_element_deinit(handle->joshvm_media_u.joshvm_media_audiotrack.audiotrack_t.filter);
    audio_element_deinit(handle->joshvm_media_u.joshvm_media_audiotrack.audiotrack_t.i2s);
}

void joshvm_audio_rcorder_release(joshvm_media_t* handle)
{
	audio_pipeline_terminate(handle->joshvm_media_u.joshvm_media_audiorecorder.audiorecorder_t.pipeline);	
    audio_pipeline_unregister(handle->joshvm_media_u.joshvm_media_audiorecorder.audiorecorder_t.pipeline, \
		                      handle->joshvm_media_u.joshvm_media_audiorecorder.audiorecorder_t.raw_reader);	
	audio_pipeline_unregister(handle->joshvm_media_u.joshvm_media_audiorecorder.audiorecorder_t.pipeline,\
							  handle->joshvm_media_u.joshvm_media_audiorecorder.audiorecorder_t.filter);
    audio_pipeline_unregister(handle->joshvm_media_u.joshvm_media_audiorecorder.audiorecorder_t.pipeline,\
		                      handle->joshvm_media_u.joshvm_media_audiorecorder.audiorecorder_t.i2s);
    audio_pipeline_deinit(handle->joshvm_media_u.joshvm_media_audiorecorder.audiorecorder_t.pipeline);
    audio_element_deinit(handle->joshvm_media_u.joshvm_media_audiorecorder.audiorecorder_t.raw_reader);	
    audio_element_deinit(handle->joshvm_media_u.joshvm_media_audiorecorder.audiorecorder_t.filter);
    audio_element_deinit(handle->joshvm_media_u.joshvm_media_audiorecorder.audiorecorder_t.i2s);	
}


