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

#include "accessor.h"

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

}  // namespace gfx_blend
}  // namespace esphome