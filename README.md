# tx\_tools

`tx_sdr` tool for transmitting data to SDRs,
based on `rtl_sdr` from [librtlsdr](https://github.com/librtlsdr/librtlsdr),
and `rx_sdr` from [rx_tools](https://github.com/rxseger/rx_tools),
using the [SoapySDR](https://github.com/pothosware/SoapySDR) vendor-neutral SDR support library,
intended to support a wide range of TX-capable devices.

## Warning: mostly untested pre-release

Mostly untested pre-release quality code, YMMV.

## Installation

[Install SoapySDR](https://github.com/pothosware/SoapySDR/wiki#installation), then run:

    mkdir build ; cd build
    cmake ..
    make

## Tools included

After building, these binaries should then be available at the build directory:

* `tx_sdr` - transmits raw I/Q data
* `pulse_gen` - create I/Q data file from pulse text
* `code_gen` - create I/Q data file from code text

Also some test and example programs:

* `encode_ascii` - simple CLI to test ASCII encoding
* `encode_dmc` - simple CLI to test DMC encoding
* `encode_hex` - simple CLI to test HEX encoding
* `encode_imc` - simple CLI to test IMC encoding
* `encode_mc` - simple CLI to test MC encoding
* `code_dump` - an example how to parse and process code text
* `example_gen` - an example how to parse code text programmatically

## Output formats

* `CU4` - 4-bit /channel, unsigned I/Q data (1 byte per sample)
* `CS4` - 4-bit /channel, signed I/Q data (1 byte per sample)
* `CU8` - 8-bit /channel, unsigned I/Q data
* `CS8` - 8-bit /channel, signed I/Q data
* `CU12` - 12-bit /channel, unsigned I/Q data (3 bytes per sample)
* `CS12` - 12-bit /channel, signed I/Q data (3 bytes per sample)
* `CU16` - 16-bit /channel, unsigned I/Q data
* `CS16` - 16-bit /channel, signed I/Q data
* `CU32` - 32-bit /channel, unsigned I/Q data
* `CS32` - 32-bit /channel, signed I/Q data
* `CU64` - 64-bit /channel, unsigned I/Q data
* `CS64` - 64-bit /channel, signed I/Q data
* `CF32` - 32-bit /channel, float I/Q data
* `CF64` - 64-bit /channel, double I/Q data

For `CU8` (`CU4`) the value range is `0` to `0xff` (`0xf`) with uniform distribution,
i.e. a bias of `127.5` (`7.5`).
This matches the output format of the RTL-SDR receivers.

For `CS8` (`CS4`) the value range is `-127` to `127` (`-7` to `7`) with uniform distribution,
i.e. a smaller dynamic range than the unsigned formats, but without bias.

## Input formats

* CODE text
* PULSE text
  * OOK text
  * ASK text
  * FSK text
  * PSK text
  * TONE text

### Future plans

Tools to be added soon will implement modulation and encoding for:

* `OOK`, `ASK`, `AM`
* `FSK`, `4-FSK`, `GFSK`, `FM`
* `Manchester`

## Device support

Currently tested with a LimeSDR-USB, LimeSDR-mini, and LimeNET Micro,
but supporting all devices supported by SoapySDR is the goal.
Experimental, use at your own risk, but bug reports and patches are welcome.
