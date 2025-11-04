// #include <Wire.h>
// #include "Adafruit_VEML6070.h"
// #include <WiFi.h>
// #include "esp_camera.h"
// #include <HTTPClient.h>
// #include <WebServer.h>

// #define CAMERA_MODEL_AI_THINKER
// #include "camera_pins.h"

// // Wi-Fi
// const char *ssid = "효진";
// const char *password = "hyojin!!";

// // UV 센서
// Adafruit_VEML6070 uvSensor = Adafruit_VEML6070();
// #define UV_LED_PIN 2

// // JWT
// String jwtToken = "";

// // WebServer
// WebServer server(80);

// // 선언
// void handleSetToken();
// void handleCameraStream();
// void handleCaptureRoute();
// bool captureAndUploadImage();
// String getUVState(uint16_t uvReading);
// void sendUVToServer(uint16_t uvReading, String state);

// void setup() {
//   Serial.begin(115200);
//   Serial.println("\n[시스템 시작]");

//   pinMode(UV_LED_PIN, OUTPUT);
//   digitalWrite(UV_LED_PIN, LOW);

//   Wire.begin(14, 15); // SDA, SCL
//   uvSensor.begin(VEML6070_1_T);

//   // 카메라 설정
//   camera_config_t config;
//   config.ledc_channel = LEDC_CHANNEL_0;
//   config.ledc_timer   = LEDC_TIMER_0;
//   config.pin_d0       = Y2_GPIO_NUM;
//   config.pin_d1       = Y3_GPIO_NUM;
//   config.pin_d2       = Y4_GPIO_NUM;
//   config.pin_d3       = Y5_GPIO_NUM;
//   config.pin_d4       = Y6_GPIO_NUM;
//   config.pin_d5       = Y7_GPIO_NUM;
//   config.pin_d6       = Y8_GPIO_NUM;
//   config.pin_d7       = Y9_GPIO_NUM;
//   config.pin_xclk     = XCLK_GPIO_NUM;
//   config.pin_pclk     = PCLK_GPIO_NUM;
//   config.pin_vsync    = VSYNC_GPIO_NUM;
//   config.pin_href     = HREF_GPIO_NUM;
//   config.pin_sccb_sda = SIOD_GPIO_NUM;
//   config.pin_sccb_scl = SIOC_GPIO_NUM;
//   config.pin_pwdn     = PWDN_GPIO_NUM;
//   config.pin_reset    = RESET_GPIO_NUM;
//   config.xclk_freq_hz = 20000000;
//   config.frame_size   = FRAMESIZE_QVGA;
//   config.pixel_format = PIXFORMAT_JPEG;
//   config.grab_mode    = CAMERA_GRAB_LATEST;
//   config.fb_location  = CAMERA_FB_IN_PSRAM;
//   config.jpeg_quality = 12;
//   config.fb_count     = 1;

//   if (psramFound()) {
//     config.jpeg_quality = 10; // 더 좋은 화질
//     config.fb_count = 2;
//   }

//   esp_err_t err = esp_camera_init(&config);
//   if (err != ESP_OK) {
//     Serial.printf("카메라 초기화 실패: 0x%x\n", err);
//     return;
//   }

//   sensor_t *s = esp_camera_sensor_get();
//   s->set_framesize(s, FRAMESIZE_VGA);
//   s->set_quality(s, 15);
//   s->set_brightness(s, 1);
//   s->set_contrast(s, 2);
//   s->set_saturation(s, 1);
//   s->set_whitebal(s, 1);
//   s->set_gain_ctrl(s, 1);

//   WiFi.begin(ssid, password);
//   WiFi.setSleep(false);
//   Serial.print("Wi-Fi 연결 중");
//   while (WiFi.status() != WL_CONNECTED) {
//     delay(500);
//     Serial.print(".");
//   }
//   Serial.println("\nWi-Fi 연결 완료!");
//   Serial.print("IP: ");
//   Serial.println(WiFi.localIP());

