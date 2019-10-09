/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2018 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include <freertos/semphr.h>

#include "recorder_engine.h"
#include "esp_audio.h"
#include "esp_log.h"

#include "board.h"
#include "audio_element.h"
#include "audio_mem.h"
#include "audio_common.h"
#include "audio_pipeline.h"
#include "fatfs_stream.h"
#include "spiffs_stream.h"
#include "raw_stream.h"
#include "i2s_stream.h"
#include "wav_decoder.h"
#include "amr_decoder.h"
#include "mp3_decoder.h"
#include "aac_decoder.h"
#include "http_stream.h"
#include "filter_resample.h"

#include "joshvm_audio_wrapper.h"
#include "joshvm_esp32_media.h"//test

esp_audio_handle_t player;
static const char *TAG = "AUDIO_WRAPPER";
/*//#define EXT_RAM_ATTR_ __attribute__((section(".ext_ram.bss")))
#define ESP_AUDIO_STATE_TASK_SIZE  2*1024
static StackType_t * esp_audio_state_task_stack = NULL;
static StaticTask_t	esp_audio_state_task_TCB;
static TaskHandle_t	esp_audio_state_task_handler;
#define JOSHVM_AUDIO_STATE_TASK_SIZE  2*1024
static StackType_t * joshvm_audio_state_task_stack = NULL;
static StaticTask_t joshvm_audio_state_task_TCB;
static TaskHandle_t	joshvm_audio_state_task_handler;*/
static audio_pipeline_handle_t pipeline;
static audio_element_handle_t i2s_stream,spiffs_stream,mp3_decoder,filter;
static int audio_pos = 0;



extern void javanotify_simplespeech_event(int, int);
static void joshvm_spiffs_audio_play_init(void);

static void esp_audio_state_task (void *para)
{
    QueueHandle_t que = (QueueHandle_t) para;
    esp_audio_state_t esp_state = {0};
    while (1) {
        xQueueReceive(que, &esp_state, portMAX_DELAY);
        ESP_LOGI(TAG, "esp_auido status:%x,err:%x", esp_state.status, esp_state.err_msg);
        if ((esp_state.status == AUDIO_STATUS_STOPED)
            || (esp_state.status == AUDIO_STATUS_FINISHED)
            || (esp_state.status == AUDIO_STATUS_ERROR)) {

			javanotify_simplespeech_event(2, 0);
			joshvm_esp32_media_callback();
        }
    }
    vTaskDelete(NULL);
}

int _http_stream_event_handle(http_stream_event_msg_t *msg)
{
    if (msg->event_id == HTTP_STREAM_RESOLVE_ALL_TRACKS) {
        return ESP_OK;
    }

    if (msg->event_id == HTTP_STREAM_FINISH_TRACK) {
        return http_stream_next_track(msg->el);
    }
    if (msg->event_id == HTTP_STREAM_FINISH_PLAYLIST) {
        return http_stream_restart(msg->el);
    }
    return ESP_OK;
}

