#include "hec.h"

uint8_t hec_compute(uint16_t data) {
    uint8_t crc = 0;
    for (int i = 0; i < 16; i++) {
        uint8_t bit = (data >> i) & 1;
        crc ^= bit;
    }
    return crc;
}

bool hec_check(uint16_t data, uint8_t hec) {
    return hec_compute(data) == hec;
}

bool hec_correct(uint16_t *data, uint8_t hec) {
    for (int i = 0; i < 16; i++) {
        uint16_t flipped = *data ^ (1 << i);
        if (hec_compute(flipped) == hec) {
            *data = flipped;
            return true;
        }
    }
    return false;
}
