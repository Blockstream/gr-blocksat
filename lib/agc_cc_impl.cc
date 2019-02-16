/* -*- c++ -*- */
/*
 * Copyright 2006,2010,2012 Free Software Foundation, Inc.
 *
 * This file is part of GNU Radio
 *
 * GNU Radio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * GNU Radio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Radio; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "agc_cc_impl.h"
#include <gnuradio/io_signature.h>
#include <volk/volk.h>

namespace gr {
	namespace blocksat {

		agc_cc::sptr
		agc_cc::make(float rate, float reference, float gain, float max_gain)
		{
			return gnuradio::get_initial_sptr
				(new agc_cc_impl(rate, reference, gain, max_gain));
		}

		agc_cc_impl::agc_cc_impl(float rate, float reference, float gain,
		                         float max_gain)
			: sync_block("agc_cc",
			             io_signature::make(1, 1, sizeof(gr_complex)),
			             io_signature::make2(1, 2, sizeof(gr_complex),
			                                 sizeof(float))),
			  d_rate(rate),
			  d_reference(reference),
			  d_reference_var(reference * reference),
			  d_gain(gain),
			  d_max_gain(max_gain)
		{
			const int alignment_multiple =
				volk_get_alignment() / sizeof(gr_complex);
			set_alignment(std::max(1, alignment_multiple));
		}

		agc_cc_impl::~agc_cc_impl()
		{
		}

		int
		agc_cc_impl::work(int noutput_items,
		                  gr_vector_const_void_star &input_items,
		                  gr_vector_void_star &output_items)
		{
			const gr_complex *in = (const gr_complex*)input_items[0];
			gr_complex *out      = (gr_complex*)output_items[0];
			float *level         = output_items.size() >= 2 ? (float *) output_items[1] : NULL;

			if (level != NULL)
			{
				for(int i = 0; i < noutput_items; i++)
				{
					out[i]   = scale(in[i]);
					level[i] = 1.0f / sqrtf(d_gain);
				}
			} else
			{
				for(int i = 0; i < noutput_items; i++)
				{
					out[i] = scale(in[i]);
				}
			}

			return noutput_items;
		}

	} /* namespace blocksat */
} /* namespace gr */
