#include <Wire.h>
#include "Adafruit_VEML6070.h"
#include <WiFi.h>
#include "esp_camera.h"
#include <HTTPClient.h>
#include <WebServer.h>  // HTTP 서버용
#include <base64.h>     // Base64 인코딩을 위해 추가

#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

// --- Wi-Fi 설정 ---
// const char *ssid = "효진";
// const char *password = "hyojin!!";
const char *ssid = "iPhone 16";
const char *password = "lovelove778";

// --- UV 센서 ---
Adafruit_VEML6070 uvSensor = Adafruit_VEML6070();
#define UV_LED_PIN 2

// --- JWT 토큰 저장용 ---
String jwtToken = "";

// --- 웹서버 (포트 80) ---
WebServer server(80);

// --- 함수 선언 ---
void startCameraServer();
void handleSetToken();
String getUVState(uint16_t uvReading);
void sendUVToServer(uint16_t uvReading, String state);
void handleCameraStream();
void captureAndSendImage();  // 이미지를 캡처하고 서버로 전송하는 함수 추가

void setup() {
  Serial.begin(115200);
  Serial.println("\n[시스템 시작]");

  // UV LED 핀 초기화
  pinMode(UV_LED_PIN, OUTPUT);
  digitalWrite(UV_LED_PIN, LOW);

  // UV 센서 I2C 설정
  Wire.begin(14, 15);  // SDA, SCL
  uvSensor.begin(VEML6070_1_T);

  // 카메라 설정
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
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_QVGA;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_LATEST;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  if (psramFound()) {
    config.jpeg_quality = 10;
    config.fb_count = 2;
  }

  // 카메라 초기화
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("카메라 초기화 실패: 0x%x\n", err);
    return;
  }

  // ✅ 카메라 세팅 추가 (이 부분만 삽입)
  sensor_t *s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_VGA);   // 해상도 조절 (800x600)
  s->set_quality(s, 15);                 // 품질 향상
  s->set_brightness(s, 1);               // 밝기 증가
  s->set_contrast(s, 2);                 // 대비 증가
  s->set_saturation(s, 1);               // 채도 증가
  s->set_whitebal(s, 1);                 // 자동 화이트밸런스 ON
  s->set_gain_ctrl(s, 1);                // 자동 게인 ON

  // Wi-Fi 연결
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  Serial.print("Wi-Fi 연결 중");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi 연결 완료!");

  // Wi-Fi 연결된 후 IP 출력
  Serial.print("Wi-Fi 연결된 IP 주소: ");
  Serial.println(WiFi.localIP());  // 추가된 부분

  // HTTP 서버 라우팅
  server.on("/set-token", HTTP_ANY, handleSetToken); // ← 반드시 HTTP_ANY
  server.on("/stream", HTTP_GET, handleCameraStream); // 스트리밍을 위한 라우터 추가
  server.on("/capture", HTTP_GET, captureAndSendImage); // 캡처 및 전송 기능 라우터 추가
  server.begin();
  Serial.println("📡 /set-token HTTP 서버 시작");

  // 카메라 서버 시작
  startCameraServer();
  Serial.print("Camera Ready! 접속 주소: http://");
  Serial.println(WiFi.localIP());
}

// void loop() {
//   uint16_t uvReading = uvSensor.readUV();
//   Serial.print("UV Reading: ");
//   Serial.println(uvReading);

//   String state = getUVState(uvReading);
//   sendUVToServer(uvReading, state);

//   server.handleClient();  // 웹서버 클라이언트 핸들링
//   delay(1000);  // UV 센서 값을 1초 간격으로 보내기 위해 delay 추가
// }
void loop() {
  uint16_t uvReading = uvSensor.readUV();
  Serial.print("UV Reading: ");
  Serial.println(uvReading);

  String state = getUVState(uvReading);
  sendUVToServer(uvReading, state);

  // ✅ 일정 이상일 때만 사진도 전송
  if (uvReading > 2 && jwtToken != "") {
    Serial.println("📸 UV 값이 기준 이상이므로 사진 전송");
    captureAndSendImage();
  } else {
    Serial.println("UV 값이 너무 낮아 사진 저장 생략됨: " + String(uvReading));
  }

  server.handleClient();
  delay(1000);
}

// UV 상태 분류
String getUVState(uint16_t uvReading) {
  if (uvReading <= 3) {
    Serial.println("유분 없음 (건성)");
    return "건성";
  } else if (uvReading <= 10) {
    Serial.println("보통 유분");
    return "보통";
  } else {
    Serial.println("유분 많음 (지성)");
    return "지성";
  }
}

void handleSetToken() {
  if (server.method() == HTTP_OPTIONS) {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
    server.send(204); // No Content
    return;
  }

  if (server.method() == HTTP_POST) {
    String body = server.arg("plain");
    int start = body.indexOf(":\"") + 2;
    int end = body.indexOf("\"", start);
    jwtToken = body.substring(start, end);

    Serial.println("📥 받은 토큰:");
    Serial.println(jwtToken);

    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
    server.send(200, "application/json", "{\"message\": \"Token received\"}");
  } else {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
    server.send(405, "text/plain", "Method Not Allowed");
  }
}

