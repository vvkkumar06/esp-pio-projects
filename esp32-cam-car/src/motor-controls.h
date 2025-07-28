
#pragma once
// Function declarations
void forward();
void backward();
void left();
void right();
void stopCar();
void turnOffIndicators();

// External variables (only declared here, defined in .cpp)
extern const int in1;
extern const int in2;
extern const int in3;
extern const int in4;

extern const int headLight;
extern const int rightIndicator;
extern const int leftIndicator;
extern const int breakLight;

extern int speedVal;
extern bool isEngineRunning;
extern int speedKmph;
