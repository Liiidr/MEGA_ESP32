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
#include "joshvm.h"
#include "recorder_engine.h"
#include "esp_audio.h"
#include "esp_log.h"

#include "joshvm_audio_wrapper.h"
#include "dueros_service.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_mem.h"
#include "i2s_stream.h"
#include "raw_stream.h"
#include "filter_resample.h"
#include "fatfs_stream.h"
#include "spiffs_stream.h"
#include "http_stream.h"
#include "amr_decoder.h"
#include "amrwb_encoder.h"
#include "mp3_decoder.h"
#include "wav_decoder.h"
#include "aac_decoder.h"
#include "ringbuf.h"

#include "display_service.h"
#include "periph_adc_button.h"
#include "periph_spiffs.h"
#include "board.h"
#include "periph_button.h"
#include "esp_peripherals.h"
#include "app_wifi_service.h"

#include "joshvm_esp32_media.h"
#include "sd_card_init.h"

static const char *TAG              = "JOSHVM_Audio";
extern esp_audio_handle_t           player;
static display_service_handle_t disp_serv = NULL;
static joshvm_cyclebuf voicebuff;

static volatile int voicebuff_pos_case = 0;			//0: read_pos < write_pos;  1: read_pos > write_pos
static volatile int voicedata_read_flag = 0;
static QueueHandle_t Queue_vad_play = NULL;
#define QUEUE_VAD_PLAY_LEN	4
#define QUEUE_VAD_PLAY_SIZE	4
#define MP3_2_STREAM_URI "file://userdata/bingo_2.mp3"
#define MP3_1_STREAM_URI "file://userdata/bingo_1.mp3"
#define MP3_STREAM_URI "file://userdata/ding.mp3"
#define AMR_STREAM_MP3_SD_URI "file://sdcard/44100.mp3"
#define AMR_STREAM_WAV_SD_URI "file://sdcard/48000.wav"



extern void javanotify_simplespeech_event(int, int);
extern void JavaTask();
extern int esp32_record_voicefile(unsigned char* filename, int time);
extern int esp32_playback_voice(int index);
extern void esp32_device_control(int);
extern int esp32_read_voice_buffer(unsigned char*, int);
extern int esp32_playback_voice_url(const char *url);
extern void esp32_stop_playback(void);
extern void esp32_stop_record(void);
extern void JavaNativeTest();

//xTaskHandle pvCreatedTask_vadtask,pvCreatedTask_player_task,pvCreatedTask_player_task_2;  // check task stack


void rec_engine_cb(rec_event_type_t type, void *user_data)
{
	BaseType_t xReturn = pdFAIL;
	uint32_t senddata; 
    if (REC_EVENT_WAKEUP_START == type) {
        ESP_LOGI(TAG, "rec_engine_cb - REC_EVENT_WAKEUP_START");
			
		//javanotify_simplespeech_event(1, 0);
		senddata = QUEUE_WAKEUP_START;
		xReturn = xQueueSend(Queue_vad_play, &senddata, 0);
		if(pdPASS != xReturn)ESP_LOGE(TAG, "QUEUE_WAKEUP_START sended faild");

		ESP_LOGE(TAG,"REC_EVENT_WAKEUP_START heap free size = %d",heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));

		//esp32_playback_voice(4);
		test_esp32_media();

		//display_service_set_pattern(disp_serv, DISPLAY_PATTERN_TURN_ON, 0);

		//printf("vad task free stack :				%d\n",uxTaskGetStackHighWaterMark( pvCreatedTask_vadtask ));
		//printf("player task free stack :			%d\n",uxTaskGetStackHighWaterMark( pvCreatedTask_player_task ));
		//printf("player(native) task free stack :	%d\n",uxTaskGetStackHighWaterMark( pvCreatedTask_player_task_2 ));
		
    } else if (REC_EVENT_VAD_START == type) {
        ESP_LOGI(TAG, "rec_engine_cb - REC_EVENT_VAD_START");
		
		//javanotify_simplespeech_event(1, 2);
		voicedata_read_flag = 1;		
		senddata = QUEUE_VAD_START;
		xReturn = xQueueSend(Queue_vad_play, &senddata, 0);
		if(pdPASS != xReturn)ESP_LOGE(TAG, "QUEUE_VAD sended faild");
		
    } else if (REC_EVENT_VAD_STOP == type) {
        ESP_LOGI(TAG, "rec_engine_cb - REC_EVENT_VAD_STOP");
		//javanotify_simplespeech_event(1, 3);
		voicedata_read_flag = 0;
		senddata = QUEUE_VAD_STOP;
		xReturn = xQueueSend(Queue_vad_play, &senddata, 0);
		if(pdPASS != xReturn)ESP_LOGE(TAG, "QUEUE_VAD_STOP sended faild");		

    } else if (REC_EVENT_WAKEUP_END == type) {
        ESP_LOGI(TAG, "rec_engine_cb - REC_EVENT_WAKEUP_END");
		
		//javanotify_simplespeech_event(1, 1);
		senddata = QUEUE_WAKEUP_END;
		xReturn = xQueueSend(Queue_vad_play, &senddata, 0);
		if(pdPASS != xReturn)ESP_LOGE(TAG, "QUEUE_WAKEUP_END sended faild");		
		//splay_service_set_pattern(disp_serv, DISPLAY_PATTERN_TURN_OFF, 0);
    } else {
    }
}

