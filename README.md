# ESP32-CAM-Take-Photo-and-Display-Web-Server
請問有人用過ESP32-CAM 拍照+伺服馬達，會出現以下的錯誤嗎?
[E][camera.c:495] i2s_run(): Timeout waiting for VSYNC

問題解決改用
  #include <Servo.h>   https://github.com/RoboticsBrno/ServoESP32
