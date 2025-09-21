#define TINY_GSM_MODEM_SIM7600
#include <Arduino.h>
#include <TinyGsmClient.h>
#include <ArduinoHttpClient.h>
#include <SSLClient.h>

#define SerialAT Serial1
#define SerialMon Serial
#define RXD2 27
#define TXD2 26
#define powerPin 4

const char apn[] = ""; // leave blank for auto-detect
const char smsNumber[] = "+919667165303";
const char url[] = "https://beehive-mac-check.netlify.app/.netlify/functions/checkWhitelist?mac=00:4B:12:2F:E5:CC";

TinyGsm modem(SerialAT);
TinyGsmClient baseClient(modem);
SSLClient sslClient(&baseClient);

void sendSMS(String msg)
{
    SerialAT.println("AT+CMGF=1");
    delay(500);
    SerialAT.print("AT+CMGS=\"");
    SerialAT.print(smsNumber);
    SerialAT.println("\"");
    delay(500);
    SerialAT.print(msg);
    delay(500);
    SerialAT.write(26); // CTRL+Z
    delay(5000);
}

void setup()
{
    Serial.begin(115200);

    SerialAT.begin(115200, SERIAL_8N1, RXD2, TXD2);
    delay(10000);

    Serial.println("Initializing modem...");
    if (!modem.init() || !modem.restart())
    {
        Serial.println("Modem init failed");
        return;
    }

    Serial.println("Waiting for network...");
    if (!modem.waitForNetwork())
    {
        Serial.println("Network not found");
        return;
    }

    Serial.print("Connecting to GPRS...");
    if (!modem.gprsConnect(apn))
    {
        Serial.println(" failed");
        return;
    }
    Serial.println(" success");

    sslClient.setInsecure(); // Disable certificate verification
    HttpClient http(sslClient, "beehive-mac-check.netlify.app", 443);

    Serial.println("Making HTTPS request...");
    http.beginRequest();
    http.get("/.netlify/functions/checkWhitelist?mac=00:4B:12:2F:E5:CC");
    http.endRequest();

    int status = http.responseStatusCode();
    String resp = http.responseBody();

    Serial.println("HTTP status: " + String(status));
    Serial.println("Response: " + resp);

    // String msg = (resp.indexOf("true") != -1) ? "Authorized" : "Unauthorized";
    // sendSMS(msg);

    Serial.println("Done!");
}

void loop()
{
    // nothing here
}
