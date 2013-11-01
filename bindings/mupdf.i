// -*- C++ -*-

/* ********************************************************************************************** */

%module mupdf

/* ********************************************************************************************** */

%include "numpy-typemaps.i"

/* ********************************************************************************************** */

%{
#include "mupdf/fitz.h"
#include "fitz-extension.h"
%}

/**************************************************************************************************
 *
 * Exception Handling
 *
 *   Instead to exit the program when we enter mupdf throw function at last,
 *   we raise a Python exception
 *
 **************************************************************************************************/

// Fixme: mupdf uses fprintf(stderr, ...)
// Fixme: could we use mupdf jmp_buf ?
// Fixme: define a mupdf exception ?

%{
#include <setjmp.h>
jmp_buf exception_buffer;
char *exception_message = NULL;

void
python_throw_exit_callback(char *message)
{
  exception_message = message;
  fz_longjmp(exception_buffer, 1);
}
%}

%init %{
  fz_set_throw_exit_callback(python_throw_exit_callback);
%}

%exception {
  if (! fz_setjmp(exception_buffer))
  {
    $action // that could raise an exception and thus enter python_throw_exit_callback
  }
  else // an exception raised
  {
    PyErr_SetString(PyExc_NameError, exception_message);
    return NULL;
  }
}

/**************************************************************************************************
 *
 * Fixes
 *
 **************************************************************************************************/

// from /usr/include/setjmp.h
typedef struct __jmp_buf_tag jmp_buf[1];

// why ?
%ignore fz_open_file_w;
%ignore fz_buffer_vprintf;
%ignore fz_store_type_s;
%ignore fz_function_s;

// undefined symbols:
%ignore fz_get_annot_type;
%ignore pdf_debug_function;

/**************************************************************************************************
 *
 * Typemaps
 *
 **************************************************************************************************/

// Swig should handle char * return
// Fixme: check for memory leaks

/* ********************************************************************************************** */

%include "fitz.i"
%include "fitz-extension.h"

/***************************************************************************************************
 *
 * End
 *
 **************************************************************************************************/
