#ifndef JOSHVM_ESP32_RECORD_H_
#define JOSHVM_ESP32_RECORD_H_

#include "audio_element.h"
#include "audio_pipeline.h"
#include "i2s_stream.h"
#include "filter_resample.h"
#include "fatfs_stream.h"
#include "http_stream.h"
#include "wav_encoder.h"

#include "esp_log.h"

#include "joshvm_esp32_media.h"


//---define



//---enum
enum{
	joshvm_meida_format_wav = 0,
	joshvm_meida_format_amrnb,
	joshvm_meida_format_amrwb,
	joshvm_meida_format_opus
}joshvm_meida_format_e;

//---struct



//---fun

/**
 * @brief init meida recorder 
 *
 * @note 
 * @param 	
 *			handle
 *
 * @return (error code)
 *     - 0: ok
 *     	 -1: fail
 *		...
 */
int joshvm_meida_recorder_init(joshvm_media_t * handle);

/**
 * @brief get meida recorder state
 *
 * @note 
 * @param 	
 *			handle
 *[out]	  state
 *    		- 1:stop
 *			  2:pause
 *			  3:playing/recording
 *
 * @return (error code)
 *     - 0: ok
 *     	 -1: fail
 *		...
 */
int joshvm_media_recorde_get_state(joshvm_media_t* handle,int* state);



#endif


