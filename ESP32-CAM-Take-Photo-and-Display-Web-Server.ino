/*********
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/esp32-cam-take-photo-display-web-server/

  IMPORTANT!!!
   - Select Board "AI Thinker ESP32-CAM"
   - GPIO 0 must be connected to GND to upload a sketch
   - After connecting GPIO 0 to GND, press the ESP32-CAM on-board RESET button to put your board in flashing mode

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*********/

#include "WiFi.h"
#include "WiFiClientSecure.h"
#include "esp_camera.h"
#include "esp_timer.h"
#include "img_converters.h"
#include "Arduino.h"
#include "soc/soc.h"           // Disable brownour problems
#include "soc/rtc_cntl_reg.h"  // Disable brownour problems
#include "driver/rtc_io.h"
#include <ESPAsyncWebServer.h>
#include <StringArray.h>
#include <SPIFFS.h>
#include <FS.h>
#include <ESP32Servo.h>

// Replace with your network credentials
const char* ssid = "live3";            //要改
const char* password = "XXXXXX";  //要改
int angle = 40;           //轉角度     //要改
String myLineNotifyToken = "XXXXX";    //要改 Line Notify Token，You can refer this post to get Line token：https://t.ly/LZwKn

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

boolean takeNewPhoto = false;

// Photo File Name to save in SPIFFS
#define FILE_PHOTO "/photo.jpg"

//mg996r 馬達
#define SERVO_1      15  //將servo物件連接到 pin 14
Servo servoN1;
Servo servoN2;
Servo servo1;            // 建立一個 servo 物件，最多可建立 12個 servo
int servo1Pos = 10;       // 設定 Servo 起始位置的變數不要設0

// OV2640 camera module pins (CAMERA_MODEL_AI_THINKER)
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { text-align:center; }
    .vert { margin-bottom: 10%; }
    .hori{ margin-bottom: 0%; }
  </style>
</head>
<body>
  <div id="container">
    <h2>ESP32-CAM Last Photo</h2>
    <p>It might take more than 5 seconds to capture a photo.</p>
    <p>
      <button onclick="rotatePhoto();">ROTATE</button>
      <button onclick="capturePhoto()">CAPTURE PHOTO</button>
      <button onclick="location.reload();">REFRESH PAGE</button>
    </p>
  </div>
  <div><img src="saved-photo" id="photo"></div>
</body>
<script>
  var deg = 0;
  function capturePhoto() {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', "/capture", true);
    xhr.send();  
    setTimeout("location.reload();",1000 ); 
    
  }
  function rotatePhoto() {
    var img = document.getElementById("photo");
    deg += 90;
    if(isOdd(deg/90)){ document.getElementById("container").className = "vert"; }
    else{ document.getElementById("container").className = "hori"; }
    img.style.transform = "rotate(" + deg + "deg)";
  }

  function isOdd(n) { return Math.abs(n % 2) == 1; }
</script>
</html>)rawliteral";


String sendImage2LineNotify(String msg) {  //傳LINE


  // 傳line
  camera_fb_t * fb = NULL;
  fb = esp_camera_fb_get();//取得相機影像放置fb
  if (!fb) {
    delay(100);
    Serial.println("Camera capture failed, Reset");
    ESP.restart();
  }
  WiFiClientSecure client_tcp;//啟動SSL wificlient
  Serial.println("Connect to notify-api.line.me");
  if (client_tcp.connect("notify-api.line.me", 443)) {
    Serial.println("Connection successful");
    String head = "--Taiwan\r\nContent-Disposition: form-data; name=\"message\"; \r\n\r\n" + msg + "\r\n--Taiwan\r\nContent-Disposition: form-data; name=\"imageFile\"; filename=\"esp32-cam.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
    String tail = "\r\n--Taiwan--\r\n";
    uint16_t imageLen = fb->len;
    uint16_t extraLen = head.length() + tail.length();
    uint16_t totalLen = imageLen + extraLen;
    //開始POST傳送訊息
    client_tcp.println("POST /api/notify HTTP/1.1");
    client_tcp.println("Connection: close");
    client_tcp.println("Host: notify-api.line.me");
    client_tcp.println("Authorization: Bearer " + myLineNotifyToken);
    client_tcp.println("Content-Length: " + String(totalLen));
    client_tcp.println("Content-Type: multipart/form-data; boundary=Taiwan");
    client_tcp.println();
    client_tcp.print(head);
    uint8_t *fbBuf = fb->buf;
    size_t fbLen = fb->len;
    Serial.println("Data Sending....");
    //照片，分段傳送
    for (size_t n = 0; n < fbLen; n = n + 2048) {
      if (n + 2048 < fbLen) {
        client_tcp.write(fbBuf, 2048);
        fbBuf += 2048;
      } else if (fbLen % 2048 > 0) {
        size_t remainder = fbLen % 2048;
        client_tcp.write(fbBuf, remainder);
      }
    }
    client_tcp.print(tail);
    client_tcp.println();
    String getResponse = "", Feedback = "";
    boolean state = false;
    int waitTime = 3000;   // 依據網路調整等候時間，3000代表，最多等3秒
    long startTime = millis();
    delay(1000);
    Serial.print("Get Response");
    while ((startTime + waitTime) > millis())    {
      Serial.print(".");
      delay(100);
      bool jobdone = false;
      while (client_tcp.available())
      { //當有收到回覆資料時
        jobdone = true;
        char c = client_tcp.read();
        if (c == '\n')
        {
          if (getResponse.length() == 0) state = true;
          getResponse = "";
        }
        else if (c != '\r')
          getResponse += String(c);
        if (state == true) Feedback += String(c);
        startTime = millis();
      }
      if (jobdone) break;
    }
    client_tcp.stop();
    esp_camera_fb_return(fb);//清除緩衝區
    return Feedback;
  }
  else {
    esp_camera_fb_return(fb);
    return "Send failed.";
  }

}