static void joshvm_cyclebuf_init(joshvm_cyclebuf *buff )
{
	uint8_t* p = audio_calloc(1, JOSHVM_CYCLEBUF_BUFFER_SIZE);
	buff->buffer = p;
	buff->read_pointer = buff->buffer;
	buff->write_pointer = buff->buffer;
	buff->reset_pos = NULL;	
}

int esp32_read_voice_buffer(unsigned char* buffer,	int length)
{
	if(length < 0)return -1;
	int ret_length;//length of data can be read
	unsigned char* rp_backup = NULL;
	unsigned char* wp_backup = NULL;
	if(voicebuff.reset_pos != NULL){//------ new vad,need to reset pointer		
		voicebuff.read_pointer = voicebuff.reset_pos;
		voicebuff.reset_pos = NULL;
	}
	{//------vad_start already
		rp_backup = voicebuff.read_pointer;
		wp_backup = voicebuff.write_pointer;
		//printf("wp_backup - rp_backup = %d \n",(int)(wp_backup - rp_backup));
		//printf("wp_backup = %d,rp_backup = %d ---------in-----\n",wp_backup- voicebuff.buffer,voicebuff.read_pointer- voicebuff.buffer);
		if(0 == voicebuff_pos_case){//------voicebuff_pos_case == 0: read_pos < write_pos; 						
			if (rp_backup == wp_backup){
				//printf("-----1------\n");
				return 0;
			}			
			if((ret_length = wp_backup - rp_backup) < 0) return -1;
			ret_length = (ret_length > length) ? length : ret_length;

			if(ret_length >= length){//----enougth data can be read
				if(ret_length > 0){
					memcpy(buffer,rp_backup,ret_length);
				}else{	
					//printf("-----2------\n");
					return -1;
				}
				voicebuff.read_pointer += ret_length;
				if(voicebuff.read_pointer >= voicebuff.buffer + JOSHVM_CYCLEBUF_BUFFER_SIZE){
					voicebuff.read_pointer = voicebuff.buffer;
					voicebuff_pos_case = 0;
				}		
				//printf("wp_backup = %d,rp_backup = %d ---------out-1----\n",wp_backup- voicebuff.buffer,voicebuff.read_pointer- voicebuff.buffer);
				return ret_length;
			}else{//----read data  ret_length < length
				if(ret_length){
					memcpy(buffer,rp_backup,ret_length);
				}else{	
					//printf("-----3------\n");
					return -1;
				}
				voicebuff.read_pointer += ret_length;
				if(voicebuff.read_pointer >= voicebuff.buffer + JOSHVM_CYCLEBUF_BUFFER_SIZE){
					voicebuff.read_pointer = voicebuff.buffer;
					voicebuff_pos_case = 0;
				}
				//printf("wp_backup = %d,rp_backup = %d ---------out-2----\n",wp_backup- voicebuff.buffer,voicebuff.read_pointer- voicebuff.buffer);
				return ret_length;
			}
		}else{//------voicebuff_pos_case == 1: read_pos > write_pos	
			if (rp_backup == wp_backup){
				//printf("-----4------\n");
				return 0;
			}

			if(rp_backup + length < voicebuff.buffer + JOSHVM_CYCLEBUF_BUFFER_SIZE){//----read_pointer haven't exceeded the buffer largest pointer
				if(length > 0){
					memcpy(buffer,rp_backup,length);
				}else{	
					//printf("------5-----\n");
					return -1;
				}
				voicebuff.read_pointer += length;
				ret_length = length;
				//printf("wp_backup = %d,rp_backup = %d ---------out-3----\n",wp_backup- voicebuff.buffer,voicebuff.read_pointer- voicebuff.buffer);
				return ret_length;
			}else{
				int temp_ret1 = voicebuff.buffer + JOSHVM_CYCLEBUF_BUFFER_SIZE - rp_backup;
				int temp_ret2 = wp_backup - voicebuff.buffer;
				if((ret_length = temp_ret1 + temp_ret2) < 0) return -1;
				ret_length = (ret_length > length) ? length : ret_length;

				if(ret_length >= length){//----enougth data can be read
					if(temp_ret1 == length){
						memcpy(buffer,rp_backup,temp_ret1);
					}else{
						if(temp_ret1 > 0){
							memcpy(buffer,rp_backup,temp_ret1);
						}else{	
							//printf("-----6------\n");
							return -1;
						}
						if((length - temp_ret1) > 0){
							memcpy(buffer + temp_ret1,voicebuff.buffer,(length - temp_ret1));
						}else{
							//printf("------7-----\n");
							return -1;
						}
					}
					voicebuff.read_pointer = voicebuff.buffer + (length - temp_ret1); 
					if(voicebuff.read_pointer >= voicebuff.buffer + JOSHVM_CYCLEBUF_BUFFER_SIZE){
						voicebuff.read_pointer = voicebuff.buffer;
						voicebuff_pos_case = 0;
					}
					//printf("wp_backup = %d,rp_backup = %d ---------out-4----\n",wp_backup- voicebuff.buffer,voicebuff.read_pointer- voicebuff.buffer);
					return ret_length;
				}else{
					if(temp_ret1 > 0){
						memcpy(buffer,rp_backup,temp_ret1);
					}else{
						//printf("------8-----\n");
						return -1;
					}
					if(temp_ret2 > 0){
						memcpy(buffer + temp_ret1,voicebuff.buffer,temp_ret2);
					}else{
						//printf("------9-----\n");
						return -1;
					}
					voicebuff.read_pointer =  voicebuff.buffer + temp_ret2;
					if(voicebuff.read_pointer >= voicebuff.buffer + JOSHVM_CYCLEBUF_BUFFER_SIZE){
						voicebuff.read_pointer = voicebuff.buffer;
						voicebuff_pos_case = 0;
					}
					//printf("wp_backup = %d,rp_backup = %d ---------out-5----\n",wp_backup- voicebuff.buffer,voicebuff.read_pointer- voicebuff.buffer);
					return ret_length;					
				}
			}
		}
	}
}

