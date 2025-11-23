#pragma once
#include <SDL/SDL.h>
#ifdef ENABLE_AUDIO
#include <SDL/SDL_mixer.h>
#endif
#include <SDL/SDL_ttf.h>

extern int windowWidth;
extern int windowHeight;
extern SDL_Surface *window;

std::pair<float, float> screenToScratchCoords(float screenX, float screenY, int windowWidth, int windowHeight);
