#include "text_allegro.hpp"
#include "../scratch/render.hpp"
#include "os.hpp"
#include "text.hpp"
#include <iostream>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

std::unordered_map<std::string, FONT *> TextObjectAllegro::fonts;
std::unordered_map<std::string, size_t> TextObjectAllegro::fontUsageCount;

TextObjectAllegro::TextObjectAllegro(std::string txt, double posX, double posY, std::string fontPath)
    : TextObject(txt, posX, posY, fontPath) {

    // get font
    if (fontPath.empty()) {
        fontPath = FONT_ARIAL_NARROW;
    }
    fontPath = OS::getRomFSLocation() + fontPath;
    fontPath = fontPath + ".pcx"; // Allegro fonts are often PCX

    // open font if not loaded
    if (fonts.find(fontPath) == fonts.end()) {
        FONT *loadedFont = load_font(fontPath.c_str(), NULL, NULL);
        if (!loadedFont) {
            Log::logError("Failed to load font " + fontPath + ": " + allegro_error);
            font = ::font; // fallback to default allegro font
        } else {
            fonts[fontPath] = loadedFont;
            fontUsageCount[fontPath] = 1;
            pathFont = fontPath;
            font = loadedFont;
        }
    } else {
        font = fonts[fontPath];
        pathFont = fontPath;
        fontUsageCount[fontPath]++;
    }

    // Set initial text
    setText(txt);
}

TextObjectAllegro::~TextObjectAllegro() {
    if (font && !pathFont.empty() && font != ::font) {
        fontUsageCount[pathFont]--;
        if (fontUsageCount[pathFont] <= 0) {
            destroy_font(fonts[pathFont]);
            fonts.erase(pathFont);
            fontUsageCount.erase(pathFont);
        }
    }
}

void TextObjectAllegro::setText(std::string txt) {
    if (text == txt) return;
    text = txt;
}

void TextObjectAllegro::render(int xPos, int yPos) {
    if (!font || text.empty()) return;

    if (scale == 1.0f) {
        int renderX = xPos;
        int renderY = yPos;

        if (centerAligned) {
            renderX -= text_length(font, text.c_str()) / 2;
            renderY -= text_height(font) / 2;
        }

        textout_ex((BITMAP*)Render::getRenderer(), font, text.c_str(), renderX, renderY, color, -1);
    } else {
        int unscaledW = text_length(font, text.c_str());
        int unscaledH = text_height(font);
        BITMAP *text_buffer = create_bitmap(unscaledW, unscaledH);
        clear_to_color(text_buffer, makecol32(255, 0, 255)); // Transparent color
        textout_ex(text_buffer, font, text.c_str(), 0, 0, color, -1);

        int scaledW = unscaledW * scale;
        int scaledH = unscaledH * scale;
        int renderX = centerAligned ? xPos - (scaledW / 2) : xPos;
        int renderY = centerAligned ? yPos - (scaledH / 2) : yPos;

        stretch_sprite((BITMAP*)Render::getRenderer(), text_buffer, renderX, renderY, scaledW, scaledH);
        destroy_bitmap(text_buffer);
    }
}

std::vector<float> TextObjectAllegro::getSize() {
    if (!font) return {0.0f, 0.0f};
    return {(float)text_length(font, text.c_str()) * scale, (float)text_height(font) * scale};
}

void TextObjectAllegro::cleanupText() {
    for (auto &[fontPath, font] : fonts) {
        if (font) {
            destroy_font(font);
        }
    }

    // Clear the maps
    fonts.clear();
    fontUsageCount.clear();

    Log::log("Cleaned up all text.");
}