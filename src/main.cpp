#include "esp_camera.h"
#include <Arduino.h>
#include <math.h>

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
   USER TUNING (VERY IMPORTANT FOR SPG)
   ========================================================= */
// 0 = keep camera auto (AEC/AGC)  -> dễ drift theo nhịp thở
// 1 = lock exposure/gain (khuyến nghị) -> giảm drift, ổn định K(t)
#define LOCK_AUTO_EXPOSURE_GAIN 1

// Manual tuning (chỉ có tác dụng khi LOCK_AUTO_EXPOSURE_GAIN = 1)
// Bạn sẽ cần tự tune 2 giá trị này để: ảnh KHÔNG cháy (255) và cũng không quá tối.
static int CAM_AEC_VALUE = 250; // exposure (thử: 150~600)
static int CAM_AGC_GAIN  = 3;   // gain     (thử: 0~10)

// Debug sáng/bão hoà (0 = tắt để CSV sạch)
#define DEBUG_CAM 0

/* =========================================================
   ROI + FILTER PARAMETERS
   ========================================================= */
const int ROI = 64;          // ROI 64x64

// High-pass cutoff (Hz): suppress breathing (0.1–0.4 Hz) while keeping HR band.
// Recommended range: 0.5–0.7 Hz
const float FC_HP = 0.7f;

// Optional low-pass cutoff (Hz): suppress high-frequency noise while keeping HR.
// Recommended range: 2.5–4.0 Hz
const float FC_LP = 3.0f;

// Moving average window: keep small so HR is not attenuated.
// Recommended: 3–7 samples (at ~25 Hz)
#define MA_N 3

/* =========================================================
   FILTER STATES
   ========================================================= */
static float x_prev = 0.0f;   // previous input (for HPF)
static float hp_prev = 0.0f;  // previous HPF output
static float lp_prev = 0.0f;  // previous LPF output

static float ma_buf[MA_N] = {0};
static int   ma_idx = 0;
static float ma_sum = 0.0f;

/* =========================================================
   1st-ORDER HIGH-PASS FILTER (remove breathing + slow drift)
   y[n] = alpha * (y[n-1] + x[n] - x[n-1])
   alpha = RC / (RC + dt), RC = 1/(2*pi*fc)
   ========================================================= */
static inline float highpass_1st(float x, float dt) {
  float RC = 1.0f / (2.0f * 3.1415926f * FC_HP);
  float alpha = RC / (RC + dt);
  float y = alpha * (hp_prev + x - x_prev);
  x_prev = x;
  hp_prev = y;
  return y;
}

/* =========================================================
   1st-ORDER LOW-PASS FILTER (optional smoothing)
   y[n] = y[n-1] + beta * (x[n] - y[n-1])
   beta = dt / (RC + dt), RC = 1/(2*pi*fc)
   ========================================================= */
static inline float lowpass_1st(float x, float dt) {
  float RC = 1.0f / (2.0f * 3.1415926f * FC_LP);
  float beta = dt / (RC + dt);
  lp_prev = lp_prev + beta * (x - lp_prev);
  return lp_prev;
}

/* =========================================================
   MOVING AVERAGE (small window, running-sum)
   ========================================================= */
static inline float movingAverage(float x) {
  ma_sum -= ma_buf[ma_idx];
  ma_buf[ma_idx] = x;
  ma_sum += x;
  ma_idx = (ma_idx + 1) % MA_N;
  return ma_sum / MA_N;
}

/* =========================================================
   APPLY CAMERA SENSOR SETTINGS (lock exposure/gain)
   ========================================================= */
static void apply_sensor_settings() {
  sensor_t *s = esp_camera_sensor_get();
  if (!s) return;

  // Ensure expected format/size (driver-level)
  if (s->set_pixformat) s->set_pixformat(s, PIXFORMAT_GRAYSCALE);
  if (s->set_framesize) s->set_framesize(s, FRAMESIZE_QQVGA);

#if LOCK_AUTO_EXPOSURE_GAIN
  // Turn OFF auto controllers to reduce low-frequency drift in mean brightness
  if (s->set_gain_ctrl)     s->set_gain_ctrl(s, 0);      // AGC off
  if (s->set_exposure_ctrl) s->set_exposure_ctrl(s, 0);  // AEC off

  // Optional: reduce auto color features (even in grayscale, some pipelines still change)
  if (s->set_whitebal)  s->set_whitebal(s, 0);
  if (s->set_awb_gain)  s->set_awb_gain(s, 0);

  // Apply manual values (tune these!)
  if (s->set_agc_gain)  s->set_agc_gain(s, CAM_AGC_GAIN);
  if (s->set_aec_value) s->set_aec_value(s, CAM_AEC_VALUE);

  // Keep neutral image processing
  if (s->set_brightness) s->set_brightness(s, 0);
  if (s->set_contrast)   s->set_contrast(s, 0);
#endif
}

/* =========================================================
   SETUP
   ========================================================= */
void setup() {
  Serial.begin(115200);
  delay(2000);

  // CSV header (keep clean for Python)
  Serial.println("time_s,K_filt");

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

  // Apply sensor settings (lock AEC/AGC to reduce drift)
  apply_sensor_settings();
}

/* =========================================================
   LOOP
   ========================================================= */
void loop() {
  // --- dt measurement (more robust filters than fixed alpha) ---
  static uint32_t last_us = micros();
  uint32_t now_us = micros();
  float dt = (now_us - last_us) / 1e6f;
  last_us = now_us;
  // fallback if a stall happens
  if (dt <= 0.0f || dt > 0.2f) dt = 0.04f;

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

#if DEBUG_CAM
  uint8_t max_px = 0;
#endif

  for (int y = 0; y < ROI; y++) {
    int row = (y0 + y) * w;
    for (int x = 0; x < ROI; x++) {
      uint8_t px = img[row + (x0 + x)];
      sum    += px;
      sum_sq += px * px;
#if DEBUG_CAM
      if (px > max_px) max_px = px;
#endif
    }
  }

  float mean = sum / count;
  if (mean < 1.0f) mean = 1.0f; // avoid divide-by-zero/extreme noise
  float var  = (sum_sq / count) - (mean * mean);
  if (var < 0) var = 0;
  float std  = sqrtf(var);

  // ---- Speckle contrast K ----
  float K_raw = std / mean;

  // ---- Filtering ----
  // Warm-up: skip first few seconds so filter transients don't pollute the log
  static int warmup = 0;
  warmup++;
  if (warmup < (int)(25 * 5)) { // ~5 s at ~25 Hz
    esp_camera_fb_return(fb);
    delay(40);
    return;
  }

  // HPF to suppress breathing/slow drift
  float k_hp = highpass_1st(K_raw, dt);

  // Optional LPF to suppress high-frequency noise
  float k_lp = lowpass_1st(k_hp, dt);

  // Small moving average
  float K_filt = movingAverage(k_lp);

  // ---- Time ----
  static unsigned long t0 = millis();
  float t = (millis() - t0) / 1000.0f;

  // ---- CSV output ----
  Serial.print(t, 4);
  Serial.print(",");
  Serial.println(K_filt, 6);

#if DEBUG_CAM
  // WARNING: debug line will break strict CSV. Keep DEBUG_CAM=0 for real logging.
  static int dbg = 0;
  if ((++dbg % 50) == 0) {
    Serial.print("# mean=");
    Serial.print(mean, 1);
    Serial.print(" max=");
    Serial.println((int)max_px);
  }
#endif

  esp_camera_fb_return(fb);
  delay(40);   // ~25 Hz
}