//   // 라우팅
//   server.on("/set-token", HTTP_ANY, handleSetToken);
//   server.on("/stream", HTTP_GET, handleCameraStream);
//   server.on("/capture", HTTP_GET, handleCaptureRoute);
//   server.begin();
//   Serial.println("/set-token, /stream, /capture 준비됨");
// }

// void loop() {
//   uint16_t uvReading = uvSensor.readUV();
//   Serial.print("UV Reading: ");
//   Serial.println(uvReading);

//   String state = getUVState(uvReading);
//   sendUVToServer(uvReading, state);

//   // 기준 이상일 때 자동 업로드 (HTTP 응답은 보내지 않음)
//   if (uvReading > 2 && jwtToken.length()) {
//     bool ok = captureAndUploadImage();
//     Serial.println(ok ? "업로드 성공" : "업로드 실패");
//   } else {
//     Serial.println("UV 낮음 또는 토큰 없음 → 업로드 생략");
//   }

//   server.handleClient();
//   delay(1000);
// }

// // 라우트 핸들러: 수동 캡처
// void handleCaptureRoute() {
//   if (!jwtToken.length()) {
//     server.send(401, "text/plain", "No token");
//     return;
//   }
//   bool ok = captureAndUploadImage();
//   if (OK) server.send(200, "text/plain", "OK");
//   else server.send(500, "text/plain", "Upload failed");
// }

// bool captureAndUploadImage() {
//   camera_fb_t *fb = esp_camera_fb_get();
//   if (!fb || fb->format != PIXFORMAT_JPEG) {
//     if (fb) esp_camera_fb_return(fb);
//     Serial.println("캡처 실패");
//     return false;
//   }

//   HTTPClient http;
//   http.setTimeout(12000); // 타임아웃 여유
//   http.begin("http://43.202.221.1:8080/api/image/upload");
//   http.addHeader("Content-Type", "application/octet-stream");
//   http.addHeader("Authorization", "Bearer " + jwtToken);

//   int code = http.POST(fb->buf, fb->len);
//   String body = http.getString();
//   http.end();
//   esp_camera_fb_return(fb);

//   Serial.printf("HTTP %d\n", code);
//   if (body.length()) Serial.println(body);

//   return (code > 0 && code < 300);
// }

// void handleSetToken() {
//   if (server.method() == HTTP_OPTIONS) {
//     server.sendHeader("Access-Control-Allow-Origin", "*");
//     server.sendHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
//     server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
//     server.send(204);
//     return;
//   }

//   if (server.method() == HTTP_POST) {
//     String body = server.arg("plain");
//     int start = body.indexOf(":\"") + 2;
//     int end = body.indexOf("\"", start);
//     jwtToken = body.substring(start, end);

//     Serial.println("받은 토큰:");
//     Serial.println(jwtToken);

//     server.sendHeader("Access-Control-Allow-Origin", "*");
//     server.sendHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
//     server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
//     server.send(200, "application/json", "{\"message\":\"Token received\"}");
//   } else {
//     server.sendHeader("Access-Control-Allow-Origin", "*");
//     server.sendHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
//     server.send(405, "text/plain", "Method Not Allowed");
//   }
// }

// void handleCameraStream() {
//   WiFiClient client = server.client();
//   String response = "HTTP/1.1 200 OK\r\nContent-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
//   client.print(response);

//   while (client.connected()) {
//     camera_fb_t *fb = esp_camera_fb_get();
//     if (!fb) {
//       Serial.println("프레임 캡처 실패");
//       continue;
//     }
//     client.print("--frame\r\n");
//     client.print("Content-Type: image/jpeg\r\n");
//     client.print("Content-Length: " + String(fb->len) + "\r\n\r\n");
//     client.write(fb->buf, fb->len);
//     client.print("\r\n");
//     esp_camera_fb_return(fb);
//     delay(100);
//   }
// }

