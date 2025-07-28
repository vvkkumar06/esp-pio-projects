#include <Arduino.h>
#include "motor-controls.h"


const int in1 = 12; // Right motor forward
const int in2 = 13; // Right motor backward
const int in3 = 15; // Left motor forward
const int in4 = 14; // Left motor backward

const int headLight = 4;
const int rightIndicator = 1;
const int leftIndicator = 3;
const int breakLight = 2;

// Speed (PWM duty cycle)
int speedVal = 0; // Range: 0 - 1023
bool isEngineRunning = false;
int speedKmph = 0;
// ================= MOTOR CONTROL =================

void left()
{
  if (isEngineRunning)
  {
    analogWrite(in1, speedVal);
    analogWrite(in2, 0);
    analogWrite(in3, speedVal);
    analogWrite(in4, 0);
  }
}

void right()
{
  if (isEngineRunning)
  {
    analogWrite(in1, 0);
    analogWrite(in2, speedVal); // Right motor runs forward
    analogWrite(in3, 0);        // Left motor stops
    analogWrite(in4, speedVal);
  }
}

void forward()
{
  if (isEngineRunning)
  {
    analogWrite(in1, 0);
    analogWrite(in2, speedVal);
    analogWrite(in3, speedVal);
    analogWrite(in4, 0);
  }
}

void backward()
{
  if (isEngineRunning)
  {
    analogWrite(in1, speedVal);
    analogWrite(in2, 0);
    analogWrite(in3, 0);
    analogWrite(in4, speedVal);
  }
}

void stopCar()
{
  if (isEngineRunning)
  {
    analogWrite(in1, 0);
    analogWrite(in2, 0);
    analogWrite(in3, 0);
    analogWrite(in4, 0);
  }
}

void turnOffIndicators()
{
  digitalWrite(leftIndicator, LOW);
  digitalWrite(rightIndicator, LOW);
}