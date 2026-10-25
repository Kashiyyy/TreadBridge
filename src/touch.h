#pragma once
#include <Arduino.h>

// AXS5106L Touch-Controller. x/y in Display-Koordinaten (Hochformat 172 x 320).
bool touchBegin();                 // true, wenn der Touch-Controller antwortet
bool touchPoll(int& x, int& y);    // true, solange ein Finger das Display beruehrt
