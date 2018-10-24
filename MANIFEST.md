title: Blockstream Satellite Receiver OOT Module
brief: Blockstream Satellite receiver signal processing blocks.
tags:
  - sdr
  - Blockstream Satellite
  - bitcoin
author:
  - Blockstream
copyright_owner:
  - None
license:
repo: https://github.com/Blockstream/gr-blocksat
website: https://blockstream.com/satellite/
#icon: <icon_url> # Put a URL to a square image here that will be used as an icon on CGRAN
---
The "gr-blocksat" GNU Radio out-of-tree (OOT) module contains the signal
processing blocks that are used in the Blockstream Satellite receiver. More
specifically, blocks for:

- Frame timing recovery;
- Carrier phase recovery;
- Interfacing to an Aff3ct-based Turbo Decoder;
- Modulation error ratio measurement;
- Carrier-frequency offset monitoring;
- Numerically-controlled oscillator (NCO);
- and other miscellaneous signal processing tasks.

This module is used in the Blockstream Satellite receiver flowgraph, whose
source is available in
[blockstream/satellite](https://github.com/Blockstream/satellite).

