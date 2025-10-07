#include "image.hpp"
#include "../scratch/image.hpp"
#include "os.hpp"
#include "render.hpp"
#include "../scratch/render.hpp"
#define STBI_NO_GIF
#define STB_IMAGE_IMPLEMENTATION
#include "unzip.hpp"
#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgrast.h"
#include "stb_image.h"
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

extern BITMAP *screen;

std::unordered_map<std::string, AllegroImage *> images;
static std::vector<std::string> toDelete;

Image::Image(std::string filePath) {
    if (!loadImageFromFile(filePath, false)) return;
    imageId = filePath.substr(0, filePath.find_last_of('.'));
    width = images[imageId]->width;
    height = images[imageId]->height;
    scale = 1.0;
    rotation = 0.0;
    opacity = 1.0;
    images[imageId]->imageUsageCount++;
}

Image::~Image() {
    auto it = images.find(imageId);
    if (it != images.end() && it->second) {
        images[imageId]->imageUsageCount--;
        if (images[imageId]->imageUsageCount <= 0)
            freeImage(imageId);
    }
}

void Image::render(double xPos, double yPos, bool centered) {
    if (images.find(imageId) == images.end()) return;
    AllegroImage *image = images[imageId];
    if (!image || !image->sprite) return;

    image->setScale(scale);
    image->setRotation(rotation);

    int dest_x = xPos;
    int dest_y = yPos;

    if (centered) {
        dest_x -= image->renderWidth / 2;
        dest_y -= image->renderHeight / 2;
    }

    set_trans_blender(0, 0, 0, opacity * 255);

    image->freeTimer = image->maxFreeTime;
    drawing_mode(DRAW_MODE_TRANS, NULL, 0, 0);
    stretch_sprite((BITMAP*)Render::getRenderer(), image->sprite, dest_x, dest_y, image->renderWidth, image->renderHeight);
    drawing_mode(DRAW_MODE_SOLID, NULL, 0, 0);
}

void Image::renderNineslice(double xPos, double yPos, double width, double height, double padding, bool centered) {
    if (images.find(imageId) == images.end()) return;
    AllegroImage *image = images[imageId];
    if (!image || !image->sprite) return;

    image->setScale(1.0);
    image->setRotation(0.0);

    set_trans_blender(0, 0, 0, opacity * 255);

    const int iDestX = static_cast<int>(xPos - (centered ? width / 2 : 0));
    const int iDestY = static_cast<int>(yPos - (centered ? height / 2 : 0));
    const int iWidth = static_cast<int>(width);
    const int iHeight = static_cast<int>(height);
    const int iSrcPadding = std::max(1, static_cast<int>(std::min(std::min(padding, static_cast<double>(image->width) / 2), static_cast<double>(image->height) / 2)));

    const int srcCenterWidth = std::max(0, image->width - 2 * iSrcPadding);
    const int srcCenterHeight = std::max(0, image->height - 2 * iSrcPadding);

    const int dstCenterWidth = std::max(0, iWidth - 2 * iSrcPadding);
    const int dstCenterHeight = std::max(0, iHeight - 2 * iSrcPadding);

    image->freeTimer = image->maxFreeTime;

    BITMAP* target_buffer = (BITMAP*)Render::getRenderer();
    // Top-left
    blit(image->sprite, target_buffer, 0, 0, iDestX, iDestY, iSrcPadding, iSrcPadding);
    // Top
    stretch_blit(image->sprite, target_buffer, iSrcPadding, 0, srcCenterWidth, iSrcPadding, iDestX + iSrcPadding, iDestY, dstCenterWidth, iSrcPadding);
    // Top-right
    blit(image->sprite, target_buffer, image->width - iSrcPadding, 0, iDestX + iSrcPadding + dstCenterWidth, iDestY, iSrcPadding, iSrcPadding);
    // Left
    stretch_blit(image->sprite, target_buffer, 0, iSrcPadding, iSrcPadding, srcCenterHeight, iDestX, iDestY + iSrcPadding, iSrcPadding, dstCenterHeight);
    // Center
    stretch_blit(image->sprite, target_buffer, iSrcPadding, iSrcPadding, srcCenterWidth, srcCenterHeight, iDestX + iSrcPadding, iDestY + iSrcPadding, dstCenterWidth, dstCenterHeight);
    // Right
    stretch_blit(image->sprite, target_buffer, image->width - iSrcPadding, iSrcPadding, iSrcPadding, srcCenterHeight, iDestX + iSrcPadding + dstCenterWidth, iDestY + iSrcPadding, iSrcPadding, dstCenterHeight);
    // Bottom-left
    blit(image->sprite, target_buffer, 0, image->height - iSrcPadding, iDestX, iDestY + iSrcPadding + dstCenterHeight, iSrcPadding, iSrcPadding);
    // Bottom
    stretch_blit(image->sprite, target_buffer, iSrcPadding, image->height - iSrcPadding, srcCenterWidth, iSrcPadding, iDestX + iSrcPadding, iDestY + iSrcPadding + dstCenterHeight, dstCenterWidth, iSrcPadding);
    // Bottom-right
    blit(image->sprite, target_buffer, image->width - iSrcPadding, image->height - iSrcPadding, iDestX + iSrcPadding + dstCenterWidth, iDestY + iSrcPadding + dstCenterHeight, iSrcPadding, iSrcPadding);
}