static void setup_player(void)
{
    if (player) {
        return ;
    }

    esp_audio_cfg_t cfg = DEFAULT_ESP_AUDIO_CONFIG();
    audio_board_handle_t board_handle = audio_board_init();
    cfg.vol_handle = board_handle->audio_hal;
    cfg.vol_set = (audio_volume_set)audio_hal_set_volume;
    cfg.vol_get = (audio_volume_get)audio_hal_get_volume;
    cfg.resample_rate = 48000;
    cfg.prefer_type = ESP_AUDIO_PREFER_MEM;
    cfg.evt_que = xQueueCreate(3, sizeof(esp_audio_state_t));
    player = esp_audio_create(&cfg);
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);

	//extern xTaskHandle pvCreatedTask_player_task;
	//esp_audio_state_task_stack = (StackType_t *) audio_calloc(1,ESP_AUDIO_STATE_TASK_SIZE);
    xTaskCreate(esp_audio_state_task, "player_task", 2 * 1024, cfg.evt_que, 2, NULL);
	/*esp_audio_state_task_handler = xTaskCreateStaticPinnedToCore((TaskFunction_t) 		esp_audio_state_task,
													(const char *)  		"esp_audio_state_task", 
													(const uint32_t) 		ESP_AUDIO_STATE_TASK_SIZE, 
													(void *) 				cfg.evt_que, 
													(UBaseType_t) 			2, 
													(StackType_t *) 		esp_audio_state_task_stack, 
													(StaticTask_t *) 		&esp_audio_state_task_TCB,
													(BaseType_t) 			tskNO_AFFINITY);
	*/
    // Create readers and add to esp_audio
    fatfs_stream_cfg_t fs_reader = FATFS_STREAM_CFG_DEFAULT();
    fs_reader.type = AUDIO_STREAM_READER;

    esp_audio_input_stream_add(player, fatfs_stream_init(&fs_reader));
    http_stream_cfg_t http_cfg = HTTP_STREAM_CFG_DEFAULT();
    http_cfg.event_handle = _http_stream_event_handle;
    http_cfg.type = AUDIO_STREAM_READER;
    http_cfg.enable_playlist_parser = true;
    audio_element_handle_t http_stream_reader = http_stream_init(&http_cfg);
    esp_audio_input_stream_add(player, http_stream_reader);

    // Create writers and add to esp_audio
    i2s_stream_cfg_t i2s_writer = I2S_STREAM_CFG_DEFAULT();
    i2s_writer.i2s_config.sample_rate = 48000;
    i2s_writer.type = AUDIO_STREAM_WRITER;

    esp_audio_output_stream_add(player, i2s_stream_init(&i2s_writer));

    // Add decoders and encoders to esp_audio
    wav_decoder_cfg_t wav_dec_cfg = DEFAULT_WAV_DECODER_CONFIG();
	amr_decoder_cfg_t amr_dec_cfg = DEFAULT_AMR_DECODER_CONFIG();
    amr_dec_cfg.task_core = 1;
    mp3_decoder_cfg_t mp3_dec_cfg = DEFAULT_MP3_DECODER_CONFIG();
    mp3_dec_cfg.task_core = 1;
    aac_decoder_cfg_t aac_cfg = DEFAULT_AAC_DECODER_CONFIG();
    aac_cfg.task_core = 1;
    esp_audio_codec_lib_add(player, AUDIO_CODEC_TYPE_DECODER, aac_decoder_init(&aac_cfg));
    esp_audio_codec_lib_add(player, AUDIO_CODEC_TYPE_DECODER, wav_decoder_init(&wav_dec_cfg));
	esp_audio_codec_lib_add(player, AUDIO_CODEC_TYPE_DECODER, amr_decoder_init(&amr_dec_cfg));
    esp_audio_codec_lib_add(player, AUDIO_CODEC_TYPE_DECODER, mp3_decoder_init(&mp3_dec_cfg));

    audio_element_handle_t m4a_dec_cfg = aac_decoder_init(&aac_cfg);
    audio_element_set_tag(m4a_dec_cfg, "m4a");
    esp_audio_codec_lib_add(player, AUDIO_CODEC_TYPE_DECODER, m4a_dec_cfg);

    audio_element_handle_t ts_dec_cfg = aac_decoder_init(&aac_cfg);
    audio_element_set_tag(ts_dec_cfg, "ts");
    esp_audio_codec_lib_add(player, AUDIO_CODEC_TYPE_DECODER, ts_dec_cfg);

    // Set default volume
    esp_audio_vol_set(player, 50);
    AUDIO_MEM_SHOW(TAG);
    ESP_LOGI(TAG, "esp_audio instance is:%p", player);
}

void joshvm_audio_wrapper_init(void)
{
    setup_player();
	joshvm_spiffs_audio_play_init();
}

int joshvm_audio_play_handler(const char *url)
{
	int ret;
    ESP_LOGI(TAG, "Playing : %s", url);
    ret = esp_audio_play(player, AUDIO_CODEC_TYPE_DECODER, url, 0);
	return ret;
}