void setup() {
  // Serial port for debugging purposes
  Serial.begin(115200);

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    ESP.restart();
  }
  else {
    delay(500);
    Serial.println("SPIFFS mounted successfully");
  }

  // Print ESP32 Local IP Address
  Serial.print("IP Address: http://");
  Serial.println(WiFi.localIP());

  // Turn-off the 'brownout detector'
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  // OV2640 camera module
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    ESP.restart();
  }

  //drop down frame size for higher initial frame rate
  sensor_t * s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_VGA);  // VGA|CIF|QVGA|HQVGA|QQVGA   ( UXGA? SXGA? XGA? SVGA? )

  //Flash
  ledcAttachPin(4, 4);
  ledcSetup(4, 5000, 8);

  //mg996r 馬達
  servoN1.attach(2, 500, 2400);
  servoN2.attach(13, 500, 2400);
  servo1.setPeriodHertz(50);    // standard 50 hz servo
  servo1.attach(SERVO_1, 500, 2400); // using SG90,MG996R servo min/max of 500us and 2400us  , for MG995 large servo, use 1000us and 2000us,

  servo1.write(servo1Pos);

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/html", index_html);
  });

  server.on("/capture", HTTP_GET, [](AsyncWebServerRequest * request) {
    takeNewPhoto = true;
    request->send_P(200, "text/plain", "Taking Photo");
  });

  server.on("/saved-photo", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, FILE_PHOTO, "image/jpg", false);
  });

  // Start server
  server.begin();

}

void loop() {
  if (takeNewPhoto) {
       ledcWrite(4, 10);
    delay(200);

    //mg996r 馬達
    if (servo1Pos <= 170) {
      servo1Pos += angle;
      servo1.write(servo1Pos);
      delay(500);
      servo1Pos -= angle;
      servo1.write(servo1Pos);
    }
    Serial.println(servo1Pos);
    Serial.println("Down");
      delay(1000);

      
//    capturePhotoSaveSpiffs();

    // line
    Serial.println("starting to Line");
    String payload = sendImage2LineNotify("There is someone coming....");
    Serial.println(payload);
    delay(10000);

    takeNewPhoto = false;
    delay(1000);
    ledcWrite(4, 0);
  }
  delay(1);
}

// Check if photo capture was successful
bool checkPhoto( fs::FS &fs ) {
  File f_pic = fs.open( FILE_PHOTO );
  unsigned int pic_sz = f_pic.size();
  return ( pic_sz > 100 );
}


// Capture Photo and Save it to SPIFFS
void capturePhotoSaveSpiffs( void ) {



  
  camera_fb_t * fb = NULL; // pointer
  bool ok = 0; // Boolean indicating if the picture has been taken correctly

  do {
    // Take a photo with the camera
    Serial.println("Taking a photo...");
      delay(1000);
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      return;
    }

    // Photo file name
    Serial.printf("Picture file name: %s\n", FILE_PHOTO);
    File file = SPIFFS.open(FILE_PHOTO, FILE_WRITE);

    // Insert the data in the photo file
    if (!file) {
      Serial.println("Failed to open file in writing mode");
    }
    else {
      file.write(fb->buf, fb->len); // payload (image), payload length
      Serial.print("The picture has been saved in ");
      Serial.print(FILE_PHOTO);
      Serial.print(" - Size: ");
      Serial.print(file.size());
      Serial.println(" bytes");
    }
    // Close the file
    file.close();
    esp_camera_fb_return(fb);

    // check if file has been correctly saved in SPIFFS
    ok = checkPhoto(SPIFFS);
  } while ( !ok );
}