// String getUVState(uint16_t uvReading) {
//   if (uvReading <= 3) return "건성";
//   else if (uvReading <= 10) return "보통";
//   else return "지성";
// }

// void sendUVToServer(uint16_t uvReading, String state) {
//   if (WiFi.status() != WL_CONNECTED || !jwtToken.length()) return;

//   HTTPClient http;
//   http.begin("http://43.202.221.1:8080/api/uv");
//   http.addHeader("Content-Type", "application/json");
//   http.addHeader("Authorization", "Bearer " + jwtToken);

//   String payload = String("{\"uv\":") + String(uvReading) +
//                    ",\"state\":\"" + state +
//                    "\",\"deviceId\":\"esp32-cam-01\"}";
//   int code = http.POST(payload);
//   if (code <= 0) {

#include <Wire.h>
#include "Adafruit_VEML6070.h"
#include <WiFi.h>
#include "esp_camera.h"
#include <HTTPClient.h>
#include <WebServer.h>
#include <math.h>

#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

// ---------- 8T 상수(Adafruit 매크로 보강) ----------
#ifndef VEML6070_8_T
#define VEML6070_8_T 3
#endif

// ===================== Wi-Fi =====================
const char *ssid     = "iPhone 16";
const char *password = "lovelove778";

// ===================== UV 센서 =====================
Adafruit_VEML6070 uvSensor = Adafruit_VEML6070();

// ===================== JWT/서버 =====================
String jwtToken = "";
WebServer server(80);

// ===================== 카메라 선언 =====================
void handleSetToken();
void handleCameraStream();
void handleCaptureRoute();
bool captureAndUploadImage();
void handleRecalibrate();

String getUVState(uint16_t uv);
void sendUVToServer(uint16_t uv, String state);

// ===================== UV Adaptive 제어 =====================
enum UV_IT { IT_1T=VEML6070_1_T, IT_2T=VEML6070_2_T, IT_4T=VEML6070_4_T, IT_8T=VEML6070_8_T };
UV_IT currentIT = IT_8T;   // ★ 두피 대응: 최장 적분(최대 감도) 고정

// 통계(베이스라인/분산)
float uvEMA = 0.0f;
const float EMA_ALPHA = 0.10f;   // 주변광 업데이트 완만하게
float meanUV = 0.0f;
float varUV  = 0.0f;
const float VAR_ALPHA = 0.04f;

uint16_t baseUV = 0;

// 오토레인지 임계 (상향만 허용, 하향 금지)
const uint16_t RANGE_HIGH = 380;
const uint16_t RANGE_LOW  = 8;

// 캘리브레이션 샘플
const int CALI_SAMPLES = 50;

// ===== 트리거 로직 =====
// 민감도 상향
const float Z_ON      = 0.4f;  // 1.0 -> 0.4
const float Z_OFF     = 0.6f;  // 이하면 재장전
const float DELTA_ON  = 0.2f;  // 0.6 -> 0.2 (raw - EMA)
const int   CONSEC_ON = 1;     // 2 -> 1 (즉시 발동)
int onCount = 0;

// 에지 상태/휴지기
bool armed = true;
const uint32_t REFRACTORY_MS = 700;
unsigned long  lastTriggerMs = 0;

// HOLD(누르고 있는 상태)에서 전송/업로드 주기
const uint32_t HOLD_SEND_MS   = 500;   // 사진 0.5초마다
const uint32_t HOLD_WINDOW_MS = 4000;  // ★ 첫 트리거 후 최소 이 시간 동안은 계속 업로드
unsigned long  lastHoldSendMs = 0;

// “버튼 눌림 시점 전용” 버스트 측정
const uint8_t  BURST_SAMPLES   = 9;
const uint16_t BURST_GAP_MS    = 3;
const float    BURST_EMA_ALPHA = 0.30f;

