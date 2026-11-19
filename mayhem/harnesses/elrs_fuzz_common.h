// Shared pieces for the ExpressLRS host fuzz harnesses (mayhem/harnesses/*.cpp).
//
// The firmware's mains (rx_main.cpp / tx_main.cpp) own a few globals the libraries reference
// (crsfRouter, ChannelData; linkStats and UID live in lib/OTA/OTA.cpp); each harness defines them once via ELRS_FUZZ_GLOBALS.
// The endpoint below dispatches exactly like RXEndpoint::handleMessage (lib/rx-crsf/RXEndpoint.cpp)
// for the generic CRSF parameter protocol (device ping / parameter read / parameter write), with one
// parameter of every type registered, so the real CRSFEndpoint code serialises and updates them.
#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

#include "CRSFRouter.h"
#include "CRSFEndpoint.h"
#include "CRSFConnector.h"
#include "crsf_protocol.h"
#include "common.h"

#define ELRS_FUZZ_GLOBALS                                  \
    CRSFRouter crsfRouter;                                 \
    uint32_t ChannelData[CRSF_NUM_CHANNELS];

extern CRSFRouter crsfRouter;

// A connector that keeps every message the router forwards through it. Copying frame_size + 2
// bytes makes ASan check that the router only ever forwards a complete, in-bounds frame.
class FuzzConnector final : public CRSFConnector
{
public:
    explicit FuzzConnector(crsf_addr_e forwardsTo) { addDevice(forwardsTo); }

    void forwardMessage(const crsf_header_t *message) override
    {
        const uint8_t *raw = reinterpret_cast<const uint8_t *>(message);
        bytes.insert(bytes.end(), raw, raw + message->frame_size + CRSF_FRAME_NOT_COUNTED_BYTES);
        frames++;
    }

    std::vector<uint8_t> bytes;
    unsigned frames = 0;
};

class FuzzEndpoint final : public CRSFEndpoint
{
public:
    explicit FuzzEndpoint(crsf_addr_e id) : CRSFEndpoint(id) {}

    void registerParameters() override
    {
        registerParameter(&folder);
        registerParameter(&rate, [](propertiesCommon *item, int32_t arg) {
            setTextSelectionValue((selectionParameter *)item, (uint8_t)arg);
        }, folder.common.id);
        registerParameter(&power, [](propertiesCommon *item, int32_t arg) {
            setUint8Value((int8Parameter *)item, (uint8_t)arg);
        });
        registerParameter(&offset, [](propertiesCommon *item, int32_t arg) {
            setInt16Value((int16Parameter *)item, (int16_t)arg);
        });
        registerParameter(&name);
        registerParameter(&info);
        registerParameter(&bind, [this](propertiesCommon *item, int32_t arg) {
            sendCommandResponse((commandParameter *)item, (commandStep_e)arg, "Binding...");
        });
    }

    void handleMessage(const crsf_header_t *message) override
    {
        const auto ext = (const crsf_ext_header_t *)message;
        if (message->type == CRSF_FRAMETYPE_DEVICE_PING ||
            message->type == CRSF_FRAMETYPE_PARAMETER_READ ||
            message->type == CRSF_FRAMETYPE_PARAMETER_WRITE)
        {
            parameterUpdateReq(ext->orig_addr, ext->type, ext->payload[0], (void *)(ext->payload + 1));
        }
    }

private:
    folderParameter folder = {{"Packet Settings", CRSF_FOLDER}, nullptr};
    selectionParameter rate = {{"Packet Rate", CRSF_TEXT_SELECTION}, 0, "50Hz;100Hz;150Hz;250Hz;500Hz", "Hz"};
    int8Parameter power = {{"Max Power", CRSF_UINT8}, {{0, 0, 7}}, "mW"};
    int16Parameter offset = {{"Offset", CRSF_INT16}, {{0, (uint16_t)-500, 500}}, "us"};
    stringParameter name = {{"Model", CRSF_STRING}, "fuzz"};
    stringParameter info = {{"Version", CRSF_INFO}, "4.0.0"};
    commandParameter bind = {{"Bind", CRSF_COMMAND}, lcsIdle, "Bind"};
};
