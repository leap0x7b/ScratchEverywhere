#pragma once
#include <allegro.h>

extern int windowWidth;
extern int windowHeight;

std::pair<float, float> screenToScratchCoords(float screenX, float screenY, int windowWidth, int windowHeight);
