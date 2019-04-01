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


#ifndef INCLUDED_BLOCKSAT_DA_CARRIER_PHASE_REC_H
#define INCLUDED_BLOCKSAT_DA_CARRIER_PHASE_REC_H

#include <blocksat/api.h>
#include <gnuradio/block.h>

namespace gr {
  namespace blocksat {

    /*!
     * \brief Data-aided carrier phase recovery
     * \ingroup blocksat
     *
     */
    class BLOCKSAT_API da_carrier_phase_rec : virtual public gr::block
    {
     public:
      typedef boost::shared_ptr<da_carrier_phase_rec> sptr;

      /*!
       * \brief Make data-aided carrier phase recovery block
       *
       * \param preamble_syms Complex vector with preamble symbols
       * \param noise_bw Target noise bandwidth for the PI control loop
       * \param damp_factor Target damping factor for the PI control loop
       * \param M Constellation order
       * \param data_aided Activates purely data-aided operation
       * \param reset_per_frame Reset state on every frame
       * \param tracking_syms Complex vector with tracking symbols
       * \param tracking_interval Interval between tracking sequences
       * \param frame_len Frame length in symbols
       * \param debug_stats Activates printing of debug statistics
       * \param alpha Controls the SNR averaging
       */
      static sptr make(const std::vector<gr_complex> &preamble_syms,
                       float noise_bw, float damp_factor, int M,
                       bool data_aided, bool reset_per_frame,
                       const std::vector<gr_complex> &tracking_syms,
                       int tracking_interval, int frame_len, bool debug_stats,
                       float alpha);

      /*!
       * \brief Get data-aided SNR measurement
       */
      virtual float get_snr() = 0;
    };

  } // namespace blocksat
} // namespace gr

#endif /* INCLUDED_BLOCKSAT_DA_CARRIER_PHASE_REC_H */
