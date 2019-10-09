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



typedef esp_err_t (*mediaplayer_start)(void **handle);
typedef esp_err_t (*mediaplayer_callback)(void **handle,int);
typedef esp_err_t (*mediarecorder_start)(void **handle);
typedef esp_err_t (*audiotrack_start)(void **handle);
typedef esp_err_t (*audiorecorder_start)(void **handle);

typedef struct{
	mediaplayer_start start;
	char *url;						//dataSource
	mediaplayer_callback callback;
	uint32_t sample_rate;
	uint8_t channel;
	uint8_t	bit_rate;
	
	
}joshvm_media_mediaplayer_t;

typedef struct{
	mediarecorder_start start;
	int format;
	char *url;	
	uint32_t sample_rate;
	uint8_t channel;
	uint8_t	bit_rate;
	void *recorder;
	

}joshvm_media_mediarecorder_t;

typedef struct{
	audiotrack_start start;

}joshvm_media_audiotrack_t;

typedef struct{
	audiorecorder_start start;

}joshvm_media_audiorecorder_t;



typedef struct {
	uint8_t media_type;

	union{
		joshvm_media_mediaplayer_t joshvm_media_mediaplayer;
		joshvm_media_mediarecorder_t joshvm_media_mediarecorder;
		joshvm_media_audiotrack_t joshvm_media_audiotrack;
		joshvm_media_audiorecorder_t joshvm_media_audiorecorder;
	}joshvm_media_u;
	
}joshvm_media_t;


//---fun
void joshvm_esp32_media_callback();

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
 *     	 - 0: ok
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
int joshvm_esp32_media_prepare(joshvm_media_t* handle, void(*callback)(void*, int));
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
int joshvm_esp32_media_start(joshvm_media_t* handle, void(*callback)(void*, int));


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
int joshvm_esp32_media_pause(joshvm_media_t* handle);

/**
 * @brief stop Media / Audio 
 *
 * @note 
 *
 *
 * @param handle
 *    
 * @return (error code)
 *    -	0: ok
 *     	-1: fail
 *		...
 */
int joshvm_esp32_media_stop(joshvm_media_t* handle);

/**
 * @brief reset Media / Audio,reset parameter,release 
 *
 * @note 
 *
 *
 * @param handle
 *    
 * @return (error code)
 *    -	0: ok
 *     	-1: fail
 *		...
 */
int joshvm_esp32_media_reset(joshvm_media_t* handle);

/**
 * @brief release 
 *
 * @note 
 *
 *
 * @param handle
 *    
 * @return (error code)
 *    -	0: ok
 *     	-1: fail
 *		...
 */
int joshvm_esp32_media_release(void* handle);

/**
 * @brief release 
 *
 * @note 
 *
 * @param handle
 *		  state
 *    		- 1:stop
 *			  2:pause
 *			  3:playing/recording
 * @return (error code)
 *    -	0: ok
 *     	-1: fail
 *		...
 */
 enum{
 	JOSHVM_MEDIA_RESERVE = 0,//after play/record
	JOSHVM_MEDIA_STOPPED = 1,
	JOSHVM_MEDIA_PAUSED = 2,
	JOSHVM_MEDIA_PLAYING = 3,
	JOSHVM_MEDIA_RECORDING = 3
 }media_state_e;
int joshvm_esp32_media_get_state(void* handle, int* state);

/**
 * @brief read 
 *
 * @note 
 *
 * @param 	handle
 *			buffer:read address
 *			size:the most number of bytes to read 
 *			byteRead:number of bytes to read
 *			callback:
 * @return (error code)
 *    -	0: ok
 *     	-1: fail
 *		...
 */
int joshvm_esp32_media_read(void* handle, unsigned char* buffer, int size, int* bytesRead, void(*callback)(void*, int));

/**
 * @brief  write
 *
 * @note 
 *
 * @param 	handle
 *			buffer:write address
 *			size:the most number of bytes to writed
 *			byteWritten:number of bytes to writed
 *			callback:
 * @return (error code)
 *    -	0: ok
 *     	-1: fail
 *		...
 */
int joshvm_esp32_media_write(void* handle, unsigned char* buffer, int size, int* bytesWritten, void(*callback)(void*, int));

/**
 * @brief  clear buffer,keep the state of paused / stopped 
 *
 * @note 
 *
 * @param 	handle
 *
 * @return (error code)
 *    -	0: ok
 *     	-1: fail
 *		...
 */
int joshvm_esp32_media_flush(void* handle);

/**
 * @brief   
 *
 * @note 
 *
 * @param 	handle
 *			size: the size of the buffer can be return
 *
 * @return (error code)
 *    -	0: ok
 *     	-1: fail
 *		...
 */
int joshvm_esp32_media_get_buffsize(void* handle, int* size);

/**
 * @brief   
 *
 * @note 
 *
 * @param 	handle
 *			value:set sample rate
 *
 * @return (error code)
 *    -	0: ok
 *     	-1: fail
 *		...
 */
int joshvm_esp32_media_set_audio_sample_rate(joshvm_media_t* handle, uint32_t value);
int joshvm_esp32_media_set_channel_config(joshvm_media_t* handle, uint8_t value);
int joshvm_esp32_media_set_audio_bit_rate(joshvm_media_t* handle, uint8_t value);

/**
 * @brief   
 *
 * @note 
 *
 * @param 	handle
 *			sourse:Such as "file://sdcard/test.wav" or "http://iot.espressif.com/file/example.mp3".
 *               If NULL to be set, the uri setup by`esp_audio_setup` will used.
 *
 * @return (error code)
 *    -	0: ok
 *     	-1: fail
 *		...
 */
int joshvm_esp32_media_set_source(joshvm_media_t* handle, char* source);

/**
 * @brief   
 *
 * @note 
 *
 * @param 	handle:
 *			file:file name and file path,Such as "file://sdcard/test.wav"
 *
 * @return (error code)
 *    -	0: ok
 *     	-1: fail
 *		...
 */
int joshvm_esp32_media_set_output_file(void* handle, char* file);

/**
 * @brief   
 *
 * @note 
 *
 * @param 	handle:
 *			format:output format
 *
 * @return (error code)
 *    -	0: ok
 *     	-1: fail
 *		...
 */
int joshvm_esp32_media_set_output_format(void* handle, int format);

/**
 * @brief   
 *
 * @note 
 *
 * @param 	handle:
 *			pos:current position
 *
 * @return (error code)
 *    -	0: ok
 *     	-1: fail
 *		...
 */
int joshvm_esp32_media_get_position(void* handle, int* pos);

/**
 * @brief   
 *
 * @note 
 *
 * @param 	handle:
 *			pos:current position
 *			callback:
 * @return (error code)
 *    -	0: ok
 *     	-1: fail
 *		...
 */
int joshvm_esp32_media_set_position(void* handle, int pos, void(*callback)(void*, int));

/**
 * @brief   
 *
 * @note 
 *
 * @param 	handle:
 *			duration : get tatol time of media (ms)
 * @return (error code)
 *    -	0: ok
 *     	-1: fail
 *		...
 */
int joshvm_esp32_media_get_duration(void* handle, int* duration);

int joshvm_esp32_media_get_max_volume(int* volume);
int joshvm_esp32_media_get_volume(int* volume);
int joshvm_esp32_media_set_volume(int volume);
int joshvm_esp32_media_add_volume();
int joshvm_esp32_media_sub_volume();

void test_esp32_media(void);

#endif

