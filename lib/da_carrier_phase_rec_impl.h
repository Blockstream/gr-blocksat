/* -*- c++ -*- */
/*
 * Copyright 2017 <+YOU OR YOUR COMPANY+>.
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
 *
 * Data-aided Carrier Phase Recovery Loop
 *
 * This module consists is an implementation of a carrier phase recovery loop
 * that leverages on the knowledge of the preamble portion appended to transmit
 * frames, reason why it is called "data-aided" (based on known data). The loop
 * expects to receive symbols corresponding to the full frame (preamble +
 * payload) in its input. When receiving the preamble, the phase error detector
 * of the loop constrasts the phase of the incoming symbol with the phase of the
 * expected preamble symbol that is known a priori (passed as argument to the
 * block). In contrast, when receiving payload symbols, the error detector
 * compares the incoming phase with the phase of the "sliced" version of the
 * incoming symbol, where "sliced" should be understood as the nearest
 * constellation point. Therefore, over the payload portion of the frame, the
 * loop operates in "decision-directed" mode, rather than data-aided.
 *
 * An important aspect of this implementation is that it (optionally) resets the
 * loop state at the beggining of every frame. The motivation for doing so is to
 * avoid the so-called "cycle slips". The latter happens over the long term,
 * when the loop that is already locked slowly slips into another stationary
 * state. In case of carrier phase recovery, cycle slips come from the fact that
 * the loop can settle in any of the M possible rotations of the constellation,
 * where M is the constellation order. If noise is too strong, then, it possible
 * that the loop keeps slipping from one stationary state to the other. In this
 * context, then, by restarting the loop in the beggining of every frame, there
 * will be less time for the small errors to accumulate such that a slip
 * occurs. Furthermore, the loop is expected to be already settled when the
 * payload starts for each frame if the preamble length is long enough.
 *
 * In the loop implementation, one option for the phase error detector is to
 * compute the cross conjugate product between the sliced (symbols after
 * decision) or the know pilot and the noisy received symbols. This is the
 * decision-directed operation. However, this approach requires an "atan"
 * operation. To avoid this, the maximum-likelihood approach is used.
 *
 */

#ifndef INCLUDED_BLOCKSAT_DA_CARRIER_PHASE_REC_IMPL_H
#define INCLUDED_BLOCKSAT_DA_CARRIER_PHASE_REC_IMPL_H

#include <blocksat/da_carrier_phase_rec.h>
#include "constellation.h"

namespace gr {
	namespace blocksat {

		class da_carrier_phase_rec_impl : public da_carrier_phase_rec
		{
		private:
			float d_noise_bw;
			float d_damp_factor;
			float d_K1;
			float d_K2;
			float d_integrator;
			int d_const_order;
			float d_nco_phase;
			bool d_data_aided;
			bool d_reset_per_frame;
			std::vector<gr_complex> d_preamble_syms;
			std::vector<int> d_preamble_idxs;
			int d_preamble_len;
			bool d_tracking_en;
			std::vector<gr_complex> d_tracking_syms;
			int d_tracking_len;
			int d_tracking_interval;
			int d_data_plus_tracking_interval;
			int d_n_tracking_seqs;
			int d_payload_len;
			int d_frame_len;
			int d_data_len;
			Constellation d_const;
			bool d_debug_stats;
			float d_alpha;
			float d_beta;
			float d_avg_err;
			float d_n_sym_err;
			float d_n_sym_tot;
			float d_fs_phase; /* phase error detected by frame sync */

			/*
			 * \brief Update the PI loop
			 *
			 * Update the PI loop output and accumulate the filtered error in
			 * the phase accumulator.
			 *
			 *  \param float detected error
			 */
			inline void loop_step(float error) {
				d_integrator = (error * d_K2) + d_integrator;
				d_nco_phase  = d_nco_phase + (error * d_K1) + d_integrator;
			}

		public:
			da_carrier_phase_rec_impl(const std::vector<gr_complex> &preamble_syms,
			                          float noise_bw, float damp_factor, int M,
			                          bool data_aided, bool reset_per_frame,
			                          const std::vector<gr_complex> &tracking_syms,
			                          int tracking_interval, int frame_len,
			                          bool debug_stats);
			~da_carrier_phase_rec_impl();

			// Where all the action really happens
			void forecast (int noutput_items, gr_vector_int &ninput_items_required);
			int general_work(int noutput_items,
			                 gr_vector_int &ninput_items,
			                 gr_vector_const_void_star &input_items,
			                 gr_vector_void_star &output_items);

			float set_K1(float zeta, float Bn_Ts);
			float set_K2(float zeta, float Bn_Ts);
			float get_snr();
		};

	} // namespace blocksat
} // namespace gr

#endif /* INCLUDED_BLOCKSAT_DA_CARRIER_PHASE_REC_IMPL_H */
