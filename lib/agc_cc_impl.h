/* -*- c++ -*- */
/*
 * Copyright 2006,2012 Free Software Foundation, Inc.
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

#ifndef INCLUDED_BLOCKSAT_AGC_CC_IMPL_H
#define INCLUDED_BLOCKSAT_AGC_CC_IMPL_H

#include <blocksat/agc_cc.h>

namespace gr {
	namespace blocksat {

		class agc_cc_impl : public agc_cc
		{
		private:
			float d_rate;           // adjustment rate
			float d_reference;      // reference value
			float d_reference_var;	// reference value
			float d_gain;           // current gain
			float d_max_gain;       // max allowable gain

		public:
			agc_cc_impl(float rate = 1e-4, float reference = 1.0,
			            float gain = 1.0, float max_gain = 65536);
			~agc_cc_impl();

			float get_rate() const { return d_rate; }
			float get_reference() const { return d_reference; }
			float get_gain() const { return d_gain; }
			float get_max_gain() const { return d_max_gain; }

			void set_rate(float rate) { d_rate = rate; }
			void set_reference(float reference) {
				d_reference = reference;
				d_reference_var = reference * reference;
			}
			void set_gain(float gain) { d_gain = gain; }
			void set_max_gain(float max_gain) { d_max_gain = max_gain; }

			int work(int noutput_items,
			         gr_vector_const_void_star &input_items,
			         gr_vector_void_star &output_items);

			/*
			 * \brief scale the input based on the AGC loop gain
			 */
			inline gr_complex scale(gr_complex input) {
				gr_complex output = input * sqrtf(d_gain);
				d_gain +=  d_rate * (d_reference_var -
				                     (output.real() * output.real() +
				                      output.imag() * output.imag()));
				if (d_max_gain > 0.0 && d_gain > d_max_gain) {
					d_gain = d_max_gain;
				}
				return output;
			};
		};

	} /* namespace blocksat */
} /* namespace gr */

#endif /* INCLUDED_BLOCKSAT_AGC_CC_IMPL_H */
