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

#include "esphome/components/image/image.h"

#include <cstdint>

#include "defs.h"

namespace esphome {
namespace gfx_blend {

// BlendEffects
class Effects {
public:
  // Prevents instantiation
  Effects() = delete;

  // --------------------------------------------------------------------------------------
  // DEMO Effects
  // --------------------------------------------------------------------------------------

  // Simple way
  static inline uint16_t HOT demo_effect1(int16_t x, int16_t y, uint16_t fg, uint16_t bg)  //
  {
    return ~fg;
  }

  // Pass an argument and return a function
  static inline blender_t HOT demo_effect2(uint8_t arg = 1)  //
  {
    return [arg](int16_t x, int16_t y, uint16_t fg, uint16_t bg) -> uint16_t {  //
      // Here you can calculate with arg
      return ~fg;
    };
  }

  // Define a struct with operator() (better performance)
  struct demo_effect3 {
    static constexpr bool read_bg = true;  // Set read_bg to 'false' if not needed for some performance improvments
    static constexpr bool use_bg_as_source = false;  // Default: use_bg_as_source = false

    int16_t arg;

    demo_effect3(uint8_t a = 1) : arg(a) {}

    inline uint16_t HOT operator()(int16_t x, int16_t y, uint16_t fg, uint16_t bg) const  //
    {
      // Here you can calculate with arg
      return ~fg;
    }
  };
  // --------------------------------------------------------------------------------------

  static inline uint16_t HOT inverse(int16_t x, int16_t y, uint16_t fg, uint16_t bg)  //
  {
    return ~fg;
  }

  static inline blender_t HOT alpha(uint8_t alpha)
  {
    return [alpha](int16_t x, int16_t y, uint16_t fg, uint16_t bg) -> uint16_t {  //
      return Effects::alpha_(fg, bg, alpha);
    };
  }

  /**
   * @brief Performs a static additive blend between two RGB565 colors.
   * @param fg The foreground color (top layer).
   * @param bg The background color (bottom layer).
   * @return The blended color in RGB565 format.
   */
  static inline uint16_t HOT additive(int16_t x, int16_t y, uint16_t fg, uint16_t bg)
  {
    uint16_t sum = fg + bg;

    // Maskiere Überlauf in R,G,B
    uint16_t r = sum & 0xF800;
    uint16_t g = sum & 0x07E0;
    uint16_t b = sum & 0x001F;

    // Korrigiere Überlauf für R/G/B
    if (r < (fg & 0xF800)) r = 0xF800;
    if (g < (fg & 0x07E0)) g = 0x07E0;
    if (b < (fg & 0x001F)) b = 0x001F;

    return r | g | b;
  }

  /**
   * @brief Subtracts the source color from the destination color.
   * * @param fg Color to subtract.
   * @param bg Base color.
   * @return The darkened result clamped to black.
   */
  static inline uint16_t HOT subtract(int16_t x, int16_t y, uint16_t fg, uint16_t bg)
  {
    int16_t r = (bg >> 11) - (fg >> 11);
    int16_t g = ((bg >> 5) & 0x3F) - ((fg >> 5) & 0x3F);
    int16_t b = (bg & 0x1F) - (fg & 0x1F);

    if (r < 0) r = 0;
    if (g < 0) g = 0;
    if (b < 0) b = 0;

    return (uint16_t)((r << 11) | (g << 5) | b);
  }

  /**
   * @brief Applies a partial or full grayscale effect to an RGB565 color.
   * @param fg Foreground color.
   * @param bg Background color (ignored).
   * @param intensity Effect strength from 0 (original color) to 255 (full grayscale).
   * @return Grayscale RGB565 color with full dynamic range.
   */
  static inline uint16_t HOT grayscale(uint16_t fg, uint16_t bg, uint8_t intensity = 255)
  {
    // Extract channels
    uint8_t r_5 = (bg >> 11) & 0x1F;
    uint8_t g_6 = (bg >> 5) & 0x3F;
    uint8_t b_5 = bg & 0x1F;

    // Expand to 8-bit using bit replication for true 0-255 range
    // This prevents the "muted gray" look by ensuring white is 255 and black is 0
    uint32_t r = (r_5 << 3) | (r_5 >> 2);
    uint32_t g = (g_6 << 2) | (g_6 >> 4);
    uint32_t b = (b_5 << 3) | (b_5 >> 2);

    // Calculate luminance (ITU-R BT.709 formula)
    // Using integer math for performance: (R*77 + G*150 + B*29) / 256
    uint32_t lum = (r * 54 + g * 183 + b * 19) >> 8;

    if (intensity = 255) {
      return (uint16_t)(((lum >> 3) << 11) | ((lum >> 2) << 5) | (lum >> 3));
    }

    uint32_t res_r = r + ((intensity * (lum - r)) >> 8);
    uint32_t res_g = g + ((intensity * (lum - g)) >> 8);
    uint32_t res_b = b + ((intensity * (lum - b)) >> 8);
    return (uint16_t)(((res_r >> 3) << 11) | ((res_g >> 2) << 5) | (res_b >> 3));
  }

