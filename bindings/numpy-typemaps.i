// -*- C++ -*-

/***************************************************************************************************
 *
 * Define Numpy interface
 *
 */

%{
#define SWIG_FILE_WITH_INIT
%}

%include "numpy.i"

%init %{
import_array();
%}

/* ********************************************************************************************** */

%{
#include <stdint.h>
%}

%include "std-typedef.i"

/* ********************************************************************************************** */

// Fixme: uint8_t
%apply (unsigned char *INPLACE_ARRAY3, int DIM1, int DIM2, int DIM3) {
  (uint8_t *buffer,
   size_t number_of_rows,
   size_t number_of_columns,
   size_t number_of_channels)};

/***************************************************************************************************
 *
 * End
 *
 **************************************************************************************************/