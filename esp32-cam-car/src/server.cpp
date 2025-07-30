#include "server.h"
#include "motor-controls.h"
#include "camera-config.h"

// WiFi credentials
const char *ssid = "K-1003";
const char *password = "Ashajha_123";

WebServer server(80);
WiFiClient streamClient;

TaskHandle_t streamTaskHandle = NULL;

void connectWifi()
{
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);

  unsigned long startAttemptTime = millis();
  const unsigned long wifiTimeout = 15000;

  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < wifiTimeout)
  {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("\n✅ WiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }
  else
  {
    Serial.println("\n❌ WiFi connection failed! Restarting...");
    delay(1000);
    ESP.restart();
  }
}

void routes()
{
  // Routes
  server.on("/", []()
            { server.send(200, "text/html", "<html><body><img src='/stream' width='100%' height='100%' ></body></html>"); });
  server.on("/stream", HTTP_GET, handleStream);
  server.on("/status", []()
            {
    Serial.println("[COMMAND] Status");
    server.send(200, "text/plain", "Ok"); });

  server.on("/headlight", []
            {
    Serial.println("[COMMAND] Headlight " + server.arg("val"));

    if (server.arg("val") == "on") {
      digitalWrite(headLight, HIGH);
    }
    if (server.arg("val") == "off") {
      digitalWrite(headLight, LOW);
    } });

  server.on("/engine-on", []()
            {
    Serial.println("[COMMAND] Engine On");
    isEngineRunning = true;
    server.send(200, "text/plain", "Engine On"); });

  server.on("/engine-off", []()
            {
    Serial.println("[COMMAND] Engine Off");
    isEngineRunning = false;
    server.send(200, "text/plain", "Engine Off"); });

  server.on("/forward", []()
            {
    Serial.println("[COMMAND] Forward");
    turnOffIndicators();
    digitalWrite(breakLight, LOW);
    forward();
    server.send(200, "text/plain", "Forward"); });

  server.on("/backward", []()
            {
    Serial.println("[COMMAND] Backward");
    turnOffIndicators();
    digitalWrite(breakLight, HIGH);
    backward();
    server.send(200, "text/plain", "Backward"); });

  server.on("/left", []()
            {
    Serial.println("[COMMAND] Left");
    turnOffIndicators();
    digitalWrite(leftIndicator, HIGH);
    left();
    server.send(200, "text/plain", "Left"); });

  server.on("/right", []()
            {
    Serial.println("[COMMAND] Right");
    turnOffIndicators();
    digitalWrite(rightIndicator, HIGH);
    right();
    server.send(200, "text/plain", "Right"); });

  server.on("/stop", []()
            {
    Serial.println("[COMMAND] Stop");
    digitalWrite(breakLight, HIGH);
    stopCar();
    server.send(200, "text/plain", "Stop"); });

  server.on("/speed", []()
            {
    if (server.hasArg("val")) {
      speedKmph = server.arg("val").toFloat();     // Speed in km/h
      speedVal = map(speedKmph, 0, 100, 0, 1000);  // Convert to PWM
      Serial.print("[SPEED] Set to: ");
      Serial.print(speedKmph);
      Serial.print(" km/h → PWM: ");
      Serial.println(speedVal);
    }
    
    // Respond with the real speed (km/h)
    server.send(200, "text/plain", String(speedKmph)); });
  server.on("/whoami", []()
            { server.send(200, "text/plain", "esp32scar"); });
}

void streamTask(void *pvParameters)
{
  WiFiClient client = *((WiFiClient *)pvParameters);
  ((WiFiClient *)pvParameters)->~WiFiClient(); // Call destructor
  free(pvParameters); // Free the malloc'ed memory

  String response = "HTTP/1.1 200 OK\r\n";
  response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
  client.print(response);

  while (client.connected())
  {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb)
      continue;

    uint8_t *jpg_buf = nullptr;
    size_t jpg_len = 0;

    if (fb->format != PIXFORMAT_JPEG)
    {
      bool converted = frame2jpg(fb, 80, &jpg_buf, &jpg_len);
      esp_camera_fb_return(fb);
      if (!converted)
        continue;
    }
    else
    {
      jpg_buf = fb->buf;
      jpg_len = fb->len;
    }

    client.printf("--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", jpg_len);
    size_t written = client.write(jpg_buf, jpg_len);
    client.print("\r\n");

    if (fb->format != PIXFORMAT_JPEG)
      free(jpg_buf);

    if (written != jpg_len)
      break; // Client disconnected

    delay(1);
  }
  client.stop();
  streamTaskHandle = NULL;
  vTaskDelete(NULL);
}

void handleStream()
{
  // If a stream is already running, stop it
  if (streamTaskHandle != NULL)
  {
    delay(100);
    for (int i = 0; i < 50 && streamTaskHandle != NULL; ++i)
      delay(10);
  }

  // Allocate memory for the client copy and use placement new
  WiFiClient *clientCopy = (WiFiClient *)malloc(sizeof(WiFiClient));
  new (clientCopy) WiFiClient(server.client()); // Placement new: copy-construct

  xTaskCreatePinnedToCore(
      streamTask,
      "streamTask",
      8192,
      clientCopy,
      1,
      &streamTaskHandle,
      0);
}