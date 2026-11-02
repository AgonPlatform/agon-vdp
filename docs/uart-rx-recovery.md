# UART receive-buffer recovery

`video/uart_rx_recovery.c` is the ESP-IDF v4.4.6 UART driver with one lock-scope
correction: publish `rx_buffer_full_flg` inside the same critical section that
disables RX interrupts. This prevents another core's reader from recovering
between publication and masking, leaving reception disabled with the flag clear.

The PlatformIO platform remains pinned to `espressif32@6.6.0` (Arduino ESP32
2.0.14 / ESP-IDF 4.4.6). Compiling the driver as a project C object supplies the
UART implementation without modifying the installed SDK; the original
`libdriver.a(uart.c.obj)` must not also be linked. Reassess this local replacement
when changing the SDK version.

Source: Espressif `esp-idf` tag `v4.4.6`, `components/driver/uart.c`.
Original SHA-256: `0a90ac006b6a0cadaac4c47ab035a54c782fb6171fda82e9dd059a3d84c4b0be`.
Patched SHA-256: `6fe284dbb987890ec80d337d6863a5f7113c2986b080f37269b9bba49be8e38c`.
Its Espressif copyright and Apache-2.0 notice are retained; the full license is
in `licenses/ESP-IDF-UART-LICENSE`.

The production driver contains no diagnostic delay, rescue routine or private
test command. Controlled before/after tests on Heber Agon Console8 reproduced
the original stuck state and verified both 4,096-byte payloads after the fix,
with matching CRCs, no receive errors and no diagnostic rescue. This establishes
the controlled interleaving; natural failure frequency was not measured.
