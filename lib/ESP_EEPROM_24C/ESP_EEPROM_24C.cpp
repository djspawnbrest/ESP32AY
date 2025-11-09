#include "ESP_EEPROM_24C.h"

ESP_EEPROM_24C::ESP_EEPROM_24C(uint8_t address) : _address(address) {}

uint8_t ESP_EEPROM_24C::begin() {
    Wire.beginTransmission(_address);
    Wire.write(0);
    Wire.write(0);
    return Wire.endTransmission();
}

void ESP_EEPROM_24C::setAddress(uint8_t address) {
    _address = address;
}

uint8_t ESP_EEPROM_24C::read(uint32_t addr, uint8_t* data, uint16_t len) {
    uint16_t offset = 0;
    while (len > 0) {
        uint16_t chunk = (len > 32) ? 32 : len;
        Wire.beginTransmission(_address);
        Wire.write((uint8_t)(addr >> 8));
        Wire.write((uint8_t)(addr & 0xFF));
        uint8_t status = Wire.endTransmission();
        if (status != 0) return status;
        
        Wire.requestFrom(_address, (uint8_t)chunk);
        for (uint16_t i = 0; i < chunk; i++) {
            if (Wire.available()) {
                data[offset++] = Wire.read();
            }
        }
        addr += chunk;
        len -= chunk;
    }
    return 0;
}

uint8_t ESP_EEPROM_24C::write(uint32_t addr, uint8_t* data, uint16_t len) {
    uint16_t offset = 0;
    while (len > 0) {
        uint16_t pageOffset = addr % PAGE_SIZE;
        uint16_t chunk = PAGE_SIZE - pageOffset;
        if (chunk > len) chunk = len;
        
        Wire.beginTransmission(_address);
        Wire.write((uint8_t)(addr >> 8));
        Wire.write((uint8_t)(addr & 0xFF));
        for (uint16_t i = 0; i < chunk; i++) {
            Wire.write(data[offset++]);
        }
        uint8_t status = Wire.endTransmission();
        if (status != 0) return status;
        
        // delay(5);

        // Wait for write cycle to complete (polling)
        for (uint8_t i = 0; i < 100; i++) {
            delayMicroseconds(10);
            Wire.beginTransmission(_address);
            if (Wire.endTransmission() == 0) break;
        }
        
        addr += chunk;
        len -= chunk;
    }
    return 0;
}

uint8_t ESP_EEPROM_24C::write(uint32_t addr, uint8_t value) {
    return write(addr, &value, 1);
}