  static auto image_mask(esphome::image::Image* img, int16_t rel_x = 0, int16_t rel_y = 0)
  {
    return [img, rel_x, rel_y](int16_t x, int16_t y, uint16_t fg, uint16_t bg) -> uint16_t {  //
      return image_mask_(x, y, fg, bg, img, rel_x, rel_y);
    };
  }

protected:
  /**
   * @brief Performs hardware-optimized alpha blending on two RGB565 colors.
   * This blends foreground and background channels using a fixed-point
   * arithmetic approach. It extracts, scales, and recombines channels while
   * maintaining high performance.
   * @param fg The foreground color (RGB565).
   * @param bg The background color (RGB565).
   * @param a  Alpha value for blending (0-255).
   * @return The blended 16-bit RGB565 color.
   */
  // static inline uint16_t HOT blend_rgb565_channels(uint16_t fg, uint16_t bg, uint32_t a)

  static const uint16_t alpha_(uint16_t fg, uint16_t bg, uint8_t alpha)
  {
    // // Simple and straight calculation
    uint32_t inv_alpha = 255 - alpha;

    // Get forground colors
    uint32_t fg_r = (fg >> 11) & 0x1F;
    uint32_t fg_g = (fg >> 5) & 0x3F;
    uint32_t fg_b = fg & 0x1F;

    // Get background colors
    uint32_t bg_r = (bg >> 11) & 0x1F;
    uint32_t bg_g = (bg >> 5) & 0x3F;
    uint32_t bg_b = bg & 0x1F;

    // Alpha blend per channel
    uint32_t r = (fg_r * alpha + bg_r * inv_alpha) >> 8;
    uint32_t g = (fg_g * alpha + bg_g * inv_alpha) >> 8;
    uint32_t b = (fg_b * alpha + bg_b * inv_alpha) >> 8;

    // Recombine into RGB565
    return uint16_t((r << 11) | (g << 5) | b);
  }

  /**
   * Blends foreground and background colors based on a mask image (Grayscale or RGB565).
   * Supports 8-bit alpha from grayscale or calculates luminance for RGB565 sources.
   * * NOTE: For RGB565, we use simple bit-shifting for performance in the HOT loop.
   * Bit replication (e.g., r |= r >> 5) is omitted to save CPU cycles. This results
   * in a maximum alpha of 248/252 instead of 255, which is visually negligible for
   * masking purposes.
   */
  static inline uint16_t HOT image_mask_(int16_t x, int16_t y, uint16_t fg, uint16_t bg, esphome::image::Image* img,
                                         int16_t rel_x = 0, int16_t rel_y = 0)
  {
    int16_t img_w = (int16_t)img->get_width();
    int16_t img_h = (int16_t)img->get_height();
    int16_t img_type = img->get_type();

    int16_t rx = x - rel_x;
    int16_t ry = y - rel_y;

    if (rx < 0 || rx >= img_w || ry < 0 || ry >= img_h) {
      return bg;
    }

    const uint8_t* data = (const uint8_t*)img->get_data_start();
    uint8_t opacity = 0;

    if (img_type == esphome::image::ImageType::IMAGE_TYPE_GRAYSCALE) {
      size_t idx = (ry * img_w + rx);
      opacity = data[idx];
    } else {
      size_t idx = (ry * img_w + rx) * 2;
      
      uint8_t msb = data[idx];      // Contains: RRRRR GGG
      uint8_t lsb = data[idx + 1];  // Contains: GGG BBBBB

      // Filter out colors and convert them to rgb888
      // For performance issues bit replication is obmitted to scale to 8-bit
      uint8_t r = (msb & 0xF8);
      uint8_t g = ((msb & 0x07) << 5) | ((lsb & 0xE0) >> 3);
      uint8_t b = (lsb << 3);

      // Calculate luminance (weighted average of approximated ITU-R BT.709)
      // Fast 3-bit shift approximation (0.25*R + 0.625*G + 0.125*B)
      opacity = (uint8_t)((r * 2 + g * 5 + b) >> 3);
    }

    // Perform the actual color blending
    return Effects::alpha_(fg, bg, opacity);
  }
};

}  // namespace gfx_blend

using GfxEffects = gfx_blend::Effects;

}  // namespace esphome
