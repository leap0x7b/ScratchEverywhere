#include "input.hpp"
#include "blockExecutor.hpp"
#include "render.hpp"
#include "sprite.hpp"
#include <allegro.h>
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <map>
#include <string>
#include <vector>

Input::Mouse Input::mousePointer;
Sprite *Input::draggingSprite = nullptr;

std::vector<std::string> Input::inputButtons;
std::map<std::string, std::string> Input::inputControls;
int Input::keyHeldFrames = 0;

#define CONTROLLER_DEADZONE_X 10000
#define CONTROLLER_DEADZONE_Y 18000
#define CONTROLLER_DEADZONE_TRIGGER 1000

#ifdef ENABLE_CLOUDVARS
extern std::string cloudUsername;
extern bool cloudProject;
#endif

extern bool useCustomUsername;
extern std::string customUsername;

std::vector<int> Input::getTouchPosition() {
    std::vector<int> pos;
    pos.push_back(static_cast<int>(mouse_x));
    pos.push_back(static_cast<int>(mouse_y));
    return pos;
}

void Input::getInput() {
    inputButtons.clear();
    mousePointer.isPressed = false;
    mousePointer.isMoving = false;

    poll_keyboard();
    poll_mouse();
    poll_joystick();

    bool anyKeyPressed = false;

    for (int i = 0; i < KEY_MAX; ++i) {
        if (key[i]) {
            std::string keyName = scancode_to_name(i);
            std::transform(keyName.begin(), keyName.end(), keyName.begin(), ::tolower);
            if (keyName == "up") keyName = "up arrow";
            else if (keyName == "down") keyName = "down arrow";
            else if (keyName == "left") keyName = "left arrow";
            else if (keyName == "right") keyName = "right arrow";
            else if (keyName == "enter") keyName = "enter";
            inputButtons.push_back(keyName);
            anyKeyPressed = true;
        }
    }

    if (num_joysticks > 0) {
        if (joy[0].stick[0].axis[1].pos < -64) {
            Input::buttonPress("dpadUp"); anyKeyPressed = true;
        }
        if (joy[0].stick[0].axis[1].pos > 64) {
            Input::buttonPress("dpadDown"); anyKeyPressed = true;
        }
        if (joy[0].stick[0].axis[0].pos < -64) {
            Input::buttonPress("dpadLeft"); anyKeyPressed = true;
        }
        if (joy[0].stick[0].axis[0].pos > 64) {
            Input::buttonPress("dpadRight"); anyKeyPressed = true;
        }
        if (joy[0].button[0].b) {
            Input::buttonPress("A"); anyKeyPressed = true;
        }
        if (joy[0].button[1].b) {
            Input::buttonPress("B"); anyKeyPressed = true;
        }
        if (joy[0].button[2].b) {
            Input::buttonPress("X"); anyKeyPressed = true;
        }
        if (joy[0].button[3].b) {
            Input::buttonPress("Y"); anyKeyPressed = true;
        }
        if (joy[0].button[4].b) {
            Input::buttonPress("shoulderL"); anyKeyPressed = true;
        }
        if (joy[0].button[5].b) {
            Input::buttonPress("shoulderR"); anyKeyPressed = true;
        }
        if (joy[0].button[6].b) {
            Input::buttonPress("LT"); anyKeyPressed = true;
        }
        if (joy[0].button[7].b) {
            Input::buttonPress("RT"); anyKeyPressed = true;
        }
        if (joy[0].button[8].b) {
            Input::buttonPress("back"); anyKeyPressed = true;
        }
        if (joy[0].button[9].b) {
            Input::buttonPress("start"); anyKeyPressed = true;
        }
        if (joy[0].button[10].b) {
            Input::buttonPress("LeftStickPressed"); anyKeyPressed = true;
        }
        if (joy[0].button[11].b) {
            Input::buttonPress("RightStickPressed"); anyKeyPressed = true;
        }

        if (joy[0].stick[1].axis[0].pos < -64) {
            Input::buttonPress("RightStickLeft"); anyKeyPressed = true;
        }
        if (joy[0].stick[1].axis[0].pos > 64) {
            Input::buttonPress("RightStickRight"); anyKeyPressed = true;
        }
        if (joy[0].stick[1].axis[1].pos < -64) {
            Input::buttonPress("RightStickUp"); anyKeyPressed = true;
        }
        if (joy[0].stick[1].axis[1].pos > 64) {
            Input::buttonPress("RightStickDown"); anyKeyPressed = true;
        }
    }

    if (anyKeyPressed) {
        keyHeldFrames++;
        inputButtons.push_back("any");
        if (keyHeldFrames == 1 || keyHeldFrames > 13)
            BlockExecutor::runAllBlocksByOpcode("event_whenkeypressed");
    } else keyHeldFrames = 0;

    // Get raw mouse coordinates
    std::vector<int> rawMouse = getTouchPosition();

    auto coords = screenToScratchCoords(rawMouse[0], rawMouse[1], windowWidth, windowHeight);
    mousePointer.x = coords.first;
    mousePointer.y = coords.second;

    if (mouse_b & 1 || mouse_b & 2) {
        mousePointer.isPressed = true;
    }

    doSpriteClicking();
}

std::string Input::getUsername() {
    if (useCustomUsername) {
        return customUsername;
    }
#ifdef ENABLE_CLOUDVARS
    if (cloudProject) return cloudUsername;
#endif
    return "Player";
}
