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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnuradio/io_signature.h>
#include <gnuradio/math.h>
#include <volk/volk.h>
#include <chrono>
#include <ctime>
#include "frame_synchronizer_cc_impl.h"

#undef DEBUG
#undef INFO
#undef DEBUG_PHASE_CORR
#undef DEBUG_GAIN_EQ

#ifdef DEBUG
#define debug_printf printf
#else
#define debug_printf(...) if (false) printf(__VA_ARGS__)
#endif

#ifdef INFO
#define info_printf printf
#else
#define info_printf(...) if (false) printf(__VA_ARGS__)
#endif

namespace gr {
	namespace blocksat {

		frame_synchronizer_cc::sptr
		frame_synchronizer_cc::make(
			const std::vector<gr_complex> &preamble_syms,
			int frame_len, int M, int n_success_to_lock,
			bool en_eq, bool en_phase_corr, bool verbose)
		{
			return gnuradio::get_initial_sptr
				(new frame_synchronizer_cc_impl(
					preamble_syms, frame_len, M, n_success_to_lock, en_eq,
					en_phase_corr, verbose));
		}

		/*
		 * The private constructor
		 */
		frame_synchronizer_cc_impl::frame_synchronizer_cc_impl(
			const std::vector<gr_complex> &preamble_syms,
			int frame_len, int M, int n_success_to_lock,
			bool en_eq, bool en_phase_corr, bool verbose)
			: gr::block("frame_synchronizer_cc",
			            gr::io_signature::make(1, 1, sizeof(gr_complex)),
			            gr::io_signature::make2(1, 2, sizeof(gr_complex),
			                                    sizeof(gr_complex))),
			d_frame_len(frame_len),
			d_M(M),
			d_n_success_to_lock(n_success_to_lock),
			d_en_eq(en_eq),
			d_en_phase_corr(en_phase_corr),
			d_verbose(verbose),
			d_i_frame(0),
			d_i_frame_start(0),
			d_last_i_frame_start(0),
			d_locked(false),
			d_success_cnt(0),
			d_fail_cnt(0),
			d_mag_pmf_peak(0.0),
			d_eq_gain(1.0),
			d_phase_corr(2 * M_PI) /* initial value purposely non-zero */
		{
			/* Constants
			 *
			 * NOTE: the PMF yields a peak after d_preamble_len symbol intervals
			 * from the frame start, since this is when all preamble symbols are
			 * being multiplied by the PMF taps. From this PMF peak index, we
			 * subtract an extra d_preamble_len term in order to point to the
			 * beginning of the frame.
			 **/
			d_preamble_len              = preamble_syms.size();
			d_peak_delay                = 2*d_preamble_len - 1;
			d_complex_frame_size_bytes  = sizeof(gr_complex) * frame_len;

			/* Internal buffers */
			d_align          = volk_get_alignment();
			d_pmf_out_buffer = (gr_complex*) volk_malloc(d_frame_len * sizeof(gr_complex), d_align);
			d_mag_pmf_buffer = (float*) volk_malloc(d_frame_len * sizeof(float), d_align);
			d_i_max          = (uint32_t*) volk_malloc(sizeof(uint32_t), d_align);
			d_pmf_tap_buffer = (gr_complex*) volk_malloc(d_preamble_len * sizeof(gr_complex), d_align);

			/* PMF taps - the flipped and conjugated version of preamble */
			d_preamble_syms.resize(d_preamble_len);
			d_preamble_syms  = preamble_syms;
			d_pmf_taps.resize(d_preamble_len);
			for(int i = 0; i < d_preamble_len; i++) {
				gr_complex tap = conj(preamble_syms[d_preamble_len - i - 1]);
				/* Std vector with taps - used to initialize filter */
				d_pmf_taps.push_back(tap);
				/* Volk buffer with taps - used for direct dot product */
				d_pmf_tap_buffer[i] = conj(preamble_syms[i]);
			}
			d_pmf = new gr::filter::kernel::fir_filter_with_buffer_ccc(d_pmf_taps);

			/* GR Block configs */
			set_output_multiple(d_frame_len);
		}

		/*
		 * Our virtual destructor.
		 */
		frame_synchronizer_cc_impl::~frame_synchronizer_cc_impl()
		{
			volk_free(d_pmf_out_buffer);
			volk_free(d_mag_pmf_buffer);
			volk_free(d_i_max);
			delete d_pmf;
		}

