#pragma once
#include <Arduino.h>

// AXS5106L touch controller. x/y in display coordinates (portrait 172 x 320).
bool touchBegin();                 // true if the touch controller answers
bool touchPoll(int& x, int& y);    // true while a finger touches the display