static void joshvm_audio_state_task (void *para)
{
	audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);

	audio_pipeline_set_listener(pipeline, evt);
	while(1)
	{
        audio_event_iface_msg_t msg;
        esp_err_t ret = audio_event_iface_listen(evt, &msg, portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[ * ] Event interface error : %d", ret);
            continue;
        }

        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *) i2s_stream
            && msg.cmd == AEL_MSG_CMD_REPORT_STATUS
            && (((int)msg.data == AEL_STATUS_STATE_STOPPED) || ((int)msg.data == AEL_STATUS_STATE_FINISHED))) {
            ESP_LOGI(TAG, "[ * ] bing end!");
			audio_pipeline_terminate(pipeline);	
			javanotify_simplespeech_event(2, 0);
            //break;
        }
	}
    audio_pipeline_terminate(pipeline);
    audio_pipeline_unregister(pipeline, spiffs_stream);
    audio_pipeline_unregister(pipeline, mp3_decoder);
    audio_pipeline_unregister(pipeline, filter); 
	audio_pipeline_unregister(pipeline, i2s_stream);
    audio_pipeline_remove_listener(pipeline);

    audio_event_iface_destroy(evt);
    //audio_pipeline_deinit(pipeline);
    //audio_element_deinit(spiffs_stream);
    //audio_element_deinit(aac_decoder);
    //audio_element_deinit(i2s_stream);
	//audio_element_deinit(filter);

    vTaskDelete(NULL);
}

static void joshvm_spiffs_audio_play_init(void)
{
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
	pipeline = audio_pipeline_init(&pipeline_cfg);

	spiffs_stream_cfg_t spiffs_cfg = SPIFFS_STREAM_CFG_DEFAULT();
    spiffs_cfg.type = AUDIO_STREAM_READER;
    spiffs_stream  = spiffs_stream_init(&spiffs_cfg);

    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    i2s_stream = i2s_stream_init(&i2s_cfg);

   	//wav_decoder_cfg_t wav_cfg = DEFAULT_WAV_DECODER_CONFIG();
    //audio_element_handle_t wav_decoder = wav_decoder_init(&wav_cfg); 

	mp3_decoder_cfg_t mp3_cfg = DEFAULT_MP3_DECODER_CONFIG();
	mp3_decoder = mp3_decoder_init(&mp3_cfg); 

    rsp_filter_cfg_t rsp_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
    rsp_cfg.src_rate = 16000;
    rsp_cfg.src_ch = 1;
    rsp_cfg.dest_rate = 48000;
    rsp_cfg.dest_ch = 2;
    rsp_cfg.type = AUDIO_CODEC_TYPE_DECODER;
    filter = rsp_filter_init(&rsp_cfg);

	audio_pipeline_register(pipeline, spiffs_stream,     "file_reader");
    audio_pipeline_register(pipeline, mp3_decoder,       "mp3_decoder");
    audio_pipeline_register(pipeline, filter,   "filter_upsample");
    audio_pipeline_register(pipeline, i2s_stream,        "i2s_writer");
	
	audio_pipeline_link(pipeline, (const char *[]) {"file_reader", "mp3_decoder", "filter_upsample", "i2s_writer"}, 4);

	//extern xTaskHandle pvCreatedTask_player_task_2;
	//joshvm_audio_state_task_stack = (StackType_t*)audio_calloc(1,JOSHVM_AUDIO_STATE_TASK_SIZE);
	xTaskCreate(joshvm_audio_state_task, "joshvm_audio_state_task", 2 * 1024, NULL, 1, NULL);
	/*joshvm_audio_state_task_handler = xTaskCreateStaticPinnedToCore((TaskFunction_t) 		joshvm_audio_state_task,
														(const char *) 	 		"joshvm_audio_state_task",
														(const uint32_t)		JOSHVM_AUDIO_STATE_TASK_SIZE,
														(void * const) 			NULL,
														(UBaseType_t) 			1, 
														(StackType_t *) 		joshvm_audio_state_task_stack,
														(StaticTask_t *) 		&joshvm_audio_state_task_TCB,
														(BaseType_t) 			tskNO_AFFINITY);*/
}

void joshvm_spiffs_audio_play_handler(const char *url)
{
	audio_element_set_uri(spiffs_stream, url);
    audio_pipeline_run(pipeline);
}

