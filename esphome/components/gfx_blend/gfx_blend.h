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

#include "esphome/core/color.h"
#include "esphome/core/log.h"

#include "esphome/components/display/display.h"
#include "esphome/components/display/display_buffer.h"
#include "esphome/components/display/display_color_utils.h"

#include <initializer_list>
#include <type_traits>
#include <utility>
#include <vector>

#include "accessor.h"
#include "defs.h"
#include "effects.h"
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


/**
 * Reads a pixel color from the display's raw RGB565 buffer.
 * Handles rotation by mapping coordinates back to the native hardware layout.
 */
inline uint16_t HOT GfxBlend::read_raw_pixel_from_buffer_(int x, int y)
{
  uint8_t* buffer = DisplayBufferAccessor::get_raw_buffer(this->disp_);
  if (!buffer) return 0x0000;

  int native_w = DisplayBufferAccessor::get_native_w(this->disp_);

  // Optimization: Access native_h only when needed and use direct mapping.
  switch (this->disp_->get_rotation()) {
    case esphome::display::DISPLAY_ROTATION_90_DEGREES: {
      std::swap(x, y);
      x = native_w - x - 1;
      break;
    }
    case esphome::display::DISPLAY_ROTATION_180_DEGREES: {
      int native_h = DisplayBufferAccessor::get_native_h(this->disp_);
      x = native_w - x - 1;
      y = native_h - y - 1;
      break;
    }
    case esphome::display::DISPLAY_ROTATION_270_DEGREES: {
      int native_h = DisplayBufferAccessor::get_native_h(this->disp_);
      std::swap(x, y);
      y = native_h - y - 1;
      break;
    }
    default:
      break;
  }

  // Calculate buffer position (RGB565 = 2 bytes per pixel)
  uint32_t pos = (y * native_w + x) * 2;
  uint16_t raw = (uint16_t(buffer[pos]) << 8) | buffer[pos + 1];

  return raw;
}


void GfxBlend::dump_config()
{
  ESP_LOGCONFIG(TAG, MODULE_NAME);
  const uint8_t type = this->disp_->get_display_type();
  ESP_LOGCONFIG(TAG, "  Target display: %s", this->display_type_to_string_(type));

  if (type != esphome::display::DisplayType::DISPLAY_TYPE_COLOR) {
    ESP_LOGE(TAG, "Incompatible display: 16-bit RGB565 required for correct blending");
  }

  ESP_LOGCONFIG(TAG, "  Width: %d", this->disp_->get_width());
  ESP_LOGCONFIG(TAG, "  Height: %d", this->disp_->get_height());
}

/**
 * Creates a NoBgWrapper for multiple effects (variadic).
 * Disables background read access for the contained effects.
 * Usage: gfx.needs_no_bg(E1, E2)
 */

template <typename... Args>
auto GfxBlend::needs_no_bg(Args&&... args)
{
  return NoBgWrapper<std::vector<blender_t>>{create_vector_(std::forward<Args>(args)...)};
}

/**
 * Overload of needs_no_bg for use with initialization lists {e1, e2}.
 * Usage: gfx.needs_no_bg({E1, E2})
 */
auto GfxBlend::needs_no_bg(std::initializer_list<blender_t> effects)
{
  return NoBgWrapper<std::vector<blender_t>>{std::vector<blender_t>(effects)};
}

/**
 * Creates a BgAsSourceWrapper for multiple effects (variadic).
 * Causes the background content to serve as the input color for the effects.
 * Usage: gfx.bg_source(E1, E2)
 */
template <typename... Args>
auto GfxBlend::bg_as_source(Args&&... args)
{
  return BgAsSourceWrapper<std::vector<blender_t>>{create_vector_(std::forward<Args>(args)...)};
}

/**
 * Overload of bg_as_source for use with initialization lists {e1, e2}.
 * gfx.bg_source({E1, E2})
 */
auto GfxBlend::bg_as_source(std::initializer_list<blender_t> effects)
{
  return BgAsSourceWrapper<std::vector<blender_t>>{std::vector<blender_t>(effects)};
}

/**
 * Provides read access to the currently registered pipeline steps.
 */
const std::vector<std::unique_ptr<GfxPipelineStep>>& GfxBlend::get_pipeline() const { return this->pipeline_; }

/**
 * Processes a pixel through all steps of the pipeline.
 * @return The final pixel color after applying all blending operations.
 */
uint16_t GfxBlend::apply_pipeline(int16_t x, int16_t y, uint16_t fg, uint16_t bg)
{
  uint16_t current_fg = fg;

  for (auto const& step : pipeline_) {
    // Executes the blending step. Each step takes the result of the previous one as the new 'fg'.
    current_fg = step->blend(x, y, current_fg, bg);
  }

  return current_fg;
}

/**
 * Resets the pipeline: deletes all effects and restores default flags.
 */
void GfxBlend::clear()
{
  this->pipeline_.clear();
  this->read_bg_ = true;
  this->use_bg_as_source_ = false;
}

