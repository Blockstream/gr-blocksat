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
 *
 */
#include "constellation.h"

#define SQRT_TWO 0.7071068f

namespace gr {
	namespace blocksat {
		Constellation::Constellation(int M)
			: d_M(M)
		{
			/* For branchless decoding, use the offset and mask defined beloe in
			 * order to index the constellation */
			if (M == 4)
			{
				d_im_mask          = 0x1;
				d_const_offset     = 2;
			}
			else if (M == 2)
			{
				d_im_mask          = 0;
				d_const_offset     = 0;
			}
		}

		Constellation::~Constellation()
		{
		}
	} /* namespace blocksat */
} /* namespace gr */
