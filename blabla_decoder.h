#pragma once
#include <stdio.h>
#include <stdint.h>
#define buffer_size 1509

#define BARKER_FIRST_BYTE 0xba
#define BARKER_SECOND_BYTE 0xaa
#define BARKER_SIZE 2
#define LENGTH_SIZE 2
#define HEC_SIZE 1
#define SOURCE_PLUS_DESTINATION_SIZE 4
#define CRC_SIZE 2
#define HEADER_SIZE LENGTH_SIZE+HEC_SIZE+SOURCE_PLUS_DESTINATION_SIZE+CRC_SIZE
#define BLABLA_DECODER_SIZE 1560
typedef struct{
uint8_t private_decoder [BLABLA_DECODER_SIZE];
}blabla_decoder;
typedef struct __attribute__((packed))
{
    uint16_t length;
    uint8_t hec;
    uint16_t source;
    uint16_t destination;
    uint8_t payload[buffer_size-HEADER_SIZE];
    uint16_t crc;
} Decoded_message;
typedef void (*Message_decoded_callback)(Decoded_message *decoded_message);
size_t blabla_decode_write(blabla_decoder *decoder, uint8_t *data, size_t size);
void blabla_init(blabla_decoder *decoder);
void set_decoded_msg_callback(blabla_decoder *decoder, void (*callback)(Decoded_message *));

