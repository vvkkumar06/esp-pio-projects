#include "BeehiveMonitor.h"
#include <WiFi.h>

BeehiveMonitor::BeehiveMonitor(int dhtPin, int dhtType, int hxDataPin, int hxSckPin, int RxPin, int TxPin) : dht(dhtPin, dhtType), modem(SerialAT), baseClient(modem)
{
    SerialAT.begin(115200, SERIAL_8N1, RxPin, TxPin);
    delay(3000); // Give time for modem to initialize
    scale.begin(hxDataPin, hxSckPin);
}

float BeehiveMonitor::getWeight()
{
    if (!scale.is_ready())
    {
        Serial.println("HX711 not ready");
        return 0;
    }
    return scale.get_units(5); // average 5 readings
}

void BeehiveMonitor::setApexServer(const char *host, int port, const char *path)
{
    apexHost = host;
    apexPort = port;
    apexPath = path;
}

void BeehiveMonitor::begin()
{
    pinMode(OFFLINE_LED, OUTPUT);
    pinMode(ONLINE_LED, OUTPUT);
    digitalWrite(OFFLINE_LED, HIGH);
     digitalWrite(ONLINE_LED, LOW);
    calibrateScale(2);
    initNVS();
    scale.tare();                       // Reset zero
    scale.set_scale(loadCalibration()); // Adjust after calibration
    dht.begin();
    initTime();
    // Whitelist check (fully internal)
    if (!isWhitelisted())
    {
        Serial.println("Device not authorized! Contact admin to whitelist.");
        while (true)
            delay(1000); // Block forever
    }
}

void BeehiveMonitor::sendData(int beehiveId)
{
    // WiFiClientSecure client;
    SSLClient client{&baseClient};
    client.setInsecure(); // Skip certificate validation for simplicity

    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();
    float weight = getWeight();
    if (weight < 0)
        weight = 0;
    if (isnan(temperature) || isnan(humidity))
    {
        Serial.println("Failed to read from DHT sensor!");
        return;
    }

    String payload = buildPayload(beehiveId, temperature, humidity, weight);
    Serial.println("Payload: " + payload);

    HttpClient http(client, apexHost, apexPort);
    http.beginRequest();
    http.post(apexPath);
    http.sendHeader("Content-Type", "application/json");
    http.sendHeader("Content-Length", payload.length());
    http.beginBody();
    http.print(payload);
    http.endRequest();

    int statusCode = http.responseStatusCode();
    String response = http.responseBody();

    Serial.print("Data send status: ");
    Serial.println(statusCode);
    Serial.print("Response: ");
    Serial.println(response);
    // Restart if connection failed
    if (statusCode <= 0)
    {
        digitalWrite(OFFLINE_LED, HIGH); // Indicate offline
        digitalWrite(ONLINE_LED, LOW);
        Serial.println("❌ HTTP/SSL connection failed, restarting ESP32...");
        delay(2000); // give time for logs to flush
        ESP.restart();
    }
}

bool BeehiveMonitor::isWhitelisted()
{
    // WiFiClientSecure client;
    SSLClient client{&baseClient};
    client.setInsecure(); // Skip certificate validation for simplicity
    String mac = WiFi.macAddress();
    HttpClient http(client, whitelistHost, whitelistPort);

    // Build URL with MAC as query parameter
    String pathWithMac = String(whitelistPath) + "?mac=" + mac;

    Serial.println("Checking whitelist for MAC: " + mac);
    http.beginRequest();
    int err = http.get(pathWithMac);
    http.endRequest();
    if (err < 0)
    {
        digitalWrite(OFFLINE_LED, HIGH); // Indicate offline
        digitalWrite(ONLINE_LED, LOW);
        Serial.print("HTTP request failed, error code: ");
        Serial.println(err);
        if (!client.connected())
            Serial.println("Client not connected!");
        Serial.println("Warning: Cannot reach whitelist API, allowing device to operate.");
        return true; // Fail-safe
    }

    int statusCode = http.responseStatusCode();
    String response = http.responseBody();
    Serial.print("HTTP Status Code: ");
    Serial.println(statusCode);

    if (statusCode != 200)
    {
        Serial.println("Failed to check whitelist! Status code not 200.");
        Serial.println("Warning: Allowing device to operate due to API failure.");
        return true; // Fail-safe
    }

    if (response.indexOf("true") != -1)
    { // Netlify API returns {"authorized":true}
        Serial.println("Device authorized ✅");
        digitalWrite(ONLINE_LED, HIGH); // Indicate online
        digitalWrite(OFFLINE_LED, LOW); // Turn off offline LED
        return true;
    }

    Serial.println("Device not found in whitelist ❌");
    return false;
}

String BeehiveMonitor::buildPayload(int beehiveId, float temperature, float humidity, float weight)
{
    String json = "{";
    json += "\"BEEHIVE_ID\":" + String(beehiveId) + ",";
    json += "\"TEMPERATURE\":" + String(temperature, 1) + ",";
    json += "\"HUMIDITY\":" + String(humidity, 1) + ",";
    json += "\"WEIGHT\":" + String(weight, 2) + ",";
    json += "\"CREATED_DATE\":\"" + getCurrentTime() + "\"";
    json += "}";
    return json;
}

