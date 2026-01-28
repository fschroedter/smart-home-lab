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

#include "esphome/components/display/display_buffer.h"

namespace esphome {
namespace gfx_blend {

template <typename TBlender>
class GfxProxy;

// using esphome::gfx_blend::GfxProxy;

enum GradientDirection { GRADIENT_HORIZONTAL, GRADIENT_VERTICAL };

/**
 * Mix-in class containing drawing algorithms.
 * T is the class that actually implements draw_blend_pixel_at (the Canvas).
 */

template <typename T>
class GfxShapes {
public:
  // ============================================================
  // ESPHome Display Extensions
  // ============================================================

  T& filled_rectangle(int x, int y, int w, int h, esphome::Color c)
  {
    return static_cast<T&>(*this).draw_generic([&](auto& it) {  //
      it.filled_rectangle(x, y, w, h, c);
    });
  }

  // Standard Circle
  T& filled_circle(int x, int y, int radius, esphome::Color c)
  {
    return static_cast<T&>(*this).draw_generic([&](auto& it) {  //
      it.filled_circle(x, y, radius, c);
    });
  }

  T& filled_ring(int center_x, int center_y, int radius1, int radius2, esphome::Color c)
  {
    return static_cast<T&>(*this).draw_generic([&](auto& it) {  //
      it.filled_ring(center_x, center_y, radius1, radius2, c);
    });
  }

  T& filled_triangle(int x1, int y1, int x2, int y2, int x3, int y3, esphome::Color c)
  {
    return static_cast<T&>(*this).draw_generic([&](auto& it) {  //
      it.filled_triangle(x1, y1, x2, y2, x3, y3, c);
    });
  }

  T& print(int x, int y, esphome::display::BaseFont* font, esphome::Color color, const char* text,
           esphome::Color background = esphome::Color())
  {
    return static_cast<T&>(*this).draw_generic([&](auto& it) {  //
      it.print(x, y, font, color, esphome::display::TextAlign::TOP_LEFT, text, background);
    });
  }

  // ============================================================
  // Shorthand aliases for common drawing operations
  // ============================================================

  // --- Rectangle ---------------------------------------------
  // Rounded Rectangle(Overload by adding r)
  T& filled_rectangle(int x, int y, int w, int h, int r, esphome::Color c)
  {
    return static_cast<T&>(*this).draw_generic([&](auto& it) {  //
      this->filled_round_rectangle(it, x, y, w, h, r, c);
    });
  }

  // Gradient Rectangle (Overload by adding second color and direction)
  T& filled_rectangle(int x, int y, int w, int h, esphome::Color c1, esphome::Color c2, GradientDirection dir)
  {
    return static_cast<T&>(*this).draw_generic([&](auto& it) {  //
      this->filled_rectangle_gradient(it, x, y, w, h, c1, c2, dir);
    });
  }

  // Rounded Gradient Rectangle (Overload with 'r', two colors and direction)
  T& filled_rectangle(int x, int y, int w, int h, int r, esphome::Color c1, esphome::Color c2,
                      GradientDirection dir = GRADIENT_HORIZONTAL)
  {
    return static_cast<T&>(*this).draw_generic([&](auto& it) {  //
      filled_round_rectangle_gradient(it, x, y, w, h, r, c1, c2, dir);
    });
  }

  // --- Circle / Ellipse ---------------------------------------

  // Reuse ellipse gradient logic with equal radii for a perfect circle
  T& filled_circle(int x, int y, int radius, esphome::Color c1, esphome::Color c2,
                   GradientDirection dir = GRADIENT_HORIZONTAL)
  {
    return static_cast<T&>(*this).draw_generic([&](auto& it) {  //
      filled_ellipse_gradient(it, x, y, radius, radius, c1, c2, dir);
    });
  }

  // Ellipse (Overload by providing rx and ry instead of a single radius)
  T& filled_circle(int x, int y, int rx, int ry, esphome::Color c)
  {
    return static_cast<T&>(*this).draw_generic([&](auto& it) {  //
      filled_ellipse(it, x, y, rx, ry, c);
    });
  }

  // Ellipse Gradient (Overload with rx, ry, two colors and direction)
  T& filled_circle(int x, int y, int rx, int ry, esphome::Color c1, esphome::Color c2,
                   GradientDirection dir = GRADIENT_HORIZONTAL)
  {
    return static_cast<T&>(*this).draw_generic([&](auto& it) {  //
      filled_ellipse_gradient(it, x, y, rx, ry, c1, c2, dir);
    });
  }

  // ============================================================
  // Custom-defined shapes
  // ============================================================
  /**
   * Draws a filled rectangle with rounded corners using alpha blending.
   * @param r Radius of the corners
   */

