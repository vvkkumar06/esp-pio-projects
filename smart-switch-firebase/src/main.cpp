
#define ENABLE_SERVICE_AUTH
#define ENABLE_DATABASE
#include "secrets.h"
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <FirebaseClient.h>
#include <ArduinoJson.h>
#include <FirebaseJson.h>
#include "ExampleFunctions.h"


struct SwitchRelay {
  String path;
  int pin;
  bool currentState;
};
#define len(x) (sizeof(x) / sizeof((x)[0]))
// Network and Firebase credentials defined in secrets.h



ServiceAuth sa_auth(FIREBASE_CLIENT_EMAIL, FIREBASE_PROJECT_ID, PRIVATE_KEY, 3000);


// Firebase app
FirebaseApp app;

// Async client
WiFiClientSecure ssl_client;
using AsyncClient = AsyncClientClass;
AsyncClient aClient(ssl_client);

RealtimeDatabase rtdb;

AsyncResult dbResult;

// Relay GPIO pins
const String room = "room1";

SwitchRelay switchRelays[] = {
  {"/switch_1", D1, false},
  {"/switch_2", D2, false},
  {"/switch_3", D3, false},
  {"/switch_4", D4, false},
  {"/switch_5", D5, false},
  {"/switch_6", D6, false},
  {"/switch_7", D7, false},
  {"/switch_8", D8, false}
};

FirebaseJsonData result;

void setupRelayPins();
void changeRelayState(const String& path, bool state);
void setup() {
  Serial.begin(115200);
  setupRelayPins();
  // Connect to Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected to Wi-Fi. IP Address: ");
  Serial.println(WiFi.localIP());

  Serial.println("Connecting to Firebase...");
  Serial.println("Initializing app...");

  set_ssl_client_insecure_and_buffer(ssl_client);

  app.setTime(get_ntp_time());

  initializeApp(aClient, app, getAuth(sa_auth), auth_debug_print, "🔐 authTask");

  app.getApp<RealtimeDatabase>(rtdb);
  rtdb.url(FIREBASE_REALTIME_DATABASE_URL);
}

void loop() {
  // To maintain the authentication and async tasks.
  app.loop();

  if (app.ready()) {
    // Await call which waits until the result was received.
    FirebaseJson json = rtdb.get<String>(aClient, "/room1/switchboard");
    if (aClient.lastError().code() == 0) {
      size_t len = json.iteratorBegin();
      String key, value;
      int type;

      for (size_t i = 0; i < len; i++) {
        json.iteratorGet(i, type, key, value);  // Get key at index i

        if (json.get(result, key) && switchRelays[i].path == "/" + key) {
          changeRelayState(switchRelays[i].path, result.to<bool>());
        } else {
          Serial.print(key);
          Serial.println(F(": (key not found)"));
        }
      }
      json.iteratorEnd();  // Clean up
    } else
      Firebase.printf("Error, msg: %s, code: %d\n", aClient.lastError().message().c_str(), aClient.lastError().code());
  }
}

void setupRelayPins()
{
  for (size_t i = 0; i < len(switchRelays); i++)
  {
    pinMode(switchRelays[i].pin, OUTPUT);
    digitalWrite(switchRelays[i].pin, HIGH);  // Initialize all relays to HIGH (off)
  }
}

void changeRelayState(const String& path, bool newState)
{
  for (size_t i = 0; i < len(switchRelays); i++)
  {
    if (switchRelays[i].path == path)
    {
      if (switchRelays[i].currentState != newState) {
        switchRelays[i].currentState = newState;
        digitalWrite(switchRelays[i].pin, newState ? LOW : HIGH);
        Serial.printf("Relay %s set to %s\n", switchRelays[i].path.c_str(), newState ? "ON" : "OFF");
      }
      return;
    }
  }
  Serial.printf("Relay with path %s not found.\n", path.c_str());
}
