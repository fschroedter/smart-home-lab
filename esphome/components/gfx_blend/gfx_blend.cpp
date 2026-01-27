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

#include "gfx_blend.h"

#include "esphome/core/color.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"

#include "esphome/components/display/display.h"
#include "esphome/components/display/display_buffer.h"

#include <initializer_list>
#include <type_traits>
#include <utility>
#include <vector>

#include "defs.h"

namespace esphome {
namespace gfx_blend {

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

}  // namespace gfx_blend
}  // namespace esphome