  template <typename T_TARGET>
  static void filled_round_rectangle(T_TARGET& it, int x, int y, int w, int h, int r, esphome::Color c)
  {
    // 1. Basisfälle und Validierung
    if (r <= 0) {
      it.filled_rectangle(x, y, w, h, c);
      return;
    }

    // Radius begrenzen: darf nicht größer als die Hälfte der kleinsten Seite sein
    const int max_r = (w < h ? w : h) >> 1;
    if (r > max_r) r = max_r;

    // 2. Zentrale Blöcke zeichnen (vermeidet Overdraw für korrektes Blending)
    const int r2 = r * r;
    const int double_r = r << 1;

    it.filled_rectangle(x, y + r, w, h - double_r, c);          // Mittelteil
    it.filled_rectangle(x + r, y, w - double_r, r, c);          // Oberer Balken
    it.filled_rectangle(x + r, y + h - r, w - double_r, r, c);  // Unterer Balken

    // 3. Ecken zeichnen (Scanline-Verfahren)
    for (int dy = 0; dy < r; dy++) {
      // dy_dist: Entfernung vom Kreismittelpunkt der Ecke
      const int dy_dist = r - dy - 1;
      const int dy2 = dy_dist * dy_dist;

      // Wir suchen für jede Zeile dy den Startpunkt dx, ab dem gezeichnet wird
      for (int dx = 0; dx < r; dx++) {
        const int dx_dist = r - dx - 1;

        // Pythagoras: Wenn innerhalb des Kreises, zeichne den Rest der Zeile
        if ((dx_dist * dx_dist) + dy2 <= r2) {
          const int line_len = r - dx;

          // Zeichne horizontale Linien für alle 4 Ecken gleichzeitig
          it.filled_rectangle(x + dx, y + dy, line_len, 1, c);             // Oben Links
          it.filled_rectangle(x + w - r, y + dy, line_len, 1, c);          // Oben Rechts
          it.filled_rectangle(x + dx, y + h - 1 - dy, line_len, 1, c);     // Unten Links
          it.filled_rectangle(x + w - r, y + h - 1 - dy, line_len, 1, c);  // Unten Rechts

          break;  // Nächste Zeile (dy), da der Rest der Zeile innerhalb der Rundung liegt
        }
      }
    }
  }

  template <typename T_TARGET>
  static void filled_rectangle_gradient(T_TARGET& it, int x, int y, int w, int h, esphome::Color c1, esphome::Color c2,
                                        GradientDirection dir = GRADIENT_HORIZONTAL)
  {
    if (dir == GRADIENT_HORIZONTAL) {
      // Horizontal gradient - draw vertical lines
      for (int dx = 0; dx < w; dx++) {
        float t = (w > 1) ? (float)dx / (float)(w - 1) : 0.0f;
        uint8_t r = c1.r + t * (c2.r - c1.r);
        uint8_t g = c1.g + t * (c2.g - c1.g);
        uint8_t b = c1.b + t * (c2.b - c1.b);
        it.vertical_line(x + dx, y, h, esphome::Color(r, g, b));
      }
    } else {
      // Vertical gradient - draw horizontal lines
      for (int dy = 0; dy < h; dy++) {
        float t = (h > 1) ? (float)dy / (float)(h - 1) : 0.0f;
        uint8_t r = c1.r + t * (c2.r - c1.r);
        uint8_t g = c1.g + t * (c2.g - c1.g);
        uint8_t b = c1.b + t * (c2.b - c1.b);
        it.horizontal_line(x, y + dy, w, esphome::Color(r, g, b));
      }
    }
  }

  template <typename T_TARGET>
  static void filled_round_rectangle_gradient(T_TARGET& it, int x, int y, int w, int h, int r, esphome::Color c1,
                                              esphome::Color c2, GradientDirection dir = GRADIENT_HORIZONTAL)
  {
    int r2 = r * r;
    for (int dy = 0; dy < h; dy++) {
      for (int dx = 0; dx < w; dx++) {
        // Check rounded corners
        bool draw = true;
        if (dx < r && dy < r && (r - dx) * (r - dx) + (r - dy) * (r - dy) > r2)
          draw = false;
        else if (dx >= w - r && dy < r && (dx - (w - r - 1)) * (dx - (w - r - 1)) + (r - dy) * (r - dy) > r2)
          draw = false;
        else if (dx < r && dy >= h - r && (r - dx) * (r - dx) + (dy - (h - r - 1)) * (dy - (h - r - 1)) > r2)
          draw = false;
        else if (dx >= w - r && dy >= h - r &&
                 (dx - (w - r - 1)) * (dx - (w - r - 1)) + (dy - (h - r - 1)) * (dy - (h - r - 1)) > r2)
          draw = false;

        if (draw) {
          // Calculate mix ratio based on direction
          float t = (dir == GRADIENT_HORIZONTAL) ? (float)dx / (float)(w - 1) : (float)dy / (float)(h - 1);

          // Clamp t to [0, 1] to avoid math errors on small shapes
          t = std::max(0.0f, std::min(1.0f, t));

          uint8_t red = c1.r + t * (c2.r - c1.r);
          uint8_t green = c1.g + t * (c2.g - c1.g);
          uint8_t blue = c1.b + t * (c2.b - c1.b);

          it.draw_pixel_at(x + dx, y + dy, esphome::Color(red, green, blue));
        }
      }
    }
  }

