// camera-config.h
#pragma once
#include <Arduino.h>
#include "esp_camera.h"

// FPS Monitor (can be optionally removed if not used outside)
extern unsigned long lastFpsPrint;
extern int frameCount;

// 📸 Declare startCamera
bool startCamera();
