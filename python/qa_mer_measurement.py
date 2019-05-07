#!/usr/bin/env python2
# -*- coding: utf-8 -*-
#
# Copyright 2019 Blockstream Corp.
#
# This is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3, or (at your option)
# any later version.
#
# This software is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this software; see the file COPYING.  If not, write to
# the Free Software Foundation, Inc., 51 Franklin Street,
# Boston, MA 02110-1301, USA.
#

from gnuradio import gr, gr_unittest
from gnuradio import blocks, digital, analog
import random
import blocksat_swig as blocksat
from math import sqrt, log
import numpy as np

class qa_mer_measurement (gr_unittest.TestCase):

    def setUp (self):
        self.tb = gr.top_block()

    def tearDown (self):
        self.tb = None

    def test_001_t (self):
        """MER of noisy QPSK symbols"""

        # Parameters
        snr_db     = 10.0
        alpha      = 0.01
        M          = 2
        frame_len  = 10000

        # Constants
        bits_per_sym = int(log(M,2))
        n_bits       = int(bits_per_sym * frame_len)
        rndm         = random.Random()

        # Iterate over SNR levels
        for snr_db in reversed(range(3,15)):
            print("Trying SNR: %f dB" %(float(snr_db)))

            # Random input data
            in_vec  = tuple([rndm.randint(0,1) for i in range(0, n_bits)])
            src     = blocks.vector_source_b(in_vec)

            # Random noise
            noise_var = 1.0/(10**(float(snr_db)/10))
            noise_std = sqrt(noise_var)
            print("Noise amplitude: %f" %(noise_std))
            noise   = analog.noise_source_c(analog.GR_GAUSSIAN, noise_std, 0)

            # Fixed flowgraph blocks
            pack    = blocks.repack_bits_bb(1, bits_per_sym, "", False, gr.GR_MSB_FIRST)
            const   = digital.constellation_bpsk().base()
            cmap    = digital.chunks_to_symbols_bc(const.points())
            nadder  = blocks.add_cc()
            mer     = blocksat.mer_measurement(alpha, M, frame_len)
            snk     = blocks.vector_sink_f()
            snk_n   = blocks.vector_sink_c()

            # Connect source into flowgraph
            self.tb.connect(src, pack, cmap)
            self.tb.connect(cmap, (nadder, 0))
            self.tb.connect(noise, (nadder, 1))
            self.tb.connect(nadder, mer, snk)
            self.tb.connect(noise, snk_n)
            self.tb.run()

            # Collect results
            mer_measurements = snk.data()
            noise_samples    = snk_n.data()
            print(mer_measurements)
            avg_mer          = np.mean(mer_measurements)

            # Check results
            self.assertAlmostEqual(round(avg_mer), snr_db, places=1)

if __name__ == '__main__':
    gr_unittest.run(qa_mer_measurement, "qa_mer_measurement.xml")
