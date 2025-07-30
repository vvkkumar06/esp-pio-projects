#include "motor-controls.h"
#include "camera-config.h"
#include "server.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

void setup()
{
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  Serial.begin(115200);
  delay(1000);

  // Motor pin modes
  pinMode(in1, OUTPUT);
  pinMode(in2, OUTPUT);
  pinMode(in3, OUTPUT);
  pinMode(in4, OUTPUT);

  pinMode(headLight, OUTPUT);
  // pinMode(rightIndicator, OUTPUT);
  // pinMode(leftIndicator, OUTPUT);
  pinMode(breakLight, OUTPUT);

  stopCar();
  connectWifi();
  delay(3000);
  if (!startCamera())
  {
    Serial.println("Camera init failed. Halting.");
    while (true)
      delay(1000);
  }
  routes();
  server.begin();
  Serial.println("HTTP stream server started");
  // Serial.end();
}

void loop()
{
  server.handleClient();
  delay(10);
}
