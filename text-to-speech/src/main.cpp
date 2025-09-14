#include <WiFi.h>
#include <Audio.h>  // ESP32 Audio library
#include "HTTPClient.h"

// Wi-Fi credentials
const char* ssid = "K-1003";
const char* password = "Ashajha_123";

// Audio object
Audio audio;

#define I2S_DOUT 15
#define I2S_BCLK 14
#define I2S_LRC 13

void playText(String text);

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");

  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);

  String text = "Hello Vivek! How are you? This code should now compile successfully and play the text to speech(TTS)";
  
  // playText(text);
  audio.connecttohost("http://ice1.somafm.com/u80s-128-mp3");  // internet radio


}

void playText(String text) {
  int maxChunkSize = 50;
  int startPos = 0;

  while (startPos < text.length()) {
    int endPos = min(startPos + maxChunkSize, (int)text.length());

    String chunk = text.substring(startPos, endPos);

    chunk.replace(" ", "%20");

    String tts_url = "http://translate.google.com/translate_tts?ie=UTF-8&q="
                     + chunk + "&tl=en&client=tw-ob";

    audio.connecttohost(tts_url.c_str());

    // Wait until playback finishes
    while (audio.isRunning()) {
      audio.loop();
    }

    startPos = endPos;
    delay(500);
  }
}

void loop(){
         // Wait until playback finishes
    while (audio.isRunning()) {
      audio.loop();
    }
}