1、替换wifi_service.c
替换路径
D:\GitHub\msys32\home\zz\esp\esp-adf\components\wifi_service\src 
2、修改SPIFFS挂载名，spiffs_stream.c  line94    "/spiffs"改为"/suerdata"
路径：\esp\esp-adf\components\audio_stream
3、i2s_stream.c  line359 添加判断i2s_driver_install返回值
路径：\esp\esp-adf\components\audio_stream