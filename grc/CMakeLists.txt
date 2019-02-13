# Copyright 2011 Free Software Foundation, Inc.
#
# This file is part of GNU Radio
#
# GNU Radio is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3, or (at your option)
# any later version.
#
# GNU Radio is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with GNU Radio; see the file COPYING.  If not, write to
# the Free Software Foundation, Inc., 51 Franklin Street,
# Boston, MA 02110-1301, USA.

install(FILES
    blocksat_frame_sync_fast.xml
    blocksat_protocol_sink.xml
    blocksat_turbo_decoder.xml
    blocksat_mer_measurement.xml
    blocksat_da_carrier_phase_rec.xml
    blocksat_nco_cc.xml
    blocksat_ffw_coarse_freq_rec.xml
    blocksat_wrap_fft_index.xml
    blocksat_frame_synchronizer.xml
    blocksat_exponentiate_const_cci.xml
    blocksat_runtime_cfo_ctrl.xml
    blocksat_variable_tuning_control.xml
    blocksat_variable_rx_logger.xml
    blocksat_argpeak.xml
    blocksat_soft_decoder_cf.xml DESTINATION share/gnuradio/grc/blocks
)
