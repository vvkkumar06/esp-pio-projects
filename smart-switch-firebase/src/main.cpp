
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
#include <WiFiManager.h>
const long gmtOffset_sec = 19800; // +5:30 IST in seconds
const int daylightOffset_sec = 0; // No daylight saving in India

const char *ntpServer = "pool.ntp.org";

struct SwitchRelay
{
  String path;
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

SwitchRelay switchRelays[] = {
    {"/switch_1", false},
    {"/switch_2", false},
    {"/switch_3", false},
    {"/switch_4", false},
    {"/switch_5", false},
    {"/switch_6", false},
    {"/switch_7", false},
    {"/switch_8", false}};
#define WIFI_RESET_PIN D4
uint8_t relayStates = 0b11111111; // All relays OFF (assuming active LOW relays)
#define LATCH_PIN D1              // ST_CP
#define CLOCK_PIN D2              // SH_CP
#define DATA_PIN D3               // DS

#define OFFLINE_MODE D5
#define ONLINE_MODE D6
FirebaseJsonData result;

void setupRelayPins();
void changeRelayState(const String &path, bool state);
bool getTimeFromNTP();
void updateShiftRegister(bool reset = false);
void syncStatesToFirebase();
// Declaration (prototype)
uint32_t get_ntp_server_time();
void setFirebaseTimeSafelyWithRetry(uint8_t maxRetries = 5);

WiFiManager wm;

bool isOnline = false; // false for offline, true for online

void indicateOffline()
{
  digitalWrite(OFFLINE_MODE, HIGH);
  digitalWrite(ONLINE_MODE, LOW);
  isOnline = false;
}

void indicateOnline()
{
  digitalWrite(OFFLINE_MODE, LOW);
  digitalWrite(ONLINE_MODE, HIGH);
  syncStatesToFirebase();
  isOnline = true;
}
void setup()
{
  Serial.begin(115200);
  pinMode(WIFI_RESET_PIN, INPUT_PULLUP);
  pinMode(OFFLINE_MODE, OUTPUT);
  pinMode(ONLINE_MODE, OUTPUT);
  indicateOffline(); // Start in offline mode

  // Check if button is pressed during boot
  if (digitalRead(WIFI_RESET_PIN) == LOW)
  {
    Serial.println("WiFi reset button held down — clearing credentials...");
    wm.resetSettings(); // Clear saved Wi-Fi
    delay(1000);
    ESP.restart();
  }

  setupRelayPins();

  if (!wm.autoConnect("SmartSwitch-Setup", "admin@123"))
  {
    Serial.println("Failed to connect, restarting...");
    delay(3000);
    ESP.restart();
  }

  Serial.print("Connecting to Wi-Fi");

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("\nWiFi connected");
    Serial.println();
    Serial.print("Connected to Wi-Fi. IP Address: ");
    Serial.println(WiFi.localIP());
    if (getTimeFromNTP())
    {
      Serial.println("Connecting to Firebase...");
      Serial.println("Initializing app...");

      set_ssl_client_insecure_and_buffer(ssl_client);

      setFirebaseTimeSafelyWithRetry();

      initializeApp(aClient, app, getAuth(sa_auth), auth_debug_print, "🔐 authTask");

      app.getApp<RealtimeDatabase>(rtdb);
      rtdb.url(FIREBASE_REALTIME_DATABASE_URL);
      indicateOnline();
    }
    else
    {
      Serial.println("NTP failed, switching to offline");
      indicateOffline();
    }
  }
  else
  {
    Serial.println("\nWiFi failed, switching to offline");
    indicateOffline();
  }
}

void loop()
{
  if (digitalRead(WIFI_RESET_PIN) == LOW)
  {
    Serial.println("WiFi reset button held down — clearing credentials...");
    wm.resetSettings(); // Clear saved Wi-Fi
    delay(1000);
    ESP.restart();
  }
  // To maintain the authentication and async tasks.
  app.loop();

  if (app.ready())
  {
    // Await call which waits until the result was received.
    FirebaseJson json = rtdb.get<String>(aClient, SWITCHBOARD_PATH);
    if (aClient.lastError().code() == 0)
    {
      if (!isOnline)
      {
        indicateOnline(); // Switch to online mode if we successfully fetched data
        json = rtdb.get<String>(aClient, SWITCHBOARD_PATH);
      }
      size_t len = json.iteratorBegin();
      String key, value;
      int type;

      for (size_t i = 0; i < len; i++)
      {
        json.iteratorGet(i, type, key, value); // Get key at index i

        if (json.get(result, key) && switchRelays[i].path == "/" + key)
        {
          changeRelayState(switchRelays[i].path, result.to<bool>());
        }
        else
        {
          indicateOffline();
          Serial.print(key);
          Serial.println(F(": (key not found)"));
        }
      }
      json.iteratorEnd(); // Clean up
    }
    else
    {
      indicateOffline();
      Firebase.printf("Error, msg: %s, code: %d\n", aClient.lastError().message().c_str(), aClient.lastError().code());
    }
  }
  else
  {
    indicateOffline();
    Serial.println("App not ready, waiting for next loop...");
  }
}

