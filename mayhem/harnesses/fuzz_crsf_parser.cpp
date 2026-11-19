// CRSF serial stream (flight controller / handset UART) -> CRSFParser -> CRSFRouter -> endpoint/connectors.
// The bytes are fed in <= 64-byte chunks, the size SerialIO::processSerialInput() reads per call.
#include "elrs_fuzz_common.h"
#include "CRSFParser.h"

ELRS_FUZZ_GLOBALS

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    // Fresh router, endpoint and connectors for every input: nothing survives between inputs.
    crsfRouter = CRSFRouter();
    FuzzEndpoint receiver(CRSF_ADDRESS_CRSF_RECEIVER);
    receiver.registerParameters();
    FuzzConnector flightController(CRSF_ADDRESS_FLIGHT_CONTROLLER);
    FuzzConnector handset(CRSF_ADDRESS_RADIO_TRANSMITTER);
    crsfRouter.addEndpoint(&receiver);
    crsfRouter.addConnector(&flightController);
    crsfRouter.addConnector(&handset);

    CRSFParser parser;
    unsigned found = 0;
    for (size_t off = 0; off < size; off += 64)
    {
        const uint16_t n = (uint16_t)(size - off < 64 ? size - off : 64);
        parser.processBytes(&flightController, data + off, n, [&found](const crsf_header_t *message) {
            found += message->type;
        });
    }
    return 0;
}
