/* -*- c++ -*- */
/*
 * Copyright 2019 Blockstream
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

#ifndef INCLUDED_BLOCKSAT_FRAME_SYNCHRONIZER_CC_H
#define INCLUDED_BLOCKSAT_FRAME_SYNCHRONIZER_CC_H

#include <blocksat/api.h>
#include <gnuradio/block.h>

namespace gr {
	namespace blocksat {

		/*!
		 * \brief Frame synchronizer
		 * \ingroup blocksat
		 *
		 */
		class BLOCKSAT_API frame_synchronizer_cc : virtual public gr::block
		{
		public:
			typedef boost::shared_ptr<frame_synchronizer_cc> sptr;

			/*!
			 * \brief Make frame synchronizer
			 * \param &preamble_syms Vector of complex preamble symbols
			 * \param frame_len Frame length
			 * \param M Constellation order
			 * \param n_success_to_lock How many consecutive peaks until lock
			 * \param en_eq Enable gain scaling
			 * \param en_phase_corr Enable phase correction per frame
			 * \param verbose Activate verbose mode
			 */
			static sptr make(const std::vector<gr_complex> &preamble_syms,
			                 int frame_len, int M, int n_success_to_lock,
			                 bool en_eq, bool en_phase_corr, bool verbose);

			/*!
			 * \brief Get magnitude of PMF peak
			 */
			virtual float get_mag_pmf_peak() = 0;

			/*!
			 * \brief Get locked state
			 */
			virtual bool get_state() = 0;
		};

	} // namespace blocksat
} // namespace gr

#endif /* INCLUDED_BLOCKSAT_FRAME_SYNCHRONIZER_CC_H */

