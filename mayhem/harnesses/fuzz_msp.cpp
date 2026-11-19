// MSP v1/v2 byte stream (displayport / backpack UART) -> MSP::processReceivedByte state machine.
#include <cstdint>
#include "msp.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    MSP msp;
    unsigned sum = 0;
    for (size_t i = 0; i < size; i++)
    {
        if (msp.processReceivedByte(data[i]))
        {
            mspPacket_t *packet = msp.getReceivedPacket();
            sum += packet->function + packet->flags + packet->type;
            while (!packet->readError)
                sum += packet->readByte();
            msp.markPacketReceived();
        }
    }
    static volatile unsigned sink;
    sink = sum;
    return 0;
}
