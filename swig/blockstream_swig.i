/* -*- c++ -*- */

#define BLOCKSTREAM_API

%include "gnuradio.i"			// the common stuff

//load generated python docstrings
%include "blockstream_swig_doc.i"

%{
#include "blockstream/frame_sync_fast.h"
#include "blockstream/turbo_decoder.h"
#include "blockstream/mer_measurement.h"
#include "blockstream/da_carrier_phase_rec.h"
#include "blockstream/nco_cc.h"
#include "blockstream/wrap_fft_index.h"
#include "blockstream/exponentiate_const_cci.h"
#include "blockstream/runtime_cfo_ctrl.h"
#include "blockstream/argpeak.h"
%}


%include "blockstream/frame_sync_fast.h"
GR_SWIG_BLOCK_MAGIC2(blockstream, frame_sync_fast);
%include "blockstream/turbo_decoder.h"
GR_SWIG_BLOCK_MAGIC2(blockstream, turbo_decoder);
%include "blockstream/mer_measurement.h"
GR_SWIG_BLOCK_MAGIC2(blockstream, mer_measurement);
%include "blockstream/da_carrier_phase_rec.h"
GR_SWIG_BLOCK_MAGIC2(blockstream, da_carrier_phase_rec);
%include "blockstream/nco_cc.h"
GR_SWIG_BLOCK_MAGIC2(blockstream, nco_cc);
%include "blockstream/wrap_fft_index.h"
GR_SWIG_BLOCK_MAGIC2(blockstream, wrap_fft_index);
%include "blockstream/exponentiate_const_cci.h"
GR_SWIG_BLOCK_MAGIC2(blockstream, exponentiate_const_cci);
%include "blockstream/runtime_cfo_ctrl.h"
GR_SWIG_BLOCK_MAGIC2(blockstream, runtime_cfo_ctrl);
%include "blockstream/argpeak.h"
GR_SWIG_BLOCK_MAGIC2(blockstream, argpeak);
