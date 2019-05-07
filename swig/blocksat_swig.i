/* -*- c++ -*- */

#define BLOCKSAT_API

%include "gnuradio.i"			// the common stuff

//load generated python docstrings
%include "blocksat_swig_doc.i"

%{
#include "blocksat/turbo_decoder.h"
#include "blocksat/mer_measurement.h"
#include "blocksat/da_carrier_phase_rec.h"
#include "blocksat/exponentiate_const_cci.h"
#include "blocksat/soft_decoder_cf.h"
#include "blocksat/agc_cc.h"
#include "blocksat/frame_synchronizer_cc.h"
#include "blocksat/ffw_coarse_freq_req_cc.h"
%}


%include "blocksat/turbo_decoder.h"
GR_SWIG_BLOCK_MAGIC2(blocksat, turbo_decoder);
%include "blocksat/mer_measurement.h"
GR_SWIG_BLOCK_MAGIC2(blocksat, mer_measurement);
%include "blocksat/da_carrier_phase_rec.h"
GR_SWIG_BLOCK_MAGIC2(blocksat, da_carrier_phase_rec);
%include "blocksat/exponentiate_const_cci.h"
GR_SWIG_BLOCK_MAGIC2(blocksat, exponentiate_const_cci);
%include "blocksat/soft_decoder_cf.h"
GR_SWIG_BLOCK_MAGIC2(blocksat, soft_decoder_cf);
%include "blocksat/agc_cc.h"
GR_SWIG_BLOCK_MAGIC2(blocksat, agc_cc);
%include "blocksat/frame_synchronizer_cc.h"
GR_SWIG_BLOCK_MAGIC2(blocksat, frame_synchronizer_cc);
%include "blocksat/ffw_coarse_freq_req_cc.h"
GR_SWIG_BLOCK_MAGIC2(blocksat, ffw_coarse_freq_req_cc);
