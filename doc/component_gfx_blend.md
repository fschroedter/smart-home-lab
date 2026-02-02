[![ESPHome](https://img.shields.io/badge/ESPHome-Powered-white?logo=esphome&logoColor=000000)](https://esphome.io/)


[← Back to Overview](../README.md)

# GFX Blend Component


## Sample
```yaml
external_components:  
  - source: github://fschroedter/smart-home-lab
    components: [ gfx_blend ]

# Activate gfx_blend component
gfx_blend:

display:
  ...
  lambda: |-
    // Use an image as background for effect demonstration
    it.image(0, 0, id(my_image));

    static Gfx gfx(&it);

    // Draw rectangle with border radius=20
    gfx.filled_rectangle( 20, 20, 150, 150, 20, Color(0, 255, 0) );
```

## General Usage
```cpp
static Gfx gfx(&it);

gfx.with({E1, E2}, [&](){ ... });

gfx.with(
  gfx.source_as_bg({E1, E2}),
  [&]() { ... }
);

gfx.with(
  gfx.no_bg({E1, E2}),
  [&]() { ... }
);
```

## More examples
```cpp
// Define a custom effect
auto inverse = [](int16_t x, int16_t y, uint16_t fg, uint16_t bg) {        
  return (uint16_t)~bg;
};

// Setter mode
gfx.with( inverse );
gfx.filled_rectangle( 20, 20, 50, 50, Color(0, 0, 0) );
```
```cpp
// Scope mode
gfx.with({ 
  inverse 
}, [&](){
  gfx.filled_rectangle( 70, 20, 50, 50, Color(0, 0, 0) );
})
```
```cpp
// Use pre-defined effects in variadic scope mode
gfx.with(
  GfxEffects::alpha(180),    
  gfx.filled_rectangle( 20, 70, 50, 50, Color(0, 0, 0) );
);
```

```cpp
// Use background as source
gfx.with(
  gfx.bg_as_source({        
    GfxEffects::inverse,       
    GfxEffects::alpha(200)
  }), [&](){
      gfx.filled_rectangle( 70, 70, 50, 50, Color(0, 0, 0) );
  }
);
```

```cpp
// Image Mask
gfx.with( GfxEffects::image_mask( id(my_mask), 10, 10 ) );
gfx.filled_rectangle( 10, 10, 100, 100, Color(255, 0, 0) );
```

