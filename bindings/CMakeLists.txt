# -*- CMake -*-

####################################################################################################
#
# Python
#

set(Python_ADDITIONAL_VERSIONS 2.7)
find_package(PythonInterp)
find_package(PythonLibs)

####################################################################################################
#
# SWIG
#

find_package(SWIG REQUIRED)
include(${SWIG_USE_FILE})

set(CMAKE_SWIG_FLAGS_COMMON "-Wall")

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
  set(CMAKE_SWIG_FLAGS ${CMAKE_SWIG_FLAGS_COMMON} -DSystemLinux -DNDEBUG)
endif(CMAKE_SYSTEM_NAME MATCHES "Linux")

####################################################################################################
#
# Python Wrapper
#

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${PYTHON_INCLUDE_PATH})

#set_source_files_properties(mupdf.i PROPERTIES CPLUSPLUS ON)
set(mupdf_files mupdf.i
  fitz-extension.c)
SET(SWIG_MODULE_mupdf_EXTRA_DEPS ${mupdf_SOURCE_DIR}/include/mupdf/fitz.h fitz-extension.h)
swig_add_module(mupdf python ${mupdf_files})
swig_link_libraries(mupdf ${PYTHON_LIBRARIES} mupdf)

####################################################################################################
#
# Install
#

install(TARGETS
  ${SWIG_MODULE_mupdf_REAL_NAME}
  LIBRARY DESTINATION lib/python${PYTHON_VERSION})

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/mupdf.py
  DESTINATION lib/python${PYTHON_VERSION})

####################################################################################################
#
# End
#
####################################################################################################