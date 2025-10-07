#pragma once
#include "../scratch/text.hpp"
#include <allegro.h>
#include <unordered_map>

class TextObjectAllegro : public TextObject {
  private:
    static std::unordered_map<std::string, FONT *> fonts;
    static std::unordered_map<std::string, size_t> fontUsageCount;
    std::string pathFont;
    FONT *font = nullptr;
    size_t memorySize = 0;
    int textWidth = 0;
    int textHeight = 0;

  public:
    TextObjectAllegro(std::string txt, double posX, double posY, std::string fontPath = "");
    ~TextObjectAllegro() override;

    void setText(std::string txt) override;
    void render(int xPos, int yPos) override;
    std::vector<float> getSize() override;
    static void cleanupText();
};