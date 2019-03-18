/* -*- c++ -*- */
/*
 * Copyright 2019 Blockstream
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#ifndef INCLUDED_BLOCKSAT_FFW_COARSE_FREQ_REQ_CC_IMPL_H
#define INCLUDED_BLOCKSAT_FFW_COARSE_FREQ_REQ_CC_IMPL_H

#include <blocksat/ffw_coarse_freq_req_cc.h>
#include <gnuradio/fft/fft.h>

namespace gr {
	namespace blocksat {

		class ffw_coarse_freq_req_cc_impl : public ffw_coarse_freq_req_cc
		{
		private:
			int               d_fft_len;
			float             d_alpha;
			int               d_M;
			unsigned int      d_sleep_per;
			bool              d_debug;
			float             d_beta;
			int               d_half_fft_len;
			fft::fft_complex *d_fft;
			int               d_align;
			gr_complex       *d_fft_buffer;
			float            *d_mag_buffer;
			float            *d_avg_buffer;
			float             d_delta_f;
			float             d_f_e;
			float             d_phase_inc;
			gr_complex        d_nco_phasor;
			gr_complex        d_nco_phasor_0;
			float             d_phase_accum;
			unsigned int      d_i_block;
			unsigned int      d_n_equal_corr;


		public:
			ffw_coarse_freq_req_cc_impl(int fft_len, float alpha, int M,
			                            int sleep_per, bool debug);
			~ffw_coarse_freq_req_cc_impl();

			// Where all the action really happens
			int work(int noutput_items,
			         gr_vector_const_void_star &input_items,
			         gr_vector_void_star &output_items);

			float get_frequency(void);
			void reset(void);
		};

	} // namespace blocksat
} // namespace gr

#endif /* INCLUDED_BLOCKSAT_FFW_COARSE_FREQ_REQ_CC_IMPL_H */

