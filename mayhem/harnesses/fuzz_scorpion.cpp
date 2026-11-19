// Scorpion ESC telemetry UART -> SerialScorpion_TLM (src/src/rx-serial/SerialScorpion_TLM.cpp)
// -> CRSF battery/RPM/temperature frames. Compiled into this TU like test/test_scorpion.
#include <algorithm>
#include "elrs_fuzz_common.h"
#include "rx-serial/SerialScorpion_TLM.h"
#include "common.h"

ELRS_FUZZ_GLOBALS
bool crsfBatterySensorDetected = false;

using std::min;
#include "../../src/src/rx-serial/SerialIO.cpp"
#include "../../src/src/rx-serial/SerialScorpion_TLM.cpp"
#include "elrs_serial_fuzz.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    nativeClockMs() = 0;
    crsfBatterySensorDetected = false;
    crsfRouter = CRSFRouter();
    FuzzConnector handset(CRSF_ADDRESS_RADIO_TRANSMITTER);
    crsfRouter.addConnector(&handset);
    FuzzUart uart;
    {
        SerialScorpion_TLM esc(uart, uart);
        elrsDriveSerial(esc, uart, data, size);
    }
    return 0;
}