// 확인용 미니 버스트(평소 raw==0일 때 눌림 감지 보조)
const uint8_t  MINI_BURST_SAMPLES = 3;
const uint16_t MINI_BURST_GAP_MS  = 2;

// 폴링 주기(아이들)
const uint16_t IDLE_POLL_MS = 80;

// ===================== 유틸 =====================
void setIT(UV_IT it) {
  currentIT = it;
  uvSensor.begin((veml6070_integrationtime_t)it);
  delay(50);
}

// 상향만 허용, 하향 금지(감도 유지)
void autoRange(uint16_t raw) {
  if (raw > RANGE_HIGH && currentIT != IT_8T) {
    if (currentIT == IT_1T) setIT(IT_2T);
    else if (currentIT == IT_2T) setIT(IT_4T);
    else if (currentIT == IT_4T) setIT(IT_8T);
  }
}

void updateStats(float x) {
  meanUV = (1 - VAR_ALPHA) * meanUV + VAR_ALPHA * x;
  float diff = x - meanUV;
  varUV  = (1 - VAR_ALPHA) * varUV + VAR_ALPHA * (diff * diff);
}

uint16_t medianOf(uint16_t *buf, uint8_t n) {
  for (uint8_t i=1; i<n; ++i) {
    uint16_t k = buf[i]; int8_t j = i - 1;
    while (j >= 0 && buf[j] > k) { buf[j+1] = buf[j]; j--; }
    buf[j+1] = k;
  }
  return buf[n/2];
}

// 눌림 순간: 빠른 다중 샘플 → 중앙값 + “눌림 전용 EMA” (버스트 중 IT 고정)
void burstMeasure(uint16_t &medianOut, float &emaOut) {
  uint16_t buf[BURST_SAMPLES];
  float ema = 0.0f;
  for (uint8_t i=0; i<BURST_SAMPLES; ++i) {
    uint16_t r = uvSensor.readUV();
    // autoRange(r); // ★ 버스트 동안 IT 고정(감도 변동 금지)
    if (i == 0) ema = r;
    else ema = (1 - BURST_EMA_ALPHA) * ema + BURST_EMA_ALPHA * r;
    buf[i] = r;
    delay(BURST_GAP_MS);
  }
  medianOut = medianOf(buf, BURST_SAMPLES);
  emaOut = ema;
}

// 확인용 미니 버스트: 하나라도 >0이면 true (눌림 보조 감지)
bool miniProbeAnyPositive() {
  for (uint8_t i=0; i<MINI_BURST_SAMPLES; ++i) {
    uint16_t r = uvSensor.readUV();
    if (r > 0) return true;
    delay(MINI_BURST_GAP_MS);
  }
  return false;
}

// 주변광용 느린 업데이트(아이들 동안만)
void sampleAmbientSlow(uint16_t r) {
  uvEMA = (1 - EMA_ALPHA) * uvEMA + EMA_ALPHA * (float)r;
  updateStats(uvEMA);
}

// ===================== 캘리브레이션 =====================
void calibrateUV() {
  setIT(IT_8T);  // ★ 8T 기준 캘리브레이션
  uint32_t sum = 0;
  for (int i=0; i<CALI_SAMPLES; ++i) {
    uint16_t r = uvSensor.readUV();
    // autoRange(r); // 캘리브레이션에서도 감도 고정
    sum += r;
    delay(20);
  }
  baseUV = sum / CALI_SAMPLES;
  uvEMA  = baseUV;
  meanUV = baseUV;
  varUV  = fmaxf(5.0f, (float)baseUV * 0.2f);
  Serial.printf("[UV Calibrated] base=%u, IT=%d\n", baseUV, (int)currentIT);
}

// ===================== 판정(동적 기준) =====================
String getUVState(uint16_t uv) {
  float sigma = sqrtf(fmaxf(1.0f, varUV));
  float z = ((float)uv - (float)baseUV) / fmaxf(1.0f, sigma);
  if (z <= 1.0f) return "건성";
  else if (z <= 2.5f) return "보통";
  else return "지성";
}

