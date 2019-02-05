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
#include "turbo_decoder_impl.h"
#include <volk/volk.h>

#undef DEBUG
#undef DEBUG_DEC

#ifdef DEBUG
#define debug_printf printf
#else
#define debug_printf(...) if (false) printf(__VA_ARGS__)
#endif

namespace gr {
	namespace blocksat {

		turbo_decoder::sptr
		turbo_decoder::make(int N, int K, int n_ite, bool flip_llrs)
		{
			return gnuradio::get_initial_sptr
				(new turbo_decoder_impl(N, K, n_ite, flip_llrs));
		}

		/*
		 * The private constructor
		 */
		turbo_decoder_impl::turbo_decoder_impl(int N, int K, int n_ite,
		                                       bool flip_llrs)
			: gr::block("turbo_decoder",
			            gr::io_signature::make(1, 1, sizeof(float)),
			            gr::io_signature::make(1, 1, sizeof(unsigned char)))
		{

			std::vector<int> poly = {013, 015};
			int tail_length = (int)(2 * std::floor(std::log2((float)std::max(poly[0], poly[1]))));
			int N_cw        = 2 * K + tail_length;

			interleaver = new module::Interleaver_LTE<int> (K);
			interleaver->init();

			sub_enc = new module::Encoder_RSC_generic_sys<B_8> (K, N_cw, true, poly);
			std::vector<std::vector<int>> trellis = sub_enc->get_trellis();

#ifdef DEBUG_DEC
			printf("Treliss: ");
			for(int i =0; i<8; i++)
				printf("%d ", trellis[0][i]);
			printf("\n");
#endif

			sub_dec = new module::Decoder_RSC_BCJR_seq_very_fast <int,float,float,tools::max<float>,tools::max<float>> (K, trellis);

			dec = new module::Decoder_turbo_fast<int, float>(K, N, n_ite, *interleaver, *sub_dec, *sub_dec);

			set_fixed_rate(true);
			set_relative_rate((double)K/(double)N);
			set_output_multiple(K);

			d_N                = N;
			d_K                = K;
			d_flip_llrs        = flip_llrs;
			d_llr_buffer       = (float*)volk_malloc(N * sizeof(float),
			                                         volk_get_alignment());
			d_int_buffer       = (int*)volk_malloc(K * sizeof(int),
			                                       volk_get_alignment());
		}

		/*
		 * Our virtual destructor.
		 */
		turbo_decoder_impl::~turbo_decoder_impl()
		{
			volk_free(d_llr_buffer);
			volk_free(d_int_buffer);
		}

		int
		turbo_decoder_impl::fixed_rate_ninput_to_noutput(int ninput)
		{
			debug_printf("%s: ninput: %d\tnoutput: %f\n", __func__,
			             ninput, 0.5 + ninput*relative_rate());

			return (int)(0.5 + ninput*relative_rate());
		}

		int
		turbo_decoder_impl::fixed_rate_noutput_to_ninput(int noutput)
		{
			debug_printf("%s: noutput: %d\tninput: %f\n", __func__,
			             noutput, 0.5 + noutput/relative_rate());

			return (int)(0.5 + noutput/relative_rate());
		}

		void
		turbo_decoder_impl::forecast(int noutput_items,
		                             gr_vector_int& ninput_items_required)
		{
			int n_codewords          = noutput_items / d_K;
			ninput_items_required[0] = n_codewords * d_N;

			debug_printf("%s: noutput = %d\tninput = %d\tn_codewords = %d\n",
			             __func__, noutput_items, ninput_items_required[0],
			             n_codewords);
		}

		int
		turbo_decoder_impl::general_work(int noutput_items,
		                                 gr_vector_int& ninput_items,
		                                 gr_vector_const_void_star &input_items,
		                                 gr_vector_void_star &output_items)
		{

			const float *inbuffer    = (float *)input_items[0];
			unsigned char *outbuffer = (unsigned char *)output_items[0];
			int n_codewords          = noutput_items / d_K;

			debug_printf("%s: ninput %d\tnoutput %d\tn_codewords %d\n", __func__,
			             ninput_items[0], noutput_items, n_codewords);

			for (int i = 0; i < n_codewords; i++) {
				if (d_flip_llrs) {
					// Flip sign of LLRs
					volk_32f_s32f_multiply_32f(d_llr_buffer,
					                           inbuffer + (i * d_N),
					                           -1.0f, d_N);
					// Decode
					dec->_decode_siho(d_llr_buffer, d_int_buffer, 0);
				} else {
					// Decode
					dec->_decode_siho(inbuffer + (i * d_N),
					                  d_int_buffer, 0);
				}

				// Convert int output of the decoder to unsigned char
				for (int j = 0; j < d_K; j++)
				{
					outbuffer[(i * d_K) + j] = (unsigned char) d_int_buffer[j];
				}

#ifdef DEBUG_DEC
				printf("dec_in = ");
				for(int j = 0; j < d_N; j++)
					printf("%f ", inbuffer[i * d_N + j]);
				printf("\n");

				printf("dec_out = ");
				for(int j = 0; j < d_K; j++)
					printf("%d ", outbuffer[i * d_K + j]);
				printf("\n");
#endif
			}

			consume_each(n_codewords * d_N);
			return noutput_items;
		}

	} /* namespace blocksat */
} /* namespace gr */

