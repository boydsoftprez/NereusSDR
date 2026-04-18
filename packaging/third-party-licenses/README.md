# Third-Party License Notices

NereusSDR binaries bundle the following dependencies, each governed by
its own license. The full text of each dependency's license ships in
this directory alongside NereusSDR's own LICENSE.

| Dependency | Purpose | License | File |
| --- | --- | --- | --- |
| Qt 6 | GUI framework, networking, audio | LGPLv3 (dynamic linking) | qt6.txt |
| FFTW3 | Spectrum FFT | GPLv2-or-later | fftw3.txt |
| WDSP | DSP engine | GPLv2-or-later | wdsp.txt |

NereusSDR itself is GPLv3 (see root LICENSE). Aggregation with
GPLv2-or-later dependencies upgrades to GPLv3; dynamic linking against
LGPLv3 Qt is permitted under LGPLv3 §4(d) with the distribution of this
notice file.
