// MSP request bytes from a WiFi TCP/WebSocket client -> MSP2CROSSFIRE::parse -> CRSFRouter,
// the path TcpMspConnector::processData() takes (lib/WIFI/TcpMspConnector.cpp).
#include <cstring>
#include "elrs_fuzz_common.h"
#include "msp2crsf.h"

ELRS_FUZZ_GLOBALS

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    crsfRouter = CRSFRouter();
    FuzzConnector tcp(CRSF_ADDRESS_BLUETOOTH_WIFI);
    FuzzConnector flightController(CRSF_ADDRESS_FLIGHT_CONTROLLER);
    crsfRouter.addConnector(&tcp);
    crsfRouter.addConnector(&flightController);

    // AsyncTCP hands the connector one lwIP pbuf's payload: at most one TCP segment (TCP_MSS 1460),
    // sitting in a receive buffer of at least that size. Model that buffer (zero-filled past the
    // segment) so reads past the segment but inside the pbuf behave as on the device, while a read
    // past the receive buffer (a declared MSP length larger than the buffer) is caught by ASan.
    static const size_t kTcpMss = 1460;
    if (size > kTcpMss)
        return -1;
    uint8_t *buf = new uint8_t[kTcpMss]();
    memcpy(buf, data, size);
    MSP2CROSSFIRE msp2crsf;
    msp2crsf.parse(&tcp, buf, size, CRSF_ADDRESS_BLUETOOTH_WIFI, CRSF_ADDRESS_FLIGHT_CONTROLLER);
    delete[] buf;
    return 0;
}
