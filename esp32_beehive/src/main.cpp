
#include <WiFi.h>
#include <BeehiveMonitor.h>
#include <BeehivePreferences.h>
#include "secrets.h"

BeehiveMonitor beehive(2, DHT11, 18, 19, 27, 26);
BeehivePreferences beehivePreferences;

void setup()
{
    Serial.begin(115200);
    Serial.println("🚀 Beehive Monitor Starting...");
    beehivePreferences.startConfigPortal();
    beehive.initializeModem();

    // // Set APEX server details
    beehive.setApexServer(APEX_SERVER_HOST, APEX_SERVER_PORT, APEX_SERVER_PATH);

    delay(3000); // Wait for modem to stabilize

    beehive.begin();
    
}

void loop()
{
    beehive.sendData(BEEHIVE_ID);
    delay(5000);
}
