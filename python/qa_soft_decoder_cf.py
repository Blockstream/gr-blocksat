#!/usr/bin/env python2
# -*- coding: utf-8 -*-
#
# Copyright 2019 <+YOU OR YOUR COMPANY+>.
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
import random, math
import blocksat_swig as blocksat

class qa_soft_decoder_cf (gr_unittest.TestCase):

    def setUp (self):
        self.tb = gr.top_block ()

    def tearDown (self):
        self.tb = None

    def test_001_t (self):
        """QPSK points in each quadrant"""

        # Parameters
        M            = 4
        N0           = 1.0
        symbol       = ((-0.5 -0.5j), (0.5 -0.5j), (-0.5 +0.5j), (0.5 + 0.5j))

        # Flowgraph
        src          = blocks.vector_source_c(symbol)
        soft_decoder = blocksat.soft_decoder_cf(M, N0)
        snk          = blocks.vector_sink_f()
        self.tb.connect(src, soft_decoder, snk)
        self.tb.run ()

        # Check data
        llrs          = snk.data()
        expected_llrs = (1.4142, 1.4142,
                         1.4142, -1.4142,
                         -1.4142, 1.4142,
                         -1.4142, -1.4142)
        print(llrs)
        self.assertFloatTuplesAlmostEqual(llrs, expected_llrs, places = 4)

    def test_002_t (self):
        """Decoding of noisy QPSK symbols"""

        # Parameters
        M            = 4
        N0           = 1.0
        max_len      = 50
        snr_db       = 20

        # Constants
        rndm         = random.Random()
        noise_v      = 1/math.sqrt((10**(float(snr_db)/10)))
        in_vec       = tuple([rndm.randint(0,1) for i in range(0, max_len)])

        # Flowgraph
        src          = blocks.vector_source_b(in_vec)
        pack         = blocks.repack_bits_bb(1, 2, "", False, gr.GR_MSB_FIRST)
        const        = digital.constellation_qpsk().base()
        cmap         = digital.chunks_to_symbols_bc(const.points())
        nadder       = blocks.add_cc()
        noise        = analog.noise_source_c(analog.GR_GAUSSIAN, noise_v, 0)
        soft_decoder = blocksat.soft_decoder_cf(M, N0)
        slicer       = digital.binary_slicer_fb()
        inverter     = digital.map_bb([1,0]);
        snk          = blocks.vector_sink_b()
        snk_llr      = blocks.vector_sink_f()
        self.tb.connect(src, pack, cmap)
        self.tb.connect(cmap, (nadder, 0))
        self.tb.connect(noise, (nadder, 1))
        self.tb.connect(nadder, soft_decoder, slicer, inverter, snk)
        self.tb.connect(soft_decoder, snk_llr)
        self.tb.run ()
        # check data
        dec_vec      = snk.data()
        llrs         = snk_llr.data()

        print(llrs)
        print(in_vec)
        print(dec_vec)
        self.assertFloatTuplesAlmostEqual(in_vec, dec_vec, places = 4)

if __name__ == '__main__':
    gr_unittest.run(qa_soft_decoder_cf, "qa_soft_decoder_cf.xml")
