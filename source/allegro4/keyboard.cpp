#include "keyboard.hpp"
#include "../scratch/render.hpp"
#include "text.hpp"
#include <allegro.h>
#include <string>

/**
 * Uses Allegro text input.
 */
std::string Keyboard::openKeyboard(const char *hintText) {
    TextObject *text = createTextObject(std::string(hintText), 0, 0);
    text->setCenterAligned(true);
    text->setColor(Math::color(0, 0, 0, 170));
    if (text->getSize()[0] > Render::getWidth() * 0.85) {
        float scale = (float)Render::getWidth() / (text->getSize()[0] * 1.15);
        text->setScale(scale);
    }

    TextObject *enterText = createTextObject("ENTER TEXT :", 0, 0);
    enterText->setCenterAligned(true);
    enterText->setColor(Math::color(0, 0, 0, 255));

    std::string inputText = "";
    bool inputActive = true;

    while (inputActive) {
        if (keypressed()) {
            int k = readkey();
            char ascii = k & 0xff;
            int scancode = k >> 8;

            if (scancode == KEY_ENTER) {
                inputActive = false;
            } else if (scancode == KEY_BACKSPACE) {
                if (!inputText.empty()) {
                    inputText.pop_back();
                }
            } else if (scancode == KEY_ESC) {
                inputText = "";
                inputActive = false;
            } else if (ascii >= 32 && ascii <= 126) {
                inputText += ascii;
            }

            if (inputText.empty()) {
                text->setText(std::string(hintText));
                text->setColor(Math::color(0, 0, 0, 170));
            } else {
                text->setText(inputText);
                text->setColor(Math::color(0, 0, 0, 255));
            }
        }

        // set text size
        text->setScale(1.0f);
        if (text->getSize()[0] > Render::getWidth() * 0.95) {
            float scale = (float)Render::getWidth() / (text->getSize()[0] * 1.05);
            text->setScale(scale);
        } else {
            text->setScale(1.0f);
        }

        Render::beginFrame(0, 117, 77, 117);

        text->render(Render::getWidth() / 2, Render::getHeight() * 0.25);
        enterText->render(Render::getWidth() / 2, Render::getHeight() * 0.15);

        Render::endFrame(false);
    }

    delete text;
    delete enterText;
    return inputText;

    return "";
}