void joshvm_spiffs_audio_stop_handler(void)
{
	audio_pipeline_terminate(pipeline);
}

audio_err_t joshvm_audio_pause(void)
{
	int ret;
	esp_audio_pos_get(player, &audio_pos);
    if((ret = esp_audio_stop(player, TERMINATION_TYPE_NOW)) == ESP_OK){
		return ret;
	}
	return ret = ESP_FAIL;
}

void joshvm_audio_resume_handler(const char *url)
{
    //ESP_LOGI(TAG, "Resume audio, offset:%d url:%s", audio_info->offset, audio_info->url);
    //player_pause = 0;
    esp_audio_play(player, AUDIO_CODEC_TYPE_DECODER, url, audio_pos);
}

audio_err_t joshvm_audio_stop_handler(void)
{
    ESP_LOGI(TAG, "Stop audio play");
	int ret;
	ret =  esp_audio_stop(player, TERMINATION_TYPE_NOW);
	return ret;
}

int joshvm_audio_get_state()
{
    esp_audio_state_t st = {0};
    esp_audio_state_get(player, &st);
    return st.status;
}



audio_err_t joshvm_volume_get_handler(int volume)
{
	int ret;
	 if((ret = 	esp_audio_vol_get(player, &volume)) == ESP_OK){
		return ret = ESP_OK;
	 }
	 return ret = ESP_FAIL;
}

audio_err_t joshvm_volume_set_handler(int volume)
{
	int ret;
	 if((ret = 	esp_audio_vol_set(player, volume)) == ESP_OK){
		return ret = ESP_OK;
	 }
	 return ret = ESP_FAIL;
}


void joshvm_volume_adjust_handler(int volume)
{
    ESP_LOGI(TAG, "adj_volume by %d", volume);
    int vol = 0;
    esp_audio_vol_get(player, &vol);
    vol += volume;
    int ret = esp_audio_vol_set(player, vol);
    if (ret == 0) {
       	ESP_LOGI(TAG, "report volume_changed");
        //duer_dcs_on_volume_changed();
    }
}


