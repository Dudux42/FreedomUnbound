#include "launcher_art.hpp"
#if defined(PORTABLEKIT_HAS_RENDERER)
#include "imgui.h"
#include "imgui_internal.h"
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "../portablekit/third_party/stb_image.h"
#include "launcher_art_data.hpp"
#include "launcher_logo_data.hpp"
#include "credit_logo_data.hpp"
#include "ui/widgets.hpp"
#include <algorithm>
#include <cstring>

namespace portablekit::fu {
static bool draw_logo(float x, float y, float width, float height, ImDrawList *draw, bool show_credit) {
    static ImTextureData *texture = nullptr;
    static bool attempted = false;
    if (!attempted) {
        attempted = true;
        int w = 0, h = 0, channels = 0;
        auto *pixels = stbi_load_from_memory(kLauncherLogo, sizeof(kLauncherLogo), &w, &h, &channels, 4);
        if (pixels) {
            texture = new ImTextureData();
            texture->Create(ImTextureFormat_RGBA32, w, h);
            std::memcpy(texture->GetPixels(), pixels, static_cast<size_t>(w) * h * 4);
            stbi_image_free(pixels);
            ImGui::RegisterUserTexture(texture);
        }
    }
    // "Launcher by Team Wyvern Works": small credit at the top-left of the
    // logo area, above the main logo. Text only if the PNG fails to decode.
    static ImTextureData *credit = nullptr;
    static bool credit_attempted = false;
    if (!credit_attempted) {
        credit_attempted = true;
        int w = 0, h = 0, channels = 0;
        auto *pixels = stbi_load_from_memory(kCreditLogo, sizeof(kCreditLogo), &w, &h, &channels, 4);
        if (pixels) {
            credit = new ImTextureData();
            credit->Create(ImTextureFormat_RGBA32, w, h);
            std::memcpy(credit->GetPixels(), pixels, static_cast<size_t>(w) * h * 4);
            stbi_image_free(pixels);
            ImGui::RegisterUserTexture(credit);
        }
    }
    if (show_credit) {
        static constexpr const char *kCredit = "Launcher by Team Wyvern Works";
        const float text_size = std::max(13.0f, ImGui::GetFontSize() * 0.78f);
        const float icon = std::min(text_size * 2.4f, height * 0.22f);
        const float gap = text_size * 0.45f;
        float text_x = x;
        if (credit && icon >= text_size) {
            const float aspect = static_cast<float>(credit->Width) / credit->Height;
            draw->AddImage(credit->GetTexRef(), {x, y}, {x + icon * aspect, y + icon});
            text_x = x + icon * aspect + gap;
        }
        const float strip = std::max(icon, text_size);
        draw->AddText(ImGui::GetFont(), text_size, {text_x, y + (strip - text_size) * 0.5f},
            portablekit::ui::colors::kTextDim, kCredit);
        y += strip + gap;
        height -= strip + gap;
    }
    if (!texture) return false;
    const float scale = std::min(width / texture->Width, height / texture->Height);
    const ImVec2 extent{texture->Width * scale, texture->Height * scale};
    const ImVec2 at{x + (width - extent.x) * 0.5f, y + (height - extent.y) * 0.5f};
    draw->AddImage(texture->GetTexRef(), at, {at.x + extent.x, at.y + extent.y});
    return true;
}
static void draw_background(ImDrawList *draw) {
    static ImTextureData *texture = nullptr;
    static bool attempted = false;
    if (!attempted) {
        attempted = true;
        int width = 0, height = 0, channels = 0;
        auto *pixels = stbi_load_from_memory(kLauncherArt, sizeof(kLauncherArt), &width, &height, &channels, 4);
        if (pixels) {
            texture = new ImTextureData();
            texture->Create(ImTextureFormat_RGBA32, width, height);
            std::memcpy(texture->GetPixels(), pixels, static_cast<size_t>(width) * height * 4);
            stbi_image_free(pixels);
            ImGui::RegisterUserTexture(texture);
        }
    }
    const ImVec2 size = ImGui::GetIO().DisplaySize;
    draw->AddRectFilled({0, 0}, size, IM_COL32(20, 35, 45, 255));
    if (!texture) return;
    const float scale = std::max(size.x / texture->Width, size.y / texture->Height);
    const ImVec2 extent{texture->Width * scale, texture->Height * scale};
    const ImVec2 at{(size.x - extent.x) * 0.5f, (size.y - extent.y) * 0.5f};
    draw->AddImage(texture->GetTexRef(), at, {at.x + extent.x, at.y + extent.y});
}
bool draw_launcher_logo(float x, float y, float width, float height) {
    return draw_logo(x,y,width,height,ImGui::GetBackgroundDrawList(),true);
}
void draw_launcher_background() { draw_background(ImGui::GetBackgroundDrawList()); }
void draw_loading_art(float x, float y, float width, float height) {
    auto *draw=ImGui::GetForegroundDrawList();
    draw_background(draw);
    draw->AddRectFilled({0,0},ImGui::GetIO().DisplaySize,IM_COL32(5,14,21,75));
    draw_logo(x,y,width,height,draw,false);
}
}
#else
namespace portablekit::fu { void draw_launcher_background() {} }
namespace portablekit::fu { bool draw_launcher_logo(float, float, float, float) { return false; } }
namespace portablekit::fu { void draw_loading_art(float,float,float,float) {} }
#endif
