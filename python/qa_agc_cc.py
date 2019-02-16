#!/usr/bin/env python2
# -*- coding: utf-8 -*-
#
# Copyright 2017 <+YOU OR YOUR COMPANY+>.
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
from math import sqrt

class qa_agc_cc (gr_unittest.TestCase):

    def rms(self, syms, N):
        """Compute the RMS of the last N symbols
        """
        abs_sq = [abs(x)**2 for x in syms[-N:]]
        return sum(abs_sq) / len(abs_sq)

    def setUp (self):
        self.tb = gr.top_block()

    def tearDown (self):
        self.tb = None

    def test_001_t (self):
        """AGC on random noisy QPSK symbols"""

        # Parameters
        snr_db     = 0.0
        const_mult = 1.5
        agc_rate   = 1e-4
        agc_ref    = 1.0
        agc_gain   = 1.0
        N_last     = 1000 # analyze the last symbols

        # Constants
        n_symbols = int(10*(1/agc_rate))
        noise_v   = 1/sqrt((10**(float(snr_db)/10)))
        rndm      = random.Random()

        # Input data
        in_vec  = tuple([rndm.randint(0,1) for i in range(0, n_symbols)])

        # Flowgraph
        src     = blocks.vector_source_b(in_vec)
        pack    = blocks.repack_bits_bb(1, 2, "", False, gr.GR_MSB_FIRST)
        const   = digital.constellation_qpsk().base()
        cmap    = digital.chunks_to_symbols_bc(const.points())
        mult    = blocks.multiply_const_cc(const_mult)
        nadder  = blocks.add_cc()
        noise   = analog.noise_source_c(analog.GR_GAUSSIAN, noise_v, 0)
        agc     = blocksat.agc_cc(agc_rate, agc_ref, agc_gain)
        snk     = blocks.vector_sink_c()
        snk2    = blocks.vector_sink_c()
        # Reference AGC approach
        rms_cf  = blocks.rms_cf(0.0001)
        f2c     = blocks.float_to_complex()
        div     = blocks.divide_cc()
        snk3    = blocks.vector_sink_c()
        self.tb.connect(src, pack, cmap, mult)
        self.tb.connect(mult, (nadder, 0))
        self.tb.connect(noise, (nadder, 1))
        self.tb.connect(nadder, agc, snk)
        self.tb.connect(nadder, snk2)
        self.tb.connect(nadder, (div, 0))
        self.tb.connect(nadder, rms_cf, (f2c, 0), (div, 1))
        self.tb.connect(div, snk3)
        self.tb.run()

        # Collect results
        agc_syms     = snk.data()
        pre_agc_syms = snk2.data()
        ref_agc_syms = snk3.data()
        rms_agc      = self.rms(agc_syms, N_last)
        rms_pre_agc  = self.rms(pre_agc_syms, N_last)
        rms_agc_ref  = self.rms(ref_agc_syms, N_last)

        print('RMS before AGC: %f' %(rms_pre_agc))
        print('RMS after AGC: %f' %(rms_agc))
        print('RMS after reference AGC: %f' %(rms_agc_ref))

        # Check results
        self.assertAlmostEqual(rms_agc, 1.0, places=1)


if __name__ == '__main__':
    gr_unittest.run(qa_agc_cc, "qa_agc_cc.xml")
