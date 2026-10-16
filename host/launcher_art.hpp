#pragma once
namespace portablekit::fu {
void draw_launcher_background();
bool draw_launcher_logo(float x, float y, float width, float height);
// Opaque loading-screen scenery and main logo, above the guest framebuffer.
void draw_loading_art(float x, float y, float width, float height);
}
