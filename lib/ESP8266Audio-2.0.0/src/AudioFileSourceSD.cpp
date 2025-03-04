/*
  AudioFileSourceSPIFFS
  Input SD card "file" to be used by AudioGenerator
  
  Copyright (C) 2017  Earle F. Philhower, III

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "AudioFileSourceSD.h"

AudioFileSourceSD::AudioFileSourceSD()
{
    _semaphore = nullptr;
    _useSemaphore = false;
}

AudioFileSourceSD::AudioFileSourceSD(SemaphoreHandle_t semaphore)
{
    _semaphore = semaphore;
    _useSemaphore = true;
}

AudioFileSourceSD::AudioFileSourceSD(const char *filename)
{
    _semaphore = nullptr;
    _useSemaphore = false;
    open(filename);
}

AudioFileSourceSD::AudioFileSourceSD(const char *filename, SemaphoreHandle_t semaphore)
{
    _semaphore = semaphore;
    _useSemaphore = true;
    open(filename);
}

AudioFileSourceSD::~AudioFileSourceSD()
{
    if (f) close();
}

bool AudioFileSourceSD::open(const char *filename)
{
    if (_useSemaphore && _semaphore) {
        if (xSemaphoreTake(_semaphore, portMAX_DELAY) != pdTRUE) {
            return false;
        }
        f = SD.open(filename, FILE_READ);
        xSemaphoreGive(_semaphore);
        return f ? true : false;
    } else {
        f = SD.open(filename, FILE_READ);
        return f ? true : false;
    }
}

uint32_t AudioFileSourceSD::read(void *data, uint32_t len)
{
    if (_useSemaphore && _semaphore) {
        if (xSemaphoreTake(_semaphore, portMAX_DELAY) != pdTRUE) {
            return 0;
        }
        size_t result = f.read(reinterpret_cast<uint8_t*>(data), len);
        xSemaphoreGive(_semaphore);
        return result;
    } else {
        return f.read(reinterpret_cast<uint8_t*>(data), len);
    }
}

bool AudioFileSourceSD::seek(int32_t pos, int dir)
{
    if (!f) return false;

    if (_useSemaphore && _semaphore) {
        if (xSemaphoreTake(_semaphore, portMAX_DELAY) != pdTRUE) {
            return false;
        }
        bool result;
        if (dir == SEEK_SET) result = f.seek(pos);
        else if (dir == SEEK_CUR) result = f.seek(f.position() + pos);
        else if (dir == SEEK_END) result = f.seek(f.size() + pos);
        else result = false;
        xSemaphoreGive(_semaphore);
        return result;
    } else {
        if (dir == SEEK_SET) return f.seek(pos);
        else if (dir == SEEK_CUR) return f.seek(f.position() + pos);
        else if (dir == SEEK_END) return f.seek(f.size() + pos);
        return false;
    }
}

bool AudioFileSourceSD::close()
{
    if (_useSemaphore && _semaphore) {
        if (xSemaphoreTake(_semaphore, portMAX_DELAY) != pdTRUE) {
            return false;
        }
        f.close();
        xSemaphoreGive(_semaphore);
        return true;
    } else {
        f.close();
        return true;
    }
}

bool AudioFileSourceSD::isOpen()
{
    if (_useSemaphore && _semaphore) {
        if (xSemaphoreTake(_semaphore, portMAX_DELAY) != pdTRUE) {
            return false;
        }
        bool result = f ? true : false;
        xSemaphoreGive(_semaphore);
        return result;
    } else {
        return f ? true : false;
    }
}

uint32_t AudioFileSourceSD::getSize()
{
    if (!f) return 0;

    if (_useSemaphore && _semaphore) {
        if (xSemaphoreTake(_semaphore, portMAX_DELAY) != pdTRUE) {
            return 0;
        }
        size_t result = f.size();
        xSemaphoreGive(_semaphore);
        return result;
    } else {
        return f.size();
    }
}

uint32_t AudioFileSourceSD::getPos()
{
    if (!f) return 0;

    if (_useSemaphore && _semaphore) {
        if (xSemaphoreTake(_semaphore, portMAX_DELAY) != pdTRUE) {
            return 0;
        }
        size_t result = f.position();
        xSemaphoreGive(_semaphore);
        return result;
    } else {
        return f.position();
    }
}
