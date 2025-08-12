#include <Wire.h>
#include "Adafruit_VEML6070.h"
#include <WiFi.h>
#include "esp_camera.h"
#include <HTTPClient.h>
#include <WebServer.h>

#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

// Wi-Fi
const char *ssid = "효진";
const char *password = "hyojin!!";

// UV 센서
Adafruit_VEML6070 uvSensor = Adafruit_VEML6070();
#define UV_LED_PIN 2

// JWT
String jwtToken = "";

// WebServer
WebServer server(80);

// 선언
void handleSetToken();
void handleCameraStream();
void handleCaptureRoute();
bool captureAndUploadImage();
String getUVState(uint16_t uvReading);
void sendUVToServer(uint16_t uvReading, String state);

void setup() {
  Serial.begin(115200);
  Serial.println("\n[시스템 시작]");

  pinMode(UV_LED_PIN, OUTPUT);
  digitalWrite(UV_LED_PIN, LOW);

  Wire.begin(14, 15); // SDA, SCL
  uvSensor.begin(VEML6070_1_T);

  // 카메라 설정
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size   = FRAMESIZE_QVGA;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode    = CAMERA_GRAB_LATEST;
  config.fb_location  = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count     = 1;

  if (psramFound()) {
    config.jpeg_quality = 10; // 더 좋은 화질
    config.fb_count = 2;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("카메라 초기화 실패: 0x%x\n", err);
    return;
  }

  sensor_t *s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_VGA);
  s->set_quality(s, 15);
  s->set_brightness(s, 1);
  s->set_contrast(s, 2);
  s->set_saturation(s, 1);
  s->set_whitebal(s, 1);
  s->set_gain_ctrl(s, 1);

  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  Serial.print("Wi-Fi 연결 중");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi 연결 완료!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  // 라우팅
  server.on("/set-token", HTTP_ANY, handleSetToken);
  server.on("/stream", HTTP_GET, handleCameraStream);
  server.on("/capture", HTTP_GET, handleCaptureRoute);
  server.begin();
  Serial.println("/set-token, /stream, /capture 준비됨");
}

void loop() {
  uint16_t uvReading = uvSensor.readUV();
  Serial.print("UV Reading: ");
  Serial.println(uvReading);

  String state = getUVState(uvReading);
  sendUVToServer(uvReading, state);

  // 기준 이상일 때 자동 업로드 (HTTP 응답은 보내지 않음)
  if (uvReading > 2 && jwtToken.length()) {
    bool ok = captureAndUploadImage();
    Serial.println(ok ? "업로드 성공" : "업로드 실패");
  } else {
    Serial.println("UV 낮음 또는 토큰 없음 → 업로드 생략");
  }

  server.handleClient();
  delay(1000);
}

// 라우트 핸들러: 수동 캡처
void handleCaptureRoute() {
  if (!jwtToken.length()) {
    server.send(401, "text/plain", "No token");
    return;
  }
  bool ok = captureAndUploadImage();
  if (OK) server.send(200, "text/plain", "OK");
  else server.send(500, "text/plain", "Upload failed");
}

bool captureAndUploadImage() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb || fb->format != PIXFORMAT_JPEG) {
    if (fb) esp_camera_fb_return(fb);
    Serial.println("캡처 실패");
    return false;
  }

  HTTPClient http;
  http.setTimeout(12000); // 타임아웃 여유
  http.begin("http://43.202.221.1:8080/api/image/upload");
  http.addHeader("Content-Type", "application/octet-stream");
  http.addHeader("Authorization", "Bearer " + jwtToken);

  int code = http.POST(fb->buf, fb->len);
  String body = http.getString();
  http.end();
  esp_camera_fb_return(fb);

  Serial.printf("HTTP %d\n", code);
  if (body.length()) Serial.println(body);

  return (code > 0 && code < 300);
}

void handleSetToken() {
  if (server.method() == HTTP_OPTIONS) {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
    server.send(204);
    return;
  }

  if (server.method() == HTTP_POST) {
    String body = server.arg("plain");
    int start = body.indexOf(":\"") + 2;
    int end = body.indexOf("\"", start);
    jwtToken = body.substring(start, end);

    Serial.println("받은 토큰:");
    Serial.println(jwtToken);

    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
    server.send(200, "application/json", "{\"message\":\"Token received\"}");
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
      Serial.println("프레임 캡처 실패");
      continue;
    }
    client.print("--frame\r\n");
    client.print("Content-Type: image/jpeg\r\n");
    client.print("Content-Length: " + String(fb->len) + "\r\n\r\n");
    client.write(fb->buf, fb->len);
    client.print("\r\n");
    esp_camera_fb_return(fb);
    delay(100);
  }
}

String getUVState(uint16_t uvReading) {
  if (uvReading <= 3) return "건성";
  else if (uvReading <= 10) return "보통";
  else return "지성";
}

void sendUVToServer(uint16_t uvReading, String state) {
  if (WiFi.status() != WL_CONNECTED || !jwtToken.length()) return;

  HTTPClient http;
  http.begin("http://43.202.221.1:8080/api/uv");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + jwtToken);

  String payload = String("{\"uv\":") + String(uvReading) +
                   ",\"state\":\"" + state +
                   "\",\"deviceId\":\"esp32-cam-01\"}";
  int code = http.POST(payload);
  if (code <= 0) {
    Serial.printf("UV 전송 실패: %s\n", http.errorToString(code).c_str());
  }
  http.end();
}
