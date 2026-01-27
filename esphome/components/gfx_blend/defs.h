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

#include <cstdint>
#include <functional>
#include <type_traits>
#include <vector>

namespace esphome {
namespace gfx_blend {
/**
 * The fallback type for lists and generic storage.
 * Keeping `std::function` here for `initializer_list` compatibility.
 */
using blender_t = std::function<uint16_t(int16_t x, int16_t y, uint16_t fg, uint16_t bg)>;

/**
 * Wrapper for effects that do not require background read access.
 * Statically marks the type with needs_bg to enable hardware optimizations.
 */
template <typename T>
struct NoBgWrapper {
  static constexpr bool read_bg = false;
  T func;  // Can be a single lambda or a std::vector<blender_t>

  uint16_t operator()(int16_t x, int16_t y, uint16_t fg, uint16_t bg) const
  {
    if constexpr (std::is_same_v<T, std::vector<blender_t>>) {
      uint16_t res = fg;
      for (auto const& f : func) res = f(x, y, res, bg);
      return res;
    } else {
      return func(x, y, fg, bg);
    }
  }
};

/**
 * This is where make_effect_no_bg is declared.
 * It takes a function/lambda and returns it wrapped in a NoBgWrapper.
 */
template <typename F>
auto make_effect_no_bg(F&& func)
{
  return NoBgWrapper<std::decay_t<F>>(std::forward<F>(func));
}

/**
 * Wrapper that sets the background as the primary source (fg) for the pipeline.
 * Forces background read access and marks the type statically.
 */
template <typename T>
struct BgAsSourceWrapper {
  static constexpr bool use_bg_as_source = false;
  T func;

  uint16_t operator()(int16_t x, int16_t y, uint16_t fg, uint16_t bg) const
  {
    if constexpr (std::is_same_v<T, std::vector<blender_t>>) {
      // Starting value for the chain: bg becomes fg of the first effect
      uint16_t current_fg = bg;
      for (auto const& f : func) {
        // Each effect uses the result of the previous one as the new foreground
        current_fg = f(x, y, current_fg, bg);
      }
      return current_fg;
    } else {
      // Single effect: No auxiliary variable is needed here
      return func(x, y, bg, bg);
    }
  }
};

/**
 * Converts RGB565 to ESPHome Color using bit replication for scaling.
 * Faster than ColorUtil and prevents brightness loss by accurately scaling
 * 5/6-bit channels to 8-bit (e.g., 0xFFFF becomes pure white).
 * @param rgb565 The native 16-bit display color value.
 * @return The converted esphome::Color object.
 */
inline HOT esphome::Color rgb565_to_color(uint16_t rgb565)
{
  // Extract channels
  uint8_t r5 = (rgb565 >> 11) & 0x1F;
  uint8_t g6 = (rgb565 >> 5) & 0x3F;
  uint8_t b5 = rgb565 & 0x1F;

  // Scale to 8-bit using bit replication
  uint8_t r8 = (r5 << 3) | (r5 >> 2);
  uint8_t g8 = (g6 << 2) | (g6 >> 4);
  uint8_t b8 = (b5 << 3) | (b5 >> 2);

  return esphome::Color(r8, g8, b8);
}

}  // namespace gfx_blend
}  // namespace esphome