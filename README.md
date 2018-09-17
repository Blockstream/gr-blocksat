# gr-blockstream

GNU Radio Blockstream Satellite Receiver Module

## Intro

This repository contains a GNU Radio out-of-tree OOT module with the
blocks that compose the Blockstream Satellite receiver implementation.
It is a pre-requisite for building the receiver from
[blockstream/satellite](https://github.com/Blockstream/satellite).

The repository follows the standard tree that is adopted for OOT
modules and generated automatically by `gr_modtool`. For more
information, please visit
[gnuradio/OutOfTreeModules](https://wiki.gnuradio.org/index.php/OutOfTreeModules).

## Build and Installation

In the
[blockstream/satellite](https://github.com/Blockstream/satellite)
repo, this project (`gr-blockstream`) is taken as a submodule and
built/installed from there directly. However, in case you want to
build and install `gr-blockstream` separately, you can run:

```
$ make
$ sudo make install
```

