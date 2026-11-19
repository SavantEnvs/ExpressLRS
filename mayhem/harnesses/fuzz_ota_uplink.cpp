// Receiver side of the RF link: over-the-air uplink packets as the radio hands them to rx_main.cpp's
// ProcessRFPacket(): OtaValidatePacketCrc, then by packet type
//   RCDATA -> OtaUnpackChannelData (hybrid / wide / 8ch / 12ch / 16ch switch encodings)
//   DATA   -> StubbornReceiver::ReceiveData reassembly (or OtaUnpackAirportData in airport mode),
//             and a completed buffer goes to CRSFRouter::processMessage from the OTA connector,
//             as DataUlReceiveComplete() does.
// Input: [config] then repeated [nonce][packet (8 bytes, or 13 in full-res mode)].
//   config bit0: full-res (8-byte OTA8 packets), bits1-2: switch mode, bit3: airport mode.
#include <cstring>
#include "elrs_fuzz_common.h"
#include "OTA.h"
#include "FIFO.h"
#include "stubborn_receiver.h"
#include "telemetry_protocol.h"
#include "msptypes.h"
#include "RXOTAConnector.h"

ELRS_FUZZ_GLOBALS

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 1)
        return -1;
    const uint8_t config = data[0];
    const bool fullRes = config & 1;
    const OtaSwitchMode_e switchMode = (OtaSwitchMode_e)(((config >> 1) & 3) % 3);
    const bool airport = config & 8;

    // Bound to a fixed bind phrase; everything the RF code keeps in globals is reset here.
    static const uint8_t uid[UID_LEN] = {0, 0, 0x12, 0x34, 0x56, 0x78};
    memcpy(UID, uid, UID_LEN);
    OtaIsFullRes = fullRes;
    isArmed = false;
    OtaUpdateCrcInitFromUid();
    OtaUpdateSerializers(switchMode, fullRes ? OTA8_PACKET_SIZE : OTA4_PACKET_SIZE);
    OtaResetChannelDataComplete();
    OtaSetHybrid8NextSwitchIndex(0);
    OtaSetFullResNextChannelSet(false);
    for (auto &ch : ChannelData)
        ch = CRSF_CHANNEL_VALUE_UNSET;

    crsfRouter = CRSFRouter();
    FuzzEndpoint receiver(CRSF_ADDRESS_CRSF_RECEIVER);
    receiver.registerParameters();
    RXOTAConnector otaConnector;
    FuzzConnector flightController(CRSF_ADDRESS_FLIGHT_CONTROLLER);
    crsfRouter.addEndpoint(&receiver);
    crsfRouter.addConnector(&otaConnector);
    crsfRouter.addConnector(&flightController);

    uint8_t dataUlBuffer[ELRS_DATA_UL_BUFFER];
    StubbornReceiver dataUlReceiver;
    dataUlReceiver.setMaxPackageIndex(ELRS_MSP_MAX_PACKAGES);
    dataUlReceiver.SetDataToReceive(dataUlBuffer, ELRS_DATA_UL_BUFFER);
    FIFO<AP_MAX_BUF_LEN> apOutputBuffer;

    const size_t pktSize = fullRes ? OTA8_PACKET_SIZE : OTA4_PACKET_SIZE;
    size_t pos = 1;
    while (size - pos >= 1 + pktSize)
    {
        OtaNonce = data[pos++];
        OTA_Packet_s pkt;
        memset(&pkt, 0, sizeof(pkt));
        memcpy(&pkt, data + pos, pktSize);
        pos += pktSize;

        if (!OtaValidatePacketCrc(&pkt))
            continue;

        switch (pkt.std.type)
        {
        case PACKET_TYPE_RCDATA:
            OtaUnpackChannelData(&pkt, ChannelData);
            OtaIsChannelDataComplete(ChannelData);
            break;
        case PACKET_TYPE_DATA:
            if (airport)
            {
                OtaUnpackAirportData(&pkt, &apOutputBuffer);
                while (apOutputBuffer.size())
                    apOutputBuffer.pop();
                break;
            }
            if (fullRes)
                dataUlReceiver.ReceiveData(pkt.full.data_ul.packageIndex, pkt.full.data_ul.payload, sizeof(pkt.full.data_ul.payload));
            else
                dataUlReceiver.ReceiveData(pkt.std.data_ul.packageIndex, pkt.std.data_ul.payload, sizeof(pkt.std.data_ul.payload));
            break;
        default:
            break;
        }

        // rx_main.cpp loop(): a completed uplink buffer is a CRSF message unless it is one of the
        // two MSP commands handled out of band (wifi mode switch / MAVLink passthrough).
        if (dataUlReceiver.HasFinishedData())
        {
            if (dataUlBuffer[0] != MSP_ELRS_SET_RX_WIFI_MODE && dataUlBuffer[0] != MSP_ELRS_MAVLINK_TLM)
                crsfRouter.processMessage(&otaConnector, (crsf_header_t *)dataUlBuffer);
            dataUlReceiver.Unlock();
        }
    }
    return 0;
}