void handleCameraStream() {
  WiFiClient client = server.client();
  String response = "HTTP/1.1 200 OK\r\nContent-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
  client.print(response);

  while (client.connected()) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("카메라 프레임 캡처 실패");
      continue;
    }

    client.print("--frame\r\n");
    client.print("Content-Type: image/jpeg\r\n");
    client.print("Content-Length: " + String(fb->len) + "\r\n\r\n");
    client.write(fb->buf, fb->len);  // JPEG 이미지 전송
    client.print("\r\n");

    esp_camera_fb_return(fb);
    delay(100);  // 일정 시간 간격으로 프레임을 전송하여 스트리밍 효과 구현
  }
}

// 서버로 UV 데이터 전송
void sendUVToServer(uint16_t uvReading, String state) {
  if (WiFi.status() == WL_CONNECTED && jwtToken != "") {
    HTTPClient http;
    http.begin("http://43.202.221.1:8080/api/uv");  // ← 백엔드 서버 주소로 수정

    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Bearer " + jwtToken);

    String jsonPayload = "{\"uv\":" + String(uvReading) +
                         ",\"state\":\"" + state +
                         "\",\"deviceId\":\"esp32-cam-01\"}";

    int responseCode = http.POST(jsonPayload);
    if (responseCode > 0) {
      Serial.println("→ 서버 전송 성공");
      Serial.println(http.getString());
    } else {
      Serial.println("→ 서버 전송 실패: " + http.errorToString(responseCode));
    }

    http.end();
  } else {
    Serial.println("🚫 Wi-Fi 또는 토큰 없음");
  }
}

// void captureAndSendImage() {
//   camera_fb_t *fb = esp_camera_fb_get();

//   if (!fb || fb->format != PIXFORMAT_JPEG) {
//     Serial.println("📸 이미지 캡처 실패");
//     if (fb) esp_camera_fb_return(fb);
//     server.send(500, "text/plain", "캡처 실패");
//     return;
//   }

//   if (WiFi.status() == WL_CONNECTED && jwtToken != "") {
//     HTTPClient http;
//     // http.begin("http://43.202.221.1:8080/api/image/upload");
//     http.begin("http://172.20.10.7:8080/api/image/upload");
//     http.addHeader("Content-Type", "application/octet-stream");
//     http.addHeader("Authorization", "Bearer " + jwtToken);

//     int httpResponseCode = http.POST(fb->buf, fb->len);
//     if (httpResponseCode > 0) {
//       Serial.println("✅ 이미지 업로드 성공");
//       Serial.println(http.getString());
//       server.send(200, "text/plain", "이미지 업로드 성공");
//     } else {
//       Serial.println("❌ 업로드 실패: " + http.errorToString(httpResponseCode));
//       server.send(500, "text/plain", "ESP32 → 서버 업로드 실패");
//     }

//     http.end();
//   } else {
//     Serial.println("🚫 Wi-Fi 또는 토큰 없음");
//     server.send(401, "text/plain", "Wi-Fi 또는 토큰 없음");
//   }

//   esp_camera_fb_return(fb);
// }
void captureAndSendImage() {
  // ⬇️ UV 읽기 추가
  uint16_t uvReading = uvSensor.readUV();
  if (uvReading <= 2) {
    Serial.println("📛 UV 값 낮음. 캡처 생략됨 (" + String(uvReading) + ")");
    server.send(400, "text/plain", "UV 값이 낮아 캡처 생략됨");
    return;
  }

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb || fb->format != PIXFORMAT_JPEG) {
    Serial.println("📸 이미지 캡처 실패");
    if (fb) esp_camera_fb_return(fb);
    server.send(500, "text/plain", "캡처 실패");
    return;
  }

  if (WiFi.status() == WL_CONNECTED && jwtToken != "") {
    HTTPClient http;
    http.begin("http://43.202.221.1:8080/api/image/upload");
    http.addHeader("Content-Type", "application/octet-stream");
    http.addHeader("Authorization", "Bearer " + jwtToken);

    int httpResponseCode = http.POST(fb->buf, fb->len);
    if (httpResponseCode > 0) {
      Serial.println("✅ 이미지 업로드 성공");
      Serial.println(http.getString());
      server.send(200, "text/plain", "이미지 업로드 성공");
    } else {
      Serial.println("❌ 업로드 실패: " + http.errorToString(httpResponseCode));
      server.send(500, "text/plain", "ESP32 → 서버 업로드 실패");
    }

    http.end();
  } else {
    Serial.println("🚫 Wi-Fi 또는 토큰 없음");
    server.send(401, "text/plain", "Wi-Fi 또는 토큰 없음");
  }

  esp_camera_fb_return(fb);
}



