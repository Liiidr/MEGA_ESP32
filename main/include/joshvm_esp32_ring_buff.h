#ifndef JOSHVM_ESP32_RAW_BUFF_H_
#define JOSHVM_ESP32_RAW_BUFF_H_



//---define
#define NEED_INIT 1
#define NOT_INTI 0
#define RB_NOT_COVER 0
#define RB_COVER 1


//---enum


//---struct
typedef struct
{
    int8_t *buffer;      //���ʵ�ʵ�����
    int32_t read_offset;  //��ȡ��ַ���buffer��ƫ����
    int32_t write_offset; //д���ַ���buffer��ƫ����
    int32_t valid_size;   //buffer����Чsize
    int32_t total_size;   //buffer���ܴ�С����initʱmalloc��size
    uint8_t init_read_pointer;
} ring_buffer_t;



//---fun

/**
 * @brief init ring_buffer
 *
 * @note 
 * @param 	ring_buffer
 *			buff_size:data buffer size
 *
 * @return none
 */
void ring_buffer_init(ring_buffer_t *ring_buffer, int32_t buff_size);

/**
 * @brief deinit ring_buffer
 *
 * @note 
 * @param 	ring_buffer			
 *
 * @return none
 */
void ring_buffer_deinit(ring_buffer_t *rb);

/**
 * @brief flush ring_buffer,clear data
 *
 * @note 
 * @param 	rb:rring_buffer		
 *
 * @return none
 */
void ring_buffer_flush(ring_buffer_t *rb);

/**
 * @brief 	write datas into ring_buffer
 *
 * @note 
 * @param 	buffer_to_write:the buffer wangted to be wrote into the rb
 *			size:the size wangted to be written into ring_buffer
 			rb:ring_buffer
 			cover_type:  0:don't cover old data  1:cover old data
 *
 * @return 	the truely size of wrote datas
 */
int32_t ring_buffer_write(void *buffer_to_write, int32_t size, ring_buffer_t *rb,uint8_t cover_type);

/**
 * @brief 	read data form ring_buffer
 *
 * @note 
 * @param 	buffer:save the datas read from ring_buffer
 *			size:the size  wangted to be read from ring_buffer
 			rb:ring_buffer
 *
 * @return 	the truely size of read datas
 */
int32_t ring_buffer_read(void *buff, int32_t size,ring_buffer_t *rb);



void joshvm_rb_test(int8_t *buff,uint8_t size);


#endif