/**
 * Loads a single `AllegroImage` from an unzipped filepath .
 * @param filePath
 */
bool Image::loadImageFromFile(std::string filePath, bool fromScratchProject) {
    std::string imgId = filePath.substr(0, filePath.find_last_of('.'));
    if (images.find(imgId) != images.end()) return true;

    std::string finalPath;

    finalPath = OS::getRomFSLocation();
    if (fromScratchProject)
        finalPath = finalPath + "project/";

    finalPath = finalPath + filePath;
    if (Unzip::UnpackedInSD) finalPath = Unzip::filePath + filePath;
    FILE *file = fopen(finalPath.c_str(), "rb");
    if (!file) {
        Log::logWarning("Image file not found: " + filePath);
        return false;
    }

    int width, height;
    unsigned char *rgba_data = nullptr;

    bool isSVG = filePath.size() >= 4 &&
                 (filePath.substr(filePath.size() - 4) == ".svg" ||
                  filePath.substr(filePath.size() - 4) == ".SVG");

    if (isSVG) {
        fseek(file, 0, SEEK_END);
        long file_size = ftell(file);
        fseek(file, 0, SEEK_SET);

        char *svg_data = (char *)malloc(file_size);
        if (!svg_data) {
            Log::logWarning("Failed to allocate memory for SVG file: " + filePath);
            fclose(file);
            return false;
        }

        size_t read_size = fread(svg_data, 1, file_size, file);
        fclose(file);

        if (read_size != (size_t)file_size) {
            Log::logWarning("Failed to read SVG file completely: " + filePath);
            free(svg_data);
            return false;
        }

        rgba_data = SVGToRGBA(svg_data, file_size, width, height);
        free(svg_data);

        if (!rgba_data) {
            Log::logWarning("Failed to decode SVG: " + filePath);
            return false;
        }
    } else {
        int channels;
        rgba_data = stbi_load_from_file(file, &width, &height, &channels, 4);
        fclose(file);

        if (!rgba_data) {
            Log::logWarning("Failed to decode image: " + filePath);
            return false;
        }
    }

    // Create an Allegro BITMAP
    BITMAP *bmp = create_bitmap_ex(32, width, height);
    if (!bmp) {
        Log::logWarning("Failed to create bitmap for: " + filePath);
        free(rgba_data);
        return false;
    }

    // Copy raw RGBA data to the BITMAP
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            unsigned char *p = rgba_data + (y * width + x) * 4;
            // Allegro's makeacol32 expects RGBA
            int color = makeacol32(p[0], p[1], p[2], p[3]);
            putpixel(bmp, x, y, color);
        }
    }
    free(rgba_data);

    // Build AllegroImage object
    AllegroImage *image = MemoryTracker::allocate<AllegroImage>();
    new (image) AllegroImage();
    image->sprite = bmp;
    image->width = width;
    image->height = height;
    image->renderWidth = width;
    image->renderHeight = height;

    // calculate VRAM usage
    image->memorySize = bitmap_color_depth(bmp) / 8 * width * height;
    MemoryTracker::allocateVRAM(image->memorySize);

    images[imgId] = image;
    return true;
}

/**
 * Loads a single image from a Scratch sb3 zip file by filename.
 * @param zip Pointer to the zip archive
 * @param costumeId The filename of the image to load (e.g., "sprite1.png")
 */