		void
		frame_synchronizer_cc_impl::forecast (int noutput_items,
		                                          gr_vector_int &ninput_items_required)
		{
			unsigned ninputs  = ninput_items_required.size();
			for(unsigned i = 0; i < ninputs; i++) {
				ninput_items_required[i] = noutput_items;
			}
		}

		static void print_system_timestamp() {
			std::chrono::time_point<std::chrono::system_clock> now;
			now = std::chrono::system_clock::now();
			std::time_t now_time = std::chrono::system_clock::to_time_t(now);
			std::cout << "-- On " << std::ctime(&now_time);
		}

		int
		frame_synchronizer_cc_impl::general_work (int noutput_items,
		                                          gr_vector_int &ninput_items,
		                                          gr_vector_const_void_star &input_items,
		                                          gr_vector_void_star &output_items)
		{
			const gr_complex *in = (const gr_complex *) input_items[0];
			gr_complex *out = (gr_complex *) output_items[0];
			gr_complex *pmf_out  = output_items.size() >= 2 ? (gr_complex *) output_items[1] : NULL;
			int n_consumed = 0, n_produced = 0;
			int n_frames = noutput_items / d_frame_len;
			int i_offset;
			int i_frame_start = 0;
			gr_complex pmf_peak;
			float phase_corr;

			/* Frame-by-frame processing */
			for (int i_frame = 0; i_frame < n_frames; i_frame++) {
				i_offset    = d_frame_len * i_frame;
				n_consumed += d_frame_len;

				if (!d_locked) {
					/* cross-correlation - preamble matched filter */
					d_pmf->filterN(d_pmf_out_buffer, in + i_offset, d_frame_len);
					/* PMF output magnitude */
					volk_32fc_magnitude_32f(d_mag_pmf_buffer, d_pmf_out_buffer,
					                        d_frame_len);
					/* FIXME: remove this magnitude computation and take the
					 * maximum directly from the complex values using
					 * "volk_32fc_index_max_32u". To do so, first investigate
					 * why the result of the two functions differ by 1.
					 */

					/* PMF peak */
					volk_32f_index_max_32u(d_i_max, d_mag_pmf_buffer, d_frame_len);

					/* Frame start index indicated by the current PMF peak */
					i_frame_start  = (((int) (*d_i_max)) - d_peak_delay) % d_frame_len;
					/* The above is a remainder operation. We want a modulo
					 * operation, which is accomplished by: */
					if (i_frame_start < 0)
						i_frame_start += d_frame_len;

					info_printf("index of max value = %u\n",  *d_i_max);
					info_printf("index of frame start = %d\n", i_frame_start);

					/* Complex PMF peak and magnitude of the PMF peak */
					pmf_peak       = d_pmf_out_buffer[*d_i_max];
					d_mag_pmf_peak = abs(pmf_peak);

					/* For locking, success is to have a peak in the same index
					 * between consecutive frames. Reset the success count to
					 * zero when not successful */
					if (i_frame_start == d_last_i_frame_start) {
						d_success_cnt++;
					} else
						d_success_cnt = 0;

					info_printf("success count = %d\n", d_success_cnt);

					if (d_verbose) {
						printf("%s:\tMatched peaks = %d / %d\t",
						       "[Frame Sync]", d_success_cnt,
						       d_n_success_to_lock);
						printf("Previous peak idx: %5d\tCurrent peak idx: %5d\n",
						       d_last_i_frame_start, i_frame_start);
					}

					d_last_i_frame_start = i_frame_start;

					/* Just acquired frame timing lock */
					if (d_success_cnt == d_n_success_to_lock) {
						d_locked        = true;
						d_i_frame_start = i_frame_start;
						/* NOTE: variable "d_i_frame_start" holds the acquired
						 * timing of the frame start. It is only updated here,
						 * right when locking. In contrast, variable
						 * "i_frame_start" holds the frame start according to
						 * the processing of each individual frame. */

						printf("\n##########################################\n");
						printf("-- Frame synchronization acquired\n");
						print_system_timestamp();
						printf("##########################################\n\n");

						memcpy(out + n_produced,
						       in + i_offset + i_frame_start,
						       (d_frame_len - i_frame_start) * sizeof(gr_complex));
						n_produced += (d_frame_len - i_frame_start);
					}
				} else {
					/* Instead of filtering the entire input buffer in order to
					 * compute the cross-correlation, compute the
					 * cross-correlation value solely at the index of interest,
					 * i.e. the frame start index */
					volk_32fc_x2_dot_prod_32fc(&pmf_peak, in + i_offset +
					                           d_i_frame_start,
					                           d_pmf_tap_buffer,
					                           d_preamble_len);
					/* NOTE: the above dot product is an element-wise product
					 * between the complex vectors. It does not conjugate one of
					 * the elements before multiplication. Hence, the taps saved
					 * on the PMF tap buffer are the conjugated version of the
					 * preamble symbols. */
					d_mag_pmf_peak = abs(pmf_peak);

					/* For unlocking, failure is when the cross-correlation
					 * value observed at the frame starting index (that was
					 * locked to) falls significantly below a threshold. */
					if (d_mag_pmf_peak < (0.2 * float(d_preamble_len))) {
						d_fail_cnt++;
						info_printf("failure count = %d\n", d_fail_cnt);

						if (d_verbose) {
							printf("%s:\tUnmatched peaks = %d / %d\t",
							       "[Frame Sync]", d_fail_cnt,
							       d_n_success_to_lock);
							printf("Locked to idx: %5d\tCorr Magnitude: %f\n",
							       d_i_frame_start, d_mag_pmf_peak);
						}
					} else {
						d_fail_cnt = 0;
					}

					/* Lost lock */
					if (d_fail_cnt == d_n_success_to_lock) {
						d_locked      = false;
						d_success_cnt = 0;
						d_fail_cnt    = 0;

						printf("\n##########################################\n");
						printf("-- Frame synchronization lost\n");
						print_system_timestamp();
						printf("##########################################\n\n");

						/* To preserve alignment on the output, output the last
						 * frame entirely before unlocking */
						memcpy(out + n_produced, in + i_offset,
						       d_i_frame_start * sizeof(gr_complex));
						n_produced += d_i_frame_start;
					} else {
						/* Normal locked operation - output the entire block of
						 * symbols being processed */
						memcpy(out + n_produced, in + i_offset,
						       d_frame_len * sizeof(gr_complex));
						n_produced += d_frame_len;
					}
				}

				produce(0, n_produced);

				/* Use the magnitude of the PMF peak to derive gain scaling */
#ifdef DEBUG_GAIN_EQ
				d_eq_gain = float(d_preamble_len) / d_mag_pmf_peak;
				printf("Gain EQ\t%f\n", d_eq_gain);
#endif

				/* Use the complex-valued PMF peak to derive the phase error */
				phase_corr = gr::fast_atan2f(pmf_peak);

				/* Tag phase correction */
				if (d_locked == true && d_en_phase_corr == true)
				{
					add_item_tag(0,
					             nitems_written(0) + i_offset + d_i_frame_start,
					             pmt::string_to_symbol("fs_phase_corr"),
					             pmt::from_float(phase_corr));
					d_phase_corr = phase_corr;
				}
#ifdef DEBUG
				for (int i = 0; i < d_frame_len; i++) {
					debug_printf("%s: input symbol %4d\t (%4.4f, %4.4f)\n",
					             __func__, i, in[i + i_offset].real(),
					             in[i + i_offset].imag());
					debug_printf("%s: mag pmf out %4d\t %4.2f\n",
					             __func__, i, d_mag_pmf_buffer[i]);
				}
#endif
				/* optional outputs */
				if (pmf_out != NULL) {
					/* PMF filtering that is not executed when locked */
					if (d_locked) {
						d_pmf->filterN(d_pmf_out_buffer, in + i_offset,
						               d_frame_len);
					}
					memcpy(pmf_out + i_offset, d_pmf_out_buffer,
					       d_complex_frame_size_bytes);
					produce(1, d_frame_len);
				}

				d_i_frame++;
			}

			debug_printf("%s: n_consumed\t%d\n", __func__, n_consumed);
			debug_printf("%s: n_produced\t%d\n", __func__, n_produced);

			consume_each(n_consumed);

			return WORK_CALLED_PRODUCE;
		}

		float
		frame_synchronizer_cc_impl::get_mag_pmf_peak() {
			/* Normalize assuming unitary Es */
			return d_mag_pmf_peak / float(d_preamble_len);
		}

		bool
		frame_synchronizer_cc_impl::get_state() {
			return d_locked;
		}
	} /* namespace blocksat */
} /* namespace gr */

