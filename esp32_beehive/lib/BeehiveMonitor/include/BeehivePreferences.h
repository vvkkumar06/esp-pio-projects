#ifndef BEEHIVEPREFERENCES_H
#define BEEHIVEPREFERENCES_H

#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>

class BeehivePreferences
{
public:
    BeehivePreferences() : server(80) {} // construct WebServer in-place

    void begin()
    {
        pref.begin("beehive", false);
    }

    void end()
    {
        pref.end();
    }

    void saveAPN(const char *apn)
    {
        pref.putString("apn", apn);
    }

    String readAPN()
    {
        return pref.getString("apn", "");
    }

    void saveCalibration(float factor)
    {
        pref.putFloat("scaleFactor", factor);
    }

    float readCalibration()
    {
        return pref.getFloat("scaleFactor", 2200.0f); // default=2200
    }

    // Call this to start AP + config portal
    void startConfigPortal()
    {
        WiFi.softAP("Beehive_Config", "beehive@123");
        IPAddress IP = WiFi.softAPIP();
        Serial.print("Config AP IP: ");
        Serial.println(IP);

        server.on("/", [this]()
                  { handleRoot(); });
        server.on("/save", HTTP_POST, [this]()
                  { handleSave(); });

        server.begin();

        // Blocking loop for config portal
        while (true)
        {
            server.handleClient();
            delay(1); // small delay to avoid watchdog reset
        }
    }

private:
    Preferences pref;
    WebServer server;

    void handleRoot()
    {
        String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
        html += "<title>Beehive Config</title>";
        html += "<style>";
        html += "body { font-family: Arial, sans-serif; background-color: #f4f4f4; text-align: center; }";
        html += "form { background: #fff; padding: 20px; margin: 50px auto; width: 300px; border-radius: 10px; box-shadow: 0 0 10px rgba(0,0,0,0.2); }";
        html += "input[type=text] { width: 90%; padding: 8px; margin: 10px 0; border-radius: 5px; border: 1px solid #ccc; }";
        html += "input[type=submit] { padding: 10px 20px; border: none; background: #28a745; color: white; border-radius: 5px; cursor: pointer; }";
        html += "input[type=submit]:hover { background: #218838; }";
        html += "</style></head><body>";

        html += "<h2>Enter GSM APN</h2>";
        html += "<form action='/save' method='POST'>";
        html += "<input type='text' name='apn' placeholder='Enter APN' required><br>";
        html += "<input type='submit' value='Save'>";
        html += "</form>";
        html += "</body></html>";

        server.send(200, "text/html", html);
    }

    void handleSave()
    {
        String apn = server.arg("apn");

        begin();
        saveAPN(apn.c_str());
        end();

        String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
        html += "<title>Saved</title>";
        html += "<style>body{font-family:Arial,sans-serif;text-align:center;margin-top:50px;} h1{color:#28a745;}</style>";
        html += "</head><body>";
        html += "<h1>Saved! Restarting...</h1>";
        html += "</body></html>";

        server.send(200, "text/html", html);
        delay(2000);
        Serial.print("Restarting ESP...");
        ESP.restart();
    }
};

#endif // BEEHIVEPREFERENCES_H
