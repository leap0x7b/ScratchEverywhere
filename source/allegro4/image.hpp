#pragma once

#include <allegro.h>
#include <string>
#include <unordered_map>

class AllegroImage {
  public:
    size_t imageUsageCount = 0;
    int renderWidth;
    int renderHeight;
    BITMAP *sprite;
    size_t memorySize;
    float scale = 1.0f;
    int width;
    int height;
    float rotation = 0.0f;
    int maxFreeTime = 2;
    int freeTimer = maxFreeTime;

    /**
     * Scales an image by a scale factor.
     * @param scaleAmount
     */
    void setScale(float amount);
    /**
     * Sets Image rotation (in radians)
     * @param rotationAmount In radians
     */
    void setRotation(float amount);

    /**
     * A Simple Image object using Allegro.
     */
    AllegroImage();
    /**
     * A Simple Image object using Allegro.
     * @param filePath
     */
    AllegroImage(std::string filePath);

    ~AllegroImage();
};

unsigned char *SVGToRGBA(const void *svg_data, size_t svg_size, int &width, int &height);

extern std::unordered_map<std::string, AllegroImage *> images;