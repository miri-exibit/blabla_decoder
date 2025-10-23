#include "blabla_decoder.h"
#include "hec.h"
#include "crc16.h"
#include <errno.h>
#include <string.h>
#include <stddef.h>

typedef enum
{
    noise,
    Barker,
    Length,
    Hec, 
    SourceDestinationPayload,
    CRC
} message_status;
typedef union
{
    Decoded_message msg;
    uint8_t buffer[buffer_size];
} MessageUnionBuffer;
struct blabla_decoder
{
    size_t data_index;
    size_t buffer_index;
    message_status status;
    MessageUnionBuffer msg_union_buffer;
    Message_decoded_callback message_decoded_callback;
};
void set_decoded_msg_callback(blabla_decoder *decoder, Message_decoded_callback message_decoded_callback)
{
    decoder->message_decoded_callback = message_decoded_callback;
}
size_t blabla_decode_write(blabla_decoder *decoder, uint8_t *data, size_t size)
{
    if (decoder == NULL || data == NULL || size == 0)
    {
        perror("decoder/data/size is NULL");
        return -1;
    }
    decoder->data_index = 0;
    while (decoder->data_index < size)
    {
        switch (decoder->status)
        {
        case
         noise:
        {
            if (data[decoder->data_index] == BARKER_FIRST_BYTE)
            {
                decoder->status = Barker;
            }
            decoder->data_index++;
            break;
        }
        case Barker:
        {
            if (data[decoder->data_index] == BARKER_SECOND_BYTE)
            {
                decoder->status = Length;
            }
            else
            {
                decoder->status = noise;
            }
            decoder->data_index++;
            break;
        }
        case Length:
        {
            decoder->msg_union_buffer.buffer[decoder->buffer_index] = data [decoder->data_index];
            decoder->buffer_index++;
            if (decoder->buffer_index == LENGTH_SIZE)
            {
                if (decoder->msg_union_buffer.msg.length > buffer_size)
                {
                    decoder->status = noise;
                    decoder->buffer_index = 0;
                    perror("lenght exception");
                    return -1;
                }
                decoder->status = Hec;
            }
            decoder->data_index++;
            break;
        }

        case Hec:
        {
            decoder->msg_union_buffer.buffer[decoder->buffer_index] = data [decoder->data_index];
            decoder->buffer_index++;
            if (!hec_check(decoder->msg_union_buffer.msg.length, decoder->msg_union_buffer.msg.hec))
            {
                if (!hec_correct(&(decoder->msg_union_buffer.msg.length), decoder->msg_union_buffer.msg.hec))
                {
                    decoder->status = noise;
                    decoder->buffer_index = 0;
                    perror("HEC error,resend the chunk");
                    return -1;
                }
            }
            decoder->status = SourceDestinationPayload;
            decoder->data_index++;
            break;
        }
        case SourceDestinationPayload:
        {
            int payload_proccesed_bytes = decoder->buffer_index -LENGTH_SIZE-HEC_SIZE;
            size_t all_size = decoder->msg_union_buffer.msg.length + SOURCE_PLUS_DESTINATION_SIZE - payload_proccesed_bytes;
            if (decoder->data_index + all_size > size)
            {
                all_size = size - decoder->data_index;
            }
            if(decoder->buffer_index+all_size==LENGTH_SIZE+HEC_SIZE+SOURCE_PLUS_DESTINATION_SIZE+decoder->msg_union_buffer.msg.length)
            {
                decoder->status = CRC;
            }
            memcpy(decoder->msg_union_buffer.buffer + decoder->buffer_index, data + decoder->data_index, all_size);
            decoder->buffer_index += all_size;
            decoder->data_index += all_size;
            break;
        }

        case CRC:
        {
            int excess_payload = buffer_size - HEADER_SIZE - decoder->msg_union_buffer.msg.length;
           decoder->msg_union_buffer.buffer [decoder->buffer_index + excess_payload]= data [decoder->data_index];
            decoder->buffer_index++;
            printf("Offset of height: %zu\n", offsetof( Decoded_message, crc));
            if (decoder->buffer_index == LENGTH_SIZE + HEC_SIZE + SOURCE_PLUS_DESTINATION_SIZE + decoder->msg_union_buffer.msg.length + CRC_SIZE)
            {
                decoder->status = noise;
                decoder->buffer_index = 0;
                uint16_t res_crc = crc16_compute(decoder->msg_union_buffer.msg.payload, decoder->msg_union_buffer.msg.length);
                if (res_crc != decoder->msg_union_buffer.msg.crc)
                {
                    perror("CRC error, resend the chunk");
                    return errno;
                }
                decoder->message_decoded_callback(&(decoder->msg_union_buffer.msg));
            }
            decoder->data_index++;
            break;
        }
        }
    }
    return decoder->data_index;
}

void blabla_init(blabla_decoder *decoder)
{
    if (decoder == NULL)
        return;
    memset(decoder, 0, sizeof(blabla_decoder));
    decoder->message_decoded_callback = NULL;
    decoder->status = noise;
}
void print_decoded_message(Decoded_message *msg)
{
    printf("message decoded succesfully");
}
int main()
{
    uint8_t part1[] = {0xba, 0xaa, 0x0A, 0x00, 0x00, 0x02, 0x01, 0x04, 0x03, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15};
    uint8_t part2[] = {0x16, 0x17, 0x18, 0x19, 0xBE, 0x58};
    blabla_decoder decoder;
    blabla_init(&decoder);
    set_decoded_msg_callback(&decoder, print_decoded_message);
    printf("Sending part 1 (15 bytes):\n");
    size_t processed = blabla_decode_write(&decoder, part1, sizeof(part1));
    printf("Processed %zu bytes\n", processed);
    printf("Sending part 2 (6 bytes):\n");
    processed = blabla_decode_write(&decoder, part2, sizeof(part2));
    printf("Processed %zu bytes\n", processed);
    return 0;
}