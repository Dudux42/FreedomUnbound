#pragma once
#include "save_preview.hpp"
namespace portablekit { struct CameraDriver; }
namespace portablekit::fu {
// A profile adapter retains the existing camera and observes its once-per-flip callback.
const CameraDriver *portrait_camera_driver();
void portrait_save_seen(const SavePreview &);
void draw_save_portrait(unsigned slot,const HunterSummary &,float x,float y,float width,float height);
}
