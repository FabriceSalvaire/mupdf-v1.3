# -*- CMake -*-

####################################################################################################
#
# Tools
#

add_executable(mudraw mudraw.c)
target_link_libraries(mudraw mupdf)

add_executable(mutool
  mutool.c
  pdfclean.c
  pdfextract.c
  pdfinfo.c
  pdfposter.c
  pdfshow.c
  )
target_link_libraries(mutool mupdf)

install(TARGETS
  mudraw
  mutool
  RUNTIME DESTINATION bin
  )

####################################################################################################
#
# End
#
####################################################################################################
