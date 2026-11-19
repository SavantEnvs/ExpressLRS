// Graupner HoTT telemetry bus UART (GPS/EAM/GAM/Vario/ESC answers) -> SerialHoTT_TLM
// (src/src/rx-serial/SerialHoTT_TLM.cpp) -> CRSF sensor frames. Compiled into this TU like test/test_hott.
#include <algorithm>
#define TARGET_RX 1
#include "elrs_fuzz_common.h"
#include "common.h"

ELRS_FUZZ_GLOBALS
connectionState_e connectionState = connected;
bool crsfBatterySensorDetected = false;
bool crsfBaroSensorDetected = false;

// crsf_protocol.h only provides htobe24 for non-Linux (firmware) builds (same shim as test/test_hott)
static inline uint32_t htobe24(uint32_t val)
{
    return ((val & 0xff) << 16) | (val & 0xff00) | ((val >> 16) & 0xff);
}

using std::min;
#include "../../src/src/rx-serial/SerialIO.cpp"
#include "../../src/src/rx-serial/SerialHoTT_TLM.cpp"
#include "elrs_serial_fuzz.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    nativeClockMs() = 0;
    connectionState = connected;
    crsfBatterySensorDetected = false;
    crsfBaroSensorDetected = false;
    crsfRouter = CRSFRouter();
    FuzzConnector handset(CRSF_ADDRESS_RADIO_TRANSMITTER);
    crsfRouter.addConnector(&handset);
    FuzzUart uart;
    {
        SerialHoTT_TLM hott(uart, uart);
        elrsDriveSerial(hott, uart, data, size);
    }
    return 0;
}
