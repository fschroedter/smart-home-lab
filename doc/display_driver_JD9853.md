[![ESPHome](https://img.shields.io/badge/ESPHome-Powered-white?logo=esphome&logoColor=000000)](https://esphome.io/)

[← Back to Overview](../README.md)

# LCD Display Driver JD9853

Currently, the **JD9853 display driver** is not officially supported by ESPHome. This section demonstrates how to configure this driver by using a custom initialization sequence for [ESPHome ILI9xxx TFT displays](https://esphome.io/components/display/ili9xxx/). The JD9853 display driver is used by the **ESP32-S3 1.47" Touch Display** and the **ESP32-C6 1.47" Touch Display**.


> [!IMPORTANT]
> **Model Compatibility:** Do not confuse this board with the **ESP32-S3-LCD-1.47** or **ESP32-C6-LCD-1.47** (board versions without touch panel). Those boards use the **ST7789** driver, which is already supported by the ESPHome ILI9xxx TFT display component.

> [!NOTE]
Although I have not specifically tested the ESP32-S3-Touch-LCD-1.47, I assume it should work as well, provided the pin configuration is adjusted.



## Pixel-Level Comparison: JD9853 vs. Generic Drivers

Selecting the appropriate `init_sequence` for the **JD9853** is essential. While alternative models may seem functional, a comparative analysis highlights significant discrepancies.


| ❌ Generic Driver | ✅ JD9853 Driver | Digital Source File (RGB565) |
| :-----------------------------------------  | :-----------------------------------------  | :----------------------------------------- |
| <div align="center"><img src="assets/flower_driver_generic.jpeg" width="200"  /></div>| <div align="center"><img src="assets/flower_driver_JD9853.jpeg" width="200"  /></div> |  <div align="center"><img src="assets/flower.bmp" data-canonical-src="assets/assets/rastergrafik_v.bmp" width="172"  /></div>|
 

##  Custom Initialization Sequence
The initialization sequence includes a workaround that uses [`init_sequence`](https://github.com/fschroedter/smart-home-lab/blob/main/includes/init_sequence_JD9853.yaml) processing to insert a delay required for correct configuration. For more details, see [Deep Dive](display_driver_JD9853_deepdive.md).

For more advanced setup options, check out the official **ESPHome** configuration guide for [ILI9xxx TFT displays](https://esphome.io/components/display/ili9xxx/) page.
```yaml

# Load this package to extend the display with a custom init sequence for the JD9853 display drivers
packages:
  remote_package_files:
    url: https://github.com/fschroedter/smart-home-lab
    files: [ includes/init_sequence_JD9853.yaml ]


display:
  - platform: ili9xxx
    model: CUSTOM
    id: my_display  # <-- Note: The package is adapted to 'my_display' 
    dc_pin:
      number: GPIO15
      ignore_strapping_warning: true
    cs_pin: GPIO14
    reset_pin: GPIO22
    dimensions:
      width: 172
      height: 320
      offset_width: 34
      offset_height: 0
    invert_colors: false
```

<!-- 
## Touch Controller Configuration
[![GitHub](https://img.shields.io/badge/GitHub-Repository-blue?logo=github)](https://github.com/widget/esphome-components/)

The **AXS5106L** touch panel can be integrated into ESPHome using the external component [axs5106](https:github.com//widget/esphome-components).

<details>
<summary><b>GPIO Assignments for Touch Panel</b></summary>

| GPIO        | LCD Pin  | Function                                    |
| :---        | :---     | :---                                        |
| **GPIO 18** | TP_SDA   | I2C Data                                    |
| **GPIO 19** | TP_SCL   | I2C Clock                                   |
| **GPIO 20** | TP_RST   | Touch Panel Reset                           |
| **GPIO 21** | TP_INT   | Touch Interrupt (Signals touch to ESP32)    |
</details>

```yaml
external_components:  
  - source: github://widget/esphome-components
    components: [ axs5106 ]

i2c:
  - sda: GPIO18
    scl: GPIO19

touchscreen:
  - platform: axs5106
    id: my_touch
    interrupt_pin: GPIO21
    reset_pin: GPIO20
    calibration:
      x_min: 0
      y_min: 0
      y_max: 172
      x_max: 320
``` -->




<!-- ----
## Alpha

```cpp
// Inside JD9853Display class
protected:
  uint8_t alpha_level_{255}; // Default: fully opaque

public:
  void set_alpha(uint8_t alpha) { this->alpha_level_ = alpha; }
  void reset_alpha() { this->alpha_level_ = 255; }

  // Override the internal pixel function
  void draw_absolute_pixel_internal(int x, int y, Color color) override {
    if (this->alpha_level_ == 0) return;

    if (this->alpha_level_ == 255) {
      // Standard fast writing
      this->draw_pixel_raw_(x, y, color);
    } else {
      // Alpha Blending logic
      Color bg = this->get_pixel_raw_(x, y); // You need a helper to read from buffer_
      
      uint8_t r = (uint16_t(color.red) * alpha_level_ + uint16_t(bg.red) * (255 - alpha_level_)) >> 8;
      uint8_t g = (uint16_t(color.green) * alpha_level_ + uint16_t(bg.green) * (255 - alpha_level_)) >> 8;
      uint8_t b = (uint16_t(color.blue) * alpha_level_ + uint16_t(bg.blue) * (255 - alpha_level_)) >> 8;
      
      this->draw_pixel_raw_(x, y, Color(r, g, b));
    }
  }
``` -->

<!-- 
```cpp
# Schnell & Schlank (Einzel-Effekt, volle Optimierung)
gfx.with(BlendModes::blend_test(), [&](){ ... });

# Flexibel (Mehrere Effekte via Tuple, volle Optimierung)
gfx.with(gfx.effects(E1, E2), [&](){ ... });

# Abwärtskompatibel (Alte Syntax, Standard-Performance)
gfx.with({E1, E2}, [&](){ ... });


GFXModes::EffectBgDrawing
auto effectBgDrawing = [](int16_t x, int16_t y, uint16_t fg, uint16_t bg) {
  return (uint16_t)(bg); 
};

gfx.with(
  gfx.no_bg({ BlendModes::invert(), BlendModes::test() }),
  [&](display::DisplayBuffer &it) { ... }
);

gfx.with({E1, E2}, [&](){ ... }, attr);


gfx.with(
  gfx.no_bg({E1, E2}),
  [&](display::DisplayBuffer &it) { ... }
);

// gfx.no_bg()
// Es findet kein Auslesen des Buffers für den Hintergrund statt, 
// was eine kleine Performance-Verbesserung mit sich bringt.

// gfx.bg_as_source()
// Der Hintergrund wird als Quelle für die Effekte verwendet


gfx.with(
  gfx.no_bg({E1, E2}),
  [&](display::DisplayBuffer &it) { ... }
);

```



uint8_r attr = GFXModes::InvisibleDrawing | GFXModes::NoBackground

GFXModes::InvisibleDrawing  
=> Effekte werden nicht ob das Zeichenobjekt, sondern den Hintergrund angewendet. Sprich bg = fg
GFXModes::NoBackground      
=> background-Punkte werden beim Zeichnen nicht gelesen. sind per default schwarz 



gfx.with(gfx.bg_source(E1, E2), [](display::DisplayBuffer &it) { });

# Scope-Style
gfx.with(gfx.no_bg(E1, E2), [](display::DisplayBuffer &it) { });
gfx.with(gfx.no_bg({E1, E2}), [](display::DisplayBuffer &it) { });
gfx.with(E1, E2, [](display::DisplayBuffer &it) { });
gfx.with({E1, E2}, [](display::DisplayBuffer &it) { });


# Setter-Style
gfx.with(gfx.no_bg(E1, E2));
gfx.with(gfx.no_bg({E1, E2}));
gfx.with(E1, E2);
gfx.with({E1, E2});

gfx.filled_rectangle2(20, 20, 132, 280, Color(50, 155, 205));

gfx.no_bg();
gfx.no_bg(true);
gfx.no_bg(false);
gfx.bg_source();

# 3. Pipeline explizit löschen, wenn fertig
gfx.with({}); # oder gfx.clear(); -->
