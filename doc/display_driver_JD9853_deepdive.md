[← Back to LCD Display Driver JD9853](../doc/display_driver_JD9853.md)

# 🛠️ Deep Dive: JD9853 Initialization Sequence
This deep dive covers the technical details how the **JD9853** display driver can be implemented within the [ESPHome ili9xxx component](https://esphome.io/components/display/ili9xxx/).


## LCD Displays with JD9853 Display Driver
These devices use JD9853 display driver:
- Waveshare (WS-30499),  LCD-Display 1.47inch Touch Display (without micro controller)
- Waveshare (WS-31202), **ESP32-S3** 1.47inch Touch Display Development Board
- Waveshare (WS-31201), **ESP32-C6** 1.47inch Touch Display Development Board


## Init Sequence
Using  plattform [ili9xxx](https://esphome.io/components/display/ili9xxx/) with a custom `init_sequence`:


```yaml
display:
  - platform: ili9xxx
    model: CUSTOM
    # ...
    init_sequence:
      # Initialization sequence for JD9853
      - [0xDF, 0x98, 0x53]   # Unlock   
      - [0xB2, 0x23]
      - [0xB7, 0x00, 0x47, 0x00, 0x6F]
      - [0xBB, 0x1C, 0x1A, 0x55, 0x73, 0x63, 0xF0]
      - [0xC0, 0x44, 0xA4]      
      - [0xC1, 0x10]    #  Set Panel      
      # - [0xC2, 0x0E]  # Built-In Self Test Pattern       
      - [0xC3, 0x7D, 0x07, 0x14, 0x06, 0xCF, 0x71, 0x72, 0x77]
      - [0xC4, 0x00, 0x00, 0xA0, 0x79, 0x0B, 0x0A, 0x16, 0x79, 0x0B, 0x0A, 0x16, 0x82]
      - [0xC8,
          0x3F, 0x32, 0x29, 0x29, 0x27, 0x2B, 0x27, 0x28, 
          0x28, 0x26, 0x25, 0x17, 0x12, 0x0D, 0x04, 0x00, 
          0x3F, 0x32, 0x29, 0x29, 0x27, 0x2B, 0x27, 0x28, 
          0x28, 0x26, 0x25, 0x17, 0x12, 0x0D, 0x04, 0x00]   # SET_R_GAMMA VOP5.25 G2.2
      - [0xD0, 0x04, 0x06, 0x6B, 0x0F, 0x00]
      - [0xD7, 0x00, 0x30]
      - [0xE6, 0x14]
      - [0xDE, 0x01] 
      - [0xB7, 0x03, 0x13, 0xEF, 0x35, 0x35]
      - [0xC1, 0x14, 0x15, 0xC0]
      - [0xC2, 0x06, 0x3A]
      - [0xC4, 0x72, 0x12]
      - [0xBE, 0x00]
      - [0xDE, 0x02]
      - [0xE5, 0x00, 0x02, 0x00]
      - [0xE5, 0x01, 0x02, 0x00]
      - [0xDE, 0x00]
      - [0x35, 0x00]
      - [0x3A, 0x55]   # Pixel Format: Set, 06=RGB666；05=RGB565
      - [0x2A, 0x00, 0x22, 0x00, 0xCD]   # Start_X=34, End_X=205
      - [0x2B, 0x00, 0x00, 0x01, 0x3F]   # Start_Y=0, End_Y=319      
      # ----------------------------------------------------------------------
      # Delay hack
      # Sleep out (0x11) + delay (150ms) + Pseudo-NOP-Command (0xFF)
      - [0x11, 0xFF, 0x7E,
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
      # ----------------------------------------------------------------------
      - [0xDE, 0x02]
      - [0xE5, 0x00, 0x02, 0x00]
      - [0xDE, 0x00]
      - [0x29]   # Display On

```

## The Delay Hack
The initialization sequence requires a delay of 120–150 ms after the 'Sleep out' command `0x11` to allow the hardware to stabilize. While native C++ modules for the ILI9XXX can simply use the `ILI9XXX_DELAY(150)` command, ESPHome's YAML-based `init_sequence` does not provide a built-in way to inject time delays between hex commands. To understand how we can spawn a delay without a dedicated command in the `init_sequence`, we have to look at how ESPHome processes the `init_sequence`. 

The Ili9xxx component processes the `init_sequence` by concatenating all individual YAML arrays into a single, continuous memory block (`uint8_t *addr`). The data buffer at `*addr` consists of command and argument blocks structured as follows: `[command] + [len(args)] + [list(args)]`. The `length byte` (len(args)) determines how many bytes are treated as arguments for the current command before the next command is read. By setting the most significant bit (`0x80`) of the length byte, we can "hide" extra data that is processed in the next iteration.

The `init_sequence` entry `[0x11, 0xFF, 0x7E, ...]` is transformed into a list where a length byte (`0x80`) is inserted, resulting in `[0x11, 0x80, 0xFF, 0x7E, ...]`. All resulting lists are concatenated and passed to the `init_lcd_` method:


```cpp
void ILI9XXXDisplay::init_lcd_(const uint8_t *addr) {
  if (addr == nullptr)
    return;
  uint8_t cmd, x, num_args;
  while ((cmd = *addr++) != 0) {
    x = *addr++;
    if (x == ILI9XXX_DELAY_FLAG) {
      cmd &= 0x7F;
      ESP_LOGV(TAG, "Delay %dms", cmd);
      delay(cmd);
    } else {
      num_args = x & 0x7F;
      ESP_LOGV(TAG, "Command %02X, length %d, bits %02X", cmd, num_args, *addr);
      this->send_command(cmd, addr, num_args);
      addr += num_args;
      if (x & 0x80) {
        ESP_LOGV(TAG, "Delay 150ms");
        delay(150);  // NOLINT
      }
    }
  }
}
```
As a result, the `ILI9XXXDisplay::init_lcd_` method now initiats function calls for the `init_sequence` entry `[0x11, 0xFF, 0x7E, 0, 0, ..., 0]` as follows:


| *addr (Processed bytes)                    | cmd                  | x       | num_args     | Called functions                           |
| :----------------------------------------- | :----                | :----   |:------------ | :----------------------------------------- |
| `0x11` `0x80`                              | `0x11`               | `0x80`  | 0            | send_command(`0x11`, addr, 0) + delay(150)   |
| `0xFF` `0x7E` `0x00` `0x00` ...  `0x00`    | `0xFF`<sup>(*)</sup> | `0x7E`  | `126`        | send_command(`0xFF`, addr, 126)              |



(*) Since `0xFF` is not a valid command, both the command itself and its arguments are discarded.

## Set Panel (0xC1)

The **Set Panel (0xC1)** command is specifically tailored to reconfigure the display's internal hardware logic—including **source scan output, gate scan output, inverse-mode, and color order**—to achieve native compatibility with the standard ESPHome ILI9XXX component.

Consequently, using the adjusted init sequence, manual adjustments such as `color_order`, `transform`, or `rotation` are no longer required for a standard portrait orientation (USB port at the bottom).


```
      +------+------+------+------+------+------+------+------+
 Bit: |  D7  |  D6  |  D5  |  D4  |  D3  |  D2  |  D1  |  D0  |
      +------+------+------+------+------+------+------+------+
                              |      |      |      |      
                              SS     GS    REV    CFHR    
```
| Bit | Name | Description / Settings |
| :---   | :---      | :--- |
| D4     | SS_PANEL  | **Source Scan Direction (Horizontal)**<br>0: S1 -> S240 (Normal)<br>1: S240 -> S1 (Flipped) |
| D3     | GS_PANEL  | **Gate Scan Direction (Vertical)**<br>0: G1 -> G320 (Top -> Bottom)<br>1: G320 -> G1 (Bottom -> Top) |
| D2     | REV_PANEL | **Display Mode (Polarization)**<br>0: Normal Black Panel<br>1: Normal White Panel |
| D1     | CFHR      | **Color Filter Horizontal Alignment**<br>0: RGB Alignment <br>1: BGR Alignment  |
| D0     | ---       | Reserved / Not used |

<!-- ```
================================================================================
COMMAND [0xC1]: SETPANEL (Set Panel Related Register)
================================================================================
Bit   | Name            | Description / Settings
------|-----------------|-------------------------------------------------------
D4    | SS_PANEL        | Source Scan Direction (Horizontal)
      |                 |   0: S1   -> S240 (Normal)
      |                 |   1: S240 -> S1   (Flipped)
------|-----------------|-------------------------------------------------------
D3    | GS_PANEL        | Gate Scan Direction (Vertical)
      |                 |   0: G1   -> G320 (Top -> Bottom)
      |                 |   1: G320 -> G1   (Bottom -> Top)
------|-----------------|-------------------------------------------------------
D2    | REV_PANEL       | Display Mode (Polarization)
      |                 |   0: Normal Black Panel
      |                 |   1: Normal White Panel
------|-----------------|-------------------------------------------------------
D1    | CFHR            | Color Filter Horizontal Alignment
      |                 |   0: RGB Alignment
      |                 |   1: BGR Alignment
------|-----------------|-------------------------------------------------------
D0    | ---             | Reserved / Not used
================================================================================   
``` -->

## Built-In Self Test Pattern (0xC2)
The **Built-In Self Test Pattern (0xC2)** command provides a series of display patterns.
```
      +------+------+------+------+------+------+------+------+
 Bit: |  D7  |  D6  |  D5  |  D4  |  D3  |  D2  |  D1  |  D0  |
      +------+------+------+------+------+------+------+------+
               \_________/            |      \_____________/
                 LNPERLVL             |        TEST_PATTERN
                                 TEST_PAT_EN 
```
<table>
  <thead>
    <tr>
      <th align="left">Bit</th>
      <th align="left">Name</th>
      <th align="left">Description / Settings</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td>D6:D5</td>
      <td>LNPERLVL</td>
      <td>
        <strong>Number of lines for each gray level [1:0]</strong><br>
        <table>
          <tr>
            <td>00: 1 line</td>
            <td>01: 2 lines</td>
          </tr>
          <tr>
            <td>10: 4 lines (Default)</td>
            <td>11: 8 lines</td>
          </tr>
        </table>
      </td>
    </tr>
    <tr>
      <td>D3</td>
      <td>TEST_PAT_EN</td>
      <td>
        <strong>BIST Pattern Generator Enable</strong><br>
        0: Disable (Default)<br>
        1: Enable
      </td>
    </tr>
    <tr>
      <td>D2:D0</td>
      <td>TEST_PATTERN</td>
      <td>
        <strong>Pattern Selection [2:0]</strong><br>
        <table>
          <tr>
            <td>000: 8 Color Bars</td>
            <td>100: Green Screen</td>
          </tr>
          <tr>
            <td>001: Black Screen</td>
            <td>101: Blue Screen</td>
          </tr>
          <tr>
            <td>010: White Screen</td>
            <td>110: Gray Gradient (Y-dir)</td>
          </tr>
          <tr>
            <td>011: Red Screen</td>
            <td>111: Border (R), Inner (B/Black)</td>
          </tr>
        </table>
      </td>
    </tr>
  </tbody>
</table>

<!--
| Bit      | Name              | Description / Settings   |
| :---     | :---              | :---                     |
| D6:D5    | LNPERLVL          | **Number of lines for each gray level [1:0]**<br>00: 1 line &nbsp; \| &nbsp; 01: 2 lines<br>10: 4 lines (Default) &nbsp; \| &nbsp; 11: 8 lines |
| D3       | TEST_PAT_EN       | **BIST Pattern Generator Enable**<br>0: Disable (Default)<br>1: Enable |
| D2:D0    | TEST_PATTERN      | **Pattern Selection [2:0]**<br>000: 8 Color Bars &nbsp; \| &nbsp; 100: Green Screen<br>001: Black Screen  &nbsp; \| &nbsp; 101: Blue Screen<br>010: White Screen  &nbsp; \| &nbsp; 110: Gray Gradient (Y-dir)<br>011: Red Screen &nbsp; \| &nbsp; 111: Border (R), Inner (B/Black) | 
-->

<!-- ```
================================================================================
COMMAND [0xC2]: SET_BIST (Built-In Self Test Pattern Setting)
================================================================================
Bit   | Name            | Description / Settings
------|-----------------|-------------------------------------------------------
D6:D5 | LNPERLVL[1:0]   | Number of lines for each gray level
      |                 |   00: 1 line    | 01: 2 lines
      |                 |   10: 4 lines (Default)
      |                 |   11: 8 lines
------|-----------------|-------------------------------------------------------
D3    | TEST_PAT_EN     | BIST Pattern Generator Enable
      |                 |   0: Disable (Default)
      |                 |   1: Enable
------|-----------------|-------------------------------------------------------
D2:D0 | TEST_PATTERN    | Pattern Selection [2:0]
      |                 |   0: 8 Color Bars    4: Green Screen
      |                 |   1: Black Screen    5: Blue Screen
      |                 |   2: White Screen    6: Gray Gradient (Y-dir)
      |                 |   3: Red Screen      7: Border (R), Inner (B/Black)
================================================================================
``` -->

## Sources
 - [Osptek - Initial code](https://www.osptek.com/products/1/15/485)
 - [Waveshare - Demo for ESP32-S3 Touch LCD 1.47](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.47#Demo_3)
 - [Waveshare - Demo for ESP32-C6 Touch LCD 1.47](https://www.waveshare.com/wiki/ESP32-C6-Touch-LCD-1.47#Demo_3)
