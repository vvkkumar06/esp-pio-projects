#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <Audio.h>

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

WebServer server(80);
String pendingText = ""; // text from webpage
String askGroq(String question);
void handleRoot()
{
  String html = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
    <meta charset="UTF-8">
    <title>ESP32 TTS Speaker</title>
    <style>
      body {
        font-family: Arial, sans-serif;
        background: linear-gradient(to right, #4facfe, #00f2fe);
        color: #333;
        text-align: center;
        padding: 50px;
      }
      h1 {
        color: white;
        margin-bottom: 20px;
      }
      textarea {
        width: 80%;
        height: 100px;
        padding: 10px;
        font-size: 16px;
        border-radius: 10px;
        border: none;
        box-shadow: 0 4px 6px rgba(0,0,0,0.2);
      }
      button {
        margin-top: 20px;
        padding: 12px 30px;
        font-size: 18px;
        border: none;
        border-radius: 25px;
        background: #ff7eb3;
        color: white;
        cursor: pointer;
        box-shadow: 0 4px 6px rgba(0,0,0,0.3);
        transition: transform 0.2s;
      }
      button:hover {
        transform: scale(1.05);
        background: #ff4081;
      }
    </style>
  </head>
  <body>
    <h1>Talking ESP32 🎤</h1>
    <form action="/speak" method="GET">
      <textarea name="text" placeholder="Ask me anything"></textarea><br>
      <button type="submit">Speak</button>
    </form>
  </body>
  </html>
  )rawliteral";

  server.send(200, "text/html", html);
}

void handleSpeak()
{
  if (server.hasArg("text"))
  {
    pendingText = askGroq(server.arg("text"));
    Serial.println("Received from Web: " + pendingText);
  }
  server.sendHeader("Location", "/");
  server.send(303); // Redirect back to home
}

// --- TTS Function ---
void playText(String text) {
  int maxChunkSize = 100;   // safer limit for Google TTS
  int startPos = 0;

  while (startPos < text.length()) {
    int endPos = min(startPos + maxChunkSize, (int)text.length());
    String chunk = text.substring(startPos, endPos);
    chunk.replace(" ", "%20");

    String tts_url = "http://translate.google.com/translate_tts?ie=UTF-8&q="
                     + chunk + "&tl=en&client=tw-ob";

    Serial.print("Fetching TTS: ");
    Serial.println(chunk);

    // stop any previous playback
    if (audio.isRunning()) {
      audio.stopSong();
      delay(100);
    }

    // retry mechanism
    bool success = false;
    for (int i = 0; i < 3; i++) {
      if (audio.connecttohost(tts_url.c_str())) {
        success = true;
        break;
      }
      Serial.println("Retrying TTS fetch...");
      delay(300);
    }

    if (success) {
      isPlaying = true;
      // let audio run until this chunk finishes
      while (audio.isRunning()) {
        audio.loop();
      }
    } else {
      Serial.println("Failed to fetch TTS after retries.");
    }

    startPos = endPos;
    delay(200);  // small gap between chunks
  }
}

// --- Ask Groq LLM ---
String askGroq(String question)
{
  HTTPClient http;
  http.begin(groq_url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + String(groq_api_key));

  // request body (must include max_tokens + temperature)
  String body = "{";
  body += "\"model\":\"llama-3.3-70b-versatile\",";
  body += "\"messages\":[{\"role\":\"user\",\"content\":\"" + question + "\"}],";
  body += "\"temperature\":0.7,";
  body += "\"max_tokens\":256";
  body += "}";

  int httpResponseCode = http.POST(body);
  String answer = "";

  if (httpResponseCode == 200)
  {
    String payload = http.getString();
    Serial.println("Groq Response: " + payload);

    DynamicJsonDocument doc(8192); // bigger buffer
    DeserializationError error = deserializeJson(doc, payload);

    if (!error)
    {
      answer = doc["choices"][0]["message"]["content"].as<String>();
    }
    else
    {
      Serial.println("JSON parse failed!");
    }
  }
  else
  {
    Serial.printf("Error from Groq: %d\n", httpResponseCode);
    Serial.println(http.getString());
  }

  http.end();
  return answer;
}

// --- Setup ---
void setup()
{
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");

   Serial.print("Web server running at http://");
  Serial.println(WiFi.localIP());
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);

  // // test: ask Groq
  // String question = "Who is the Prime Minister of India?";
  // String response = askGroq(question);
  // Serial.println("Groq Answer: " + response);

  // playText(response);
  // Setup routes
  server.on("/", handleRoot);
  server.on("/speak", handleSpeak);
  server.begin();
}

void loop()
{
  server.handleClient();

  if (!isPlaying && pendingText.length() > 0)
  {
    playText(pendingText);
    pendingText = "";
  }

  if (audio.isRunning())
  {
    audio.loop();
  }
  else
  {
    isPlaying = false;
  }
}
bool connectWithRetry(const char* url, int retries = 3) {
  for (int i = 0; i < retries; i++) {
    if (audio.connecttohost(url)) {
      return true;  // success
    }
    Serial.println("Retrying TTS fetch...");
    delay(500);
  }
  return false;  // failed after retries
}
