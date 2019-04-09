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
from gnuradio import blocks
import blocksat_swig as blocksat
import numpy as np
from numpy.matlib import repmat

class qa_frame_synchronizer_cc (gr_unittest.TestCase):

    def setUp (self):
        self.tb = gr.top_block ()
        self.barker_code = (-1.0000, -1.0000, -1.0000, -1.0000, -1.0000, 1.0000,
                            1.0000, -1.0000, -1.0000, 1.0000, -1.0000, 1.0000,
                            -1.0000)

    def tearDown (self):
        self.tb = None

    def test_001_t (self):
        # Parameters
        preamble_len      = 13
        payload_len       = 20
        frame_len         = preamble_len + payload_len
        M                 = 2
        threshold         = 0.5
        n_success_to_lock = 1
        en_eq             = False
        en_phase_corr     = False
        en_freq_corr      = False
        debug_level       = 3
        n_frames          = 10
        n_silence         = 5

        rx_preamble  = (-0.9511 - 0.3090j, -0.9298 - 0.3681j, -0.9048 - 0.4258j,
                        -0.8763 - 0.4818j, -0.8443 - 0.5358j, 0.8090 + 0.5878j,
                        0.7705 + 0.6374j, -0.7290 - 0.6845j, -0.6845 - 0.7290j,
                        0.6374 + 0.7705j, -0.5878 - 0.8090j, 0.5358 + 0.8443j,
                        -0.4818 - 0.8763j)
        rx_payload   = (0.4258 + 0.9048j, -0.3681 - 0.9298j, -0.3090 - 0.9511j,
                        -0.2487 - 0.9686j, 0.1874 + 0.9823j, 0.1253 + 0.9921j,
                        -0.0628 - 0.9980j, 0.0000 - 1.0000j, 0.0628 - 0.9980j,
                        0.1253 - 0.9921j, -0.1874 + 0.9823j, 0.2487 - 0.9686j,
                        0.3090 - 0.9511j, 0.3681 - 0.9298j, -0.4258 + 0.9048j,
                        -0.4818 + 0.8763j, 0.5358 - 0.8443j, -0.5878 + 0.8090j,
                        0.6374 - 0.7705j, -0.6845 + 0.7290j)
        rx_frame      = rx_preamble + rx_payload
        # Expected result should exclude the number of frames to lock
        expected_res  = tuple(repmat(rx_frame, 1, n_frames - n_success_to_lock)[0])

        # By using the number of silence symbols, try all possible frame start
        # indexes:
        for n_silence in range(0, frame_len):
            print("Try n_silence = %d" %(n_silence))
            silence_syms = 0.0001 * np.ones(n_silence, np.complex64)
            rx_syms      = np.concatenate((silence_syms, tuple(repmat(rx_frame, 1, n_frames)[0])))


            # Flowgraph
            sym_src            = blocks.vector_source_c(rx_syms)
            frame_synchronizer = blocksat.frame_synchronizer_cc(self.barker_code,
                                                                frame_len,
                                                                M,
                                                                n_success_to_lock,
                                                                en_eq,
                                                                en_phase_corr,
                                                                en_freq_corr,
                                                                debug_level)
            sym_snk            = blocks.vector_sink_c ()
            self.tb.connect(sym_src, (frame_synchronizer, 0))
            self.tb.connect((frame_synchronizer, 0), sym_snk)
            self.tb.run()
            res_sym_out  = sym_snk.data()

            # Results
            self.assertFloatTuplesAlmostEqual (expected_res[1:frame_len],
                                               res_sym_out[1:frame_len], 6)


if __name__ == '__main__':
    gr_unittest.run(qa_frame_synchronizer_cc, "qa_frame_synchronizer_cc.xml")
