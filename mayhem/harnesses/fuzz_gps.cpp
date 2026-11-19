// GPS module UART (NMEA sentences and u-blox UBX binary) -> SerialGPS (src/src/rx-serial/SerialGPS*.cpp)
// -> CRSF GPS telemetry frames to the router. The driver sources are not a library; like upstream's
// test/test_gps they are compiled into this translation unit.
#include <cstring>
#include <string>
#include <vector>

#include "elrs_fuzz_common.h"
#include "../../src/src/rx-serial/SerialIO.cpp"
#include "../../src/src/rx-serial/SerialGPS.cpp"
#include "../../src/src/rx-serial/SerialGPS-nmea.cpp"
#include "../../src/src/rx-serial/SerialGPS-ublox.cpp"
#include "elrs_serial_fuzz.h"

ELRS_FUZZ_GLOBALS

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    nativeClockMs() = 0;
    crsfRouter = CRSFRouter();
    FuzzConnector handset(CRSF_ADDRESS_RADIO_TRANSMITTER);
    crsfRouter.addConnector(&handset);
    FuzzUart uart;
    {
        SerialGPS gps(uart, 4);
        elrsDriveSerial(gps, uart, data, size);
    }
    return 0;
}
