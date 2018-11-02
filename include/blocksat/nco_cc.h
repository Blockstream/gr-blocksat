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


#ifndef INCLUDED_BLOCKSAT_NCO_CC_H
#define INCLUDED_BLOCKSAT_NCO_CC_H

#include <blocksat/api.h>
#include <gnuradio/sync_block.h>

namespace gr {
  namespace blocksat {

    /*!
     * \brief <+description of block+>
     * \ingroup blocksat
     *
     */
    class BLOCKSAT_API nco_cc : virtual public gr::sync_block
    {
     public:
      typedef boost::shared_ptr<nco_cc> sptr;

      /*!
       * \brief Return a shared_ptr to a new instance of blocksat::nco_cc.
       *
       * To avoid accidental use of raw pointers, blocksat::nco_cc's
       * constructor is in a private implementation
       * class. blocksat::nco_cc::make is the public interface for
       * creating new instances.
       */
      static sptr make(float samp_rate, float freq, int n_steps);

      // Setters ready for parameter adjustment in runtime:
      virtual float phase_inc() const = 0;
      virtual void set_freq(float s) = 0;
    };

  } // namespace blocksat
} // namespace gr

#endif /* INCLUDED_BLOCKSAT_NCO_CC_H */
