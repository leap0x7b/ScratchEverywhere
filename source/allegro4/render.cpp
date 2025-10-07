#include "../scratch/render.hpp"
#include "../scratch/image.hpp"
#include "audio.hpp"
#include "blocks/pen.hpp"
#include "image.hpp"
#include "interpret.hpp"
#include "math.hpp"
#include "render.hpp"
#include "sprite.hpp"
#include "text.hpp"
#include "unzip.hpp"
#include <algorithm>
#include <allegro.h>
#include <allegro/gfx.h>
#include <allegro/mouse.h>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>

int windowWidth = 640;
int windowHeight = 480;
bool closeButtonPressed = false;
static BITMAP *buffer = NULL;

Render::RenderModes Render::renderMode = Render::TOP_SCREEN_ONLY;
bool Render::hasFrameBegan;
std::vector<Monitor> Render::visibleVariables;
std::chrono::system_clock::time_point Render::startTime = std::chrono::system_clock::now();
std::chrono::system_clock::time_point Render::endTime = std::chrono::system_clock::now();
bool Render::debugMode = false;

void closeButtonHandler(void) {
    closeButtonPressed = true;
}

bool Render::Init() {
    allegro_init();
    install_timer();

    set_color_depth(32);
#ifdef __DOS__
    if (set_gfx_mode(GFX_AUTODETECT, 640, 480, 0, 0) < 0) {
#else
    if (set_gfx_mode(GFX_AUTODETECT_WINDOWED, 640, 480, 0, 0) < 0) {
#endif
        set_gfx_mode(GFX_TEXT, 0, 0, 0, 0);
        Log::logWarning(std::string("Failed to initialize graphics: ") + allegro_error);
    }
    set_window_title("Scratch Everywhere!");

#ifdef ENABLE_AUDIO
    if (install_sound(DIGI_AUTODETECT, MIDI_NONE) < 0) {
        Log::logWarning(std::string("Failed to initialize sound: ") + allegro_error);
        return false;
    }
#endif

    install_keyboard();
    install_mouse();
    enable_hardware_cursor();
    show_mouse(screen);
    if (!(gfx_capabilities & GFX_HW_CURSOR)) show_mouse(NULL);
    install_joystick(JOY_TYPE_AUTODETECT);
    set_close_button_callback(closeButtonHandler);

    buffer = create_bitmap(SCREEN_W, SCREEN_H);
    if (penBitmap == nullptr) penBitmap = create_bitmap(Scratch::projectWidth, Scratch::projectHeight);

    debugMode = true;
    return true;
}
void Render::deInit() {
    if (penBitmap) destroy_bitmap(penBitmap);
    Image::cleanupImages();
    SoundPlayer::cleanupAudio();
    TextObject::cleanupText();
    if (buffer) destroy_bitmap(buffer);
    SoundPlayer::deinit();
    allegro_exit();
}

void *Render::getRenderer() {
    return static_cast<void *>(buffer);
}

// Allegro 4 does not support resizeable windows. Hardcoded to 640x480.
int Render::getWidth() {
    return 640;
}
int Render::getHeight() {
    return 480;
}

bool Render::initPen() {
    if (penBitmap != nullptr) return true;

    penBitmap = create_bitmap(Scratch::projectWidth, Scratch::projectHeight);
    clear_to_color(penBitmap, makeacol32(0, 0, 0, 0));

    return true;
}

void Render::penMove(double x1, double y1, double x2, double y2, Sprite *sprite) {
    const ColorRGB rgbColor = HSB2RGB(sprite->penData.color);
    int pen_color = makeacol32(rgbColor.r, rgbColor.g, rgbColor.b, (sprite->penData.transparency - 100) / 100 * 255);

    drawing_mode(DRAW_MODE_TRANS, NULL, 0, 0);
    set_trans_blender(0, 0, 0, 128);

    const double dx = x2 - x1;
    const double dy = y2 - y1;

    const double length = sqrt(dx * dx + dy * dy);

    if (length > 0) {
        const double nx = dy / length;
        const double ny = dx / length;

        int points[8];
        points[0] = x1 + Scratch::projectWidth / 2 - ny * (sprite->penData.size / 2);
        points[1] = -y1 + Scratch::projectHeight / 2 + nx * (sprite->penData.size / 2);
        points[2] = x1 + Scratch::projectWidth / 2 + ny * (sprite->penData.size / 2);
        points[3] = -y1 + Scratch::projectHeight / 2 - nx * (sprite->penData.size / 2);
        points[4] = x2 + Scratch::projectWidth / 2 + ny * (sprite->penData.size / 2);
        points[5] = -y2 + Scratch::projectHeight / 2 - nx * (sprite->penData.size / 2);
        points[6] = x2 + Scratch::projectWidth / 2 - ny * (sprite->penData.size / 2);
        points[7] = -y2 + Scratch::projectHeight / 2 + nx * (sprite->penData.size / 2);

        polygon(penBitmap, 4, points, pen_color);
    }

    circlefill(penBitmap, x1 + 240, -y1 + 180, sprite->penData.size / 2, pen_color);
    circlefill(penBitmap, x2 + 240, -y2 + 180, sprite->penData.size / 2, pen_color);

    drawing_mode(DRAW_MODE_SOLID, NULL, 0, 0);
}

void Render::beginFrame(int _screen, int colorR, int colorG, int colorB) {
    if (!hasFrameBegan) {
        clear_to_color(buffer, makecol32(colorR, colorG, colorB));
        hasFrameBegan = true;
    }
}

void Render::endFrame(bool shouldFlush) {
    if (!(gfx_capabilities & GFX_HW_CURSOR))
        draw_sprite(buffer, mouse_sprite, mouse_x, mouse_y);
    blit(buffer, screen, 0, 0, 0, 0, SCREEN_W, SCREEN_H);
    if (shouldFlush) Image::FlushImages();
    hasFrameBegan = false;
}

void Render::drawBox(int w, int h, int x, int y, int colorR, int colorG, int colorB, int colorA) {
    rectfill(buffer, x - (w / 2), y - (h / 2), x + (w / 2), y + (h / 2), makeacol32(colorR, colorG, colorB, colorA));
}

std::pair<float, float> screenToScratchCoords(float screenX, float screenY, int windowWidth, int windowHeight) {
    float screenAspect = static_cast<float>(windowWidth) / windowHeight;
    float projectAspect = static_cast<float>(Scratch::projectWidth) / Scratch::projectHeight;

    float scratchX, scratchY;

    if (screenAspect > projectAspect) {
        // Vertical black bars
        float scale = static_cast<float>(windowHeight) / Scratch::projectHeight;
        float scaledProjectWidth = Scratch::projectWidth * scale;
        float barWidth = (windowWidth - scaledProjectWidth) / 2.0f;

        // Remove bar offset and scale to project space
        float adjustedX = screenX - barWidth;
        scratchX = (adjustedX / scaledProjectWidth) * Scratch::projectWidth - (Scratch::projectWidth / 2.0f);
        scratchY = (Scratch::projectHeight / 2.0f) - (screenY / windowHeight) * Scratch::projectHeight;

    } else if (screenAspect < projectAspect) {
        // Horizontal black bars
        float scale = static_cast<float>(windowWidth) / Scratch::projectWidth;
        float scaledProjectHeight = Scratch::projectHeight * scale;
        float barHeight = (windowHeight - scaledProjectHeight) / 2.0f;

        // Remove bar offset and scale to project space
        float adjustedY = screenY - barHeight;
        scratchX = (screenX / windowWidth) * Scratch::projectWidth - (Scratch::projectWidth / 2.0f);
        scratchY = (Scratch::projectHeight / 2.0f) - (adjustedY / scaledProjectHeight) * Scratch::projectHeight;

    } else {
        // no black bars..
        scratchX = (screenX / windowWidth) * Scratch::projectWidth - (Scratch::projectWidth / 2.0f);
        scratchY = (Scratch::projectHeight / 2.0f) - (screenY / windowHeight) * Scratch::projectHeight;
    }

    return std::make_pair(scratchX, scratchY);
}

void drawBlackBars(int screenWidth, int screenHeight) {
    float screenAspect = static_cast<float>(screenWidth) / screenHeight;
    float projectAspect = static_cast<float>(Scratch::projectWidth) / Scratch::projectHeight;

    if (screenAspect > projectAspect) {
        // Vertical bars,,,
        float scale = static_cast<float>(screenHeight) / Scratch::projectHeight;
        float scaledProjectWidth = Scratch::projectWidth * scale;
        float barWidth = (screenWidth - scaledProjectWidth) / 2.0f;

        rectfill(buffer, 0, 0, static_cast<int>(std::ceil(barWidth)), screenHeight, makecol32(0, 0, 0));
        rectfill(buffer, static_cast<int>(std::floor(screenWidth - barWidth)), 0, screenWidth, screenHeight, makecol32(0, 0, 0));
    } else if (screenAspect < projectAspect) {
        // Horizontal bars,,,
        float scale = static_cast<float>(screenWidth) / Scratch::projectWidth;
        float scaledProjectHeight = Scratch::projectHeight * scale;
        float barHeight = (screenHeight - scaledProjectHeight) / 2.0f;

        rectfill(buffer, 0, 0, screenWidth, static_cast<int>(std::ceil(barHeight)), makecol32(0, 0, 0));
        rectfill(buffer, 0, static_cast<int>(std::floor(screenHeight - barHeight)), screenWidth, screenHeight, makecol32(0, 0, 0));
    }
}

void Render::renderSprites() {
    clear_to_color(buffer, makecol32(255, 255, 255));

    double scaleX = static_cast<double>(windowWidth) / Scratch::projectWidth;
    double scaleY = static_cast<double>(windowHeight) / Scratch::projectHeight;
    double scale;
    scale = std::min(scaleX, scaleY);

    // Sort sprites by layer with stage always being first
    std::vector<Sprite *> spritesByLayer = sprites;
    std::sort(spritesByLayer.begin(), spritesByLayer.end(),
              [](const Sprite *a, const Sprite *b) {
                  // Stage sprite always comes first
                  if (a->isStage && !b->isStage) return true;
                  if (!a->isStage && b->isStage) return false;
                  // Otherwise sort by layer
                  return a->layer < b->layer;
              });

    for (Sprite *currentSprite : spritesByLayer) {
        if (!currentSprite->visible) continue;

        bool legacyDrawing = false;
        auto imgFind = images.find(currentSprite->costumes[currentSprite->currentCostume].id);
        if (imgFind == images.end()) {
            legacyDrawing = true;
        } else {
            currentSprite->rotationCenterX = currentSprite->costumes[currentSprite->currentCostume].rotationCenterX;
            currentSprite->rotationCenterY = currentSprite->costumes[currentSprite->currentCostume].rotationCenterY;
        }
        if (!legacyDrawing && imgFind->second->sprite) {
            AllegroImage *image = imgFind->second;
            image->freeTimer = image->maxFreeTime;
            bool flip = false;
            double spriteScale = (currentSprite->size * 0.01) * scale;
            currentSprite->spriteWidth = image->width / 2;
            currentSprite->spriteHeight = image->height / 2;

            // double the image scale if the image is an SVG
            if (currentSprite->costumes[currentSprite->currentCostume].isSVG) {
                spriteScale *= 2;
            }

            const double rotation = Math::degreesToRadians(currentSprite->rotation - 90.0f);
            double renderRotation = rotation;
            if (currentSprite->rotationStyle == currentSprite->LEFT_RIGHT) {
                if (std::cos(rotation) < 0) {
                    flip = true;
                }
                renderRotation = 0;
            }
            if (currentSprite->rotationStyle == currentSprite->NONE) {
                renderRotation = 0;
            }

            int renderW = image->width * spriteScale;
            int renderH = image->height * spriteScale;

            if (renderW <= 0 || renderH <= 0) continue;

            int renderX = (currentSprite->xPosition * scale) + (windowWidth / 2);
            int renderY = (currentSprite->yPosition * -scale) + (windowHeight / 2);

            BITMAP *temp_buffer = create_bitmap(renderW, renderH);
            //clear_to_color(temp_buffer, makecol32(255, 0, 255)); // Use magenta for transparency key
            clear_to_color(temp_buffer, makeacol32(0, 0, 0, 0));

            stretch_sprite(temp_buffer, image->sprite, 0, 0, renderW, renderH);

            // set ghost effect
            float ghost = std::clamp(currentSprite->ghostEffect, 0.0f, 100.0f);
            int alpha = static_cast<int>(255 * (1.0f - ghost / 100.0f));
            set_trans_blender(0, 0, 0, alpha);

            // set brightness effect
            if (currentSprite->brightnessEffect != 0) {
                float brightness = currentSprite->brightnessEffect * 0.01f;
                drawing_mode(DRAW_MODE_TRANS, NULL, 0, 0);
                int b = static_cast<int>(255 * std::abs(brightness));
                if (brightness > 0)
                    set_add_blender(0, 0, 0, b);
                else
                    set_burn_blender(0, 0, 0, b);
            }

            if (renderRotation != 0) {
                fixed allegroAngle = ftofix(Math::radiansToAllegro(renderRotation));
                pivot_sprite(buffer, temp_buffer, renderX, renderY, renderW / 2, renderH / 2, allegroAngle);
            } else {
                int drawX = renderX - (renderW / 2);
                int drawY = renderY - (renderH / 2);
                if (flip)
                    draw_sprite_h_flip(buffer, temp_buffer, drawX, drawY);
                else
                    draw_sprite(buffer, temp_buffer, drawX, drawY);
            }

            drawing_mode(DRAW_MODE_SOLID, NULL, 0, 0);
            destroy_bitmap(temp_buffer);
        } else {
            currentSprite->spriteWidth = 64;
            currentSprite->spriteHeight = 64;
            rect(buffer, (currentSprite->xPosition * scale) + (windowWidth / 2), (currentSprite->yPosition * -1 * scale) + (windowHeight * 0.5), (currentSprite->xPosition * scale) + (windowWidth / 2) + 16, (currentSprite->yPosition * -1 * scale) + (windowHeight * 0.5) + 16, makecol32(0, 0, 0));
        }

        // Draw collision points (for debugging)
        // std::vector<std::pair<double, double>> collisionPoints = getCollisionPoints(currentSprite);
        // SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // Black points

        // for (const auto &point : collisionPoints) {
        //     double screenX = (point.first * scale) + (windowWidth / 2);
        //     double screenY = (point.second * -scale) + (windowHeight / 2);

        //     SDL_Rect debugPointRect;
        //     debugPointRect.x = static_cast<int>(screenX - scale); // center it a bit
        //     debugPointRect.y = static_cast<int>(screenY - scale);
        //     debugPointRect.w = static_cast<int>(2 * scale);
        //     debugPointRect.h = static_cast<int>(2 * scale);

        //     SDL_RenderFillRect(renderer, &debugPointRect);
        // }

        if (currentSprite->isStage) renderPenLayer();
    }

    drawBlackBars(windowWidth, windowHeight);
    renderVisibleVariables();

    if (!(gfx_capabilities & GFX_HW_CURSOR))
        draw_sprite(buffer, mouse_sprite, mouse_x, mouse_y);
    blit(buffer, screen, 0, 0, 0, 0, SCREEN_W, SCREEN_H);
    Image::FlushImages();
    SoundPlayer::flushAudio();
}

std::unordered_map<std::string, TextObject *> Render::monitorTexts;

void Render::renderVisibleVariables() {
    // get screen scale
    double scaleX = static_cast<double>(windowWidth) / Scratch::projectWidth;
    double scaleY = static_cast<double>(windowHeight) / Scratch::projectHeight;
    double scale = std::min(scaleX, scaleY);

    // calculate black bar offset
    float screenAspect = static_cast<float>(windowWidth) / windowHeight;
    float projectAspect = static_cast<float>(Scratch::projectWidth) / Scratch::projectHeight;
    float barOffsetX = 0.0f;
    float barOffsetY = 0.0f;
    if (screenAspect > projectAspect) {
        float scaledProjectWidth = Scratch::projectWidth * scale;
        barOffsetX = (windowWidth - scaledProjectWidth) / 2.0f;
    } else if (screenAspect < projectAspect) {
        float scaledProjectHeight = Scratch::projectHeight * scale;
        barOffsetY = (windowHeight - scaledProjectHeight) / 2.0f;
    }

    for (auto &var : visibleVariables) {
        if (var.visible) {
            std::string renderText = BlockExecutor::getMonitorValue(var).asString();
            if (monitorTexts.find(var.id) == monitorTexts.end()) {
                monitorTexts[var.id] = createTextObject(renderText, var.x, var.y);
            } else {
                monitorTexts[var.id]->setText(renderText);
            }
            monitorTexts[var.id]->setColor(0x000000FF);

            if (var.mode != "large") {
                monitorTexts[var.id]->setCenterAligned(false);
                monitorTexts[var.id]->setScale(1.0f * (scale / 2.0f));
            } else {
                monitorTexts[var.id]->setCenterAligned(true);
                monitorTexts[var.id]->setScale(1.25f * (scale / 2.0f));
            }
            monitorTexts[var.id]->render(var.x * scale + barOffsetX, var.y * scale + barOffsetY);
        } else {
            if (monitorTexts.find(var.id) != monitorTexts.end()) {
                delete monitorTexts[var.id];
                monitorTexts.erase(var.id);
            }
        }
    }
}

void Render::renderPenLayer() {
    int renderW = 0;
    int renderH = 0;
    int renderX = 0;
    int renderY = 0;

    if (static_cast<float>(windowWidth) / windowHeight > static_cast<float>(Scratch::projectWidth) / Scratch::projectHeight) {
        renderX = std::ceil((windowWidth - Scratch::projectWidth * (static_cast<float>(windowHeight) / Scratch::projectHeight)) / 2.0f);
        renderW = windowWidth - renderX * 2;
        renderH = windowHeight;
    } else {
        renderY = std::ceil((windowHeight - Scratch::projectHeight * (static_cast<float>(windowWidth) / Scratch::projectWidth)) / 2.0f);
        renderH = windowHeight - renderY * 2;
        renderW = windowWidth;
    }

    drawing_mode(DRAW_MODE_TRANS, NULL, 0, 0);
    set_trans_blender(0, 0, 0, 128);
    stretch_sprite(buffer, penBitmap, renderX, renderY, renderW, renderH);
    drawing_mode(DRAW_MODE_SOLID, NULL, 0, 0);
}

bool Render::appShouldRun() {
    if (toExit) return false;
    if (key[KEY_ESC] || closeButtonPressed) {
        toExit = true;
        return false;
    }
    return true;
}
