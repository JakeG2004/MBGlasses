#pragma once

void SoftPWMBegin();
void SoftPWMSet(int pin, int value);
void SoftPWMSetFadeTime(int pin, int up, int down);