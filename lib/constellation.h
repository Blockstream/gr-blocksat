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

#ifndef INCLUDED_BLOCKSAT_CONSTELLATION_H
#define INCLUDED_BLOCKSAT_CONSTELLATION_H

#include <gnuradio/io_signature.h>
#include <gnuradio/math.h>

#define SQRT_TWO 0.7071068f

namespace gr {
	namespace blocksat {

		class Constellation
		{
		private:
			/* Define BPSK and QPSK constellations in one single vector */
			const gr_complex d_constellation[6] = {
				gr_complex(-1, 0),
				gr_complex(1, 0),
				gr_complex(-SQRT_TWO, -SQRT_TWO),
				gr_complex(SQRT_TWO, -SQRT_TWO),
				gr_complex(-SQRT_TWO, SQRT_TWO),
				gr_complex(SQRT_TWO, SQRT_TWO)
			};
			int d_M;
			int d_im_mask;
			int d_const_offset;

		public:
			Constellation(int M);
			~Constellation();
			inline void slice(const gr_complex *in,
			                  gr_complex *out)
			{
				int xre = branchless_binary_slicer(in->real());
				int xim = branchless_binary_slicer(in->imag());
				xim    &= d_im_mask; // throw away the imaginary part for BPSK
				*out    = d_constellation[((xim) << 1) + xre + d_const_offset];
			}
		};
	} // namespace blocksat
} // namespace gr

#endif /* INCLUDED_BLOCKSAT_CONSTELLATION_H */