void BeehiveMonitor::initTime()
{
    Serial.println("⏱ Requesting time from GSM modem...");

    // Send AT+CCLK? to modem
    SerialAT.println("AT+CCLK?");
    delay(500);

    String response = "";
    while (SerialAT.available())
    {
        response += SerialAT.readString();
    }

    // Expected format: +CCLK: "25/09/18,19:45:30+22"
    int start = response.indexOf("\"");
    int end = response.lastIndexOf("\"");

    if (start == -1 || end == -1)
    {
        Serial.println("🚫 Failed to read time from modem.");
        Serial.println("❌ Restarting esp32...");
        delay(2000); // give time for logs to flush
        ESP.restart();
        return;
    }

    String datetime = response.substring(start + 1, end);
    Serial.println("📡 Modem time: " + datetime);

    // Parse datetime string
    struct tm t = {0};
    t.tm_year = datetime.substring(0, 2).toInt() + 100; // 2000-based
    t.tm_mon = datetime.substring(3, 5).toInt() - 1;
    t.tm_mday = datetime.substring(6, 8).toInt();
    t.tm_hour = datetime.substring(9, 11).toInt();
    t.tm_min = datetime.substring(12, 14).toInt();
    t.tm_sec = datetime.substring(15, 17).toInt();

    time_t now = mktime(&t);
    struct timeval tv = {.tv_sec = now};
    settimeofday(&tv, nullptr);

    // Confirm by printing system time
    struct tm timeinfo;
    if (getLocalTime(&timeinfo))
    {
        Serial.println("✅ Time synced from GSM modem!");
        Serial.printf("📅 ESP32 time: %s", asctime(&timeinfo));
    }
    else
    {
        Serial.println("⚠️ Failed to set ESP32 time.");
    }
}

String BeehiveMonitor::getCurrentTime()
{
    time_t now;
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
        return "1970-01-01T00:00:00Z";
    }
    char buf[30];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
    return String(buf);
}
void BeehiveMonitor::saveCalibration(float factor)
{
    prefs.begin("beehive", false); // namespace, RW mode
    prefs.putFloat("scaleFactor", factor);
    prefs.end();
    Serial.println("✅ Calibration saved!");
}

float BeehiveMonitor::loadCalibration()
{
    prefs.begin("beehive", true);                      // read-only mode
    float factor = prefs.getFloat("scaleFactor", 1.0); // default=1.0
    prefs.end();
    Serial.printf("📥 Loaded calibration: %.2f\n", factor);
    return factor;
}
void BeehiveMonitor::calibrateScale(float knownWeightKg)
{
    pinMode(calibPin, INPUT_PULLUP); // Calibration pin
    pinMode(CALIB_LED, OUTPUT);
    digitalWrite(CALIB_LED, LOW); // LED off by default
    if (digitalRead(calibPin) == HIGH)
    {
        digitalWrite(CALIB_LED, HIGH); // LED ON during calibration
        scale.tare();                  // zero with no weight
        Serial.println("Place the known weight on the scale...");
        delay(10000); // Give time to stabilize

        long raw = scale.read_average(10); // average 10 readings
        float factor = raw / knownWeightKg;

        scale.set_scale(factor); // set calibration factor
        scale.tare();            // reset zero again
        saveCalibration(factor); // persist calibration
        Serial.println("Calibration done! Factor: " + String(factor));
        digitalWrite(CALIB_LED, LOW); // LED OFF after calibratio
    }
    else
    {
        Serial.println("📥 Loading stored calibration...");
        scale.set_scale(loadCalibration());
        scale.tare();
    }
}

void BeehiveMonitor::initializeModem(const char *apn)
{
    SerialMon.println("Initializing modem...");
    if (!modem.init() || !modem.restart())
    {
        SerialMon.println("Modem init failed");
        Serial.println("❌ Restarting esp32...");
        delay(2000); // give time for logs to flush
        ESP.restart();
        return;
    }

    SerialMon.println("Waiting for network...");
    if (!modem.waitForNetwork())
    {
        SerialMon.println("Network not found");
        Serial.println("❌ Restarting esp32...");
        delay(2000); // give time for logs to flush
        ESP.restart();
        return;
    }

    SerialMon.print("Connecting to GPRS...");
    if (!modem.gprsConnect(apn, "", ""))
    {
        SerialMon.println(" failed");
        return;
    }
    SerialMon.println(" success");
}

// Static method to init NVS
void BeehiveMonitor::initNVS()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        Serial.println("⚠️ NVS partition error, erasing...");
        nvs_flash_erase();
        err = nvs_flash_init();
    }

    if (err == ESP_OK)
    {
        Serial.println("✅ NVS initialized!");
    }
    else
    {
        Serial.printf("❌ NVS init failed: %s\n", esp_err_to_name(err));
    }
}