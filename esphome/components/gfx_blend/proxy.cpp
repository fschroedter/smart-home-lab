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
#include "proxy.h"

#include "esphome/core/color.h"

#include "esphome/components/display/display.h"
#include "esphome/components/display/display_buffer.h"

namespace esphome {
namespace gfx_blend {

/**
 * Proxy pixel redirector
 * Intercept every single pixel drawn by high-level functions
 */
template <typename TBlender>
inline void HOT GfxProxy<TBlender>::draw_pixel_at(int x, int y, esphome::Color color)
{
  // Read the color from the actual display (background)
  uint16_t fg = display::ColorUtil::color_to_565(color, display::ColorOrder::COLOR_ORDER_RGB);

  // Blending
  uint16_t final_color = this->blender_(x, y, fg);

  // Write back
  this->real_display_->draw_pixel_at(x, y, rgb565_to_color(final_color));
}

/**
 * Horizontal line redirector
 * Prevent horizontal lines from being processed pixel by pixel over the slow standard base class
 */
template <typename TBlender>
inline void HOT GfxProxy<TBlender>::horizontal_line(int x, int y, int width, esphome::Color color)
{
  for (int i = 0; i < width; i++) draw_pixel_at(x + i, y, color);
}

/**
 * Vertical line redirector
 * Prevent vertical lines from being processed pixel by pixel over the slow standard base class
 */
template <typename TBlender>
inline void HOT GfxProxy<TBlender>::vertical_line(int x, int y, int height, esphome::Color color)
{
  for (int i = 0; i < height; i++) draw_pixel_at(x, y + i, color);
}

// Delegate essential display properties to the real display
template <typename TBlender>
esphome::display::DisplayType GfxProxy<TBlender>::get_display_type()
{
  return real_display_->get_display_type();
}

template <typename TBlender>
inline int HOT GfxProxy<TBlender>::get_width_internal()
{
  return DisplayBufferAccessor::get_internal_w(real_display_);
}

template <typename TBlender>
inline int HOT GfxProxy<TBlender>::get_height_internal()
{
  return DisplayBufferAccessor::get_internal_h(real_display_);
}

// Directly pass through full-display operations
template <typename TBlender>
void GfxProxy<TBlender>::fill(esphome::Color color)
{
  real_display_->fill(color);
}

}  // namespace gfx_blend
}  // namespace esphome