static void vad_task(void * pram)
{
	uint32_t r_queue = 0;	
	uint8_t *voiceData = audio_calloc(1, REC_ONE_BLOCK_SIZE);
	if (NULL == voiceData) {
        ESP_LOGE(TAG, "Func:%s, Line:%d, Malloc failed", __func__, __LINE__);
		return;
    }
	
	while(1){
		xQueueReceive(Queue_vad_play, &r_queue, portMAX_DELAY);
		switch(r_queue){
			case QUEUE_WAKEUP_START:
				ESP_LOGI(TAG, "QUEUE_WAKEUP_START");	
				break;
			case QUEUE_VAD_START:
				ESP_LOGI(TAG, "QUEUE_VAD");
				voicebuff.reset_pos = voicebuff.write_pointer;	
				voicebuff_pos_case = 0;

				while(voicedata_read_flag){	
					unsigned char* w_rp_backup = voicebuff.read_pointer;
					unsigned char* w_wp_backup = voicebuff.write_pointer;
					//printf("\n--in----------w_wp_backup = %d,w_rp_backup = %d\n",w_wp_backup - voicebuff.buffer,w_rp_backup- voicebuff.buffer);
					int ret = rec_engine_data_read(voiceData, REC_ONE_BLOCK_SIZE, 110 / portTICK_PERIOD_MS);
					ESP_LOGD(TAG, "index = %d", ret);
					if ((ret == 0) || (ret == -1)) {						
						voicedata_read_flag = 0;
						continue;
					}

					if(ret > 0){						
						if(voicebuff_pos_case == 0){//------voicebuff_pos_case == 0: read_pos < write_pos; 
							if((w_wp_backup + REC_ONE_BLOCK_SIZE) >= (voicebuff.buffer + JOSHVM_CYCLEBUF_BUFFER_SIZE)){
								int temp_length1 = voicebuff.buffer + JOSHVM_CYCLEBUF_BUFFER_SIZE - w_wp_backup;								
								int temp_length2 = REC_ONE_BLOCK_SIZE - temp_length1;
								int temp_length3 = w_rp_backup - voicebuff.buffer;

								if(temp_length3 <= temp_length2){							
									//printf("--out 1--------w_wp_backup = %d,w_rp_backup = %d\n",voicebuff.write_pointer- voicebuff.buffer,w_rp_backup- voicebuff.buffer);
									continue;
								}	

								if((temp_length2 == 0) && (w_rp_backup == voicebuff.buffer)){
									//printf("--out 2---------w_wp_backup = %d,w_rp_backup = %d\n",voicebuff.write_pointer- voicebuff.buffer,w_rp_backup- voicebuff.buffer);
									continue;
								}
								
								if(temp_length1)memcpy(w_wp_backup,voiceData,temp_length1);
								if(temp_length2)memcpy(voicebuff.buffer,voiceData + temp_length1,temp_length2);
								voicebuff.write_pointer = voicebuff.buffer + temp_length2;
								if(w_rp_backup > voicebuff.write_pointer){
									voicebuff_pos_case = 1; 
								}
								//printf("--out 3---------w_wp_backup = %d,w_rp_backup = %d\n",voicebuff.write_pointer- voicebuff.buffer,w_rp_backup- voicebuff.buffer);
								continue;
							}

							memcpy(w_wp_backup,voiceData,REC_ONE_BLOCK_SIZE);
							voicebuff.write_pointer += REC_ONE_BLOCK_SIZE;
							if(voicebuff.write_pointer >= voicebuff.buffer + JOSHVM_CYCLEBUF_BUFFER_SIZE){
								voicebuff.write_pointer = voicebuff.buffer;
								if(w_rp_backup > voicebuff.write_pointer){
									voicebuff_pos_case = 1;
								}
							}							
						}else{
							if(w_rp_backup - w_wp_backup <= REC_ONE_BLOCK_SIZE){								
								//printf("--out-4---------w_wp_backup = %d,w_rp_backup = %d\n",voicebuff.write_pointer- voicebuff.buffer,w_rp_backup- voicebuff.buffer);
								continue;	
							}
							
							memcpy(w_wp_backup,voiceData,REC_ONE_BLOCK_SIZE);							
							voicebuff.write_pointer += REC_ONE_BLOCK_SIZE;
							if(voicebuff.write_pointer >= voicebuff.buffer + JOSHVM_CYCLEBUF_BUFFER_SIZE){
								voicebuff.write_pointer = voicebuff.buffer;
								if(w_rp_backup > voicebuff.write_pointer){
									voicebuff_pos_case = 1; 
								}
							}
						}							
					}
					//printf("--out end----------w_wp_backup = %d,w_rp_backup = %d\n",voicebuff.write_pointer- voicebuff.buffer,w_rp_backup- voicebuff.buffer);
				}		
				break;	
			case QUEUE_VAD_STOP:
				ESP_LOGI(TAG, "QUEUE_VAD_STOP");
				break;	
			case QUEUE_WAKEUP_END:									
				ESP_LOGI(TAG, "QUEUE_WAKEUP_END");
				break;	
			default :
				break;
		}
	}
	free(voiceData);
	voiceData = NULL;
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
    ESP_LOGI(TAG, "Recorder has been created");
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
    ESP_LOGI(TAG, "Recorder has been created");
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

esp_err_t periph_callback(audio_event_iface_msg_t *event, void *context)
{
    ESP_LOGD(TAG, "Periph Event received: src_type:%x, source:%p cmd:%d, data:%p, data_len:%d",
             event->source_type, event->source, event->cmd, event->data, event->data_len);
    switch (event->source_type) {
        case PERIPH_ID_BUTTON: {
                if ((int)event->data == get_input_rec_id() && event->cmd == PERIPH_BUTTON_PRESSED) {
                    ESP_LOGI(TAG, "PERIPH_NOTIFY_KEY_REC");
                    rec_engine_trigger_start();
                } else if ((int)event->data == get_input_mode_id() &&
                           ((event->cmd == PERIPH_BUTTON_RELEASE) || (event->cmd == PERIPH_BUTTON_LONG_RELEASE))) {
                    ESP_LOGI(TAG, "PERIPH_NOTIFY_KEY_REC_QUIT");
                }
                break;
            }
        case PERIPH_ID_TOUCH: {

				break;
        	}
        case PERIPH_ID_ADC_BTN:
            if (((int)event->data == get_input_volup_id()) && (event->cmd == PERIPH_ADC_BUTTON_RELEASE)) {
                int player_volume = 0;
                esp_audio_vol_get(player, &player_volume);
                player_volume += 10;
                if (player_volume > 100) {
                    player_volume = 100;
                }
                esp_audio_vol_set(player, player_volume);
                ESP_LOGW(TAG, "AUDIO_USER_KEY_VOL_UP [%d]", player_volume);
            } else if (((int)event->data == get_input_voldown_id()) && (event->cmd == PERIPH_ADC_BUTTON_RELEASE)) {
                int player_volume = 0;
                esp_audio_vol_get(player, &player_volume);
                player_volume -= 10;
                if (player_volume < 0) {
                    player_volume = 0;
                }
                esp_audio_vol_set(player, player_volume);
                ESP_LOGW(TAG, "AUDIO_USER_KEY_VOL_DOWN [%d]", player_volume);
            } else if (((int)event->data == get_input_play_id()) && (event->cmd == PERIPH_ADC_BUTTON_RELEASE)) {
				
				ESP_LOGW(TAG, "AUDIO_USER_KEY_PLAY");
				rec_engine_trigger_start();
            } else if (((int)event->data == get_input_set_id()) && (event->cmd == PERIPH_ADC_BUTTON_RELEASE)) {
                //esp_audio_vol_set(player, 0);
                ESP_LOGW(TAG, "AUDIO_USER_KEY_SET");
				rec_engine_trigger_stop();
            }else if (((int)event->data == get_input_rec_id()) && (event->cmd == PERIPH_ADC_BUTTON_RELEASE)) {
                ESP_LOGW(TAG, "AUDIO_USER_KEY_REC");
            	
       		}else if (((int)event->data == get_input_mode_id()) && (event->cmd == PERIPH_ADC_BUTTON_RELEASE)) {
                ESP_LOGW(TAG, "AUDIO_USER_KEY_MODE");
				/*static char buf[1024];
				vTaskGetRunTimeStats(buf);
				printf("JOSHVM_Audio,Run Time Stats:\nTask Name	  Time	  Percent\n%s\n", buf);

				vTaskList(buf);
				printf("JOSHVM_Audio,Task List:\nTask Name	 Status   Prio	  HWM	 Task Number\n%s\n", buf);
				*/		
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}


void joshvm_app_init(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);
    ESP_LOGI(TAG, "ADF version is %s", ADF_VER);
    audio_board_handle_t board_handle = audio_board_init();
	audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);

	
/*
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);
    if (set != NULL) {
        esp_periph_set_register_callback(set, periph_callback, NULL);
    }
*/
    //audio_board_key_init(set);
    //audio_board_sdcard_init(set);
    app_sdcard_init();
    //disp_serv = audio_board_led_init();

	app_wifi_service();
/*
    rec_config_t eng = DEFAULT_REC_ENGINE_CONFIG();
    eng.vad_off_delay_ms = 800;
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
    eng.user_data = NULL;
	rec_engine_create(&eng);
*/
	Queue_vad_play = xQueueCreate(QUEUE_VAD_PLAY_LEN, QUEUE_VAD_PLAY_SIZE);
	if(NULL == Queue_vad_play){
		ESP_LOGE(TAG,"Queue_vad_play created failed");
	}
	
	joshvm_cyclebuf_init(&voicebuff);
	xTaskCreate(vad_task, "vad_task", 4096, NULL, 3,NULL);	
	ESP_LOGE(TAG,"heap_caps_get_free_size = %d",heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
	
	//joshvm_audio_wrapper_init();
	ESP_LOGE(TAG,"heap_caps_get_free_size = %d",heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));

	while (1) {

  		JavaTask();
		//JavaNativeTest();
		//test_esp32_media();
		
		for (int i = 10; i >= 0; i--) {
	        printf("Restarting in %d seconds...\n", i);
	        vTaskDelay(1000 / portTICK_PERIOD_MS);
	    }
	    printf("Restarting now.\n");
	    fflush(stdout);
	    esp_restart();	  
	}
}

/**
 * JOSH VM interface
 **/
int esp32_playback_voice(int i) {
	//NOT implemented
	esp_err_t ret = ESP_FAIL;
	int player_volume = 0;
	switch(i){
	case 0:
		//t = joshvm_audio_play_handler(AMR_STREAM_MP3_URI);
		display_service_set_pattern(disp_serv, DISPLAY_PATTERN_TURN_ON, 0);
		javanotify_simplespeech_event(2, 0);
		return 0;
	break;
	case 1:
		display_service_set_pattern(disp_serv, DISPLAY_PATTERN_TURN_OFF, 0);
		javanotify_simplespeech_event(2, 0);
		//joshvm_spiffs_audio_play_handler(AMR_STREAM_MP3_URI);
		//ret = joshvm_audio_play_handler(AMR_STREAM_MP3_URI);
		return 0;
	break;
	case 2:

        esp_audio_vol_get(player, &player_volume);
        player_volume += 10;
        if (player_volume > 100) {
            player_volume = 100;
        }
        esp_audio_vol_set(player, player_volume);
        printf("AUDIO_USER_KEY_VOL_UP [%d]\n", player_volume);

		javanotify_simplespeech_event(2, 0);
		return 0;
	case 3:

		esp_audio_vol_get(player, &player_volume);
		player_volume -= 10;
		if (player_volume < 0) {
			player_volume = 0;
		}
		esp_audio_vol_set(player, player_volume);
		printf("AUDIO_USER_KEY_VOL_DOWN [%d]\n", player_volume);

		javanotify_simplespeech_event(2, 0);
		return 0;
	case 4:

		joshvm_spiffs_audio_play_handler(MP3_STREAM_URI);
		
		return 0;
	case 5:

		joshvm_spiffs_audio_play_handler(MP3_2_STREAM_URI);
		
		return 0;
	case 6:

		joshvm_spiffs_audio_play_handler(MP3_1_STREAM_URI);
		
		return 0;
	break;
	default:
	break;
	}
	
	return ret;
}

int esp32_record_voicefile(unsigned char* filename, int time) {
	//NOT implemented
	return 0;
}

int esp32_playback_voice_url(const char *url)
{
	int ret;
	ret = joshvm_audio_play_handler(url);	
	if(ret == ESP_ERR_AUDIO_NO_ERROR){

	}
	return ret;
}

void esp32_device_control(int command) {
	switch (command) {
		case 0: //PAUSE
		printf("JOSHVM_Audio:esp32_device_control: PAUSE\n");
		rec_engine_detect_suspend(REC_VOICE_SUSPEND_ON);
		break;
		case 1: //RESUME
		printf("JOSHVM_Audio:esp32_device_control: RESUME\n");
		rec_engine_detect_suspend(REC_VOICE_SUSPEND_OFF);
		break;
		case 2: //LISTEN
		printf("JOSHVM_Audio:esp32_device_control: LISTEN\n");
		rec_engine_vad_enable(true);
		break;
		case 3: //SLEEP
		printf("JOSHVM_Audio:esp32_device_control: SLEEP\n");
		rec_engine_vad_enable(false);
		break;
	}
}

void esp32_stop_playback() {
	printf("JOSHVM_Audio:esp_audio_stop\n");
	esp_audio_stop(player, TERMINATION_TYPE_NOW);
}

void esp32_stop_record() {
	printf("JOSHVM_Audio:rec_engine_trigger_stop\n");
	rec_engine_trigger_stop();
}