void setupRelayPins()
{
  pinMode(LATCH_PIN, OUTPUT);
  pinMode(CLOCK_PIN, OUTPUT);
  pinMode(DATA_PIN, OUTPUT);
  updateShiftRegister(true); // Send initial relay states
}

void changeRelayState(const String &path, bool newState)
{
  // Ensure path starts with "/room1/switchboard/switch_"
  const String prefix = "/switch_";
  if (!path.startsWith(prefix))
  {
    Serial.printf("Invalid path format: %s\n", path.c_str());
    return;
  }

  // Extract the switch number
  int relayIndex = path.substring(prefix.length()).toInt() - 1;

  if (relayIndex < 0 || relayIndex >= 8)
  {
    Serial.printf("Invalid relay index extracted from path: %s\n", path.c_str());
    return;
  }

  // Check if state is different before updating
  if (switchRelays[relayIndex].currentState != newState)
  {

    if (newState)
    {
      relayStates &= ~(1 << relayIndex); // Active LOW: turn ON
    }
    else
    {
      relayStates |= (1 << relayIndex); // turn OFF
    }
    switchRelays[relayIndex].currentState = newState;
    delay(100);
    Serial.printf("Switch_%d : %s\n", relayIndex + 1, newState ? "ON" : "OFF");
  }
  updateShiftRegister();
}
bool getTimeFromNTP()
{
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;

  if (!getLocalTime(&timeinfo))
  {
    Serial.println("NTP getLocalTime failed");
    return false;
  }
  return true;
}
void updateShiftRegister(bool reset)
{
  digitalWrite(LATCH_PIN, LOW);
  shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, reset ? 0b11111111 : relayStates);
  digitalWrite(LATCH_PIN, HIGH);
}

void syncStatesToFirebase()
{
  if (!isOnline)
  {
    for (int i = 0; i < len(switchRelays); i++)
    {
      // Pull ONLINE_MODE low before sending
      digitalWrite(ONLINE_MODE, LOW);

      // Build the full Firebase path for this switch
      String fullPath = SWITCHBOARD_PATH + String("/") + switchRelays[i].path;

      // Push ONLINE_MODE high again after data set
      if (rtdb.set(aClient, fullPath, switchRelays[i].currentState))
      {
        digitalWrite(ONLINE_MODE, HIGH);
        Serial.printf("Synced %s: %s\n", switchRelays[i].path.c_str(), switchRelays[i].currentState ? "ON" : "OFF");
      }
      else
      {
        digitalWrite(ONLINE_MODE, HIGH);
        Serial.printf("Failed to sync %s: %s\n", switchRelays[i].path.c_str(), aClient.lastError().message().c_str());
      }

      delay(100); // Delay between updates
    }
  }
}

uint32_t get_ntp_server_time()
{
  uint32_t ts = 0;

  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("WiFi not connected. Cannot get NTP time.");
    return 0;
  }

  Serial.print("Getting time from NTP server... ");

#if defined(ESP8266) || defined(ESP32)
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  int retry = 0;
  while (time(nullptr) < 1000000000 && retry < 20)
  {
    delay(500);
    Serial.print(".");
    retry++;
  }

  ts = time(nullptr);
#elif __has_include(<WiFiNINA.h>) || __has_include(<WiFi101.h>)
  ts = WiFi.getTime();
#endif

  if (ts > 0)
    Serial.println(" success");
  else
    Serial.println(" failed");

  return ts;
}
void setFirebaseTimeSafelyWithRetry(uint8_t maxRetries)
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("WiFi not connected. Cannot set Firebase time.");
    return;
  }

  uint8_t attempt = 0;
  bool success = false;
  uint32_t ts = 0;

  while (attempt < maxRetries && !success)
  {
    Serial.printf("⏱️  NTP Sync Attempt %d/%d\n", attempt + 1, maxRetries);
    ts = get_ntp_server_time();

    if (ts > 0)
    {
      app.setTime(ts);
      Serial.printf("✅ NTP time set for Firebase: %lu\n", ts);
      success = true;
    }
    else
    {
      Serial.println("❌ Failed to get NTP time. Retrying...");
      delay(2000); // optional backoff
    }

    attempt++;
  }

  if (!success)
  {
    Serial.println("🚫 Giving up: could not set NTP time after retries.");
  }
}
