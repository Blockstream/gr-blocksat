#!/usr/bin/env python
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
from gnuradio import blocks
import blocksat_swig as blocksat
import math
import random
from numpy.matlib import repmat

SQRT_TWO = math.sqrt(2) / 2

class qa_da_carrier_phase_rec (gr_unittest.TestCase):

    def setUp (self):
        self.tb = gr.top_block ()

    def tearDown (self):
        self.tb = None

    def test_001_t (self):
        """A single frame"""

        # Block parameters
        preamble_syms     = ((1+0j), (-1 + 0j), (1 + 0j), (-1 + 0j),
                             (1 + 0j), (-1 + 0j))
        noise_bw          = 0.1
        damp_factor       = 0.707
        const_order       = 2
        data_aided_only   = False
        reset_per_frame   = True
        tracking_syms     = ((1 + 0j), (1 + 0j), (-1 + 0j), (-1 + 0j))
        tracking_interval = 2
        data_len          = 5
        n_tracking_seqs   = math.floor(data_len / tracking_interval)
        frame_len         = int(len(preamble_syms) + \
                            (n_tracking_seqs * len(tracking_syms)) + \
                            data_len)
        debug_stats       = True
        alpha             = 1.0

        # Constants
        data_syms         = ((-1 + 0j), (1 + 0j), (1 + 0j), (-1 + 0j),
                             (-1 + 0j))
        src_data          = preamble_syms + ((-1 + 0j), (1 + 0j)) + \
                            tracking_syms + ((1 + 0j), (-1 + 0j)) + \
                            tracking_syms + ((-1 + 0j),)

        # Flowgraph
        sym_src           = blocks.vector_source_c(src_data)
        phase_rec         = blocksat.da_carrier_phase_rec(preamble_syms,
                                                          noise_bw,
                                                          damp_factor,
                                                          const_order,
                                                          data_aided_only,
                                                          reset_per_frame,
                                                          tracking_syms,
                                                          tracking_interval,
                                                          frame_len,
                                                          debug_stats,
                                                          alpha)
        dst1              = blocks.vector_sink_c()
        dst2              = blocks.vector_sink_f()

        self.tb.connect(sym_src, (phase_rec, 0))
        self.tb.connect((phase_rec, 0), dst1)
        self.tb.connect((phase_rec, 1), dst2)
        self.tb.run()
        result_data = dst1.data()
        # print("Full result:", result_data)
        self.assertComplexTuplesAlmostEqual(data_syms,
                                            result_data,
                                            6)
        self.tb.run ()

    def test_002_t (self):
        """Several frames - repeated data"""

        # Block parameters
        preamble_syms     = ((1+0j), (-1 + 0j), (1 + 0j), (-1 + 0j),
                             (1 + 0j), (-1 + 0j))
        noise_bw          = 0.1
        damp_factor       = 0.707
        const_order       = 2
        data_aided_only   = False
        reset_per_frame   = True
        tracking_syms     = ((1 + 0j), (1 + 0j), (-1 + 0j), (-1 + 0j))
        tracking_interval = 2
        data_len          = 5
        n_tracking_seqs   = math.floor(data_len / tracking_interval)
        frame_len         = int(len(preamble_syms) + \
                            (n_tracking_seqs * len(tracking_syms)) + \
                            data_len)
        debug_stats       = True
        alpha             = 1.0

        # Constants
        data_syms         = ((-1 + 0j), (1 + 0j), (1 + 0j), (-1 + 0j),
                             (-1 + 0j))
        n_frames          = 50
        full_frame        = preamble_syms + ((-1 + 0j), (1 + 0j)) + \
                            tracking_syms + ((1 + 0j), (-1 + 0j)) + \
                            tracking_syms + ((-1 + 0j),)
        src_data          = repmat(full_frame, 1, n_frames)[0]
        expected_result   = repmat(data_syms, 1, n_frames)[0]

        # Flowgraph
        sym_src           = blocks.vector_source_c(src_data)
        phase_rec         = blocksat.da_carrier_phase_rec(preamble_syms,
                                                          noise_bw,
                                                          damp_factor,
                                                          const_order,
                                                          data_aided_only,
                                                          reset_per_frame,
                                                          tracking_syms,
                                                          tracking_interval,
                                                          frame_len,
                                                          debug_stats,
                                                          alpha)
        dst1              = blocks.vector_sink_c()
        dst2              = blocks.vector_sink_f()

        self.tb.connect(sym_src, (phase_rec, 0))
        self.tb.connect((phase_rec, 0), dst1)
        self.tb.connect((phase_rec, 1), dst2)
        self.tb.run()
        result_data = dst1.data()
        # print("Full result:", result_data)
        self.assertComplexTuplesAlmostEqual(expected_result,
                                            result_data,
                                            6)
        self.tb.run ()

    def test_003_t(self):
        """Several frames - random data - BPSk or QPSK symbols"""

        # Parameters
        modulation        = "qpsk"
        noise_bw          = 0.1
        damp_factor       = 0.707
        data_aided_only   = False
        reset_per_frame   = True
        debug_stats       = True
        tracking_interval = 20
        data_len          = 100
        n_frames          = 50
        alpha             = 1.0

        # Constants and other derivated paramters
        if (modulation == "bpsk"):
            constellation = ((-1 + 0j), (1 + 0j))
            preamble_syms = ((1 + 0j), (-1 + 0j), (1 + 0j), (-1 + 0j),
                                 (1 + 0j), (-1 + 0j))
            tracking_syms = ((1 + 0j), (1 + 0j), (-1 + 0j), (-1 + 0j))
        elif (modulation == "qpsk"):
            constellation = ((-SQRT_TWO - SQRT_TWO*1j),
                             (SQRT_TWO  - SQRT_TWO*1j),
                             (-SQRT_TWO + SQRT_TWO*1j),
                             (SQRT_TWO  + SQRT_TWO*1j))
            preamble_syms = ((1 + 0j), (-1 + 0j), (1 + 0j), (-1 + 0j),
                             (1 + 0j), (-1 + 0j))
            tracking_syms = ((1 + 0j), (1 + 0j), (-1 + 0j), (-1 + 0j))
        else:
            raise ValueError("Unsupported modulation")

        M                 = len(constellation)
        preamble_len      = len(preamble_syms)
        tracking_len      = len(tracking_syms)
        n_tracking_seqs   = int(math.floor((data_len - 1)/tracking_interval))
        # NOTE: -1 prevents an extra tracking sequence when data_len is an
        # integer multiple of the tracking_interval. The framer won't put an
        # extra tracking sequence in this case.
        frame_len         = int(preamble_len + \
                                (n_tracking_seqs * tracking_len) + \
                                data_len)
        payload_len       = frame_len - preamble_len

        # Generate frames with random data symbols
        src_data        = []
        expected_result = []
        for i_frame in range(0, n_frames):
            data_syms   = tuple([constellation[random.randint(0, M-1)]
                                 for i in range(0, data_len)])
            expected_result += data_syms
            src_data        += preamble_syms
            for i_tracking_seq in range(0, n_tracking_seqs + 1):
                if (len(data_syms) > tracking_interval):
                    src_data += data_syms[:tracking_interval]
                    data_syms = data_syms[tracking_interval:]
                    src_data += tracking_syms
                else:
                    src_data += data_syms

        # Flowgraph
        sym_src           = blocks.vector_source_c(src_data)
        phase_rec         = blocksat.da_carrier_phase_rec(preamble_syms,
                                                          noise_bw,
                                                          damp_factor,
                                                          M,
                                                          data_aided_only,
                                                          reset_per_frame,
                                                          tracking_syms,
                                                          tracking_interval,
                                                          frame_len,
                                                          debug_stats,
                                                          alpha)
        dst1              = blocks.vector_sink_c()
        dst2              = blocks.vector_sink_f()

        self.tb.connect(sym_src, (phase_rec, 0))
        self.tb.connect((phase_rec, 0), dst1)
        self.tb.connect((phase_rec, 1), dst2)
        self.tb.run()
        result_data = dst1.data()
        #print("Full result:", result_data)
        self.assertComplexTuplesAlmostEqual(expected_result,
                                            result_data,
                                            6)
        self.tb.run ()

if __name__ == '__main__':
    gr_unittest.run(qa_da_carrier_phase_rec, "qa_da_carrier_phase_rec.xml")
