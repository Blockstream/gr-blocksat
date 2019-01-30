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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnuradio/io_signature.h>
#include <gnuradio/expj.h>
#include <gnuradio/math.h>
#include "da_carrier_phase_rec_impl.h"

#undef DEBUG
#undef DEBUG_FORECAST

#ifdef DEBUG
#define debug_printf printf
#else
#define debug_printf(...) if (false) printf(__VA_ARGS__)
#endif

#ifdef DEBUG_FORECAST
#define debug_f_printf printf
#else
#define debug_f_printf(...) if (false) printf(__VA_ARGS__)
#endif

namespace gr {
	namespace blocksat {

		da_carrier_phase_rec::sptr
		da_carrier_phase_rec::make(const std::vector<gr_complex> &preamble_syms,
		                           float noise_bw, float damp_factor, int M,
		                           bool data_aided, bool reset_per_frame,
		                           const std::vector<gr_complex> &tracking_syms,
		                           int tracking_interval, int frame_len)
		{
			return gnuradio::get_initial_sptr
				(new da_carrier_phase_rec_impl(preamble_syms, noise_bw, damp_factor, M,
				                               data_aided, reset_per_frame,
				                               tracking_syms, tracking_interval,
				                               frame_len));
		}

		/*
		 * The private constructor
		 */
		static int is[] = {sizeof(gr_complex), sizeof(char)};
		static std::vector<int> isig(is, is+sizeof(is)/sizeof(int));
		static int os[] = {sizeof(gr_complex), sizeof(float)};
		static std::vector<int> osig(os, os+sizeof(os)/sizeof(int));
		da_carrier_phase_rec_impl::da_carrier_phase_rec_impl(
			const std::vector<gr_complex> &preamble_syms, float noise_bw,
			float damp_factor, int M, bool data_aided, bool reset_per_frame,
			const std::vector<gr_complex> &tracking_syms, int tracking_interval,
			int frame_len)
			: gr::block("da_carrier_phase_rec",
			            io_signature::makev(2, 2, isig),
			            io_signature::makev(2, 2, osig)),
			d_noise_bw(noise_bw),
			d_damp_factor(damp_factor),
			d_const_order(M),
			d_data_aided(data_aided),
			d_reset_per_frame(reset_per_frame),
			d_nco_phase(0.0),
			d_integrator(0.0),
			d_tracking_interval(tracking_interval),
			d_frame_len(frame_len),
			d_const(M)
		{
			d_tx_pilots.resize(preamble_syms.size());
			d_tx_pilots = preamble_syms;
			d_preamble_len  = preamble_syms.size();
			d_payload_len   = d_frame_len - d_preamble_len;
			d_tracking_syms.resize(tracking_syms.size());
			d_tracking_syms = tracking_syms;
			d_tracking_len  = tracking_syms.size();
			d_tracking_en   = (d_tracking_interval != 0);
			d_data_plus_tracking_interval = d_tracking_interval + d_tracking_len;
			if (d_tracking_en)
			{
				d_n_tracking_seqs = d_payload_len / d_data_plus_tracking_interval;
				d_data_len = d_payload_len - (d_n_tracking_seqs * d_tracking_len);
			} else {
				d_n_tracking_seqs = 0;
				d_data_len = d_payload_len;
			}

			d_K1 = set_K1(damp_factor, noise_bw);
			d_K2 = set_K2(damp_factor, noise_bw);

			set_output_multiple(d_data_len);
		}

		/*
		 * Our virtual destructor.
		 */
		da_carrier_phase_rec_impl::~da_carrier_phase_rec_impl()
		{
		}

		void
		da_carrier_phase_rec_impl::forecast (int noutput_items,
		                                     gr_vector_int &ninput_items_required)
		{
			unsigned ninputs  = ninput_items_required.size();
			int n_frames      = noutput_items / d_data_len;
			int n_in_required = n_frames * d_frame_len;

			debug_f_printf("%s: noutput_items\t%d\n", __func__, noutput_items);

			for(unsigned i = 0; i < ninputs; i++) {
				ninput_items_required[i] = n_in_required;
			}

			debug_f_printf("%s: n_in_required\t%d\n", __func__, n_in_required);
		}

