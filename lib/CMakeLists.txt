# Copyright 2011,2012,2016 Free Software Foundation, Inc.
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

########################################################################
# Setup library
########################################################################
include(GrPlatform) #define LIB_SUFFIX

include_directories(${Boost_INCLUDE_DIR})
link_directories(${Boost_LIBRARY_DIRS})

list(APPEND blocksat_sources
    argpeak_impl.cc
    da_carrier_phase_rec_impl.cc
    exponentiate_const_cci_impl.cc
    frame_sync_fast_impl.cc
    mer_measurement_impl.cc
    nco_cc_impl.cc
    runtime_cfo_ctrl_impl.cc
    turbo_decoder_impl.cc
    wrap_fft_index_impl.cc
    aff3ct/Encoder_RSC_generic_sys.cpp
    aff3ct/Encoder_RSC_sys.cpp
    aff3ct/Decoder_NO.cpp
    aff3ct/Decoder_turbo.cpp
    aff3ct/Decoder_turbo_fast.cpp
    aff3ct/exception.cpp
    aff3ct/invalid_argument.cpp
    aff3ct/length_error.cpp
    aff3ct/runtime_error.cpp
    aff3ct/unimplemented_error.cpp
    soft_decoder_cf_impl.cc
)
#		bash_tools.cpp
#		cannot_allocate.cpp
#		Encoder_AZCW.cpp
#		Encoder_coset.cpp
#		Encoder.cpp
#		Encoder_user.cpp
#		Factory.cpp

set(blocksat_sources "${blocksat_sources}" PARENT_SCOPE)
if(NOT blocksat_sources)
	MESSAGE(STATUS "No C++ sources... skipping lib/")
	return()
endif(NOT blocksat_sources)

add_library(gnuradio-blocksat SHARED ${blocksat_sources})
target_link_libraries(gnuradio-blocksat ${Boost_LIBRARIES} ${GNURADIO_ALL_LIBRARIES})
set_target_properties(gnuradio-blocksat PROPERTIES DEFINE_SYMBOL "gnuradio_blocksat_EXPORTS")
set_target_properties(gnuradio-blocksat PROPERTIES DEFINE_SYMBOL "MULTI_PREC")

if(APPLE)
    set_target_properties(gnuradio-blocksat PROPERTIES
        INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/lib"
    )
endif(APPLE)

########################################################################
# Install built library files
########################################################################
include(GrMiscUtils)
GR_LIBRARY_FOO(gnuradio-blocksat RUNTIME_COMPONENT "blocksat_runtime" DEVEL_COMPONENT "blocksat_devel")

########################################################################
# Build and register unit test
########################################################################
include(GrTest)

include_directories(${CPPUNIT_INCLUDE_DIRS})

list(APPEND test_blocksat_sources
    ${CMAKE_CURRENT_SOURCE_DIR}/test_blocksat.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/qa_blocksat.cc
)

add_executable(test-blocksat ${test_blocksat_sources})

target_link_libraries(
  test-blocksat
  ${GNURADIO_RUNTIME_LIBRARIES}
  ${Boost_LIBRARIES}
  ${CPPUNIT_LIBRARIES}
  gnuradio-blocksat
)

GR_ADD_TEST(test_blocksat test-blocksat)

########################################################################
# Print summary
########################################################################
message(STATUS "Using install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Building for version: ${VERSION} / ${LIBVER}")
