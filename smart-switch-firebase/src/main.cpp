
#define ENABLE_SERVICE_AUTH
#define ENABLE_DATABASE

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
// Network and Firebase credentials
#define WIFI_SSID "K-1003"
#define WIFI_PASSWORD "Ashajha_123"

#define DATABASE_URL "https://smart-switch-room1-default-rtdb.firebaseio.com"

// Authentication
#define FIREBASE_PROJECT_ID "smart-switch-room1"
#define FIREBASE_CLIENT_EMAIL "firebase-adminsdk-fbsvc@smart-switch-room1.iam.gserviceaccount.com"
const char PRIVATE_KEY[] PROGMEM = "-----BEGIN PRIVATE KEY-----\nMIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQCgVxV2/XXJm8Cb\nTehKG6EIlTQvYG/tR5CXjBohks6Ig9k4o90/ynJyrvx0+XvmP5ByuC+rz/RQbvLx\nIlYZdAIqjAoVo9entvfyG6aj5HSEYD2/JlaSLQ6RkXvyWpiDTuCVbNyzrISMGoO2\nMTK2PR8AiA77h9hhQMg5f2CS9Inw9KFD6bl9S+ek3gWAs5cM/tU+PyLeOzMdgZsu\n1IJZFXVmMi+nX0k0vP+QxFKTw0FdmbKmE7YdeokoOOWqU7xBYHRPtzoEzIeYOXtB\nPX9c0u58zugvCk8nG3lxK1sL0NXhHs/SSwvaeFDaBI334qFZ2stUHbDWPbKHi57k\n1ZWkRnYpAgMBAAECggEADMiEnJT+lBFH3NnsQCRNxWpn27MbmVZQT9TKrIyQzM9+\nPQbLd++DCmOrGwRc/XFp6cuHeXI97z+5PMJZDPRzy8KYdzs10VhEElhkA8MOCxPF\nOH0s/8B53d80e5D/gdCDFUa1ndmOQ8FFaPwpJ+BdnWQ7lZAyDybkD7l5EJ5QdRZb\nYwwcUlOysNQ9fy/QEeGCZjtZkqi8HvAQ0UssEeBlk+YMOLbtl4nJWRjjm8ertKFG\nYHF8ttBOHU4oKvaj+ngGRxOdAaqweH+0E+cLxmkYrSJB7Y7dKxvIYtPGi1B6ykUu\nsqSuSZB9mf8tveCqfEPXeRLzn4b1rt8u1XmC/1vu9QKBgQDfex//l+PPZ/zJavg2\nz5P4a0N+bhrgzN8Y6dDOxq1NDgw11F2fIdYrtAO6XvaxRlnNcdlKFHLE2zr8HfVP\nx/ya3gntvJrrrE5Yy5osun6HY9T3c959Aghe7QsiC2G52xPqRTntGBZ7fLPRQomc\n7m0/FdTDTF06i3wkflJlB/IapwKBgQC3q+fKiHY0VdJfiVhxKIRgGFLQe/fQMyFr\nIFFKxMzSDJ223AOoe68TnFxp7AcKEvzrJXA2Bx5ba8vp+76OIOePmnJBMgOL/SgB\n5aKN/nJuusMDgV301bRrq9pi42Dx2dmodmmb4Y3SseRS07Rsr1G7O90BmmiduBxU\nTIcvMjWSrwKBgB3qeIUZixhnnjJETIfhz7gQe89/4782DaNjIV2cwPQwrjfCfunf\neLEO/vTC45klhr32wJSnGhn6EvJO/Fi6t7jvgjq95asovLAsSS41pNxw48BgVWc8\nj2xNpRDgnytnBUp2C+QONmw/bD7V/l/wltU8Eeg238AHjg3Ajz0RDDq/AoGBAI/E\nDSDpA60faBXDyeh5EHSvVVM/VdAv1X6mwzrFJJVdrq2NNYfRmE6/W07FoxTtm+7r\nVRPVKpvgmrJBjPxvIRG0kK4bWc9fjss9VanTevrVUQQTZNnZ1OlakQxKcn2cSdSl\nKzEKshoziEaU02snJ9BooSs6E50wmWwaos38fRadAoGBAKJiVaus7QZtn5Lx12oA\nNNbRl0oPE6WoyndLxU130gqjIxnI2nfuG2j6Y4eysuEPbdcxTydOy79eZzexjuJz\n6VP7yYoXhcAng6xn9GmVbW2tl7nOgWOAu9ZLWmeKui7gu0XjsbE7wBJDGGIc6G3b\nhewzm+LhYGUPQik2bc7YYpKs\n-----END PRIVATE KEY-----\n";
ServiceAuth sa_auth(FIREBASE_CLIENT_EMAIL, FIREBASE_PROJECT_ID, PRIVATE_KEY, 3000);

void processData(AsyncResult& aResult);

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
// const int relayPins[8] = { D1, D2, D3, D4, D5, D6, D7, D8 };
// const String switchPaths[8] = {
//   "/switch_1", "/switch_2", "/switch_3", "/switch_4",
//   "/switch_5", "/switch_6", "/switch_7", "/switch_8"
// };

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
  rtdb.url(DATABASE_URL);
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
    digitalWrite(switchRelays[i].pin, LOW);  // Initialize all relays to LOW
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
        digitalWrite(switchRelays[i].pin, newState ? HIGH : LOW);
        Serial.printf("Relay %s set to %s\n", switchRelays[i].path.c_str(), newState ? "ON" : "OFF");
      }
      return;
    }
  }
  Serial.printf("Relay with path %s not found.\n", path.c_str());
}