void Image::loadImageFromSB3(mz_zip_archive *zip, const std::string &costumeId) {
    std::string imgId = costumeId.substr(0, costumeId.find_last_of('.'));
    if (images.find(imgId) != images.end()) return;

    // Log::log("Loading single image: " + costumeId);

    // Find the file in the zip
    int file_index = mz_zip_reader_locate_file(zip, costumeId.c_str(), nullptr, 0);
    if (file_index < 0) {
        Log::logWarning("Image file not found in zip: " + costumeId);
        return;
    }

    // Get file stats
    mz_zip_archive_file_stat file_stat;
    if (!mz_zip_reader_file_stat(zip, file_index, &file_stat)) {
        Log::logWarning("Failed to get file stats for: " + costumeId);
        return;
    }

    // Check if file is bitmap or SVG
    bool isBitmap = costumeId.size() > 4 && ([](std::string ext) {
                        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                        return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" ||
                               ext == ".bmp" || ext == ".psd" || ext == ".gif" || ext == ".hdr" ||
                               ext == ".pic" || ext == ".ppm" || ext == ".pgm";
                    }(costumeId.substr(costumeId.find_last_of('.'))));
    bool isSVG = costumeId.size() >= 4 &&
                 (costumeId.substr(costumeId.size() - 4) == ".svg" ||
                  costumeId.substr(costumeId.size() - 4) == ".SVG");

    if (!isBitmap && !isSVG) {
        Log::logWarning("File is not a supported image format: " + costumeId);
        return;
    }

    // Extract file data
    size_t file_size;
    void *file_data = mz_zip_reader_extract_to_heap(zip, file_index, &file_size, 0);
    if (!file_data) {
        Log::logWarning("Failed to extract: " + costumeId);
        return;
    }

    // Use stb_image to load image from memory
    int width, height;
    unsigned char *rgba_data = nullptr;

    if (isSVG) {
        rgba_data = SVGToRGBA(file_data, file_size, width, height);
        mz_free(file_data);
        if (!rgba_data) {
            Log::logWarning("Failed to decode SVG: " + costumeId);
            Image::cleanupImages();
            return;
        }
    } else {
        int channels;
        rgba_data = stbi_load_from_memory(
            (unsigned char *)file_data, file_size,
            &width, &height, &channels, 4);
        mz_free(file_data);

        if (!rgba_data) {
            Log::logWarning("Failed to decode image: " + costumeId);
            Image::cleanupImages();
            return;
        }
    }

    // Create an Allegro BITMAP
    BITMAP *bmp = create_bitmap_ex(32, width, height);
    if (!bmp) {
        Log::logWarning("Failed to create bitmap for: " + costumeId);
        free(rgba_data);
        return;
    }

    // Copy raw RGBA data to the BITMAP
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            unsigned char *p = rgba_data + (y * width + x) * 4;
            // Allegro's makeacol32 expects RGBA
            int color = makeacol32(p[0], p[1], p[2], p[3]);
            putpixel(bmp, x, y, color);
        }
    }
    free(rgba_data);

    // Build AllegroImage object
    AllegroImage *image = MemoryTracker::allocate<AllegroImage>();
    new (image) AllegroImage();
    image->sprite = bmp;
    image->width = width;
    image->height = height;
    image->renderWidth = width;
    image->renderHeight = height;

    // calculate VRAM usage
    image->memorySize = bitmap_color_depth(bmp) / 8 * width * height;
    MemoryTracker::allocateVRAM(image->memorySize);

    // Log::log("Successfully loaded image: " + costumeId);
    images[imgId] = image;
}

/**
 * Loads SVG data and converts it to RGBA pixel data
 */
unsigned char *SVGToRGBA(const void *svg_data, size_t svg_size, int &width, int &height) {
    // Create a null-terminated string from the SVG data
    char *svg_string = (char *)malloc(svg_size + 1);
    if (!svg_string) {
        Log::logWarning("Failed to allocate memory for SVG string");
        return nullptr;
    }
    memcpy(svg_string, svg_data, svg_size);
    svg_string[svg_size] = '\0';

    // Parse SVG
    NSVGimage *image = nsvgParse(svg_string, "px", 96.0f);
    free(svg_string);

    if (!image) {
        Log::logWarning("Failed to parse SVG");
        return nullptr;
    }

    // Determine render size
    if (image->width > 0 && image->height > 0) {
        width = (int)image->width;
        height = (int)image->height;
    } else {
        width = 32;
        height = 32;
    }

    // Create rasterizer
    NSVGrasterizer *rast = nsvgCreateRasterizer();
    if (!rast) {
        Log::logWarning("Failed to create SVG rasterizer");
        nsvgDelete(image);
        return nullptr;
    }

    // Allocate RGBA buffer
    unsigned char *rgba_data = (unsigned char *)malloc(width * height * 4);
    if (!rgba_data) {
        Log::logWarning("Failed to allocate RGBA buffer for SVG");
        nsvgDeleteRasterizer(rast);
        nsvgDelete(image);
        return nullptr;
    }

    // Calculate scale
    float scale = 1.0f;
    if (image->width > 0 && image->height > 0) {
        float scaleX = (float)width / image->width;
        float scaleY = (float)height / image->height;
        scale = std::min(scaleX, scaleY);
    }

    // Rasterize SVG
    nsvgRasterize(rast, image, 0, 0, scale, rgba_data, width, height, width * 4);

    // Clean up
    nsvgDeleteRasterizer(rast);
    nsvgDelete(image);

    return rgba_data;
}

