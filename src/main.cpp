#include "esp_camera.h"
#include <Arduino.h>

/* =========================================================
   CAMERA PINS – ESP32-S3 N16R8 CAM
   ========================================================= */
#define PWDN_GPIO_NUM    -1
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM    15
#define SIOD_GPIO_NUM    4
#define SIOC_GPIO_NUM    5
#define Y9_GPIO_NUM      16
#define Y8_GPIO_NUM      17
#define Y7_GPIO_NUM      18
#define Y6_GPIO_NUM      12
#define Y5_GPIO_NUM      10
#define Y4_GPIO_NUM      8
#define Y3_GPIO_NUM      9
#define Y2_GPIO_NUM      11
#define VSYNC_GPIO_NUM   6
#define HREF_GPIO_NUM    7
#define PCLK_GPIO_NUM    13

/* =========================================================
   ROI + FILTER PARAMETERS
   ========================================================= */
const int ROI = 64;          // ROI 64x64
const float hp_alpha = 0.95; // high-pass strength
#define MA_N 5               // moving average length

/* =========================================================
   FILTER STATES
   ========================================================= */
float K_prev = 0;
float K_hp = 0;

float ma_buf[MA_N];
int ma_idx = 0;

/* =========================================================
   HIGH-PASS FILTER (remove DC / slow drift)
   ========================================================= */
float highpass(float x) {
  float y = hp_alpha * (K_hp + x - K_prev);
  K_prev = x;
  K_hp = y;
  return y;
}

/* =========================================================
   MOVING AVERAGE (low-pass smoothing)
   ========================================================= */
float movingAverage(float x) {
  ma_buf[ma_idx] = x;
  ma_idx = (ma_idx + 1) % MA_N;

  float sum = 0;
  for (int i = 0; i < MA_N; i++) sum += ma_buf[i];
  return sum / MA_N;
}

/* =========================================================
   SETUP
   ========================================================= */
void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("time_s,K_filt");   // CSV header

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;

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
  config.pin_href  = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;

  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_GRAYSCALE;
  config.frame_size   = FRAMESIZE_QQVGA; // 160x120
  config.fb_count     = 2;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: %s\n", esp_err_to_name(err));
    while (1);
  }
}

/* =========================================================
   LOOP
   ========================================================= */
void loop() {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) return;

  uint8_t* img = fb->buf;
  int w = fb->width;   // 160
  int h = fb->height;  // 120

  // ---- ROI center ----
  int x0 = (w - ROI) / 2;
  int y0 = (h - ROI) / 2;

  float sum = 0;
  float sum_sq = 0;
  int count = ROI * ROI;

  for (int y = 0; y < ROI; y++) {
    int row = (y0 + y) * w;
    for (int x = 0; x < ROI; x++) {
      uint8_t px = img[row + (x0 + x)];
      sum    += px;
      sum_sq += px * px;
    }
  }

  float mean = sum / count;
  float var  = (sum_sq / count) - (mean * mean);
  if (var < 0) var = 0;
  float std  = sqrtf(var);

  // ---- Speckle contrast K ----
  float K_raw = std / mean;

  // ---- Filtering ----
  float K_hp_out = highpass(K_raw);
  float K_filt   = movingAverage(K_hp_out);

  // ---- Time ----
  static unsigned long t0 = millis();
  float t = (millis() - t0) / 1000.0f;

  // ---- CSV output ----
  Serial.print(t, 4);
  Serial.print(",");
  Serial.println(K_filt, 6);

  esp_camera_fb_return(fb);
  delay(40);   // ~25 Hz
}
