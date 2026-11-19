// CRSF MSP-response frames from the flight controller -> CRSFParser -> CROSSFIRE2MSP::parse reassembly,
// the path TcpMspConnector::forwardMessage() takes (lib/WIFI/TcpMspConnector.cpp).
#include <vector>
#include "elrs_fuzz_common.h"
#include "CRSFParser.h"
#include "crsf2msp.h"

ELRS_FUZZ_GLOBALS

namespace {
class MspConnector final : public CRSFConnector
{
public:
    MspConnector() { addDevice(CRSF_ADDRESS_BLUETOOTH_WIFI); }
    void forwardMessage(const crsf_header_t *message) override
    {
        if (message->type != CRSF_FRAMETYPE_MSP_RESP && message->type != CRSF_FRAMETYPE_MSP_REQ)
            return;
        crsf2msp.parse((const uint8_t *)message, [this](uint8_t *msp, uint32_t len) {
            out.insert(out.end(), msp, msp + len);
        });
    }
    CROSSFIRE2MSP crsf2msp;
    std::vector<uint8_t> out;
};
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    crsfRouter = CRSFRouter();
    MspConnector tcp;
    FuzzConnector flightController(CRSF_ADDRESS_FLIGHT_CONTROLLER);
    crsfRouter.addConnector(&tcp);
    crsfRouter.addConnector(&flightController);

    CRSFParser parser;
    for (size_t off = 0; off < size; off += 64)
    {
        const uint16_t n = (uint16_t)(size - off < 64 ? size - off : 64);
        parser.processBytes(&flightController, data + off, n);
    }
    return 0;
}
