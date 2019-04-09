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

#ifndef INCLUDED_BLOCKSAT_FRAME_SYNCHRONIZER_CC_IMPL_H
#define INCLUDED_BLOCKSAT_FRAME_SYNCHRONIZER_CC_IMPL_H

#include <blocksat/frame_synchronizer_cc.h>
#include <gnuradio/filter/fir_filter_with_buffer.h>

namespace gr {
	namespace blocksat {

		class frame_synchronizer_cc_impl : public frame_synchronizer_cc
		{
		private:
			/* From parameters */
			std::vector<gr_complex> d_preamble_syms;
			std::vector<gr_complex> d_pmf_taps;
			int d_frame_len;
			int d_M;
			int d_n_success_to_lock;
			bool d_en_eq;
			bool d_en_phase_corr;
			bool d_en_freq_corr;
			int d_debug_level;
			/* Other private variables */
			gr::filter::kernel::fir_filter_with_buffer_ccc* d_pmf;
			int           d_i_frame;
			int           d_preamble_len;
			int           d_align;
			gr_complex   *d_pmf_out_buffer;
			float        *d_mag_pmf_buffer;
			uint32_t     *d_i_max;
			gr_complex   *d_pmf_tap_buffer;
			int           d_i_frame_start;
			int           d_last_i_frame_start;
			int           d_peak_delay;
			bool          d_locked;
			unsigned int  d_success_cnt;
			unsigned int  d_fail_cnt;
			unsigned int  d_complex_frame_size_bytes;
			float         d_mag_pmf_peak;
			float         d_eq_gain;
			int           d_start_idx_cfo;
			int           d_L;
			gr_complex   *d_preamble_mod_rm;
			gr_complex   *d_preamble_corr;
			float        *d_angle;
			float        *d_angle_diff;
			float        *d_w_window;
			float        *d_w_angle_diff;
			float        *d_w_angle_avg;
			gr_complex   *d_fc_preamble_syms;
			float         d_alpha;
			float         d_beta;
			float         d_avg_freq_offset;
			bool          d_first_iter;

			float est_freq_offset(const gr_complex *in);

		public:
			frame_synchronizer_cc_impl(
				const std::vector<gr_complex> &preamble_syms, int frame_len,
				int M, int n_success_to_lock, bool en_eq,
				bool en_phase_corr, bool en_freq_corr, int debug_level);
			~frame_synchronizer_cc_impl();

			// Where all the action really happens
			void forecast (int noutput_items, gr_vector_int &ninput_items_required);
			int general_work(int noutput_items,
			                 gr_vector_int &ninput_items,
			                 gr_vector_const_void_star &input_items,
			                 gr_vector_void_star &output_items);

			// Public getters
			float get_mag_pmf_peak();
			bool get_state();
		};
	} // namespace blocksat
} // namespace gr

#endif /* INCLUDED_BLOCKSAT_FRAME_SYNCHRONIZER_CC_IMPL_H */

