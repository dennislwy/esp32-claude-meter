#pragma once

#include <Arduino.h>

// Waveshare ESP32-S3-ePaper-1.54 pin map (from the vendor example's user_config.h)

// Shared I2C bus: SHTC3 (0x70) and PCF85063 RTC (0x51)
constexpr int PIN_I2C_SDA = 47;
constexpr int PIN_I2C_SCL = 48;

// Peripheral power rails, active low
constexpr int PIN_EPD_PWR = 6;
constexpr int PIN_AUDIO_PWR = 42;

// Battery power latch: HIGH keeps the board powered on battery, LOW switches it off
constexpr int PIN_VBAT_PWR = 17;

// Battery voltage through a 1:2 divider (ADC1 channel 3, from the vendor's 01_ADC_Test)
constexpr int PIN_BATTERY_ADC = 4;
constexpr float BATTERY_DIVIDER_RATIO = 2.0f;

// 1.54" 200x200 SSD1681 e-paper on SPI
constexpr int PIN_EPD_SCK = 12;
constexpr int PIN_EPD_MOSI = 13;
constexpr int PIN_EPD_CS = 11;
constexpr int PIN_EPD_DC = 10;
constexpr int PIN_EPD_RST = 9;
constexpr int PIN_EPD_BUSY = 8;

// Buttons, active low
constexpr int PIN_BOOT_BUTTON = 0;
constexpr int PIN_PWR_BUTTON = 18;

// Green LED (LED_G): anode to 3V3 through 24k, cathode to GPIO3, so LOW lights it.
// The other half of the dual LED is driven by the charger's STAT pin.
constexpr int PIN_LED = 3;
constexpr uint8_t LED_ON = LOW;
constexpr uint8_t LED_OFF = HIGH;

// ES8311 codec (I2C 0x18 on the shared bus) and speaker amplifier, from the vendor's board_cfg
constexpr int PIN_I2S_MCLK = 14;
constexpr int PIN_I2S_BCLK = 15;
constexpr int PIN_I2S_WS = 38;
constexpr int PIN_I2S_DOUT = 45; // to the codec DAC
constexpr int PIN_I2S_DIN = 16;  // from the codec ADC (microphone)
constexpr int PIN_SPEAKER_AMP = 46; // HIGH enables the amplifier
