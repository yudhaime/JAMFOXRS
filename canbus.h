#ifndef CANBUS_H
#define CANBUS_H

#include <Arduino.h>

bool initCAN();
void updateCAN();

extern int tempCtrl;
extern int tempMotor;
extern int tempBatt;

#endif