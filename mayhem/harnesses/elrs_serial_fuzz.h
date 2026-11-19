// Drives an rx-serial telemetry driver (src/src/rx-serial/Serial*.cpp) the way rx_main's
// handleSerialIO() does: bytes arrive on the UART, processSerialInput() reads them, the RC-frame tick
// and sendQueuedData() run, and time passes. The fuzz input is a sequence of steps:
//   [ctl] [ctl & 0x3f bytes of UART data]
// where ctl >> 6 picks how far the clock advances before the step (0 / 5 / 100 / 1500 ms).
#pragma once

#include <cstdint>
#include <vector>

#include "targets.h"

class FuzzUart final : public HardwareSerial
{
public:
    std::vector<uint8_t> rx;
    std::vector<uint8_t> tx;
    size_t rxPos = 0;

    int available() override { return (int)(rx.size() - rxPos); }
    int read() override { return rxPos < rx.size() ? rx[rxPos++] : -1; }
    int peek() override { return rxPos < rx.size() ? rx[rxPos] : -1; }
    void flush() override {}
    size_t write(uint8_t c) override { tx.push_back(c); return 1; }
    size_t write(const uint8_t *s, size_t l) override { tx.insert(tx.end(), s, s + l); return l; }
    void updateBaudRate(unsigned long) override {}
};

template <typename Driver>
static void elrsDriveSerial(Driver &driver, FuzzUart &uart, const uint8_t *data, size_t size)
{
    static const unsigned long steps[4] = {0, 5, 100, 1500};
    size_t pos = 0;
    while (pos < size)
    {
        const uint8_t ctl = data[pos++];
        size_t n = ctl & 0x3f;
        if (n > size - pos)
            n = size - pos;
        nativeClockMs() += steps[ctl >> 6];
        driver.sendRCFrame(false, false, nullptr);
        uart.rx.insert(uart.rx.end(), data + pos, data + pos + n);
        pos += n;
        while (uart.available() > 0)
            driver.processSerialInput();
        driver.sendQueuedData(128);
        uart.tx.clear();
    }
}
