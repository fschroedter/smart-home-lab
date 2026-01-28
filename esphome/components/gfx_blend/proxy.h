/**
 * Graphics extension for rendering blended shapes on ESPHome displays.
 *
 * @brief This library extends ESPHome's native display capabilities with support for 
 * alpha blending (transparency), image masking, and advanced geometric primitives 
 * including rounded rectangles, ellipses, and color gradients.
 * 
 * @author fschroedter
 * @copyright MIT License
 * @note This library requires a 16-bit color display (RGB565).
 */

#pragma once
#include "esphome/core/color.h"

#include "esphome/components/display/display.h"
#include "esphome/components/display/display_buffer.h"

#include "accessor.h"
#include "defs.h"

namespace esphome {
namespace gfx_blend {

/**
 * Proxy class that redirects high-level ESPHome drawing commands (like filled_rectangle)
 * through custom draw_pixel_at logic.
 */
template <typename TBlender>
class GfxProxy : public esphome::display::DisplayBuffer {
public:
  GfxProxy(esphome::display::DisplayBuffer* real_display, TBlender b)
      : real_display_(real_display), blender_(std::move(b))
  {
  }

  inline void HOT draw_pixel_at(int x, int y, esphome::Color color) override;
  inline void HOT horizontal_line(int x, int y, int width, esphome::Color color);
  inline void HOT vertical_line(int x, int y, int height, esphome::Color color);
  inline int HOT get_width_internal() override;
  inline int HOT get_height_internal() override;
  esphome::display::DisplayType get_display_type() override;
  void draw_absolute_pixel_internal(int x, int y, esphome::Color color) override {}
  void fill(esphome::Color color);  // override { real_display_->fill(color); }
  void update() override {}

protected:
  esphome::display::DisplayBuffer* real_display_;

  /**
   * @brief The functional core of an individual pipeline step.
   * This member stores the specific blending logic for a single effect.
   * While the 'pipeline_' vector manages the sequence of operations, this
   * variable holds the actual callable (functor or lambda) that processes
   * the pixels for this specific stage.
   * In a multi-effect setup, each 'GfxPipelineStep' instance possesses its
   * own 'blender_' member, ensuring that effects remain modular and
   * can be executed sequentially.
   */
  TBlender blender_;
};

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