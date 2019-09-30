#include "audio_pipeline.h"
#include "esp_audio.h"

#include "joshvm_esp32_media.h"

esp_audio_handle_t mediaplayer;



static audio_pipeline_handle_t creat_pipeline_mediaplayer(void)
{

	return 0;
}

int joshvm_esp32_media_create(int type, void** handle)
{
	switch(type){
		case	MEDIA_PLAYER:
			*handle = creat_pipeline_mediaplayer();
	

			break;
		case	MEDIA_RECORDER:

			break;
		case	AUDIO_TRACK:

			break;
		case	AUDIO_RECORDER:

			break;
		default	:			
			break;
	}
	return 0;
}


int joshvm_esp32_media_close(void* handle)
{

	return 0;
}

int joshvm_esp32_media_prepare(void* handle, void(*callback)(void*, int))
{

	return 0;
}

int joshvm_esp32_media_start(joshvm_media_mediaplayer* handle, void(*callback)(void*, int))
{
	switch(handle->media_type){
		case MEDIA_PLAYER:
			//audio_pipeline_run();
			break;
		case MEDIA_RECORDER:

			break;
		case AUDIO_TRACK:

			break;
		case AUDIO_RECORDER:

			break;
		default :
			break;
	}
	return 0;
}


int joshvm_esp32_media_stop(void* handle)
{

	return 0;
}