// ===================== SETUP =====================
void setup() {
  Serial.begin(115200);
  Serial.println("\n[시스템 시작]");

  // I2C
  Wire.begin(15, 14);          // SDA=15, SCL=14
  Wire.setClock(100000);
  setIT(IT_8T);                // ★ 8T 시작
  calibrateUV();

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
  if (psramFound()) { config.jpeg_quality = 10; config.fb_count = 2; }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) { Serial.printf("카메라 초기화 실패: 0x%x\n", err); return; }

  sensor_t *s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_VGA);
  s->set_quality(s, 15);
  s->set_brightness(s, 1);
  s->set_contrast(s, 2);
  s->set_saturation(s, 1);
  s->set_whitebal(s, 1);
  s->set_gain_ctrl(s, 1);

  // Wi-Fi
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  Serial.print("Wi-Fi 연결 중");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nWi-Fi 연결 완료!");
  Serial.print("IP: "); Serial.println(WiFi.localIP());

  // 라우팅
  server.on("/set-token", HTTP_ANY, handleSetToken);
  server.on("/stream", HTTP_GET, handleCameraStream);
  server.on("/capture", HTTP_GET, handleCaptureRoute);
  server.on("/uv-calibrate", HTTP_POST, handleRecalibrate);
  server.begin();
  Serial.println("/set-token, /stream, /capture, /uv-calibrate 준비됨");
}

// ===================== LOOP =====================
void loop() {
  server.handleClient();

  uint16_t raw = uvSensor.readUV();
  // autoRange(raw); // ★ 감도 고정

  float sigma_now = sqrtf(fmaxf(1.0f, varUV));
  float z = ((float)raw - (float)baseUV) / fmaxf(1.0f, sigma_now);
  float deltaRise = (float)raw - uvEMA;

  unsigned long now = millis();
  bool refractory = (now - lastTriggerMs) < REFRACTORY_MS;

  // ---------- 첫 트리거 ----------
  bool triggerCond = (z >= Z_ON || deltaRise >= DELTA_ON);

  // ★ fallback: 조건이 약하지만 눌림 의심 시 미니 버스트로 확인 (raw가 계속 0인 환경 대비)
  if (armed && !refractory && !triggerCond) {
    if (miniProbeAnyPositive()) triggerCond = true;
  }

  if (armed && !refractory && triggerCond) {
    onCount++;
    if (onCount >= CONSEC_ON) {
      uint16_t medianUV; float burstEMA;
      burstMeasure(medianUV, burstEMA);

      uvEMA = (1 - 0.05f) * uvEMA + 0.05f * burstEMA;
      updateStats(uvEMA);

      uint16_t uvForDecision = medianUV;   // DB엔 한 자리 정수(raw)
      String state = getUVState(uvForDecision);

      Serial.printf("[TRIGGER] raw=%u med=%u emaBurst=%.2f z=%.2f Δ=%.2f IT=%d state=%s\n",
                    raw, medianUV, burstEMA, z, deltaRise, (int)currentIT, state.c_str());

      // ---- 교체 후 ----
      bool hasSignal = (uvForDecision > 0) || (burstEMA > 0.5f) || (uvEMA > 0.5f);

      if (jwtToken.length()) {
  // UV 데이터는 여전히 uv>0일 때만 전송(정책 유지)
      if (uvForDecision > 0) {
        sendUVToServer(uvForDecision, state);
      } else {
          Serial.println("uv==0 → UV 전송 생략(정책)");
      }

  // 사진 업로드: uv가 0이어도 EMA가 0 아니라면 업로드
    if (hasSignal) {
      bool ok = captureAndUploadImage();
      Serial.println(ok ? "업로드 성공(EMA 신호 기반)" : "업로드 실패");
    } else {
        Serial.println("raw/EMA 모두 0 수준 → 업로드 생략");
    }
    } else {
        Serial.println("토큰 없음 → 전송/업로드 생략");
      }


      // HOLD 상태 진입 준비
      armed = false;
      lastTriggerMs = now;
      lastHoldSendMs = now;
      onCount = 0;
    }
  }
  // ---------- HOLD(누르고 있는 상태) ----------
  else {
    // 주변광 통계 업데이트(가볍게)
    sampleAmbientSlow(raw);

    // ★ 첫 트리거 이후 HOLD_WINDOW_MS 동안은 z와 무관하게 0.5초마다 사진 업로드
    if (!armed) {
      bool inHoldWindow = (now - lastTriggerMs) <= HOLD_WINDOW_MS;

      if (inHoldWindow && (now - lastHoldSendMs >= HOLD_SEND_MS)) {
        uint16_t holdMed; float holdEma;
        burstMeasure(holdMed, holdEma);

        uint16_t uvHold = holdMed;
        String stateHold = getUVState(uvHold);

        // UV는 uv>0일 때만 전송 (원래 정책 유지)
        if (uvHold > 0 && jwtToken.length()) {
          sendUVToServer(uvHold, stateHold);
        }

        // ★ 사진은 uv가 0이어도 0.5초마다 계속 업로드
        if (jwtToken.length()) {
          bool ok = captureAndUploadImage();
          Serial.printf("[HOLD] med=%u ema=%.2f state=%s upload=%s\n",
                        uvHold, holdEma, stateHold.c_str(), ok ? "OK" : "FAIL");
        } else {
          Serial.println("[HOLD] 토큰 없음 → 업로드 생략");
        }

        lastHoldSendMs = now;
      }

      // HOLD 윈도우 끝나면 재장전
      if (!inHoldWindow && !refractory) {
        armed = true;
      }
    }

    Serial.printf("Idle raw=%u ema=%.2f z=%.2f Δ=%.2f IT=%d armed=%d\n",
                  raw, uvEMA, z, deltaRise, (int)currentIT, (int)armed);
  }

  delay(IDLE_POLL_MS);
}

