#pragma once

#include <Arduino.h>

struct EpaperPins
{
  int sck;
  int mosi;
  int cs;
  int dc;
  int rst;
  int busy;
};

// Driver for the 1.54" 200x200 SSD1681 e-paper panel, ported from Waveshare's epaper_driver_bsp.
// Keeps a 1-bit framebuffer (1 = white) and pushes it to the panel with partial refreshes.
class Epaper
{
public:
  static constexpr int WIDTH = 200;
  static constexpr int HEIGHT = 200;
  static constexpr size_t FRAME_BYTES = WIDTH * HEIGHT / 8;

  explicit Epaper(const EpaperPins &pins);

  // Full refresh to white, then switches the panel into partial-refresh mode.
  void begin();
  // Re-enters partial-refresh mode without a full refresh, e.g. after deep sleep.
  // `onScreen` must be the frame the panel is still showing, so the next refresh only changes what differs.
  void resume(const uint8_t *onScreen);
  // Puts the panel controller into deep sleep; begin() or resume() wakes it.
  void sleep();

  void clear();
  void setPixel(int x, int y, bool black);
  void refresh();
  const uint8_t *frame() const { return buffer_; }

private:
  void setupPins();
  void configureRam();
  void initFull();
  void initPartial();
  void reset();
  void waitUntilIdle();
  void sendCommand(uint8_t command);
  void sendData(uint8_t data);
  void sendData(const uint8_t *data, size_t len);
  void setLut(const uint8_t *lut);
  void setWindow(uint16_t xStart, uint16_t yStart, uint16_t xEnd, uint16_t yEnd);
  void setCursor(uint16_t x, uint16_t y);
  void turnOnDisplay(uint8_t sequence);

  const EpaperPins pins_;
  uint8_t buffer_[FRAME_BYTES];
};
