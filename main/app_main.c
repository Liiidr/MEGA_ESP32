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

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "tcpip_adapter.h"
#include "joshvm.h"
#include "esp_log.h"
#include "joshvm_esp32_ring_buff.h" //test


static const char *TAG = "APP_MAIN";

#define test_buff_size 10


void app_main(void)
{
	ESP_LOGI(TAG,"Begin main task free heap size = %d",heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
	esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    tcpip_adapter_init();
	joshvm_app_init();
	
/*//---------test begin
	ring_buffer_t ring_buffer;
	int8_t buf1[13] = {1,2,3,4,5,6,7,8,9,0xa,0xb,0xc,0xd};
	int8_t buf2[4] = {0x0a,0x0a,0x0a,0x0a};
	int8_t buf3[3] = {0xb,0xb,0xb};
	int8_t buf4[5] = {0xc,0xc,0xc,0xc,0xc};
	int8_t buf5[10] = {0};

	ring_buffer_init(&ring_buffer, test_buff_size);//RING_BUFFER_SIZE我用宏定义为8

	printf("\n\n\n");
	printf("Init data\n");
	joshvm_rb_test(ring_buffer.buffer,test_buff_size);

	int ret = 0;
	ret = ring_buffer_write(buf2, sizeof(buf2), &ring_buffer); 
	printf("ret = %d\n",ret);
	ret = ring_buffer_write(buf3, sizeof(buf3), &ring_buffer); 
	printf("ret = %d\n",ret);
	ret = ring_buffer_write(buf4, sizeof(buf4), &ring_buffer); 
	printf("ret = %d\n",ret);	
	printf("rb:");
	joshvm_rb_test(ring_buffer.buffer,test_buff_size);

	ret = ring_buffer_write(buf1, sizeof(buf1), &ring_buffer); 
	printf("ret = %d\n",ret);		
	printf("rb:");
	joshvm_rb_test(ring_buffer.buffer,test_buff_size);

	ret = ring_buffer_read(buf5, sizeof(buf5), &ring_buffer); 
	printf("ret = %d\n",ret);
	joshvm_rb_test(buf5,10);

*/


	
/*	uint8_t cnt = 0;
	while(1){
		cnt++;
		ring_buffer_write(&buf2, sizeof(buf2), &ring_buffer);
		printf("w_offset= %d,r_offset = %d\n",ring_buffer.write_offset,ring_buffer.read_offset);
		ring_buffer_write(&buf3, sizeof(buf3), &ring_buffer);
		printf("w_offset= %d,r_offset = %d\n",ring_buffer.write_offset,ring_buffer.read_offset);
		printf("rb: " );
		joshvm_rb_test(ring_buffer.buffer,test_buff_size);


		vTaskDelay(2000);
	}	




	ring_buffer_read(&ring_buffer, buf2, sizeof(buf2)); //ring_buffer->buffer="1234abcd" buf2="1234ab"
	printf("read 6 data : rb -->buf2 ");
	printf("buf2:");
	joshvm_rb_test(buf2,sizeof(buf2));

	ring_buffer_read(&ring_buffer, buf2, sizeof(buf2)); //ring_buffer->buffer="1234abcd" buf2="1234ab"
	printf("read 6 data : rb -->buf2 ");
	printf("buf2:");
	joshvm_rb_test(buf2,sizeof(buf2));


	ring_buffer_write(buf3, 6, &ring_buffer); //ring_buffer->buffer="34abcdcd"
	printf("buf3-->rb\n");
	printf("buf3:");
	joshvm_rb_test(buf3,6);
	printf("rb:");
	joshvm_rb_test(ring_buffer.buffer,test_buff_size);

	ring_buffer_read(&ring_buffer, buf2, 6); //ring_buffer->buffer="34abcd78" buf2="7834ab"
	printf("read:");
	joshvm_rb_test(buf2,8);
	joshvm_rb_test(ring_buffer.buffer,test_buff_size);

	ring_buffer_write("12345678abcd", 12, &ring_buffer);
	ring_buffer_deinit(&ring_buffer);

*/


//------test end	
    
}
