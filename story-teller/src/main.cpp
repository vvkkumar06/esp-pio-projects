#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <Audio.h>
#include <vector>
// WiFi credentials
const char *ssid = "K-1003";
const char *password = "Ashajha_123";

// Groq API
const char *groq_api_key = "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";
const char *groq_url = "https://api.groq.com/openai/v1/chat/completions";

// Audio
Audio audio;
#define I2S_DOUT 12
#define I2S_BCLK 14
#define I2S_LRC 13

bool isPlaying = false;
String pendingText = "";
String pendingLang = "en"; // default language

WebServer server(80);

// --- Helper Functions ---
String askGroq(String question) {
  HTTPClient http;
  http.begin(groq_url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + String(groq_api_key));

  String body = "{";
  body += "\"model\":\"llama-3.3-70b-versatile\",";
  body += "\"messages\":[{\"role\":\"user\",\"content\":\"" + question + "\"}],";
  body += "\"temperature\":0.7,";
  body += "\"max_tokens\":256";
  body += "}";

  int httpResponseCode = http.POST(body);
  String answer = "";

  if (httpResponseCode == 200) {
    String payload = http.getString();
    Serial.println("Groq Response: " + payload);

    DynamicJsonDocument doc(8192);
    if (!deserializeJson(doc, payload)) {
      answer = doc["choices"][0]["message"]["content"].as<String>();
    } else {
      Serial.println("JSON parse failed!");
    }
  } else {
    Serial.printf("Error from Groq: %d\n", httpResponseCode);
    Serial.println(http.getString());
  }

  http.end();
  return answer;
}

// URL encode UTF-8 text
String urlEncodeUTF8(const String &text) {
  String encoded = "";
  for (size_t i = 0; i < text.length(); i++) {
    char c = text[i];
    if (isalnum(c)) encoded += c;
    else encoded += "%" + String((uint8_t)c, HEX);
  }
  return encoded;
}


std::vector<String> splitIntoSentences(String text) {
  std::vector<String> sentences;
  String current = "";

  for (int i = 0; i < text.length(); i++) {
    char c = text[i];
    current += c;

    // Check for sentence-ending punctuation
    if (c == '.' || c == '!' || c == '?' || c == '।') { // '।' is Hindi/Bengali danda
      current.trim(); // remove extra spaces
      if (current.length() > 0) sentences.push_back(current);
      current = "";
    }
  }

  // Add any leftover text
  current.trim();
  if (current.length() > 0) sentences.push_back(current);

  return sentences;
}
// --- TTS Function ---
void playText(String text, String lang) {
  std::vector<String> sentences = splitIntoSentences(text);

  for (String chunk : sentences) {
    // Percent-encode UTF-8 characters
    String encodedChunk = "";
    for (int i = 0; i < chunk.length(); i++) {
      char c = chunk[i];
      if (isalnum(c)) encodedChunk += c;
      else {
        char hexStr[4];
        sprintf(hexStr, "%%%.2X", (uint8_t)c);
        encodedChunk += String(hexStr);
      }
    }

    String tts_url = "https://translate.google.com/translate_tts?ie=UTF-8&q="
                     + encodedChunk + "&tl=" + lang + "&client=tw-ob";

    Serial.println("Playing sentence: " + chunk);

    if (audio.isRunning()) audio.stopSong();
    if (audio.connecttohost(tts_url.c_str())) {
      isPlaying = true;
      while (audio.isRunning()) audio.loop();
    } else {
      Serial.println("Failed to fetch TTS sentence");
    }

    delay(100); // small pause between sentences
  }
}

// --- Web Handlers ---
void handleRoot() {
  String html = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
    <meta charset="UTF-8">
    <title>ESP32 Storyteller</title>
    <style>
      body { font-family: Arial; text-align: center; padding: 50px; background: linear-gradient(to right, #4facfe, #00f2fe);}
      textarea { width: 80%; height: 100px; font-size:16px; border-radius:10px; padding:10px;}
      select, button { padding:10px 20px; margin-top:20px; font-size:16px; border-radius:10px;}
    </style>
  </head>
  <body>
    <h1>ESP32 Storyteller 📖</h1>
    <form action="/speak" method="GET">
      <textarea name="text" placeholder="Enter story prompt"></textarea><br>
      <label for="lang">Select Language:</label>
      <select name="lang">
        <option value="en">English</option>
        <option value="hi">Hindi</option>
        <option value="bn">Bengali</option>
        <option value="ta">Tamil</option>
        <option value="te">Telugu</option>
      </select><br>
      <button type="submit">Tell Story</button>
    </form>
  </body>
  </html>
  )rawliteral";

  server.send(200, "text/html", html);
}

void handleSpeak() {
  if (server.hasArg("text") && server.hasArg("lang")) {
    pendingText = askGroq(server.arg("text"));
    pendingLang = server.arg("lang");
    Serial.println("Story fetched: " + pendingText);
    Serial.println("Language: " + pendingLang);
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

// --- Setup ---
void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nWiFi Connected!");
  Serial.print("Web server running at http://"); Serial.println(WiFi.localIP());

  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);

  server.on("/", handleRoot);
  server.on("/speak", handleSpeak);
  server.begin();
}

// --- Loop ---
void loop() {
  server.handleClient();

  if (!isPlaying && pendingText.length() > 0) {
    playText(pendingText, pendingLang);
    pendingText = "";
  }

  if (audio.isRunning()) audio.loop();
  else isPlaying = false;
}
