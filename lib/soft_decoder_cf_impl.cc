/* -*- c++ -*- */
/*
 * Copyright 2019 <+YOU OR YOUR COMPANY+>.
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
#include "soft_decoder_cf_impl.h"

#undef DEBUG

#ifdef DEBUG
#define debug_printf printf
#else
#define debug_printf(...) if (false) printf(__VA_ARGS__)
#endif

namespace gr {
	namespace blocksat {

		soft_decoder_cf::sptr
		soft_decoder_cf::make(int M, float N0)
		{
			return gnuradio::get_initial_sptr
				(new soft_decoder_cf_impl(M, N0));
		}

		/*
		 * The private constructor
		 */
		soft_decoder_cf_impl::soft_decoder_cf_impl(int M, float N0)
			: gr::block("soft_decoder_cf",
			            gr::io_signature::make(1, 1, sizeof(gr_complex)),
			            gr::io_signature::make(1, 1, sizeof(float))),
			d_M(M),
			d_N0(N0)
		{
			d_const = (d_M == 4) ? (-2.0f*sqrt(2.0f)/N0) : (-4.0f/N0);
			set_output_multiple(2);
		}

		/*
		 * Our virtual destructor.
		 */
		soft_decoder_cf_impl::~soft_decoder_cf_impl()
		{
		}

		void
		soft_decoder_cf_impl::forecast (int noutput_items,
		                                gr_vector_int &ninput_items_required)
		{
			if (d_M == 4)
			{
				ninput_items_required[0] = noutput_items >> 1;
			}
			else if (d_M == 2)
			{
				ninput_items_required[0] = noutput_items;
			}

			debug_printf("%s: ninput_items_required[0] = %d\t", __func__,
			             ninput_items_required[0]);
			debug_printf("noutput_items = %d\n", noutput_items);
		}

		int
		soft_decoder_cf_impl::general_work (int noutput_items,
		                                    gr_vector_int &ninput_items,
		                                    gr_vector_const_void_star &input_items,
		                                    gr_vector_void_star &output_items)
		{
			const gr_complex *in = (const gr_complex *) input_items[0];
			float *out = (float *) output_items[0];

			debug_printf("%s: ninput_items[0] = %d\t", __func__,
			       ninput_items[0]);
			debug_printf("noutput_items = %d\n", noutput_items);

			if (d_M == 4)
			{
				for (int i = 0; i < (noutput_items/2) ; i++) {
					// MSB of the QPSK symbol first (big endian bit order)
					out[2*i] = d_const * in[i].imag();
					// LSB next
					out[2*i + 1] = d_const * in[i].real();
				}

				consume_each(noutput_items/2);
			}
			else if (d_M == 2)
			{
				for (int i = 0; i < noutput_items; i++) {
					out[i] = d_const * in[i].real();
				}

				consume_each(noutput_items);
			}

			return noutput_items;
		}

	} /* namespace blocksat */
} /* namespace gr */

