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
			d_phase_inc(0.0),
			d_phase_accum(0.0),
			d_nco_phasor(0.0),
			d_i_block(0),
			d_n_equal_corr(0),
			d_start_index(0),
			d_i_sample(0),
			d_pend_corr_update(false),
			d_pend_f_e(0.0)
		{
			set_output_multiple(fft_len);
			d_fft = new fft::fft_complex(fft_len, true);

			d_align      = volk_get_alignment();
			d_fft_buffer = (gr_complex*) volk_malloc(fft_len * sizeof(gr_complex), d_align);
			d_mag_buffer = (float*) volk_malloc(fft_len * sizeof(float), d_align);
			d_avg_buffer = (float*) volk_malloc(fft_len * sizeof(float), d_align);

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
		ffw_coarse_freq_req_cc_impl::update_phase(float *phase, float phase_inc,
		                                          int n_samples)
		{
			*phase -= n_samples * phase_inc;

			while (*phase < -M_PI)
				*phase += M_TWOPI;
			while (*phase > M_PI)
				*phase -= M_TWOPI;
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
			float f_e;
			gr_complex *fft_inbuf;
			int start_in_range, i_update;
			int i_sample_next;
			gr_complex nco_phasor_0;

			// Process each FFT block at once
			for (int i_block = 0; i_block < n_blocks; i_block++) {
				i_offset    = d_fft_len * i_block;
				in_block    = in + i_offset;

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
				fft_inbuf = d_fft->get_inbuf();
				for (int i = 0; i < d_fft_len; i++)
					fft_inbuf[i] = d_fft_buffer[i];
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
				volk_32f_index_max_32u(&i_max, d_avg_buffer, d_fft_len);
				debug_printf("Peak: %d\n", i_max);

				/* Shift FFT peak index */
				i_max_shifted = (i_max > d_half_fft_len) ? (i_max - d_fft_len) : i_max;
				debug_printf("Shifted index: %d\n", i_max_shifted);

				/* Normalized frequency offset */
				f_e         = i_max_shifted * d_delta_f;

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
				 * correction update as pending */
				d_pend_corr_update = (f_e != d_f_e);
				d_pend_f_e         = f_e;

			output:

				/* Frequency correction
				 *
				 * Try to change the correction value only on an index that
				 * matches with respect to the frame start index, reported via
				 * message port.
				 */
				i_sample_next = (d_i_sample + d_fft_len) % d_frame_len_oversamp;

				/* Check whether the frame start index is in the range of the
				 * block of samples being processed (within variable
				 * "start_in_range") and compute the index within the block of
				 * Nfft samples corresponding to the frame start index (variable
				 * "i_update").
				 *
				 * While doing so, check whether the current FFT window is
				 * placed on the end of a frame and start of another. This is
				 * detected by checking whether the next sample index (mod frame
				 * length) is less than the current (meaning the counter has
				 * wrapped). Equality in this case covers the case when the
				 * oversampled frame length is exactly the same as the FFT
				 * length.
				 */
				if (i_sample_next <= d_i_sample) {
					start_in_range = (d_start_index >= d_i_sample) ||
						(d_start_index < i_sample_next);
					if (d_start_index < d_i_sample)
						i_update = (d_frame_len_oversamp - d_i_sample) +
							d_start_index;
					else
						i_update = d_start_index - d_i_sample;
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
					volk_32fc_s32fc_x2_rotator_32fc(out + i_offset,
					                                in + i_offset,
					                                d_nco_phasor,
					                                &nco_phasor_0,
					                                i_update);
					update_phase(&d_phase_accum, d_phase_inc, i_update);

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
					volk_32fc_s32fc_x2_rotator_32fc(out + i_offset + i_update,
					                                in + i_offset + i_update,
					                                d_nco_phasor,
					                                &nco_phasor_0,
					                                (d_fft_len - i_update));
					update_phase(&d_phase_accum, d_phase_inc,
					             (d_fft_len - i_update));
				} else {
					/* Correct entire FFT block at once */
					nco_phasor_0 = gr_expj(d_phase_accum);
					volk_32fc_s32fc_x2_rotator_32fc(out + i_offset,
					                                in + i_offset,
					                                d_nco_phasor,
					                                &nco_phasor_0,
					                                d_fft_len);
					update_phase(&d_phase_accum, d_phase_inc, d_fft_len);
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
		}

	} /* namespace blocksat */
} /* namespace gr */

