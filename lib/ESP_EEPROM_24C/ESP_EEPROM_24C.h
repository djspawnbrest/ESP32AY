#pragma once
#include <Arduino.h>
#include <Wire.h>

class ESP_EEPROM_24C {
public:
    ESP_EEPROM_24C(uint8_t address = 0x50);
    uint8_t begin();
    uint8_t read(uint32_t addr, uint8_t* data, uint16_t len);
    uint8_t write(uint32_t addr, uint8_t* data, uint16_t len);
    uint8_t write(uint32_t addr, uint8_t value);
    void setAddress(uint8_t address);

private:
    uint8_t _address;
    static constexpr uint8_t PAGE_SIZE = 32;
};
