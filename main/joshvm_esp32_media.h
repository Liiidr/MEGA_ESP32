#ifndef _JOSHVM_ESP32_MEDIA_H_
#define _JOSHVM_ESP32_MEDIA_H_


typedef enum{
	JOSHVM_OK 				= 0,
	JOSHVM_FAIL 			= -1,
	JOSHVM_NOTIFY_LATER 	= -2,
	JOSHVM_OUT_OF_MEMORY	= -3,
	JOSHVM_LIMIT_RESOURCE	= -4,
	JOSHVM_INVALID_ARGUMENT	= -5,
	JOSHVM_INVALID_STATE	= -6,	
	JOSHVM_NOT_SUPPORTED	= -7, 
	JOSHVM_TIMEOUT			= -8
}joshvm_err_t;

typedef enum{
	MEDIA_PLAYER = 0,
	MEDIA_RECORDER,
	AUDIO_TRACK,
	AUDIO_RECORDER
}joshvm_type_t;


typedef struct{
	//uint8_t media_type;

}joshvm_media_mediaplayer_t;

typedef struct{
	//uint8_t media_type;

}joshvm_media_mediarecorder_t;

typedef struct{
	//uint8_t media_type;

}joshvm_media_audiotrack_t;

typedef struct{
	//uint8_t media_type;

}joshvm_media_audiorecorder_t;

struct {
	uint8_t media_type;

	union{
		joshvm_media_mediaplayer_t joshvm_media_mediaplayer;
		joshvm_media_mediarecorder_t joshvm_media_mediarecorder;
		joshvm_media_audiotrack_t joshvm_media_audiotrack;
		joshvm_media_audiorecorder_t joshvm_media_audiorecorder;
	}joshvm_media_u;

}joshvm_media_t;


/**
 * @brief Create 
 *
 * @note 
 * @param 	type  
 *				-0	create MediaPlyaer
 *				-1	create MediaPlyaer
 *				-2	create MediaPlyaer
 *				-3	create MediaPlyaer
 *			handle
 *
 * @return (error code)
 *     - 0: ok
 *     	 -1: fail
 *		...
 */
int joshvm_esp32_media_create(int type, void** handle);
/**
 * @brief Release 
 *
 * @note 
 *
 *
 * @param handle
 *
 * @return (error code)
 *     - 0: ok
 *     	 -1: fail
 *		...
 */
int joshvm_esp32_media_close(void* handle);


/**
 * @brief prepare before play / record 
 *
 * @note 
 *
 *
 * @param handle
 *        callback
 *
 * @return (error code)
 *     - 0: ok
 *     	 -1: fail
 *		...
 */
int joshvm_esp32_media_prepare(void* handle, void(*callback)(void*, int));
/**
 * @brief run Media / Audio 
 *
 * @note 
 *
 *
 * @param handle
 *        callback
 *
 * @return (error code)
 *     - 0: ok
 *     	 -1: fail
 *		...
 */
int joshvm_esp32_media_start(joshvm_media_mediaplayer* handle, void(*callback)(void*, int));


/**
 * @brief pause Media / Audio 
 *
 * @note 
 *
 *
 * @param handle
 *        callback
 *
 * @return (error code)
 *     - 0: ok
 *     	 -1: fail
 *		...
 */
int joshvm_esp32_media_pause(void* handle);

#endif