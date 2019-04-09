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
#include <gnuradio/expj.h>
#include "frame_synchronizer_cc_impl.h"

#undef DEBUG
#undef INFO
#undef DEBUG_PHASE_CORR
#undef DEBUG_GAIN_EQ
#undef DEBUG_FINE_FREQ_REC

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
			bool en_eq, bool en_phase_corr, bool en_freq_corr,
			int debug_level)
		{
			return gnuradio::get_initial_sptr
				(new frame_synchronizer_cc_impl(
					preamble_syms, frame_len, M, n_success_to_lock, en_eq,
					en_phase_corr, en_freq_corr, debug_level));
		}

		/*
		 * The private constructor
		 */
		frame_synchronizer_cc_impl::frame_synchronizer_cc_impl(
			const std::vector<gr_complex> &preamble_syms,
			int frame_len, int M, int n_success_to_lock,
			bool en_eq, bool en_phase_corr, bool en_freq_corr,
			int debug_level)
			: gr::block("frame_synchronizer_cc",
			            gr::io_signature::make(1, 1, sizeof(gr_complex)),
			            gr::io_signature::make2(1, 2, sizeof(gr_complex),
			                                    sizeof(gr_complex))),
			d_frame_len(frame_len),
			d_M(M),
			d_n_success_to_lock(n_success_to_lock),
			d_en_eq(en_eq),
			d_en_phase_corr(en_phase_corr),
			d_debug_level(debug_level),
			d_en_freq_corr(en_freq_corr),
			d_i_frame(0),
			d_i_frame_start(0),
			d_last_i_frame_start(0),
			d_locked(false),
			d_success_cnt(0),
			d_fail_cnt(0),
			d_mag_pmf_peak(0.0),
			d_eq_gain(1.0),
			d_alpha(0.1),
			d_beta(1.0 - 0.1),
			d_avg_freq_offset(0.0)
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

			/* Buffers used for fine freq. offset estimation */
			d_L = d_preamble_len/2; // set weight window length to half preamble
			d_preamble_mod_rm  = (gr_complex*) volk_malloc(d_preamble_len * sizeof(gr_complex), d_align);
			d_preamble_corr    = (gr_complex*) volk_malloc((d_L + 1) * sizeof(gr_complex), d_align);
			d_angle            = (float*) volk_malloc((d_L + 1) * sizeof(float), d_align);
			d_angle_diff       = (float*) volk_malloc(d_L * sizeof(float), d_align);
			d_w_window         = (float*) volk_malloc(d_L * sizeof(float), d_align);
			d_w_angle_diff     = (float*) volk_malloc(d_L * sizeof(float), d_align);
			d_w_angle_avg      = (float*) volk_malloc(sizeof(float), d_align);
			d_fc_preamble_syms = (gr_complex*) volk_malloc(d_preamble_len * sizeof(gr_complex), d_align);

			/* Fine freq. offset estimation weighting function*/
			for (int m = 0; m < d_L; m++) {
				d_w_window[m] = 3.0 * ((2*d_L + 1.0)*(2*d_L + 1.0) -
				                       (2*m + 1.0)*(2*m + 1.0)) /
					(((2*d_L + 1.0)*(2*d_L + 1.0) - 1)*(2*d_L + 1));
			}

			/* Message port */
			message_port_register_out(pmt::mp("start_index"));

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
			volk_free(d_preamble_mod_rm);
			volk_free(d_preamble_corr);
			volk_free(d_angle);
			volk_free(d_angle_diff);
			volk_free(d_w_window);
			volk_free(d_w_angle_diff);
			volk_free(d_w_angle_avg);
			volk_free(d_fc_preamble_syms);
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

		float
		frame_synchronizer_cc_impl::est_freq_offset(const gr_complex *in)
		{
			gr_complex r_sum;
			float freq_offset = 0;
			int N = d_preamble_len;

			/* "Remove" modulation */
			volk_32fc_x2_multiply_32fc(d_preamble_mod_rm, in, d_pmf_tap_buffer, N);

#ifdef DEBUG_FINE_FREQ_REC
			printf("Rx preamble:\n");
			printf("[");
			for (int i = 0; i < N-1; i++)
			{
				printf("(%f + 1j*%f), ...\n", in[i].real(), in[i].imag());
			}
			printf("(%f + 1j*%f)]\n", in[N-1].real(), in[N-1].imag());
			printf("Correlation:\n");
			printf("[");
#endif

			/* Auto-correlation of the "modulation-removed" symbols
			 *
			 * m is the lag of the auto-correlation; compute L + 1 values.
			 */
			for (int m = 1; m < (d_L + 2); m++)
			{
				volk_32fc_x2_conjugate_dot_prod_32fc(&r_sum,
				                                     d_preamble_mod_rm + m,
				                                     d_preamble_mod_rm,
				                                     (N-m));
				d_preamble_corr[m-1] = (1.0f/(N - m)) * r_sum;

				/* Angle of the correlation */
				d_angle[m-1] = gr::fast_atan2f(d_preamble_corr[m-1]);

#ifdef DEBUG_FINE_FREQ_REC
				if (m == d_L)
					printf("%f]\n", d_angle[m-1]);
				else
					printf("%f, ", d_angle[m-1]);
#endif
			}

			/* Angle differences
			 *
			 * NOTE: from L + 1 auto-corr values, there are L differences
			 */
			volk_32f_x2_subtract_32f(d_angle_diff, d_angle + 1, d_angle, d_L);

			/* Put angle differences within [-pi, pi]
			 *
			 * NOTE: the problem for this wrapping is when the angle is
			 * oscillating near 180 degrees, in which case it oscillates
			 * from -pi to pi. In contrast, when the angle is put within
			 * [0, 2*pi], the analogous problem is when the angle
			 * oscillates near 0 degress, namely between 0 and
			 * 2*pi. Since due to the coarse freq. offset recovery the
			 * residual fine CFO is expected to be low, we can assume
			 * the angle won't be near 180 degrees. Hence, it is better
			 * to wrap the angle within [-pi, pi] range.
			 */
			for (int m = 0; m < d_L; m++)
			{
				if (d_angle_diff[m] > M_PI)
					d_angle_diff[m] -= 2*M_PI;
				else if (d_angle_diff[m] < -M_PI)
					d_angle_diff[m] += 2*M_PI;
			}

			/* Weighted average */
			volk_32f_x2_multiply_32f(d_w_angle_diff, d_angle_diff, d_w_window, d_L);

			/* Sum of weighted average */
			volk_32f_accumulator_s32f(d_w_angle_avg, d_w_angle_diff, d_L);

			/* Final freq offset estimate
			 *
			 * Due to angle in range [-pi,pi], the freq. offset lies within
			 * [-0.5,0.5]. Enforce that to avoid numerical problems.
			 */
			freq_offset = *d_w_angle_avg / (2*M_PI);
			return branchless_clip(freq_offset, 0.5f);
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
			float pmf_peak_phase;
			gr_complex phasor, phasor_0;
			float freq_offset;

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

					if (d_debug_level > 0) {
						printf("%-21s Matched peaks = %d / %d\t",
						       "[Frame Synchronizer ]", d_success_cnt,
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

						/* Post the start index to the CFO recovery block
						 *
						 * Start with the actual index of the frame
						 * start. However, since there is a polyphase filter
						 * bank between the CFO recovery block and the frame
						 * synchronizer, and since the CFO recovery block
						 * operates on samples (oversampled), the desirable
						 * starting point will deviate from the start index that
						 * was found here. It will deviate due to filter group
						 * delay and the oversampling ratio. This initial
						 * message is just an initial guess. The starting index
						 * to be used by the CFO recovery block is better tuned
						 * later (below) iteratively.
						 */
						d_start_idx_cfo = i_frame_start;
						message_port_pub(pmt::mp("start_index"),
						                 pmt::from_long(d_start_idx_cfo));
					}
				} else {
					/* Check the CFO indicated by the CFO recovery block
					 *
					 * The CFO recovery block attemps to place this tag on a
					 * sample that ends up being the symbol corresponding to the
					 * frame start. Here, the frame synchronizer assesses
					 * whether this is indeed the case. If not, this block sends
					 * another frame start index for the CFO block to try in
					 * order to get closer to the sample corresponding to the
					 * frame start. */
					std::vector<tag_t> tags;
					get_tags_in_window(tags,
					                   0,
					                   i_offset + d_i_frame_start - d_preamble_len,
					                   i_offset + d_i_frame_start + d_preamble_len,
					                   pmt::mp("cfo"));
					for (unsigned i = 0; i < tags.size(); i++) {
						int tag_offset = tags[i].offset - nitems_read(0);

						/* Is this offset as expected? */
						int tag_offset_err = tag_offset - d_i_frame_start;
						d_start_idx_cfo -= tag_offset_err;

						/* Debug */
						if (d_debug_level > 1) {
							printf("%-21s Got CFO tag at offset: %d\t",
							       "[Frame Synchronizer ]", tag_offset);
							printf("Expected: %d (error %d)\tSend new start: %d\n",
							       d_i_frame_start, tag_offset_err,
							       d_start_idx_cfo);
						}

						/* Re-tune start used by subscriber on non-zero error */
						if (tag_offset_err != 0) {
							message_port_pub(pmt::mp("start_index"),
							                 pmt::from_long(d_start_idx_cfo));
						}

						/* Reset fine frequency offset average when the coarse
						 * CFO correction changes (which happens whenever a tag
						 * comes) */
						d_avg_freq_offset = 0.0;
					}

					/* Estimate new fine frequency offset and update average */
					if (d_en_freq_corr) {
						freq_offset       = est_freq_offset(in + i_offset + d_i_frame_start);
						d_avg_freq_offset = (d_alpha * freq_offset) + (d_beta * d_avg_freq_offset);

						/* Send average downstream via tag */
						add_item_tag(0,
						             nitems_written(0) + d_i_frame_start,
						             pmt::string_to_symbol("fs_fine_cfo"),
						             pmt::from_float(d_avg_freq_offset));

						/* Debug */
						if (d_debug_level > 2) {
							printf("%-21s Fine freq. offset: % 8.6f\tAvg: % 8.6f\n",
							       "[Frame Synchronizer ]", freq_offset,
							       d_avg_freq_offset);
						}
					}

					/*
					 * De-rotate preamble symbols to boost the PMF peak
					 *
					 * Use the estimated fine freq. offset estimation to
					 * de-rotate.
					 *
					 * NOTE: although helpful, the benefit from applying the
					 * fine frequency correction is small. The PMF peak is more
					 * significantly impacted by the noise level.
					 *
					 * NOTE 2: this correction also helps with the estimation of
					 * the PMF peak phase that is reported dowstream.
					 */
					phasor   = gr_expj(-2 * M_PI * d_avg_freq_offset);
					phasor_0 = gr_expj(0.0);
					volk_32fc_s32fc_x2_rotator_32fc(d_fc_preamble_syms,
					                                in + i_offset + d_i_frame_start,
					                                phasor,
					                                &phasor_0,
					                                d_preamble_len);

					/* Instead of filtering the entire input buffer in order to
					 * compute the cross-correlation (preamble matched
					 * filtering), compute the cross-correlation value solely at
					 * the index of interest, i.e. the frame start index.
					 */
					volk_32fc_x2_dot_prod_32fc(&pmf_peak,
					                           d_fc_preamble_syms,
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

						if (d_debug_level > 0) {
							printf("%-21s Unmatched peaks = %d / %d\t",
							       "[Frame Synchronizer ]", d_fail_cnt,
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

				/* Use the magnitude of the PMF peak to derive gain scaling */
#ifdef DEBUG_GAIN_EQ
				d_eq_gain = float(d_preamble_len) / d_mag_pmf_peak;
				printf("Gain EQ\t%f\n", d_eq_gain);
#endif

				/* Tag phase correction */
				if (d_locked == true && d_en_phase_corr == true)
				{
					/* Use the PMF peak to derive the phase error */
					pmf_peak_phase = gr::fast_atan2f(pmf_peak);

					add_item_tag(0,
					             nitems_written(0) + d_i_frame_start,
					             pmt::string_to_symbol("fs_phase"),
					             pmt::from_float(pmf_peak_phase));
				}

				produce(0, n_produced);
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

