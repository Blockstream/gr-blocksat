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

#ifndef INCLUDED_BLOCKSAT_FFW_COARSE_FREQ_REQ_CC_H
#define INCLUDED_BLOCKSAT_FFW_COARSE_FREQ_REQ_CC_H

#include <blocksat/api.h>
#include <gnuradio/sync_block.h>

namespace gr {
	namespace blocksat {

		/*!
		 * \brief Feed-forward coarse frequency recovery
		 * \ingroup blocksat
		 *
		 */
		class BLOCKSAT_API ffw_coarse_freq_req_cc : virtual public gr::sync_block
		{
		public:
			typedef boost::shared_ptr<ffw_coarse_freq_req_cc> sptr;

			/*!
			 * \brief Make feed-forward coarse frequency recovery block
			 * \param fft_len FFT length
			 * \param alpha Averaging alpha
			 * \param M Constellation order
			 * \param sleep_per Target sleep period
			 * \param debug Activate debug prints
			 * \param frame_len Frame length in symbols
			 * \param sps Number of samples per symbol
			 */
			static sptr make(int fft_len, float alpha, int M, int sleep_per,
			                 bool debug, int frame_len, int sps);

			/*!
			 * \brief Get angular frequency offset
			 */
			virtual float get_frequency(void) = 0;

			/*!
			 * \brief Reset frequency recovery state
			 */
			virtual void reset(void) = 0;
		};

	} // namespace blocksat
} // namespace gr

#endif /* INCLUDED_BLOCKSAT_FFW_COARSE_FREQ_REQ_CC_H */

