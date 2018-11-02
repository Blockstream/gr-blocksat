/* -*- c++ -*- */

#define MODS_API

%include "gnuradio.i"			// the common stuff

//load generated python docstrings
%include "mods_swig_doc.i"

%{
#include "mods/frame_sync_fast.h"
#include "mods/turbo_encoder.h"
#include "mods/turbo_decoder.h"
#include "mods/mer_measurement.h"
#include "mods/da_carrier_phase_rec.h"
#include "mods/nco_cc.h"
#include "mods/wrap_fft_index.h"
#include "mods/exponentiate_const_cci.h"
#include "mods/runtime_cfo_ctrl.h"
#include "mods/argpeak.h"
%}


%include "mods/frame_sync_fast.h"
GR_SWIG_BLOCK_MAGIC2(mods, frame_sync_fast);
%include "mods/turbo_encoder.h"
GR_SWIG_BLOCK_MAGIC2(mods, turbo_encoder);
%include "mods/turbo_decoder.h"
GR_SWIG_BLOCK_MAGIC2(mods, turbo_decoder);
%include "mods/mer_measurement.h"
GR_SWIG_BLOCK_MAGIC2(mods, mer_measurement);
%include "mods/da_carrier_phase_rec.h"
GR_SWIG_BLOCK_MAGIC2(mods, da_carrier_phase_rec);
%include "mods/nco_cc.h"
GR_SWIG_BLOCK_MAGIC2(mods, nco_cc);
%include "mods/wrap_fft_index.h"
GR_SWIG_BLOCK_MAGIC2(mods, wrap_fft_index);
%include "mods/exponentiate_const_cci.h"
GR_SWIG_BLOCK_MAGIC2(mods, exponentiate_const_cci);
%include "mods/runtime_cfo_ctrl.h"
GR_SWIG_BLOCK_MAGIC2(mods, runtime_cfo_ctrl);
%include "mods/argpeak.h"
GR_SWIG_BLOCK_MAGIC2(mods, argpeak);
