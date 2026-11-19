// mDNS / DNS-SD query on UDP 5353 (WiFi) -> MiniMDNS::handlePacket via the upstream UNIT_TEST seam testHandle().
#include <cstdint>
#include <cstring>
#include "MiniMDNS.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    // A UDP datagram on the WiFi link is at most one MTU; MiniMDNS::inSize() is a uint16_t.
    if (size > 1500)
        return -1;
    static uint8_t out[1024];
    MiniMDNS mdns("elrs_rx", 80);
    mdns.addTxt("vendor", "elrs");
    mdns.addTxt("type", "rx");
    mdns.addTxt("options", mdns.store("-DAUTO_WIFI_ON_INTERVAL=60 -DRCVR_UART_BAUD=420000"));
    // Exact-size heap copy so an over-read past the datagram is caught by ASan.
    uint8_t *pkt = new uint8_t[size ? size : 1];
    memcpy(pkt, data, size);
    mdns.testHandle(pkt, size, out, sizeof(out));
    delete[] pkt;
    return 0;
}
