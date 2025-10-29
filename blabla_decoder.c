#include "blabla_decoder.h"
#include "hec.h"
#include "crc16.h"
#include <errno.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#define AS_PRIVATE(d) ((blabla_decoder_private *)(d))
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
typedef struct blabla_decoder_private
{
    size_t data_index;
    size_t buffer_index;
    message_status status;
    MessageUnionBuffer msg_union_buffer;
    Message_decoded_callback message_decoded_callback;
} blabla_decoder_private;
void set_decoded_msg_callback(blabla_decoder *decoder, Message_decoded_callback message_decoded_callback)
{
    
    blabla_decoder_private *p_decoder = AS_PRIVATE(decoder);
    if (p_decoder == NULL )
    {
        errno = EINVAL; /* Invalid argument */
        perror("p_decoder/data/size is NULL");
        return ;
    }
    p_decoder->message_decoded_callback = message_decoded_callback;
}
size_t blabla_decode_write(blabla_decoder *decoder, uint8_t *data, size_t size)
{
    blabla_decoder_private *p_decoder = AS_PRIVATE(decoder);
    if (p_decoder == NULL || data == NULL || size == 0)
    {
        errno = EINVAL; /* Invalid argument */
        perror("p_decoder/data/size is NULL");
        return -1;
    }
    p_decoder->data_index = 0;
    while (p_decoder->data_index < size)
    {
        switch (p_decoder->status)
        {
        case noise:
        {
            if (data[p_decoder->data_index] == BARKER_FIRST_BYTE)
            {
                p_decoder->status = Barker;
            }
            p_decoder->data_index++;
            break;
        }
        case Barker:
        {
            if (data[p_decoder->data_index] == BARKER_SECOND_BYTE)
            {
                p_decoder->status = Length;
            }
            else if (data[p_decoder->data_index] == BARKER_FIRST_BYTE)
            {
                p_decoder->status = Barker;
            }
            else
            {
                p_decoder->status = noise;
            }
            p_decoder->data_index++;
            break;
        }
        case Length:
        {
            p_decoder->msg_union_buffer.buffer[p_decoder->buffer_index] = data[p_decoder->data_index];
            p_decoder->buffer_index++;
            if (p_decoder->buffer_index == LENGTH_SIZE)
            {
                p_decoder->msg_union_buffer.msg.length = __builtin_bswap16(p_decoder->msg_union_buffer.msg.length);
                if (p_decoder->msg_union_buffer.msg.length > buffer_size)
                {
                    p_decoder->status = noise;
                    p_decoder->buffer_index = 0;

                    perror("lenght exception");
                    errno = EINVAL; 
                 return -1;
                }
                p_decoder->status = Hec;
            }
            p_decoder->data_index++;
            break;
        }

        case Hec:
        {
            p_decoder->msg_union_buffer.buffer[p_decoder->buffer_index] = data[p_decoder->data_index];
            p_decoder->buffer_index++;
            if (!hec_check(p_decoder->msg_union_buffer.msg.length, p_decoder->msg_union_buffer.msg.hec))
            {
                if (!hec_correct(&(p_decoder->msg_union_buffer.msg.length), p_decoder->msg_union_buffer.msg.hec))
                {
                    p_decoder->status = noise;
                    p_decoder->buffer_index = 0;
                    errno = EIO;
                    perror("HEC error,resend the chunk");
                    return -1;
                }
            }
            p_decoder->status = SourceDestinationPayload;
            p_decoder->data_index++;
            break;
        }
        case SourceDestinationPayload:
        {
            int payload_proccesed_bytes = p_decoder->buffer_index - LENGTH_SIZE - HEC_SIZE;
            size_t all_size = p_decoder->msg_union_buffer.msg.length + SOURCE_PLUS_DESTINATION_SIZE - payload_proccesed_bytes;
            if (p_decoder->data_index + all_size > size)
            {
                all_size = size - p_decoder->data_index;
            }
            if (p_decoder->buffer_index + all_size == LENGTH_SIZE + HEC_SIZE + SOURCE_PLUS_DESTINATION_SIZE + p_decoder->msg_union_buffer.msg.length)
            {
                p_decoder->msg_union_buffer.msg.source = __builtin_bswap16(p_decoder->msg_union_buffer.msg.source);
                p_decoder->msg_union_buffer.msg.destination = __builtin_bswap16(p_decoder->msg_union_buffer.msg.destination);
                p_decoder->status = CRC;
            }
            memcpy(p_decoder->msg_union_buffer.buffer + p_decoder->buffer_index, data + p_decoder->data_index, all_size);
            p_decoder->buffer_index += all_size;
            p_decoder->data_index += all_size;
            break;
        }

        case CRC:
        {
            int excess_payload = buffer_size - HEADER_SIZE - p_decoder->msg_union_buffer.msg.length;
            p_decoder->msg_union_buffer.buffer[p_decoder->buffer_index + excess_payload] = data[p_decoder->data_index];
            p_decoder->buffer_index++;
            if (p_decoder->buffer_index == LENGTH_SIZE + HEC_SIZE + SOURCE_PLUS_DESTINATION_SIZE + p_decoder->msg_union_buffer.msg.length + CRC_SIZE)
            {
                p_decoder->msg_union_buffer.msg.crc = __builtin_bswap16(p_decoder->msg_union_buffer.msg.crc);
                p_decoder->status = noise;
                p_decoder->buffer_index = 0;
                uint16_t res_crc = crc16_compute(p_decoder->msg_union_buffer.msg.payload, p_decoder->msg_union_buffer.msg.length);
                if (res_crc != p_decoder->msg_union_buffer.msg.crc)
                {
                    errno = EIO;
                    perror("CRC error, resend the chunk");
                    return -1;
                }
                p_decoder->message_decoded_callback(&(p_decoder->msg_union_buffer.msg));
            }
            p_decoder->data_index++;
            break;
        }
        }
    }
    return p_decoder->data_index;
}

void blabla_init(blabla_decoder *decoder)
{
    blabla_decoder_private *p_decoder = AS_PRIVATE(decoder);
    if (p_decoder == NULL)
        return;
    memset(p_decoder, 0, sizeof(blabla_decoder_private));
    p_decoder->message_decoded_callback = NULL;
    p_decoder->status = noise;
}