// ===================== 라우트 =====================
void handleCaptureRoute() {
  if (!jwtToken.length()) { server.send(401, "text/plain", "No token"); return; }
  bool ok = captureAndUploadImage();
  if (ok) server.send(200, "text/plain", "OK");
  else    server.send(500, "text/plain", "Upload failed");
}

bool captureAndUploadImage() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb || fb->format != PIXFORMAT_JPEG) {
    if (fb) esp_camera_fb_return(fb);
    Serial.println("캡처 실패");
    return false;
  }
  HTTPClient http;
  http.setTimeout(12000);
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
    int end   = body.indexOf("\"", start);
    jwtToken  = body.substring(start, end);
    Serial.println("받은 토큰:"); Serial.println(jwtToken);

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
    if (!fb) { Serial.println("프레임 캡처 실패"); continue; }
    client.print("--frame\r\n");
    client.print("Content-Type: image/jpeg\r\n");
    client.print("Content-Length: " + String(fb->len) + "\r\n\r\n");
    client.write(fb->buf, fb->len);
    client.print("\r\n");
    esp_camera_fb_return(fb);
    delay(100);
  }
}

void sendUVToServer(uint16_t uv, String state) {
  if (WiFi.status() != WL_CONNECTED || !jwtToken.length()) return;
  HTTPClient http;
  http.begin("http://43.202.221.1:8080/api/uv");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + jwtToken);
  String payload = String("{\"uv\":") + String(uv) +
                   ",\"state\":\"" + state +
                   "\",\"deviceId\":\"esp32-cam-01\"}";
  int code = http.POST(payload);
  if (code <= 0) Serial.printf("UV 전송 실패: %s\n", http.errorToString(code).c_str());
  http.end();
}

void handleRecalibrate() {
  calibrateUV();
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", "{\"message\":\"UV recalibrated\"}");
}