void Image::cleanupImages() {
    for (auto &[id, image] : images) {
        if (image->memorySize > 0) {
            MemoryTracker::deallocateVRAM(image->memorySize);
        }
        // delete image;
        image->~AllegroImage();
        MemoryTracker::deallocate<AllegroImage>(image);
    }
    images.clear();
    toDelete.clear();
}

/**
 * Frees an `AllegroImage` from memory using a `costumeId` to find it.
 * @param costumeId
 */
void Image::freeImage(const std::string &costumeId) {
    auto imageIt = images.find(costumeId);
    if (imageIt != images.end()) {
        AllegroImage *image = imageIt->second;

        // Log::log("Freed image " + costumeId);
        //  Call destructor and deallocate AllegroImage
        image->~AllegroImage();
        MemoryTracker::deallocate<AllegroImage>(image);

        images.erase(imageIt);
    }
}

/**
 * Checks every `AllegroImage` in memory to see if they can be freed.
 * An `AllegroImage` will get freed if it goes unused for 120 frames.
 */
void Image::FlushImages() {

    // Free images if ram usage is too high
    if (MemoryTracker::getVRAMUsage() + MemoryTracker::getCurrentUsage() > MemoryTracker::getMaxVRAMUsage() * 0.8) {

        size_t times = 0;
        while (MemoryTracker::getVRAMUsage() + MemoryTracker::getCurrentUsage() > MemoryTracker::getMaxVRAMUsage() * 0.5 && !images.empty()) {
            AllegroImage *imgToDelete = nullptr;
            std::string toDeleteStr = "";

            for (auto &[id, img] : images) {
                if (imgToDelete == nullptr && img->freeTimer != img->maxFreeTime) {
                    imgToDelete = img;
                    toDeleteStr = id;
                    continue;
                }
                if (imgToDelete != nullptr && img->freeTimer < imgToDelete->freeTimer && img->freeTimer != img->maxFreeTime) {
                    imgToDelete = img;
                    toDeleteStr = id;
                }
            }

            if (toDeleteStr != "") {
                Image::freeImage(toDeleteStr);
            } else {
                break;
            }
            times++;
            if (times > 15) break;
        }
    } else {
        // Free images based on a timer
        for (auto &[id, img] : images) {
            if (img->freeTimer <= 0) {
                toDelete.push_back(id);
            } else {
                img->freeTimer -= 1;
            }
        }

        for (const std::string &id : toDelete) {
            Image::freeImage(id);
        }
        toDelete.clear();
    }
}

AllegroImage::AllegroImage() {}

AllegroImage::AllegroImage(std::string filePath) {
    sprite = load_bitmap(filePath.c_str(), NULL);
    if (!sprite) {
        Log::logWarning(std::string("Error loading image: ") + allegro_error);
        return;
    }

    width = sprite->w;
    height = sprite->h;
    renderWidth = sprite->w;
    renderHeight = sprite->h;
    memorySize = bitmap_color_depth(sprite) / 8 * width * height;
    MemoryTracker::allocateVRAM(memorySize);
}

/**
 * currently does nothing in the Allegro version 😁😁
 */
void Image::queueFreeImage(const std::string &costumeId) {
    toDelete.push_back(costumeId);
}

AllegroImage::~AllegroImage() {
    MemoryTracker::deallocateVRAM(memorySize);
    if (sprite) {
        destroy_bitmap(sprite);
    }
}

void AllegroImage::setScale(float amount) {
    scale = amount;
    renderWidth = width * amount;
    renderHeight = height * amount;
}

void AllegroImage::setRotation(float rotate) {
    rotation = rotate;
}
