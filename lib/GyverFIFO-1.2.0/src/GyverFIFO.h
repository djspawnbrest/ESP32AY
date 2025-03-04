#pragma once
#include <inttypes.h>
#include <stddef.h>

template <typename T, size_t SIZE, typename Ti = uint16_t>
class GyverFIFO {
   public:
    // запись в буфер. Вернёт true при успешной записи
    bool write(T val) {
        if (len < SIZE) {
            Ti i = head + len;
            buffer[i >= SIZE ? i - SIZE : i] = val;
            ++len;
            return true;
        }
        return false;
    }

    // доступность для записи (свободное место)
    bool availableForWrite() {
        return len < SIZE;
    }

    // чтение из буфера. Если буфер пуст, поведение не определено
    T read() {
        Ti i = head;
        if (len) {
            head = (head + 1 >= SIZE) ? 0 : (head + 1);
            --len;
        }
        return buffer[i];
    }

    // возвращает крайнее значение без удаления из буфера
    T peek() {
        return buffer[head];
    }

    // вернёт количество непрочитанных элементов
    int available() {
        return len;
    }

    // очистка буфера
    void clear() {
        len = 0;
    }

   private:
    T buffer[SIZE];
    Ti head = 0, len = 0;
};