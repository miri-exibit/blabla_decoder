#ifndef CRC16_H
#define CRC16_H

#include <stdint.h>

uint16_t crc16_compute(const uint8_t *data, int length);

#endif
