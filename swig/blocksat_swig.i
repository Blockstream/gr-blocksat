/* -*- c++ -*- */

#define BLOCKSAT_API

%include "gnuradio.i"			// the common stuff

//load generated python docstrings
%include "blocksat_swig_doc.i"

%{
#include "blocksat/frame_sync_fast.h"
#include "blocksat/turbo_decoder.h"
#include "blocksat/mer_measurement.h"
#include "blocksat/da_carrier_phase_rec.h"
#include "blocksat/nco_cc.h"
#include "blocksat/wrap_fft_index.h"
#include "blocksat/exponentiate_const_cci.h"
#include "blocksat/runtime_cfo_ctrl.h"
#include "blocksat/argpeak.h"
#include "blocksat/soft_decoder_cf.h"
#include "blocksat/agc_cc.h"
%}


%include "blocksat/frame_sync_fast.h"
GR_SWIG_BLOCK_MAGIC2(blocksat, frame_sync_fast);
%include "blocksat/turbo_decoder.h"
GR_SWIG_BLOCK_MAGIC2(blocksat, turbo_decoder);
%include "blocksat/mer_measurement.h"
GR_SWIG_BLOCK_MAGIC2(blocksat, mer_measurement);
%include "blocksat/da_carrier_phase_rec.h"
GR_SWIG_BLOCK_MAGIC2(blocksat, da_carrier_phase_rec);
%include "blocksat/nco_cc.h"
GR_SWIG_BLOCK_MAGIC2(blocksat, nco_cc);
%include "blocksat/wrap_fft_index.h"
GR_SWIG_BLOCK_MAGIC2(blocksat, wrap_fft_index);
%include "blocksat/exponentiate_const_cci.h"
GR_SWIG_BLOCK_MAGIC2(blocksat, exponentiate_const_cci);
%include "blocksat/runtime_cfo_ctrl.h"
GR_SWIG_BLOCK_MAGIC2(blocksat, runtime_cfo_ctrl);
%include "blocksat/argpeak.h"
GR_SWIG_BLOCK_MAGIC2(blocksat, argpeak);
%include "blocksat/soft_decoder_cf.h"
GR_SWIG_BLOCK_MAGIC2(blocksat, soft_decoder_cf);
%include "blocksat/agc_cc.h"
GR_SWIG_BLOCK_MAGIC2(blocksat, agc_cc);
