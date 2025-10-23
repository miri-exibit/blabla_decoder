#ifndef HEC_H
#define HEC_H

#include <stdint.h>
#include <stdbool.h>

uint8_t hec_compute(uint16_t data);

bool hec_check(uint16_t data, uint8_t hec);

bool hec_correct(uint16_t *data, uint8_t hec);

#endif
