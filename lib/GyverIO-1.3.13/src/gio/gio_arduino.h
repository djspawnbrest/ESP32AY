#pragma once
#include <Arduino.h>

#include "gio_defs.h"

namespace gio {

// mode
_GIO_INLINE void mode(uint8_t P, uint8_t V) {
    pinMode(P, V);
}

// read
_GIO_INLINE int read(uint8_t P) {
    return digitalRead(P);
}

// high
_GIO_INLINE void high(uint8_t P) {
    digitalWrite(P, 1);
}

// low
_GIO_INLINE void low(uint8_t P) {
    digitalWrite(P, 0);
}

// write
_GIO_INLINE void write(uint8_t P, uint8_t V) {
    digitalWrite(P, V);
}

// toggle
_GIO_INLINE void toggle(uint8_t P) {
    digitalWrite(P, !digitalRead(P));
}

// init
_GIO_INLINE void init(uint8_t P, uint8_t V = INPUT) {
    mode(P, V);
}

}