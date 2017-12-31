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

* `tx_sdr` (based on `rtl_sdr` / `rx_sdr`): transmits raw I/Q data

### Future plans

Tools to be added soon will implement modulation and encoding for:

* `OOK`, `ASK`, `AM`
* `FSK`, `4-FSK`, `GFSK`, `FM`
* `Manchester`

## Device support

Currently tested with a (somewhat broken) LimeSDR-USB,
but supporting all devices supported by SoapySDR is the goal.
Experimental, use at your own risk, but bug reports and patches are welcome.
