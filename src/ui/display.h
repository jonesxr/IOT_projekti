#pragma once
#include <LovyanGFX.hpp>
#include "../config.h"

// LGFX – ILI9486 konfiguraatio kosketuksella
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9486  _panel;
  lgfx::Bus_SPI        _bus;
  lgfx::Touch_XPT2046  _touch_instance; // Kosketuspaneeli
public:
  LGFX() {
    { auto cfg = _bus.config();
      cfg.spi_host=SPI2_HOST; cfg.spi_mode=0;
      cfg.freq_write=40000000; cfg.freq_read=16000000;
      cfg.spi_3wire=false; cfg.use_lock=true;
      cfg.dma_channel=SPI_DMA_CH_AUTO;
      cfg.pin_sclk=12; cfg.pin_mosi=11;
      cfg.pin_miso=13; cfg.pin_dc=9;
      _bus.config(cfg); _panel.setBus(&_bus); }
    { auto cfg = _panel.config();
      cfg.pin_cs=10; cfg.pin_rst=14; cfg.pin_busy=-1;
      cfg.panel_width=320; cfg.panel_height=480;
      cfg.readable=true; cfg.invert=false;
      cfg.rgb_order=false; cfg.dlen_16bit=false; cfg.bus_shared=true;
      _panel.config(cfg); }
    { auto cfg = _touch_instance.config();
      cfg.x_min      = 0;
      cfg.x_max      = 319;
      cfg.y_min      = 0;
      cfg.y_max      = 479;
      cfg.pin_int    = -1;
      cfg.bus_shared = true;
      cfg.offset_rotation = 0;
      cfg.spi_host = SPI2_HOST;
      cfg.freq = 1000000;
      cfg.pin_sclk = 12;
      cfg.pin_mosi = 11;
      cfg.pin_miso = 13;
      cfg.pin_cs   = TOUCH_CS_PIN;
      _touch_instance.config(cfg);
      _panel.setTouch(&_touch_instance); }
    setPanel(&_panel);
  }
};

extern LGFX gfx;

// Värit globaalisti saatavilla
extern uint16_t C_BG, C_HEADER, C_TEXT, C_DIM, C_ACCENT,
                C_GREEN, C_YELLOW, C_RED, C_CARD, C_WHITE;
