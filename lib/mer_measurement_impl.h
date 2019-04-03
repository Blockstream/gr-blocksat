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

#ifndef INCLUDED_BLOCKSAT_MER_MEASUREMENT_IMPL_H
#define INCLUDED_BLOCKSAT_MER_MEASUREMENT_IMPL_H

#include <blocksat/mer_measurement.h>
#include "constellation.h"

namespace gr {
	namespace blocksat {

		class mer_measurement_impl : public mer_measurement
		{
		private:
			int d_M;
			int d_frame_len;
			float d_alpha;
			float d_beta;
			float d_avg_err;
			float d_snr_db;
			Constellation d_const;
			bool d_disable;

		public:
			mer_measurement_impl(float alpha, int M, int frame_len);
			~mer_measurement_impl();

			// Where all the action really happens
			int work(int noutput_items,
			         gr_vector_const_void_star &input_items,
			         gr_vector_void_star &output_items);

			// Public getters/setters
			float get_snr();
			void set_alpha(float alpha);
			void enable(void);
			void disable(void);
		};

	} // namespace blocksat
} // namespace gr

#endif /* INCLUDED_BLOCKSAT_MER_MEASUREMENT_IMPL_H */
