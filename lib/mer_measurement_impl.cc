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
 *
 *  MER Measurement
 *
 *  Computes the instantaenous modulation error ratio (MER)
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnuradio/io_signature.h>
#include "mer_measurement_impl.h"

namespace gr {
  namespace blocksat {

    mer_measurement::sptr
    mer_measurement::make(float alpha, int M)
    {
      return gnuradio::get_initial_sptr
        (new mer_measurement_impl(alpha, M));
    }

    /*
     * The private constructor
     */
    mer_measurement_impl::mer_measurement_impl(float alpha, int M)
      : gr::sync_block("mer_measurement",
              gr::io_signature::make(1, 1, sizeof(gr_complex)),
              gr::io_signature::make(1, 1, sizeof(float))),
        d_M(M), // Constellation Order
        d_snr_db(0.0),
        d_const(M)
    {
      set_alpha(alpha);
    }

    /*
     * Our virtual destructor.
     */
    mer_measurement_impl::~mer_measurement_impl()
    {
    }

    int
    mer_measurement_impl::work(int noutput_items,
        gr_vector_const_void_star &input_items,
        gr_vector_void_star &output_items)
    {
      const gr_complex *in = (const gr_complex *) input_items[0];
      float *out = (float *) output_items[0];
      gr_complex Ik, sym_err_k;
      float e_k;
      float lin_mer;

      // Perform ML decoding over the input iq data to generate alphabets
      int i;
      for(i = 0; i < noutput_items; i++)
      {
        // Slice the BPSK or QPSK symbol
        d_const.slice(&in[i], &Ik);

        // Symbol error:
        sym_err_k = Ik - in[i];

        // mag Sq. of the error
        e_k = (sym_err_k.real()*sym_err_k.real()) + (sym_err_k.imag()*sym_err_k.imag());

        // average magnitude squared error
        d_avg_err = d_beta*d_avg_err + d_alpha*e_k;

        // Resulting linear MER
        lin_mer = 1.0f / d_avg_err;
        /* The numerator should be Es, i.e. the average magnitude squared of the
         * ideal symbols, but it is assumed unitary. */

        // Output MER in dB
        out[i] = 10.0*log10(lin_mer);

        // Save globally
        d_snr_db = out[i];

        // Debug Prints
        // printf("Symbol In \t Real:\t %f\t Imag:\t %f\n", in[i].real(), in[i].imag());
        // printf("Sliced \t Real:\t %f\t Imag:\t %f\n", Ik.real(), Ik.imag());
        // printf("Error \t Real:\t %f\t Imag:\t %f\n", sym_err_k.real(), sym_err_k.imag());
        // printf("Mag Sq. Error \t %f \n", e_k);
        // printf("Avg Error \t %f \n", d_avg_err);
        // printf("Linear MER \t %f \n", lin_mer);
        // printf("dB MER \t %f \n", out[i]);
      }

      // Tell runtime system how many output items we produced.
      return noutput_items;
    }

    /*
     * Set the averaging alpha
     */
    void mer_measurement_impl::set_alpha(float alpha)
    {
      d_alpha   = alpha;
      d_beta    = 1 - d_alpha;
      d_avg_err = 0;
    }

    /*
     * Get SNR
     */
    float mer_measurement_impl::get_snr()
    {
      return d_snr_db;
    }

  } /* namespace blocksat */
} /* namespace gr */
