#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>

// WiFi credentials
const char* ssid = "K-1003";
const char* password = "Ashajha_123";

// AI Thinker GC2145 pin configuration (unchanged)
#define PWDN_GPIO_NUM    32
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM     0
#define SIOD_GPIO_NUM    26
#define SIOC_GPIO_NUM    27

#define Y9_GPIO_NUM      35
#define Y8_GPIO_NUM      34
#define Y7_GPIO_NUM      39
#define Y6_GPIO_NUM      36
#define Y5_GPIO_NUM      21
#define Y4_GPIO_NUM      19
#define Y3_GPIO_NUM      18
#define Y2_GPIO_NUM       5
#define VSYNC_GPIO_NUM   25
#define HREF_GPIO_NUM    23
#define PCLK_GPIO_NUM    22

WebServer server(80);
WiFiClient streamClient;

// 🧠 FPS monitor
unsigned long lastFpsPrint = 0;
int frameCount = 0;

// 🖼️ Setup camera config for GC2145
bool startCamera() {
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
  config.pixel_format = PIXFORMAT_GRAYSCALE;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.grab_mode = CAMERA_GRAB_LATEST;

  if(psramFound()){
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 12;      // lower = faster
    config.fb_count = 3;
  } else {
    config.frame_size = FRAMESIZE_QQVGA;  // smaller if no PSRAM
    config.jpeg_quality = 30;
    config.fb_count = 1;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return false;
  }

  return true;
}

// 🔁 Streaming task in separate FreeRTOS task
void streamTask(void* pvParameters) {
  WiFiClient client = streamClient;

  String response = "HTTP/1.1 200 OK\r\n";
  response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
  client.print(response);

  while (client.connected()) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) continue;

    uint8_t* jpg_buf = nullptr;
    size_t jpg_len = 0;

    if (fb->format != PIXFORMAT_JPEG) {
      bool converted = frame2jpg(fb, 30, &jpg_buf, &jpg_len);  // ✅ lower quality
      esp_camera_fb_return(fb);
      if (!converted) continue;
    } else {
      jpg_buf = fb->buf;
      jpg_len = fb->len;
    }

    client.printf("--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", jpg_len);
    client.write(jpg_buf, jpg_len);
    client.print("\r\n");

    if (fb->format != PIXFORMAT_JPEG) free(jpg_buf);

    frameCount++;
    if (millis() - lastFpsPrint >= 1000) {
      Serial.printf("FPS: %d\n", frameCount);
      frameCount = 0;
      lastFpsPrint = millis();
    }

    delay(1); // ✅ CPU relief + Wi-Fi throughput improvement
  }

  streamClient.stop();
  vTaskDelete(NULL);
}


// Route handler to start stream
void handleStream() {
  streamClient = server.client();
  xTaskCreatePinnedToCore(
    streamTask,
    "streamTask",
    8192,
    NULL,
    1,
    NULL,
    1
  );
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(4, OUTPUT);
  digitalWrite(4, LOW); // Enable flash if needed

  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  Serial.println(WiFi.localIP());
  delay(3000);
  if (!startCamera()) {
    Serial.println("Camera init failed. Halting.");
    while (true) delay(1000);
  }

  server.on("/", []() {
    server.send(200, "text/html", "<html><body><img src='/stream'></body></html>");
  });
  server.on("/stream", HTTP_GET, handleStream);
  server.begin();
  Serial.println("HTTP stream server started");
}

void loop() {
  server.handleClient();
  delay(10); 
}
