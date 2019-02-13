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

#ifndef INCLUDED_BLOCKSAT_SOFT_DECODER_CF_H
#define INCLUDED_BLOCKSAT_SOFT_DECODER_CF_H

#include <blocksat/api.h>
#include <gnuradio/block.h>

namespace gr {
	namespace blocksat {
		/*!
		 * \brief Soft constellation decoder
		 * \ingroup blocksat
		 *
		 * \details
		 *
		 *  Takes QPSK/BPSK symbols on its input and outputs soft decisions in
		 *  terms of log likelihood ratios (LLRs).
		 *
		 *  The convention adopted for the LLRs is one in which the likelihoods
		 *  of bit being "0" are in the numerator and the likelihoods of bit
		 *  being "1" are in the denominator. That is, a positive "LLR"
		 *  indicates "bit = 0" and a negative LLR favours "bit = 1".
		 *
		 *  The LLRs are computed directly from their analytical expressions for
		 *  QPSK and BPSK. That is, for a received QPSK symbol "y", the LLRs are
		 *  computed as:
		 *
		 *      LLR_MSB(y) = (2*sqrt(2)/N0) * imag(y)
		 *
		 *      LLR_LSB(y) = (2*sqrt(2)/N0) * real(y)
		 *
		 *  where N0 is the average noise energy per two dimensions. This
		 *  assumes the average symbol energy per two-dimension of the QPSK
		 *  constellation is 1, namely that the constellation is formed with
		 *  elements "+-a +-j*a", where a is equal to "1/sqrt(2)". The specific
		 *  Gray-coded QPSK constellation that is assumed is:
		 *
		 *    -0.707 +j0.707 (bit=10)  |  0.707 +j0.707 (bit=11)
		 *   -----------------------------------------------------
		 *    -0.707 -j0.707 (bit=00)  |  0.707 -j0.707 (bit=01)
		 *
		 *  Meanwhile, for received BPSK symbol "y", the LLR is computed as:
		 *
		 *      LLR(y) = = -(4/N0) * real(y),
		 *
		 *  where "y" is generically assumed a complex number, despite the real
		 *  nature of BPSK. This computation assumes the constellation is formed
		 *  by symbols "+-a", where a = 1. That is:
		 *
		 *      -1 (bit=0)  |  +1 (bit=1)
		 */
		class BLOCKSAT_API soft_decoder_cf : virtual public gr::block
		{
		public:
			typedef boost::shared_ptr<soft_decoder_cf> sptr;

			/*!
			 * \brief Make soft constellation decoder
			 * \param M Constellation order
			 * \param N0 Average noise energy per two dimensions
			 */
			static sptr make(int M, float N0);
		};
	} // namespace blocksat
} // namespace gr

#endif /* INCLUDED_BLOCKSAT_SOFT_DECODER_CF_H */

