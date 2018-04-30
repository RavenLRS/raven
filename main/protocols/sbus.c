#include "rc/rc_data.h"

#include "sbus.h"

void sbus_encode_data(sbus_data_t *data, const rc_data_t *rc_data, bool failsafe)
{
    uint8_t flags = 0;

    if (rc_data_get_channel_value(rc_data, 16) > 0)
    {
        flags |= SBUS_FLAG_CHANNEL_16;
    }
    if (rc_data_get_channel_value(rc_data, 17) > 0)
    {
        flags |= SBUS_FLAG_CHANNEL_17;
    }

    // TODO: Packet lost?
    /*
    if (data->packet_lost)
    {
        flags |= SBUS_FLAG_PACKET_LOST;
    }
    */
    if (failsafe)
    {
        flags |= SBUS_FLAG_FAILSAFE_ACTIVE;
    }
#define CH_TO_SBUS(idx) channel_to_sbus_value(rc_data_get_channel_value(rc_data, idx))
    data->ch0 = CH_TO_SBUS(0);
    data->ch1 = CH_TO_SBUS(1);
    data->ch2 = CH_TO_SBUS(2);
    data->ch3 = CH_TO_SBUS(3);
    data->ch4 = CH_TO_SBUS(4);
    data->ch5 = CH_TO_SBUS(5);
    data->ch6 = CH_TO_SBUS(6);
    data->ch7 = CH_TO_SBUS(7);
    data->ch8 = CH_TO_SBUS(8);
    data->ch9 = CH_TO_SBUS(9);
    data->ch10 = CH_TO_SBUS(10);
    data->ch11 = CH_TO_SBUS(11);
    data->ch12 = CH_TO_SBUS(12);
    data->ch13 = CH_TO_SBUS(13);
    data->ch14 = CH_TO_SBUS(14);
    data->ch15 = CH_TO_SBUS(15);
    data->flags = flags;
}