/* -*- c++ -*- */
/*
 * Copyright 2019 Blockstream Corp.
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
		turbo_decoder::make(int K, bool pct_en, int n_ite, bool flip_llrs)
		{
			return gnuradio::get_initial_sptr
				(new turbo_decoder_impl(K, pct_en, n_ite, flip_llrs));
		}

		/*
		 * The private constructor
		 */
		turbo_decoder_impl::turbo_decoder_impl(int K, bool pct_en, int n_ite,
		                                       bool flip_llrs)
			: gr::block("turbo_decoder",
			            gr::io_signature::make(1, 1, sizeof(float)),
			            gr::io_signature::make(1, 1, sizeof(unsigned char))),
			d_pct_en(pct_en),
			d_K(K)
		{
			/* Parameters */
			std::vector<int> poly = {013, 015};
			std::vector<std::vector<bool>> pct_pattern = {{1,1},{1,0},{0,1}};
			bool buff_enc = true;
			int n_frames  = 1;

			/* Derived constants */
			int tail_length_rsc = (int)(2 * std::floor(std::log2((float)std::max(poly[0], poly[1]))));
			int tail_length_enc = 2 * tail_length_rsc;
			int N_cw_rsc    = (2 * K) + tail_length_rsc;
			int N_cw_turbo  = (3 * K) + tail_length_enc;
			int N           = (pct_en) ? (((K/2) * 4) + (tail_length_enc)) : N_cw_turbo;

			/* Internal configs and buffers */
			d_N             = N;
			d_flip_llrs     = flip_llrs;
			d_llr_buffer    = (float*)volk_malloc(N * sizeof(float),
			                                      volk_get_alignment());
			d_int_buffer    = (int*)volk_malloc(K * sizeof(int),
			                                    volk_get_alignment());
			d_pct_buffer    = (float*)volk_malloc(N_cw_turbo * sizeof(float),
			                                      volk_get_alignment());

			/* Modules */
			interleaver = new module::Interleaver_LTE<int> (K);
			interleaver->init();

			sub_enc = new module::Encoder_RSC_generic_sys<B_8> (K, N_cw_rsc, true, poly);
			std::vector<std::vector<int>> trellis = sub_enc->get_trellis();

#ifdef DEBUG_DEC
			printf("Treliss: ");
			for(int i =0; i<8; i++)
				printf("%d ", trellis[0][i]);
			printf("\n");
#endif

			sub_dec = new module::Decoder_RSC_BCJR_seq_very_fast <int,float,float,tools::max<float>,tools::max<float>> (K, trellis);

			dec = new module::Decoder_turbo_fast<int, float>(K, N_cw_turbo, n_ite, *interleaver, *sub_dec, *sub_dec);

			if (pct_en) {
				pct = new module::Puncturer_turbo<int,float>(K, N,
				                                             tail_length_enc,
				                                             pct_pattern,
				                                             buff_enc,
				                                             n_frames);
			}

			/* GR Block Configurations */
			set_fixed_rate(true);
			set_relative_rate((double)K/(double)N);
			set_output_multiple(K);
		}

		/*
		 * Our virtual destructor.
		 */
		turbo_decoder_impl::~turbo_decoder_impl()
		{
			volk_free(d_llr_buffer);
			volk_free(d_int_buffer);
			delete interleaver;
			delete sub_enc;
			delete sub_dec;
			delete dec;
			if (d_pct_en)
				delete pct;
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
					volk_32f_s32f_multiply_32f(d_llr_buffer,
					                           inbuffer + (i * d_N),
					                           -1.0f, d_N);
					if (d_pct_en) {
						pct->depuncture(d_llr_buffer, d_pct_buffer);
						dec->_decode_siho(d_pct_buffer, d_int_buffer, 0);
					} else {
						dec->_decode_siho(d_llr_buffer, d_int_buffer, 0);
					}
				} else {
					if (d_pct_en) {
						pct->depuncture(inbuffer + (i * d_N), d_pct_buffer);
						dec->_decode_siho(d_pct_buffer,
						                  d_int_buffer, 0);
					} else {
						dec->_decode_siho(inbuffer + (i * d_N),
						                  d_int_buffer, 0);
					}
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

