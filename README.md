# ESP32-CAM-Take-Photo-and-Display-Web-Server
請問有人用過ESP32-CAM 拍照+伺服馬達，會出現以下的錯誤嗎?
[E][camera.c:495] i2s_run(): Timeout waiting for VSYNC


用這個會與ESP32-CAM不相容 #include <ESP32Servo.h>  https://github.com/madhephaestus/ESP32Servo

問題解決請改用  #include <Servo.h>   https://github.com/RoboticsBrno/ServoESP32