/**
 * Scoped & Setter configuration for a list of effects.
 * Executes draw_func and automatically clears the pipeline afterwards.
 * Usage: gfx.with({E1, E2}) or gfx.with({E1, E2}, Draw)
 */
template <typename D>
void GfxBlend::with(std::initializer_list<blender_t> funcs, D&& draw_func)
{
  this->clear();

  for (auto const& f : funcs) {
    this->add_step_internal_(blender_t(f));
  }

  // If draw_func is a callable function -> Scoped Mode
  if constexpr (std::is_invocable_v<D, display::DisplayBuffer&> || std::is_invocable_v<D>) {
    // SCOPED MODE
    this->draw_generic(std::forward<D>(draw_func));
    this->clear();
  }
  // Otherwise: SETTER MODE -> Pipeline remains active for subsequent commands
}

/**
 * Scoped & Setter configuration (variadic).
 * Can accept wrappers, individual effects, or a final lambda.
 * Usage: gfx.with(E1, E2) or gfx.with(E1, E2, Draw)
 */
template <typename... Args>
void GfxBlend::with(Args&&... args)
{
  auto tuple = std::forward_as_tuple(std::forward<Args>(args)...);
  constexpr size_t n = sizeof...(Args);
  this->clear();

  using LastArgT = std::tuple_element_t<n - 1, std::decay_t<decltype(tuple)>>;

  // Check if the last argument is a draw function
  if constexpr (std::is_invocable_v<LastArgT, display::DisplayBuffer&> || std::is_invocable_v<LastArgT>) {
    // SCOPED MODE
    this->add_effects_from_tuple_(tuple, std::make_index_sequence<n - 1>{});
    this->draw_generic(std::get<n - 1>(tuple));
    this->clear();
  } else {
    // SETTER MODE (All arguments are effects)
    this->add_effects_from_tuple_(tuple, std::make_index_sequence<n>{});
  }
}

/**
 * Helper function to efficiently pack variadic arguments into a vector of effects.
 */
template <typename... Args>
std::vector<blender_t> GfxBlend::create_vector_(Args&&... args)
{
  std::vector<blender_t> v;
  v.reserve(sizeof...(Args));
  (v.push_back(blender_t(std::forward<Args>(args))), ...);  // Fold expression
  return v;
}

/**
 * Helper function to unpack a tuple into individual pipeline steps.
 * Used internally for processing complex with() calls.
 */
template <typename Tpl, size_t... I>
void GfxBlend::add_effects_from_tuple_(Tpl&& tpl, std::index_sequence<I...>)
{
  // Dieser Fold-Expression ruft add_step_internal für jeden Index auf
  (this->add_step_internal_(std::get<I>(std::forward<Tpl>(tpl))), ...);
}

// /**
//  * Converts various function signatures into a standard blender_t.
//  */
// template <typename F>
// auto GfxBlend::make_blender(F&& func)
// {
//   if constexpr (std::is_invocable_r_v<uint16_t, F, uint16_t>) {
//     // Case: Unary (fg) -> uint16_t
//     return [f = std::forward<F>(func)](int16_t x, int16_t y, uint16_t fg, uint16_t bg) { return f(fg); };
//   } else if constexpr (std::is_invocable_r_v<uint16_t, F, uint16_t, uint16_t>) {
//     // Case: Binary (fg, bg) -> uint16_t
//     return [f = std::forward<F>(func)](int16_t x, int16_t y, uint16_t fg, uint16_t bg) { return f(fg, bg); };
//   } else {
//     // Case: Full (x, y, fg, bg) -> uint16_t
//     return std::forward<F>(func);
//   }
// }

template <typename F>
void GfxBlend::add_step_internal_(F&& func)
{
  using EffectDef = std::decay_t<F>;
  auto step = std::make_unique<GenericEffect<EffectDef>>(std::forward<F>(func));

  // OPTIONAL Optimization: Disable background fetch if the effect declares it's not needed
  if constexpr (requires { EffectDef::read_bg; }) {
    if (!EffectDef::read_bg) {
      this->read_bg_ = false;
    }
  }

  // FUNCTIONAL Flag: Use background as source (requires background read to be active)
  if constexpr (requires { EffectDef::use_bg_as_source; }) {
    if (EffectDef::use_bg_as_source) {
      this->use_bg_as_source_ = true;
      this->read_bg_ = true;  // reverts possible read_no_bg activation
    }
  }

  this->pipeline_.push_back(std::move(step));
}

const char* GfxBlend::display_type_to_string_(uint8_t type)
{
  static const char* const TYPES[] = {"NONE", "BINARY", "GRAYSCALE", "COLOR"};
  uint8_t index = static_cast<uint8_t>(type);
  if (index >= 4) {
    return "UNKNOWN";
  }
  return TYPES[index];
}


}  // namespace gfx_blend

using Gfx = gfx_blend::GfxBlend;

}  // namespace esphome