		/*
		 * Set PI Constants
		 *
		 * References
		 * [1] "Digital Communications: A Discrete-Time Approach", by Michael Rice
		 */
		float da_carrier_phase_rec_impl::set_K1(float zeta, float Bn_Ts) {
			float theta_n;
			float Kp_K0_K1;
			float K1;

			// Definition theta_n (See Eq. C.60 in [1])
			theta_n = Bn_Ts / (zeta + (1.0/(4*zeta)));
			// Note: for this loop, Bn*Ts = Bn*T, because the loop operates at 1
			// sample/symbol (at the symbol rate).

			// Eq. C.56 in [1]:
			Kp_K0_K1 = (4 * zeta * theta_n) /
				(1 + 2*zeta*theta_n + (theta_n*theta_n));

			// Then obtain the PI contants:
			K1 = Kp_K0_K1;
			/*
			 * This value should be divided by (Kp * K0), but the latter is unitary
			 * here. That is:
			 *
			 * Carrier Phase Error Detector Gain is unitary (Kp = 1)
			 * DDS Gain is unitary (K0 = 1)
			 */
			debug_printf("%s: K1 configured to:\t %f\n", __func__, K1);

			return K1;
		}
		float da_carrier_phase_rec_impl::set_K2(float zeta, float Bn_Ts) {
			float theta_n;
			float Kp_K0_K2;
			float K2;

			// Definition theta_n (See Eq. C.60 in [1])
			theta_n = Bn_Ts / (zeta + (1.0/(4*zeta)));
			// Note: for this loop, Bn*Ts = Bn*T, because the loop operates at 1
			// sample/symbol (at the symbol rate).

			// Eq. C.56 in [1]:
			Kp_K0_K2 = (4 * (theta_n*theta_n)) /
				(1 + 2*zeta*theta_n + (theta_n*theta_n));

			// Then obtain the PI contants:
			K2 = Kp_K0_K2;
			/*
			 * This value should be divided by (Kp * K0), but the latter is unitary
			 * here. That is:
			 *
			 * Carrier Phase Error Detector Gain is unitary (Kp = 1)
			 * DDS Gain is unitary (K0 = 1)
			 */
			debug_printf("%s: K2 configured to:\t %f\n", __func__, K2);

			return K2;
		}

