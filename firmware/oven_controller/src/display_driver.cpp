/**
 * Display Driver Implementation
 * LovyanGFX + LVGL integration for WT32-SC01
 */

#include "display_driver.h"
#include "oven_state.h"
#include "hardware_pins.h"

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// ============================================
// LovyanGFX Display Configuration
// ============================================
class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_ST7796 _panel_instance;
    lgfx::Bus_SPI _bus_instance;
    lgfx::Light_PWM _light_instance;
    lgfx::Touch_FT5x06 _touch_instance;

public:
    LGFX(void) {
        // SPI Bus configuration
        {
            auto cfg = _bus_instance.config();
            cfg.spi_host = HSPI_HOST;
            cfg.spi_mode = 0;
            cfg.freq_write = 40000000;
            cfg.freq_read = 16000000;
            cfg.spi_3wire = false;
            cfg.use_lock = true;
            cfg.dma_channel = 1;
            cfg.pin_sclk = TFT_SCLK;
            cfg.pin_mosi = TFT_MOSI;
            cfg.pin_miso = -1;
            cfg.pin_dc = TFT_DC;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }

        // Panel configuration
        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs = TFT_CS;
            cfg.pin_rst = TFT_RST;
            cfg.pin_busy = -1;
            cfg.memory_width = SCREEN_WIDTH;
            cfg.memory_height = SCREEN_HEIGHT;
            cfg.panel_width = SCREEN_WIDTH;
            cfg.panel_height = SCREEN_HEIGHT;
            cfg.offset_x = 0;
            cfg.offset_y = 0;
            cfg.offset_rotation = 0;
            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits = 1;
            cfg.readable = true;
            cfg.invert = false;
            cfg.rgb_order = false;  // Color swap handled by LVGL
            cfg.dlen_16bit = false;
            cfg.bus_shared = true;
            _panel_instance.config(cfg);
        }

        // Backlight configuration
        {
            auto cfg = _light_instance.config();
            cfg.pin_bl = TFT_BL;
            cfg.invert = false;
            cfg.freq = 44100;
            cfg.pwm_channel = 7;
            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }

        // Touch configuration
        {
            auto cfg = _touch_instance.config();
            cfg.x_min = 0;
            cfg.x_max = SCREEN_WIDTH - 1;
            cfg.y_min = 0;
            cfg.y_max = SCREEN_HEIGHT - 1;
            cfg.pin_int = TOUCH_INT;
            cfg.bus_shared = false;
            cfg.offset_rotation = 0;
            cfg.i2c_port = 1;
            cfg.i2c_addr = TOUCH_ADDR;
            cfg.pin_sda = TOUCH_SDA;
            cfg.pin_scl = TOUCH_SCL;
            cfg.freq = 400000;
            _touch_instance.config(cfg);
            _panel_instance.setTouch(&_touch_instance);
        }

        setPanel(&_panel_instance);
    }
};

// ============================================
// Static instances
// ============================================
static LGFX display;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t* buf1 = nullptr;
static lv_color_t* buf2 = nullptr;

// Buffer size: 320 x 40 lines for double buffering
#define DRAW_BUF_SIZE (SCREEN_WIDTH * 40)

// ============================================
// LVGL Display flush callback
// ============================================
static void display_flush_cb(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    display.startWrite();
    display.setAddrWindow(area->x1, area->y1, w, h);
    display.pushPixelsDMA((lgfx::swap565_t*)&color_p->full, w * h);
    display.endWrite();

    lv_disp_flush_ready(disp);
}

// ============================================
// LVGL Touch read callback
// ============================================
static void touchpad_read_cb(lv_indev_drv_t* indev, lv_indev_data_t* data) {
    uint16_t x, y;
    static bool s_was_pressed = false;

    if (display.getTouch(&x, &y)) {
        // Qualsiasi tocco, anche fuori dai pulsanti, rinvia lo spegnimento
        // automatico per inattivita' (e chiude il suo preavviso)
        if (!s_was_pressed) oven_state_register_activity();
        s_was_pressed = true;
        data->state = LV_INDEV_STATE_PR;
        data->point.x = x;
        data->point.y = y;
    } else {
        s_was_pressed = false;
        data->state = LV_INDEV_STATE_REL;
    }
}

// ============================================
// Initialize display driver
// ============================================
void display_driver_init() {
    // Initialize display hardware
    display.init();
    display.setRotation(0);  // Portrait
    display.setBrightness(255);
    display.fillScreen(0x0000);

    // Allocate draw buffers
    buf1 = (lv_color_t*)heap_caps_malloc(DRAW_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
    buf2 = (lv_color_t*)heap_caps_malloc(DRAW_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);

    if (!buf1 || !buf2) {
        Serial.println("ERROR: Failed to allocate display buffers!");
        return;
    }

    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, DRAW_BUF_SIZE);

    // Register display driver
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = SCREEN_WIDTH;
    disp_drv.ver_res = SCREEN_HEIGHT;
    disp_drv.flush_cb = display_flush_cb;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    // Register touch input driver
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchpad_read_cb;
    lv_indev_drv_register(&indev_drv);

    Serial.println("Display driver initialized");
}

// ============================================
// Get display instance
// ============================================
void* display_driver_get_display() {
    return &display;
}
