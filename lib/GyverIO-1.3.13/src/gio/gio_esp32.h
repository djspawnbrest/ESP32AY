// based on https://github.com/PaulStoffregen/OneWire/blob/master/util/OneWire_direct_gpio.h

#pragma once
#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)

#include <Arduino.h>
#include <driver/rtc_io.h>
#include <soc/gpio_struct.h>

#include "gio_defs.h"

// old idf
#if ESP_IDF_VERSION_MAJOR < 4
#define _ESP32_IDF_V3_REG()                                                                               \
    uint32_t rtc_reg(rtc_gpio_desc[pin].reg);                                                             \
    if (rtc_reg) {                                                                                        \
        ESP_REG(rtc_reg) = ESP_REG(rtc_reg) & ~(rtc_gpio_desc[pin].mux);                                  \
        ESP_REG(rtc_reg) = ESP_REG(rtc_reg) & ~(rtc_gpio_desc[pin].pullup | rtc_gpio_desc[pin].pulldown); \
    }
#else
#define _ESP32_IDF_V3_REG()
#endif

// esp Cx
#if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6)
#define _IS_ESP_Cx_
#endif

namespace gio {

// mode
_GIO_INLINE void mode(uint8_t pin, uint8_t mode) {
    switch (mode) {
        case INPUT:
            pin = digitalPinToGPIONumber(pin);
#ifdef _IS_ESP_Cx_
            GPIO.enable_w1tc.val = (1ul << (pin));
#else
            if (digitalPinIsValid(pin)) {
                _ESP32_IDF_V3_REG();
                if (pin < 32) GPIO.enable_w1tc = (1ul << pin);
                else GPIO.enable1_w1tc.val = (1ul << (pin - 32));
            }
#endif
            break;

        case OUTPUT:
            pin = digitalPinToGPIONumber(pin);
#ifdef _IS_ESP_Cx_
            GPIO.enable_w1ts.val = (1ul << (pin));
#else
            if (digitalPinIsValid(pin)) {
                _ESP32_IDF_V3_REG();
                if (pin < 32) GPIO.enable_w1ts = (1ul << pin);
                else GPIO.enable1_w1ts.val = (1ul << (pin - 32));
            }
#endif
            break;

        default:
            pinMode(pin, mode);
            break;
    }
}

// read
_GIO_INLINE int read(uint8_t pin) {
    pin = digitalPinToGPIONumber(pin);
#ifdef _IS_ESP_Cx_
    return (GPIO.in.val >> pin) & 0x1;
#else
    if (digitalPinIsValid(pin)) {
        if (pin < 32) return (GPIO.in >> pin) & 0x1;
        else return (GPIO.in1.val >> (pin - 32)) & 0x1;
    }
#endif
    return 0;
}

// low
_GIO_INLINE void low(uint8_t pin) {
    pin = digitalPinToGPIONumber(pin);
#ifdef _IS_ESP_Cx_
    GPIO.out_w1tc.val = (1ul << pin);
#else
    if (digitalPinIsValid(pin)) {
        if (pin < 32) GPIO.out_w1tc = (1ul << pin);
        else GPIO.out1_w1tc.val = (1ul << (pin - 32));
    }
#endif
}

// high
_GIO_INLINE void high(uint8_t pin) {
    pin = digitalPinToGPIONumber(pin);
#ifdef _IS_ESP_Cx_
    GPIO.out_w1ts.val = (1ul << pin);
#else
    if (digitalPinIsValid(pin)) {
        if (pin < 32) GPIO.out_w1ts = (1ul << pin);
        else GPIO.out1_w1ts.val = (1ul << (pin - 32));
    }
#endif
}

// write
_GIO_INLINE void write(uint8_t pin, uint8_t val) {
    val ? high(pin) : low(pin);
}

// toggle
_GIO_INLINE void toggle(uint8_t pin) {
    write(pin, !read(pin));
}

// init
_GIO_INLINE void init(uint8_t P, uint8_t V = INPUT) {
    pinMode(P, V);
}

}  // namespace gio

#endif