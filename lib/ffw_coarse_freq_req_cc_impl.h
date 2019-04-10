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
			int               d_frame_len;
			int               d_sps;
			int               d_frame_len_oversamp;
			float             d_beta;
			int               d_half_fft_len;
			fft::fft_complex *d_fft;
			gr_complex       *d_fft_buffer;
			float            *d_mag_buffer;
			float            *d_avg_buffer;
			uint32_t         *d_i_max_buffer;
			float             d_delta_f;
			float             d_f_e;
			float             d_pend_f_e;
			float             d_phase_inc;
			float             d_phase_accum;
			gr_complex        d_nco_phasor;
			unsigned int      d_i_block;
			unsigned int      d_n_equal_corr;
			int               d_start_index;
			int               d_i_sample;
			bool              d_pend_corr_update;
			bool              d_frame_locked;

			void update_nco_phase(int n_samples);

		public:
			ffw_coarse_freq_req_cc_impl(int fft_len, float alpha, int M,
			                            int sleep_per, bool debug,
			                            int frame_len, int sps);
			~ffw_coarse_freq_req_cc_impl();

			// Where all the action really happens
			void handle_set_start_index(pmt::pmt_t msg);
			int work(int noutput_items,
			         gr_vector_const_void_star &input_items,
			         gr_vector_void_star &output_items);

			float get_frequency(void);
			void reset(void);
		};

	} // namespace blocksat
} // namespace gr

#endif /* INCLUDED_BLOCKSAT_FFW_COARSE_FREQ_REQ_CC_IMPL_H */

