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
#include <volk/volk.h>
#include <gnuradio/expj.h>
#include "ffw_coarse_freq_req_cc_impl.h"

#define M_TWOPI (2*M_PI)

#undef DEBUG

#ifdef DEBUG
#define debug_printf printf
#else
#define debug_printf(...) if (false) printf(__VA_ARGS__)
#endif

namespace gr {
	namespace blocksat {

		ffw_coarse_freq_req_cc::sptr
		ffw_coarse_freq_req_cc::make(int fft_len, float alpha, int M,
		                             int sleep_per, bool debug, int frame_len,
		                             int sps)
		{
			return gnuradio::get_initial_sptr
				(new ffw_coarse_freq_req_cc_impl(fft_len, alpha, M, sleep_per,
				                                 debug, frame_len, sps));
		}

		/*
		 * The private constructor
		 */
		ffw_coarse_freq_req_cc_impl::ffw_coarse_freq_req_cc_impl(int fft_len,
		                                                         float alpha,
		                                                         int M,
		                                                         int sleep_per,
		                                                         bool debug,
		                                                         int frame_len,
		                                                         int sps)
			: gr::sync_block("ffw_coarse_freq_req_cc",
			                 gr::io_signature::make(1, 1, sizeof(gr_complex)),
			                 gr::io_signature::make2(1, 2, sizeof(gr_complex),
			                                         sizeof(float))),
			d_fft_len(fft_len),
			d_alpha(alpha),
			d_M(M),
			d_sleep_per(sleep_per),
			d_debug(debug),
			d_frame_len(frame_len),
			d_sps(sps),
			d_frame_len_oversamp(frame_len * sps),
			d_beta(1 - alpha),
			d_half_fft_len(fft_len / 2),
			d_delta_f(1.0 / (float(M) * fft_len)),
			d_f_e(0.0),
			d_pend_f_e(0.0),
			d_phase_inc(0.0),
			d_phase_accum(0.0),
			d_nco_phasor(0.0),
			d_i_block(0),
			d_n_equal_corr(0),
			d_start_index(0),
			d_i_sample(0),
			d_pend_corr_update(false)
		{
			set_output_multiple(fft_len);
			d_fft = new fft::fft_complex(fft_len, true);

			d_fft_buffer = (gr_complex*) volk_malloc(fft_len * sizeof(gr_complex),
			                                         volk_get_alignment());
			d_mag_buffer = (float*) volk_malloc(fft_len * sizeof(float),
			                                    volk_get_alignment());
			d_avg_buffer = (float*) volk_malloc(fft_len * sizeof(float),
			                                    volk_get_alignment());
			memset(d_avg_buffer, 0, fft_len * sizeof(float));
			d_i_max_buffer = (uint32_t*) volk_malloc(sizeof(uint32_t),
			                                         volk_get_alignment());

			message_port_register_in(pmt::mp("start_index"));
			set_msg_handler(
				pmt::mp("start_index"),
				boost::bind(&ffw_coarse_freq_req_cc_impl::handle_set_start_index,
				            this, _1));
		}

		/*
		 * Our virtual destructor.
		 */
		ffw_coarse_freq_req_cc_impl::~ffw_coarse_freq_req_cc_impl()
		{
			volk_free(d_fft_buffer);
			volk_free(d_mag_buffer);
			volk_free(d_avg_buffer);
			volk_free(d_i_max_buffer);
			delete d_fft;
		}

		void
		ffw_coarse_freq_req_cc_impl::handle_set_start_index(pmt::pmt_t msg)
		{
			if (pmt::is_integer(msg)) {
				d_start_index = pmt::to_long(msg) * d_sps;
				if (d_debug)
					printf("%-21s Set frame start index to: %d\n",
					       "[Frequency Recovery ]", d_start_index);
			}
		}

		void
		ffw_coarse_freq_req_cc_impl::update_nco_phase(int n_samples)
		{
			d_phase_accum -= n_samples * d_phase_inc;

			while (d_phase_accum < -M_PI)
				d_phase_accum += M_TWOPI;
			while (d_phase_accum > M_PI)
				d_phase_accum -= M_TWOPI;
		}

