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

#include "esphome/components/display/display_buffer.h"
#include "esphome/components/display/display_color_utils.h"

#include <initializer_list>
#include <type_traits>
#include <vector>

#include "accessor.h"
#include "defs.h"
#include "effects.h"
#include "gfx_blend.h"
#include "proxy.h"
#include "shapes.h"

namespace esphome {
namespace gfx_blend {

static const char* const TAG = "gfx_blend";
static constexpr const char* MODULE_NAME = "GfxBlend";

class GfxBlend;

/**
 * Abstract base class for all steps in the graphics pipeline.
 * Enables polymorphic storage of different effects in a vector.
 */
class GfxPipelineStep {
public:
  virtual ~GfxPipelineStep() = default;
  virtual uint16_t blend(int16_t x, int16_t y, uint16_t fg, uint16_t bg) = 0;
};

/**
 * Concrete implementation of a pipeline step that encapsulates a function object (lambda).
 * @tparam F The type of the callable object (blender).
 */
template <typename F>
class GenericEffect : public GfxPipelineStep {
public:
  explicit GenericEffect(F func) : func_(std::move(func)) {}

  uint16_t blend(int16_t x, int16_t y, uint16_t fg, uint16_t bg) override
  {
    using EffectDef = std::decay_t<F>;

    // If the wrapper indicates that bg is the source:
    if constexpr (requires { typename EffectDef::use_bg_as_source; }) {
      // For 'use_bg_as_source'==true override `fg` with `bg` before calling the function.
      return func_(x, y, bg, bg);
    } else {
      return func_(x, y, fg, bg);
    }
  }

protected:
  F func_;
};

/**
 * Main graphics blending canvas class.
 *
 * Provides functionality to render graphical elements using alpha and
 * custom blending effects.
 *
 * The class blends new pixel data with existing content by reading the
 * current framebuffer and combining it with the incoming color values.
 */
class GfxBlend : public GfxShapes<GfxBlend> {
  friend class GfxShapes<GfxBlend>;

public:
  /**
   * This constructor is only activated if T inherits from DisplayBuffer.
   * Allowed in YAML: static GFXBlend gfx(&it);
   */
  template <typename T, typename std::enable_if<std::is_base_of<esphome::display::DisplayBuffer, T>::value ||
                                                    std::is_base_of<esphome::display::Display, T>::value,
                                                int>::type = 0>
  GfxBlend(T* disp) : disp_(reinterpret_cast<esphome::display::DisplayBuffer*>(disp))
  {
    // Terminates the constructor body prematurely if the passed pointer is invalid
    if (this->disp_ == nullptr) {
      ESP_LOGE(TAG, "Critical: Display pointer is null!");
      return;  //  Terminates the constructor body prematurely
    }

    ESP_LOGD(TAG, "GfxBlend initialized with verified DisplayBuffer.");

    // Check if the display buffer supports 16-bit color
    if (disp->get_display_type() != esphome::display::DisplayType::DISPLAY_TYPE_COLOR) {
      ESP_LOGE(TAG, "Incompatible display: 16-bit RGB565 required for correct blending.");
    }
  }

  void setup() {}
  void dump_config();
  bool bg_read_enabled() const { return this->read_bg_; }
  bool bg_as_source_enabled() const { return this->use_bg_as_source_; }

  template <typename... Args>
  auto needs_no_bg(Args&&... args);
  auto needs_no_bg(std::initializer_list<blender_t> effects);

  template <typename... Args>
  auto bg_as_source(Args&&... args);
  auto bg_as_source(std::initializer_list<blender_t> effects);

  const std::vector<std::unique_ptr<GfxPipelineStep>>& get_pipeline() const;
  uint16_t apply_pipeline(int16_t x, int16_t y, uint16_t fg, uint16_t bg);
  void clear();

  template <typename D = void*>
  void with(std::initializer_list<blender_t> funcs, D&& draw_func = nullptr);

  template <typename... Args>
  void with(Args&&... args);

protected:
  esphome::display::DisplayBuffer* disp_;  // Pointer to the target display buffer instance.
  bool read_bg_{true};                     // Indicates whether blender reads from the display buffer. default: true
  bool use_bg_as_source_{false};           // If true, start pipeline with bg instead of fg. default: false

  std::vector<std::unique_ptr<GfxPipelineStep>> pipeline_;  // Storage for the active pipeline steps.

  esphome::display::DisplayBuffer* get_real_display() { return this->disp_; }

  template <typename... Args>
  std::vector<blender_t> create_vector_(Args&&... args);

  template <typename Tpl, size_t... I>
  void add_effects_from_tuple_(Tpl&& tpl, std::index_sequence<I...>);

  /**
   * @brief Internal helper to add a step to the blending pipeline.
   * This function performs compile-time introspection on the effect type 'F'
   * to automatically configure hardware optimization flags.
   * Optimization Flags:
   *
   * 1. Performance Optimization: Background Read Suppression (read_bg)
   * An OPTIONAL optimization that skips the expensive hardware read cycle (SPI/I2C).
   * Use this for a speed boost when the effect doesn't need background data.
   * - Built-in: Define 'static constexpr bool read_bg = false;' in your effect.
   * - Manual: Wrap any effect/lambda via 'it_gfx.needs_no_bg(effect)' to force this optimization.
   *
   * 2. Functional Flag: Background as Source (use_bg_as_source)
   * Redirects the current background color to the effect's foreground input.
   * This is a functional requirement for feedback or masking effects.
   * - Built-in: Define 'static constexpr bool use_bg_as_source = true;'.
   * - Manual: Use 'it.bg_as_source(effect)'.
   *
   * YAML/Lambda Usage:
   * Optimization is automatic when passing a flagged type:
   * it_gfx.with({ effect }, [&]() { ... });' // Auto-optimized (built-in flag)
   * it_gfx.with(it_gfx.needs_no_bg( effect ), [&]() { ... }); // Manually optimized via wrapper
   */
  template <typename F>
  void add_step_internal_(F&& func);
  inline uint16_t HOT read_raw_pixel_from_buffer_(int x, int y);
  const char* display_type_to_string_(uint8_t type);

  template <typename TBlender>
  friend class GfxProxy;
};

}  // namespace gfx_blend

using Gfx = gfx_blend::GfxBlend;

}  // namespace esphome
