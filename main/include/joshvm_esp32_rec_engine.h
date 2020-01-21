#ifndef JOSHVM_ESP32_REC_ENGINE_H_
#define JOSHVM_ESP32_REC_ENGINE_H_



//---define



//---enum


//---struct



//---fun
int joshvm_esp32_wakeup_get_word_count(void);


/**
 * @brief get wakeup word
 *
 * @note 
 * @param 	pos:	range from 0 to conut-1(joshvm_esp32_wakeup_get_word_count)
 *			index:	this is output param.wakeup word index
 *			wordbuf:	wakeup word string addr
 *			wordlen:	length of wakeup word
 *			descbuf:	description of wakeup word
 *			desclen:	length of descriptions			
 *
 * @return (error code)
 *     - 0: ok
 *     	 -1: fail
 *		...
 */
int joshvm_esp32_wakeup_get_word(int pos, int* index, char* wordbuf, int wordlen, char* descbuf, int desclen);


/**
 * @brief enable wakeup
 *
 * @note 
 * @param 	callback: run when wakeup
 *			
 *
 * @return (error code)
 *     - 0: ok
 *     	 -1: fail
 *		...
 */
int joshvm_esp32_wakeup_enable(void(*callback)(int));

/**
 * @brief disable wakeup
 *
 * @note 
 * @param 	
 *			
 *
 * @return (error code)
 *     - 0: ok
 *     	 -1: fail
 *		...
 */
int joshvm_esp32_wakeup_disable();

/**
 * @brief start vad or command detect
 *
 * @note 
 * @param 
 *			mode:	0:vad(default)	1:command detect
 *			callback(param0,param1):
 *					param0:	 0:vad start	1:vad stop	2:command detected
 *					param1:(only for command detected)  -1:detected failed  0-99:command ID
 *
 * @return (error code)
 *     - 0: ok
 *     	 -1: fail
 *		...
 */
int joshvm_esp32_vad_start(int mode, void(*callback)(int, int));
int joshvm_esp32_vad_pause();
int joshvm_esp32_vad_resume();
int joshvm_esp32_vad_stop();
int joshvm_esp32_vad_set_timeout(int ms);




//-------------test
//void test_rec_engine(void);
//void test_vad_callback(int index);



#endif


