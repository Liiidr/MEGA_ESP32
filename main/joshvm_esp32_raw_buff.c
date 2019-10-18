#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "audio_mem.h"
#include "joshvm_esp32_raw_buff.h"

#define TAG "JOSHVM_ESP32_RAW_BUFF>>>"

void ring_buffer_init(ring_buffer_t *rb, int32_t buff_size)
{
	ESP_LOGI(TAG,"ring_buffer_init");
    rb->buffer = (int8_t*)audio_malloc(buff_size);
    memset(rb->buffer, 0, buff_size);

    rb->read_offset = 0;
    rb->write_offset = 0;
    rb->valid_size = 0;
    rb->total_size = buff_size;
	rb->init_read_pointer = NOT_INTI;
}

void ring_buffer_deinit(ring_buffer_t *rb)
{
	ESP_LOGI(TAG,"ring_buffer_deinit");
    if (rb->buffer != NULL){
        audio_free(rb->buffer);
    }
    memset(rb, 0, sizeof(ring_buffer_t));
}

void ring_buffer_flush(ring_buffer_t *rb)
{
    rb->read_offset = rb->write_offset;    
    rb->valid_size = 0;
	rb->init_read_pointer = NOT_INTI;
}

uint32_t ring_buffer_write(void *buffer_to_write, int32_t size, ring_buffer_t *rb)
{
	ESP_LOGI(TAG,"ring_buffer_write");
    int32_t write_offset = rb->write_offset;
    int32_t total_size = rb->total_size;
    int32_t first_write_size = 0;
	uint32_t written_size = 0;

	if(size + rb->valid_size > total_size){
		rb->init_read_pointer = NEED_INIT;	
	}

    if (size + write_offset <= total_size){
        memcpy(rb->buffer + write_offset, buffer_to_write, size);
		written_size = size;
    }
    else //ring_buffer->buffer�ĺ��δд��Ŀռ�С��size,��ʱ����Ҫ���ں���д��һ���֣�Ȼ�󷵻�ͷ������ǰ�����д��
    {
        first_write_size = total_size - write_offset;
        memcpy(rb->buffer + write_offset, buffer_to_write, first_write_size);
        memcpy(rb->buffer, buffer_to_write + first_write_size, size - first_write_size);
		written_size = size;
    }
	
    rb->write_offset += size;
    rb->write_offset %= total_size;
    rb->valid_size += size;
	//adjust
	if(rb->init_read_pointer == NEED_INIT){
		rb->init_read_pointer = NOT_INTI;
		rb->read_offset = rb->write_offset;
		rb->valid_size = rb->total_size;	
	}
	return written_size;
}

uint32_t ring_buffer_read(void *buff, int32_t size,ring_buffer_t *rb)
{
	ESP_LOGI(TAG,"ring_buffer_read");

    int32_t read_offset = rb->read_offset;
	int32_t write_offset = rb->write_offset;
    int32_t total_size = rb->total_size;
    int32_t first_read_size = 0;
	uint32_t read_size = 0;//real size of data have been read

    if (size > rb->valid_size){
		size = rb->valid_size;	
    }

    if (total_size - read_offset >= size){
        memcpy(buff, rb->buffer + read_offset, size);
    }
    else{
        first_read_size = total_size - read_offset;
        memcpy(buff, rb->buffer + read_offset, first_read_size);
        memcpy(buff + first_read_size, rb->buffer, size - first_read_size);
    }
	read_size = size;

    rb->read_offset += size;
    rb->read_offset %= total_size;
    rb->valid_size -= size;

	return read_size;
}


void joshvm_rb_test(int8_t *buff,uint8_t size)
{
	for(int i=0;i<size;i++){
		printf("%x",*(buff+i));		
	}
	printf("\r\n");	
}