  template <typename T_TARGET>
  static void filled_ellipse(T_TARGET& it, int x, int y, int rx, int ry, esphome::Color c)
  {
    // Pre-calculate squares to speed up the loop
    float rx2 = rx * rx;
    float ry2 = ry * ry;

    for (int dy = -ry; dy <= ry; dy++) {
      for (int dx = -rx; dx <= rx; dx++) {
        // Standard ellipse equation: (x^2 / rx^2) + (y^2 / ry^2) <= 1
        if ((dx * dx) / rx2 + (dy * dy) / ry2 <= 1.0f) {
          it.draw_pixel_at(x + dx, y + dy, c);
        }
      }
    }
  }

  template <typename T_TARGET>
  static void filled_ellipse_gradient(T_TARGET& it, int x, int y, int rx, int ry, esphome::Color c1, esphome::Color c2,
                                      GradientDirection dir = GRADIENT_HORIZONTAL)
  {
    float rx2 = rx * rx;
    float ry2 = ry * ry;

    for (int dy_offset = -ry; dy_offset <= ry; dy_offset++) {
      for (int dx_offset = -rx; dx_offset <= rx; dx_offset++) {
        // 1. Check if the current pixel is inside the ellipse
        if ((dx_offset * dx_offset) / rx2 + (dy_offset * dy_offset) / ry2 <= 1.0f) {
          // 2. Calculate the mix ratio (t) based on direction
          float t;
          if (dir == GRADIENT_HORIZONTAL) {
            // t from -rx to rx, normalized to 0.0 to 1.0
            t = (float)(dx_offset + rx) / (float)(2 * rx);
          } else {  // GRADIENT_VERTICAL
            // t from -ry to ry, normalized to 0.0 to 1.0
            t = (float)(dy_offset + ry) / (float)(2 * ry);
          }

          // Clamp t to [0, 1] to avoid artifacts, especially with small
          // radii
          t = std::max(0.0f, std::min(1.0f, t));

          // 3. Interpolate RGB components
          uint8_t red = c1.r + t * (c2.r - c1.r);
          uint8_t green = c1.g + t * (c2.g - c1.g);
          uint8_t blue = c1.b + t * (c2.b - c1.b);

          it.draw_pixel_at(x + dx_offset, y + dy_offset, esphome::Color(red, green, blue));
        }
      }
    }
  }

protected:
  /**
   * @brief Executes a draw call, optionally routing it through a blending proxy.
   * If the pipeline is empty, the draw function executes directly on the display.
   * If effects are active, a stack-allocated proxy intercepts pixel writes to
   * apply the filter chain via template-based inlining for maximum performance.
   * @tparam F Type of the drawing lambda.
   * @param draw_func Callable receiving a display reference (real or proxy).
   * @return T& Reference to the derived class for chaining.
   */
  template <typename F>
  T& draw_generic(F&& draw_func)
  {
    // Cast on the derived class GfxBlend
    // auto* const derived = static_cast<T*>(this);
    auto& self = static_cast<T&>(*this);

    // Helper lambda to handle both draw_func() and draw_func(it)
    auto execute = [&](auto* target) {
      if constexpr (std::is_invocable_v<F, decltype(*target)&>) {
        draw_func(*target);
      } else if constexpr (std::is_invocable_v<F, decltype(target)>) {
        draw_func(target);
      } else {
        draw_func();
      }
    };

    if (self.get_pipeline().empty()) {
      // QUICKPATH: Direct rendering to the real display
      execute(self.get_real_display());
    } else {
      // BLENDPATH: Create the pixel-processing lambda
      auto pipeline_blender = [&self](int16_t x, int16_t y, uint16_t fg) -> uint16_t {
        // 1. Optimized background read: skip if no effect in the pipeline needs it

        uint16_t bg = 0;
        if (self.bg_read_enabled()) {
          bg = self.read_raw_pixel_from_buffer_(x, y);

          if (self.bg_as_source_enabled()) {
            fg = bg;
          }
        }

        // 2. Process through the effect chain
        return self.apply_pipeline(x, y, fg, bg);
      };

      // Create the proxy on the stack with the specialized blender type
      GfxProxy<decltype(pipeline_blender)> proxy(self.get_real_display(), pipeline_blender);

      // Run the user's draw commands through the proxy
      execute(&proxy);
    }

    return self;
  }

};  // Gfx Shapes

}  // namespace gfx_blend

}  // namespace esphome
