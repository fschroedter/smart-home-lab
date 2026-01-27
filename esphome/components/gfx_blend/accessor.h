/**
 * GfxBlend: Extended Blending for ESPHome
 *
 * @brief Gfx blend shapes engine for ESPHome displays.
 * This library extends ESPHome's display capabilities by adding
 * transparency (alpha blending) and advanced geometric shapes
 * like rounded rectangles, ellipses, and gradients.
 *
 * @author fschroedter
 * @copyright MIT License
 * @note This library requires a 16-bit color display (RGB565).
 */

#pragma once
#include "esphome/core/color.h"

#include "esphome/components/display/display.h"
#include "esphome/components/display/display_buffer.h"

namespace esphome {
namespace gfx_blend {
/**
 * Helper class to access protected members of esphome::display::DisplayBuffer.
 */
class DisplayBufferAccessor : public esphome::display::DisplayBuffer {
public:
  // Access to the raw underlying frame buffer
  inline static uint8_t HOT* get_raw_buffer(esphome::display::DisplayBuffer* disp)
  {
    return static_cast<DisplayBufferAccessor*>(disp)->buffer_;
  }

  // Access to internal dimensions (ignoring rotation)
  inline static int HOT get_internal_w(esphome::display::DisplayBuffer* disp)
  {
    return static_cast<DisplayBufferAccessor*>(disp)->get_width_internal();
  }

  inline static int HOT get_internal_h(esphome::display::DisplayBuffer* disp)
  {
    return static_cast<DisplayBufferAccessor*>(disp)->get_height_internal();
  }

  // Access to native hardware dimensions (required for rotation coordinate mapping)
  inline static int HOT get_native_w(esphome::display::DisplayBuffer* disp)
  {
    return static_cast<DisplayBufferAccessor*>(disp)->get_native_width();
  }

  inline static int HOT get_native_h(esphome::display::DisplayBuffer* disp)
  {
    return static_cast<DisplayBufferAccessor*>(disp)->get_native_height();
  }

  // Virtual overrides to satisfy the compiler for an instantiable subclass
  void draw_absolute_pixel_internal(int x, int y, esphome::Color color) override {}

  esphome::display::DisplayType get_display_type() override { return esphome::display::DISPLAY_TYPE_COLOR; }
};

}  // namespace gfx_blend
}  // namespace esphome