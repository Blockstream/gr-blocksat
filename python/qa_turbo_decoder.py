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

from gnuradio import gr, gr_unittest, digital, fec, analog
from gnuradio import blocks
from numpy import array
import random
import math
import blocksat_swig as blocksat
import blocksattx

# Target SNR (in dB) used for tests
SNR_DB = 0

class qa_turbo_decoder (gr_unittest.TestCase):

    def setUp (self):
        self.tb = gr.top_block ()

    def tearDown (self):
        self.tb = None

    def dump(self, const, in_vec, syms, llrs, out_vec):
        """Dump some data in case of error"""
        print("Constellation points:")
        print(const)
        print("Input bits:")
        print(in_vec[1:10])
        print("Symbols:")
        print(syms[1:10])
        print("Soft decoder LLRs:")
        print(llrs[1:10])
        print("Decoded bits:")
        print(out_vec[1:10])

    def test_001_t (self):
        """Encoder to Decoder - noiseless - no constellation de-mapper"""

        # Parameters
        N         = 18444
        K         = 6144
        n_ite     = 6
        const     = [1 -1]
        flip_llrs = False

        # Input data
        in_vec = [i%2 for i in range(K)]

        # Flowgraph
        src     = blocks.vector_source_b(in_vec)
        enc     = blocksattx.turbo_encoder(N, K);
        mapb    = digital.map_bb([1,-1]);
        c2f     = blocks.char_to_float();
        dec     = blocksat.turbo_decoder(N, K, n_ite, flip_llrs);
        snk     = blocks.vector_sink_b()
        snk_sym = blocks.vector_sink_f()
        snk_llr = blocks.vector_sink_f()
        self.tb.connect(src, enc, mapb, c2f, dec, snk)
        self.tb.connect(c2f, snk_sym)
        self.tb.connect(c2f, snk_llr)
        self.tb.run()

        # NOTE: the above mapping maps bit "0" to symbol +1 and bit "1" to
        # symbol -1. The LLR is obtained directly by converting the symbol to a
        # float, meaning the assumption is that a positive LLR implies bit 0 and
        # negative LLR implies bit 1. This assumption matches with the
        # convention that is adopted in the Turbo Decoder.

        # Collect results
        out_vec = snk.data()
        syms    = snk_sym.data()
        llrs    = snk_llr.data()
        diff    = array(in_vec) - array(out_vec)
        err     = [0 if i == 0 else 1 for i in diff]

        self.dump(const, in_vec, syms, llrs, out_vec)
        print('Number of errors: %d' %(sum(err)))

        # Check results
        self.assertEqual(sum(err), 0)

    def test_002_t (self):
        """Encoder to Decoder - noisy BPSK w/ soft constellation de-mapper"""

        # Parameters
        N         = 18444
        K         = 6144
        n_ite     = 6
        snr_db    = SNR_DB
        max_len   = 10*K
        flip_llrs = True

        # NOTE: The soft decoder block outputs positive LLR if bit "1" is more
        # likely, and negative LLR otherwise (bit = 0). That is, the soft
        # de-mapper's convention is the oposite of the one adopted in the Turbo
        # Decoder. To compensate that, we choose to flip the LLRs (multiply them
        # by "-1") inside the decoder wrapper.

        # Constants
        noise_v = 1/math.sqrt((10**(float(snr_db)/10)))
        rndm = random.Random()

        # Input data
        in_vec  = tuple([rndm.randint(0,1) for i in range(0, max_len)])

        # Flowgraph
        src     = blocks.vector_source_b(in_vec)
        enc     = blocksattx.turbo_encoder(N, K)
        const   = digital.constellation_bpsk().base()
        cmap    = digital.chunks_to_symbols_bc(const.points())
        nadder  = blocks.add_cc()
        noise   = analog.noise_source_c(analog.GR_GAUSSIAN, noise_v, 0)
        cdemap  = digital.constellation_soft_decoder_cf(const)
        dec     = blocksat.turbo_decoder(N, K, n_ite, flip_llrs);
        snk     = blocks.vector_sink_b()
        snk_sym = blocks.vector_sink_c()
        snk_llr = blocks.vector_sink_f()
        self.tb.connect(src, enc, cmap)
        self.tb.connect(cmap, (nadder, 0))
        self.tb.connect(noise, (nadder, 1))
        self.tb.connect(nadder, cdemap, dec, snk)
        self.tb.connect(cmap, snk_sym)
        self.tb.connect(cdemap, snk_llr)
        self.tb.run()

        # Collect results
        out_vec = snk.data ()
        llrs    = snk_llr.data()
        syms    = snk_sym.data()
        diff    = array(in_vec) - array(out_vec)
        err     = [0 if i == 0 else 1 for i in diff]

        self.dump(const.points(), in_vec, syms, llrs, out_vec)
        print('Number of errors: %d' %(sum(err)))

        # Check results
        self.assertEqual(sum(err), 0)

    def test_003_t (self):
        """Encoder to Decoder - noisy QPSK w/ soft constellation de-mapper"""

        # Parameters
        N         = 18444
        K         = 6144
        n_ite     = 6
        snr_db    = SNR_DB
        max_len   = 10*K
        flip_llrs = True

        # NOTE: just like in the previous example, flipping of the LLRs is
        # necessary here.

        # Constants
        noise_v = 1/math.sqrt((10**(float(snr_db)/10)))
        rndm    = random.Random()

        # Input data
        in_vec  = tuple([rndm.randint(0,1) for i in range(0, max_len)])

        # Flowgraph
        src     = blocks.vector_source_b(in_vec)
        enc     = blocksattx.turbo_encoder(N, K)
        pack    = blocks.repack_bits_bb(1, 2, "", False, gr.GR_MSB_FIRST)
        const   = digital.constellation_qpsk().base()
        cmap    = digital.chunks_to_symbols_bc(const.points())
        nadder  = blocks.add_cc()
        noise   = analog.noise_source_c(analog.GR_GAUSSIAN, noise_v, 0)
        cdemap  = digital.constellation_soft_decoder_cf(const)
        dec     = blocksat.turbo_decoder(N, K, n_ite, flip_llrs);
        snk     = blocks.vector_sink_b()
        snk_sym = blocks.vector_sink_c()
        snk_llr = blocks.vector_sink_f()
        self.tb.connect(src, enc, pack, cmap)
        self.tb.connect(cmap, (nadder, 0))
        self.tb.connect(noise, (nadder, 1))
        self.tb.connect(nadder, cdemap, dec, snk)
        self.tb.connect(cmap, snk_sym)
        self.tb.connect(cdemap, snk_llr)
        self.tb.run()

        # Collect results
        out_vec = snk.data ()
        llrs    = snk_llr.data()
        syms    = snk_sym.data()
        diff    = array(in_vec) - array(out_vec)
        err     = [0 if i == 0 else 1 for i in diff]

        self.dump(const.points(), in_vec, syms, llrs, out_vec)
        print('Number of errors: %d' %(sum(err)))

        # Check results
        self.assertEqual(sum(err), 0)


if __name__ == '__main__':
    gr_unittest.run(qa_turbo_decoder, "qa_turbo_decoder.xml")