		int
		ffw_coarse_freq_req_cc_impl::work(int noutput_items,
		                                  gr_vector_const_void_star &input_items,
		                                  gr_vector_void_star &output_items)
		{
			const gr_complex *in = (const gr_complex *) input_items[0];
			gr_complex *out = (gr_complex *) output_items[0];
			float *fft_out  = output_items.size() >= 2 ? (float *) output_items[1] : NULL;
			int n_blocks    = noutput_items / d_fft_len;
			uint32_t i_max;
			int i_max_shifted;
			gr_complex nco_conj;
			int i_offset;
			const gr_complex *in_block;
			gr_complex *out_block;
			float f_e;
			int start_in_range, i_update;
			int i_sample_next;
			gr_complex nco_phasor_0;

			// Process one FFT block at a time
			for (int i_block = 0; i_block < n_blocks; i_block++) {
				i_offset  = d_fft_len * i_block;
				in_block  = in + i_offset;
				out_block = out + i_offset;

				/* Handle sleeping */
				if (d_i_block != 0)
					goto output;

				/* Raise to the power of 2 (BPSK) or 4 (QPSK) */
				volk_32fc_x2_multiply_32fc(d_fft_buffer, in_block, in_block,
				                           d_fft_len);
				if (d_M == 4) {
					volk_32fc_x2_multiply_32fc(d_fft_buffer, d_fft_buffer,
					                           in_block, d_fft_len);
					volk_32fc_x2_multiply_32fc(d_fft_buffer, d_fft_buffer,
					                           in_block, d_fft_len);
				}

				/* FFT */
				memcpy(d_fft->get_inbuf(), d_fft_buffer,
				       sizeof(gr_complex)*d_fft_len);
				d_fft->execute();

				/* Magnitude and average magnitude */
				volk_32fc_magnitude_squared_32f(d_mag_buffer,
				                                d_fft->get_outbuf(), d_fft_len);
				volk_32f_s32f_multiply_32f(d_mag_buffer, d_mag_buffer, d_alpha,
				                           d_fft_len);
				volk_32f_s32f_multiply_32f(d_avg_buffer, d_avg_buffer, d_beta,
				                           d_fft_len);
				volk_32f_x2_add_32f(d_avg_buffer, d_avg_buffer, d_mag_buffer,
				                    d_fft_len);

				/* Peak detection */
				volk_32f_index_max_32u(d_i_max_buffer, d_avg_buffer, d_fft_len);
				i_max = *d_i_max_buffer;
				debug_printf("Peak: %d\n", i_max);

				/* Shift FFT peak index */
				i_max_shifted = (i_max > d_half_fft_len) ? (i_max - d_fft_len) : i_max;
				debug_printf("Shifted index: %d\n", i_max_shifted);

				/* Normalized frequency offset */
				f_e = i_max_shifted * d_delta_f;

				/* Debug prints */
				if (d_debug)
				{
					if (f_e != d_f_e)
					{
						printf("%-21s New freq correction: %f\t",
						       "[Frequency Recovery ]", f_e);
						printf("Consecutive equal corrections: %u\n",
						       d_n_equal_corr);
						d_n_equal_corr = 0;
					} else
						d_n_equal_corr++;
				}

				/* If the estimated CFO is different than before, mark the
				 * correction update as pending.
				 *
				 * This pending freq. offset value may or may not be applied
				 * next, depending on whether the start of frame lies within the
				 * range of the current FFT block. Furthermore, in sleep mode,
				 * not always the frequency offset is estimated. Hence, the
				 * pending frequency offset estimation should be saved next and
				 * applied when time comes.
				*/
				d_pend_corr_update = (f_e != d_f_e);
				d_pend_f_e         = f_e;

			output:

				/* Frequency correction
				 *
				 * Try to change the correction value only on an index that
				 * matches with respect to the frame start index, reported via
				 * message port. This is essential for performance. By applying
				 * a new correction within the preamble, the remaining blocks
				 * have the entire preamble to lock again.
				 *
				 * To do so, keep track of the notion of frame boundaries by
				 * using a sample counter modulo the oversampled frame
				 * length.
				 *
				 * The frame synchronizer processes blocks of symbols with size
				 * corresponding to the frame length. The actual frame start can
				 * be anywhere within this block. This start index is reported
				 * to us in this block.
				 *
				 * The reported index (of a symbol) is first mapped to a
				 * corresponding sample, by considering the oversampling
				 * ratio. By keeping a sample counter modulo the oversampled
				 * frame length, we can detect when processing the specific
				 * frame start sample index.
				 *
				 * There are three possibilities for passing through the sample
				 * start index:
				 *
				 *   1) FFT window entirely within a single frame:
				 *
				 *      In this case we know the frame start lies in the current
				 *      FFT window by checking that the sample index (modulo
				 *      oversampled frame length) of the next window is ahead of
				 *      the sample start index and that the start of the current
				 *      FFT window is behind (or equal to) the frame start
				 *      index.
				 *
				 *   2) FFT window placed between the end of a frame and start
				 *      of another:
				 *
				 *      In this case, the frame start lies in the current FFT
				 *      window when either its index is within the end of the
				 *      current frame that is covered by the FFT window or
				 *      within the starting samples of the subsequent frame that
				 *      is also covered by the FFT window.
				 *
				 *      NOTE: we know the FFT window covers the junction between
				 *      two frames by checking whether the next sample index
				 *      (modulo oversampled frame length) is less than the
				 *      current, i.e. sample counter will wrap around.
				 *
				 *   3) FFT window matched to the oversampled frame length
				 *
				 *      In case the next sample index is equal to the current,
				 *      it means that the FFT length is matched to the frame
				 *      length, such that the frame start will always be in the
				 *      current FFT block range. For completeness, we consider
				 *      that the FFT length could still span the end of a frame
				 *      and the start of another in this case. This is covered
				 *      by equality in the condition "i_sample_next <=
				 *      d_i_sample" used below.
				 *
				 * In the logic that follows, we mark whether the frame start
				 * index is in the range of the block of samples being processed
				 * in variable "start_in_range" and put the sample index within
				 * the block of Nfft samples corresponding to the frame start in
				 * variable "i_update".
				 *
				 */
				i_sample_next = (d_i_sample + d_fft_len) % d_frame_len_oversamp;

				/* FFT window overlapping two frames */
				if (i_sample_next <= d_i_sample) {
					start_in_range = (d_start_index >= d_i_sample) ||
						(d_start_index < i_sample_next);
					/* frame start on the "subsequent frame" */
					if (d_start_index < d_i_sample) {
						i_update = (d_frame_len_oversamp - d_i_sample) +
							d_start_index;
					/* frame start on the "current frame" */
					} else
						i_update = d_start_index - d_i_sample;
				/* FFT window within a single frame */
				} else {
					start_in_range = (d_start_index >= d_i_sample) &&
						(d_start_index < i_sample_next);
					i_update       = d_start_index - d_i_sample;
				}

				debug_printf("i_sample\t%d\tnext\t%d\tframe start?\t%d\t",
				             d_i_sample, i_sample_next, start_in_range);
				debug_printf("start idx\t%d\tupdate idx\t%d\n", d_start_index,
				             i_update);

				/* Update correction in the middle of the block */
				if (d_pend_corr_update && start_in_range) {
					/* Apply the current correction until the update index */
					nco_phasor_0 = gr_expj(d_phase_accum);
					volk_32fc_s32fc_x2_rotator_32fc(out_block,
					                                in_block,
					                                d_nco_phasor,
					                                &nco_phasor_0,
					                                i_update);
					update_nco_phase(i_update);

					/* Effectively update the freq. correction value */
					d_f_e              = d_pend_f_e;
					d_phase_inc        = M_TWOPI * d_f_e;
					d_nco_phasor       = gr_expj(-d_phase_inc);
					d_pend_corr_update = false;
					debug_printf("Normalized freq. error: %f\n", d_f_e);
					debug_printf("New phase inc: %f\n", d_phase_inc);

					/* Tag update point for debugging */
					add_item_tag(0,
					             nitems_written(0) + i_offset + i_update,
					             pmt::string_to_symbol("cfo"),
					             pmt::from_float(d_f_e));
					debug_printf("tag cfo at index %d - start index %d\n",
					             i_update, d_start_index);

					/* Apply update correction on remaining samples */
					nco_phasor_0 = gr_expj(d_phase_accum);
					volk_32fc_s32fc_x2_rotator_32fc(out_block + i_update,
					                                in_block + i_update,
					                                d_nco_phasor,
					                                &nco_phasor_0,
					                                (d_fft_len - i_update));
					update_nco_phase((d_fft_len - i_update));
				} else {
					/* Correct entire FFT block at once */
					nco_phasor_0 = gr_expj(d_phase_accum);
					volk_32fc_s32fc_x2_rotator_32fc(out_block,
					                                in_block,
					                                d_nco_phasor,
					                                &nco_phasor_0,
					                                d_fft_len);
					update_nco_phase(d_fft_len);
				}

				/* Optional FFT output */
				if (fft_out != NULL) {
					memcpy(fft_out + i_offset, d_avg_buffer,
					       d_fft_len * sizeof(float));
				}

				/* Keep track of the sample index with respect to frame */
				d_i_sample = i_sample_next;

				/* Keep track of FFT blocks */
				d_i_block = (d_i_block + 1) % d_sleep_per;
			}

			return noutput_items;
		}

		float
		ffw_coarse_freq_req_cc_impl::get_frequency(void)
		{
			return d_phase_inc;
		}

		void
		ffw_coarse_freq_req_cc_impl::reset(void)
		{
			d_f_e          = 0.0;
			d_phase_inc    = 0.0;
			d_phase_accum  = 0.0;
			d_i_block      = 0; /* wake up from sleep interval */
		}

	} /* namespace blocksat */
} /* namespace gr */