/*

void duer_audio_wrapper_pause()
{
    if (duer_playing_type == DUER_AUDIO_TYPE_SPEECH) {
        esp_audio_stop(player, 0);
    } else {
        ESP_LOGW(TAG, "duer_audio_wrapper_pause, type is music");
    }
}

int duer_audio_wrapper_get_state()
{
    esp_audio_state_t st = {0};
    esp_audio_state_get(player, &st);
    return st.status;
}

void duer_dcs_listen_handler(void)
{
    ESP_LOGI(TAG, "enable_listen_handler, open mic");
    rec_engine_trigger_start();
}

void duer_dcs_stop_listen_handler(void)
{
    ESP_LOGI(TAG, "stop_listen, close mic");
    rec_engine_trigger_stop();
}

void duer_dcs_volume_set_handler(int volume)
{
    ESP_LOGI(TAG, "set_volume to %d", volume);
    int ret = esp_audio_vol_set(player, volume);
    if (ret == 0) {
        ESP_LOGI(TAG, "report on_volume_changed");
        duer_dcs_on_volume_changed();
    }
}

void duer_dcs_volume_adjust_handler(int volume)
{
    ESP_LOGI(TAG, "adj_volume by %d", volume);
    int vol = 0;
    esp_audio_vol_get(player, &vol);
    vol += volume;
    int ret = esp_audio_vol_set(player, vol);
    if (ret == 0) {
        ESP_LOGI(TAG, "report on_volume_changed");
        duer_dcs_on_volume_changed();
    }
}

void duer_dcs_mute_handler(duer_bool is_mute)
{
    ESP_LOGI(TAG, "set_mute to  %d", (int)is_mute);
    int ret = 0;
    if (is_mute) {
        ret = esp_audio_vol_set(player, 0);
    }
    if (ret == 0) {
        ESP_LOGI(TAG, "report on_mute");
        duer_dcs_on_mute();
    }
}

void duer_dcs_get_speaker_state(int *volume, duer_bool *is_mute)
{
    ESP_LOGI(TAG, "duer_dcs_get_speaker_state");
    *volume = 60;
    *is_mute = false;
    int ret = 0;
    int vol = 0;
    ret = esp_audio_vol_get(player, &vol);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to get volume");
    } else {
        *volume = vol;
        if (vol != 0) {
            *is_mute = false;
        } else {
            *is_mute = true;
        }
    }
}



int duer_dcs_speak_handler(const char *url)
{
	int ret;
    ESP_LOGI(TAG, "Playing speak: %s", url);
    ret = esp_audio_play(player, AUDIO_CODEC_TYPE_DECODER, url, 0);
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    duer_playing_type = DUER_AUDIO_TYPE_SPEECH;
    xSemaphoreGive(s_mutex);
	return ret;
	
}

void duer_dcs_audio_play_handler(const duer_dcs_audio_info_t *audio_info)
{
    ESP_LOGI(TAG, "Playing audio, offset:%d url:%s", audio_info->offset, audio_info->url);
    esp_audio_play(player, AUDIO_CODEC_TYPE_DECODER, audio_info->url, audio_info->offset);
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    duer_playing_type = DUER_AUDIO_TYPE_MUSIC;
    player_pause = 0;
    xSemaphoreGive(s_mutex);

}

void duer_dcs_audio_stop_handler()
{
    ESP_LOGI(TAG, "Stop audio play");
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    int status = duer_playing_type;
    xSemaphoreGive(s_mutex);
    if (status == 1) {
        ESP_LOGI(TAG, "Is playing speech, no need to stop");
    } else {
        esp_audio_stop(player, TERMINATION_TYPE_NOW);
    }
}

void duer_dcs_audio_pause_handler()
{
    ESP_LOGI(TAG, "DCS pause audio play");
    esp_audio_pos_get(player, &audio_pos);
    player_pause = 1;
    esp_audio_stop(player, 0);
}

void duer_dcs_audio_resume_handler(const duer_dcs_audio_info_t *audio_info)
{
    ESP_LOGI(TAG, "Resume audio, offset:%d url:%s", audio_info->offset, audio_info->url);
    player_pause = 0;
    esp_audio_play(player, AUDIO_CODEC_TYPE_DECODER, audio_info->url, audio_pos);
}

int duer_dcs_audio_get_play_progress()
{
    int ret = 0;
    uint32_t total_size = 0;
    int position = 0;
    ret = esp_audio_pos_get(player, &position);
    if (ret == 0) {
        ESP_LOGI(TAG, "Get play position %d of %d", position, total_size);
        return position;
    } else {
        ESP_LOGE(TAG, "Failed to get play progress.");
        return -1;
    }
}

duer_audio_type_t duer_dcs_get_player_type()
{
    return duer_playing_type;
}

int duer_dcs_set_player_type(duer_audio_type_t num)
{
    duer_playing_type = num;
    return 0;
}

void duer_dcs_audio_active_paused()
{
    if (duer_dcs_send_play_control_cmd(DCS_PAUSE_CMD)) {
        ESP_LOGE(TAG, "Send DCS_PAUSE_CMD to DCS failed");
    }
    ESP_LOGD(TAG, "duer_dcs_audio_active_paused");
}

void duer_dcs_audio_active_play()
{
    if (duer_dcs_send_play_control_cmd(DCS_PLAY_CMD)) {
        ESP_LOGE(TAG, "Send DCS_PLAY_CMD to DCS failed");
    }
    ESP_LOGD(TAG, "duer_dcs_audio_active_play");
}

void duer_dcs_audio_active_previous()
{
    if (duer_dcs_send_play_control_cmd(DCS_PREVIOUS_CMD)) {
        ESP_LOGE(TAG, "Send DCS_PREVIOUS_CMD to DCS failed");
    }
    ESP_LOGD(TAG, "Fduer_dcs_audio_active_previous");
}

void duer_dcs_audio_active_next()
{
    if (duer_dcs_send_play_control_cmd(DCS_NEXT_CMD)) {
        ESP_LOGE(TAG, "Send DCS_NEXT_CMD to DCS failed");
    }
    ESP_LOGD(TAG, "duer_dcs_audio_active_next");
}

*/