		/*
		 * Main work
		 */
		int da_carrier_phase_rec_impl::general_work(int noutput_items,
		                                            gr_vector_int &ninput_items,
		                                            gr_vector_const_void_star &input_items,
		                                            gr_vector_void_star &output_items)
		{
			const gr_complex *rx_sym_in = (const gr_complex*) input_items[0];
			const char *is_preamble  = (const char*) input_items[1];
			gr_complex *rx_sym_out = (gr_complex *) output_items[0];
			float *error_out = (float *) output_items[1];
			gr_complex x_derotated, x_sliced;
			float phi_error;
			int n_consumed = 0, n_produced = 0;
			int n_frames = noutput_items / d_data_len;

			debug_printf("%s: ninput items[0]: %d\t(%d frames)\n", __func__,
			             ninput_items[0], n_frames);
			debug_printf("%s: ninput items[1]: %d\n", __func__, ninput_items[1]);
			debug_printf("%s: noutput items: %d\n", __func__, noutput_items);

			/* Frame-by-frame processing
			 *
			 * Indexes:
			 * i -> global symbol index used to read from input buffers
			 * j -> from 0 to payload_len
			 * k -> from 0 to segment (preamble, tracking or data interval) len
			 */
			int i = 0;
			for (int i_frame = 0; i_frame < n_frames; i_frame++) {
				// Keep track of the number of consumed inputs
				n_consumed += d_frame_len;

				// Reset the loop state for each frame, if so desired
				if (d_reset_per_frame) {
					d_nco_phase  = 0.0; // Reset the NCO phase accumulator
					d_integrator = 0.0; // Reset the integrator
				}

				/* Preamble */

				for (int k = 0; k < d_preamble_len; k++) {
#ifdef DEBUG
					if (!is_preamble[i])
						printf("%s: ERROR\tpreamble alignment incorrect\n",
						       __func__);
#endif
					/* NCO */
					x_derotated = rx_sym_in[i] * gr_expj(-d_nco_phase);

					/* DA phase error detector (second term null for BPSK) */
					phi_error = (x_derotated.imag() * d_tx_pilots[k].real()) -
						(x_derotated.real() * d_tx_pilots[k].imag());

					/* PI loop update */
					loop_step(phi_error);

					/* Debug */
					debug_printf("%s: In #%4u\t%4.2f + j%4.2f\t", __func__, k,
					             rx_sym_in[i].real(), rx_sym_in[i].imag());
					debug_printf("%6s\t%4.2f + j%4.2f\t%6s\t%4.2f + j%4.2f\t",
					             "De-rotated", x_derotated.real(),
					             x_derotated.imag(),
					             "Preamble", d_tx_pilots[k].real(),
					             d_tx_pilots[k].imag());
					debug_printf("Phase Error: %4.2f\n", phi_error);

					i++;
				}

				/* Payload */
				int j = 0;
				while (j < d_payload_len) {
					/* Data symbols */
					for (int k = 0;
					     (k < d_tracking_interval || d_tracking_interval == 0)
						     && j < d_payload_len; k++) {
						/* NCO */
						x_derotated = rx_sym_in[i] * gr_expj(-d_nco_phase);

						/* Sliced symbol (nearest constellation point) */
						d_const.slice(&x_derotated, &x_sliced);

						/* Decision-directed ML phase error detector: */
						phi_error = (x_derotated.imag() * x_sliced.real()) -
							(x_derotated.real() * x_sliced.imag());

						/*
						 * Force error to zero if data-aided only
						 *
						 * FIXME: we set phase error to zero when not processing
						 * the symbols. However, the true error is still
						 * accumulating in between and we should have a way of
						 * increase the weight of the next phase error detection
						 * when it comes (e.g. for the first pilot symbol of the
						 * next pilot segment).
						 */
						phi_error *= int(!d_data_aided);

						/* PI loop update */
						loop_step(phi_error);

						/* Outputs (only data symbols are output) */
						rx_sym_out[n_produced] = x_derotated;
						error_out[n_produced] = phi_error;
						n_produced++;

						/* Debug */
						debug_printf("%s: In #%4u\t%4.2f + j%4.2f\t", __func__,
						             j + d_preamble_len, rx_sym_in[i].real(),
						             rx_sym_in[i].imag());
						debug_printf("%6s\t%4.2f + j%4.2f\t%6s\t",
						             "De-rotated", x_derotated.real(),
						             x_derotated.imag(),
						             "Data");
						debug_printf("Phase Error: %4.2f\n", phi_error);

						j++;
						i++;
					}

					if (j == d_payload_len)
						break;

					/* Tracking symbols */
					for (int k = 0; k < d_tracking_len; k++) {
						/* NCO */
						x_derotated = rx_sym_in[i] * gr_expj(-d_nco_phase);

						/* DA phase error detector  */
						phi_error = (x_derotated.imag() *
						             d_tracking_syms[k].real()) -
							(x_derotated.real() *
							 d_tracking_syms[k].imag());

						/* PI loop update */
						loop_step(phi_error);

						/* Debug */
						debug_printf("%s: In #%4u\t%4.2f + j%4.2f\t", __func__,
						             j + d_preamble_len, rx_sym_in[i].real(),
						             rx_sym_in[i].imag());
						debug_printf("%6s\t%4.2f + j%4.2f\t%6s\t#%d\t%4.2f + j%4.2f\t",
						             "De-rotated", x_derotated.real(),
						             x_derotated.imag(),
						             "Pilot", k,
						             d_tracking_syms[k].real(),
						             d_tracking_syms[k].imag());
						debug_printf("Phase Error: %4.2f\n", phi_error);

						j++;
						i++;
					}
				}
			}

			debug_printf("%s: n_consumed\t%d\n", __func__, n_consumed);
			debug_printf("%s: n_produced\t%d\n", __func__, n_produced);

			// Always consume the same amount from both inputs
			consume_each(n_consumed);

			// Tell runtime system how many output items we produced.
			return n_produced;
		}
	} /* namespace blocksat */
} /* namespace gr */
