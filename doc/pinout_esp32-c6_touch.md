[← Back to Main README](../README.md)

# Waveshare ESP32-C6 1.47 Touch Display
## Board Specifications


 - ESP32-C6FH8 chip 
 - Integrates 8MB Flash
 - 1.47 inch Display
 - Resolution: 172 × 320 pixels
 - Display Driver: JD9853
 - Touch Driver: AXS5106L
 - IMU (QMI8658 Sensor)
 - WiFi 6 and Bluetooth BLE 5
 - Onboard TF card slot (Micro-SD)
 - Futher details on [waveshare.com](https://www.waveshare.com/esp32-c6-touch-lcd-1.47.htm) 



## Board

```
                          USB-C 
                     +------------+
               VBUS  | [ ]    [ ] |  VBAT
                GND  | [ ]    [ ] |  GND
    U0_TX / GPIO 16  | [ ]    [ ] |  GND
    U0_RX / GPIO 17  | [ ]    [ ] |  3V3
                RST  | [ ]    [ ] |  GPIO 19 / I2C_SCL
  SPI_SCLK / GPIO  1 | [ ]    [ ] |  GPIO 18 / I2C_SDA
  SPI_MOSI / GPIO  2 | [ ]    [ ] |  GPIO 13 / USB_DP
  SPI_MISO / GPIO  3 | [ ]    [ ] |  GPIO 12 / USB_DN
             GPIO  4 | [ ]    [ ] |  GPIO  9
             GPIO  5 | [ ]    [ ] |  GPIO  8
             GPIO  6 | [ ]    [ ] |  GPIO  7
                     +------------+
```
## GPIO Assignments

### LCD Display
| GPIO        | LCD Pin           | Function                     |
| :---        | :---              | :---                         |
| **GPIO 1**  | LCD_CLK           | LCD SPI Serial Clock         |
| **GPIO 2**  | LCD_DIN           | LCD SPI Data In / MOSI       |
| **GPIO 14** | LCD_CS            | LCD Chip Select              |
| **GPIO 15** | LCD_DC            | LCD Data/Command Selection   |
| **GPIO 22** | LCD_RST           | LCD Reset                    |
| **GPIO 23** | LCD_BL            | LCD Backlight PWM Dimming    |

### Touch Panel
| GPIO        | LCD Pin           | Function                         |
| :---        | :---              | :---                             |
| **GPIO 18** | TP_SDA            | Touch Panel Serial Data (I2C)    |
| **GPIO 19** | TP_SCL            | Touch Panel Clock (I2C)          |
| **GPIO 20** | TP_RST            | Touch Panel Reset                |
| **GPIO 21** | TP_INT            | Touch Panel Interrupt            |

### SD Card
| GPIO        | LCD Pin           | Function                 |
| :---        | :---              | :---                     |
| **GPIO 1**  | SD_CLK            | SPI Serial Clock         |
| **GPIO 2**  | SD_MOSI           | SPI Data In / MOSI       |
| **GPIO 3**  | SD_MISO           | SPI Data In / MOSI       |
| **GPIO 4**  | SD_CS             | Chip Select SD           |

