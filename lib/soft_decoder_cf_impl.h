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

#ifndef INCLUDED_BLOCKSAT_SOFT_DECODER_CF_IMPL_H
#define INCLUDED_BLOCKSAT_SOFT_DECODER_CF_IMPL_H

#include <blocksat/soft_decoder_cf.h>

namespace gr {
	namespace blocksat {

		class soft_decoder_cf_impl : public soft_decoder_cf
		{
		private:
			int d_M;
			float d_N0;
			float d_const;

		public:
			soft_decoder_cf_impl(int M, float N0);
			~soft_decoder_cf_impl();

			// Where all the action really happens
			void forecast (int noutput_items,
			               gr_vector_int &ninput_items_required);

			int general_work(int noutput_items,
			                 gr_vector_int &ninput_items,
			                 gr_vector_const_void_star &input_items,
			                 gr_vector_void_star &output_items);
		};

	} // namespace blocksat
} // namespace gr

#endif /* INCLUDED_BLOCKSAT_SOFT_DECODER_CF_IMPL_H */

