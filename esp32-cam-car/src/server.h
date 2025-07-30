#pragma once
#include <WiFi.h>   
#include <WebServer.h> 

void routes();
void handleStream();
void streamTask(void *pvParameters);

// WiFi credentials
extern const char *ssid;
extern const char *password;

extern WebServer server;   
extern WiFiClient streamClient;

void connectWifi();

