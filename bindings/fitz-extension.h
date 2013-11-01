/**************************************************************************************************
 *
 * Add some helper functions and missing feature to mupdf
 * 
 *************************************************************************************************/

/**************************************************************************************************/

#ifndef __FITZ_EXTENSION_H__
#define __FITZ_EXTENSION_H__

/**************************************************************************************************/

#include <stdint.h>

#include "mupdf/fitz.h"

/* ********************************************************************************************** */

// Provide FILE
FILE * fz_fopen(const char *filename, const char *mode);
int fz_fclose(FILE *file);

/* ********************************************************************************************** */

// Expose private API
const char * get_font_name(const fz_font *font);
int font_is_bold(const fz_font *font);
int font_is_italic(const fz_font *font);

/* ********************************************************************************************** */

// Provide access to arrays
fz_text_block * get_text_block(fz_text_page *page, unsigned int block_index);
fz_text_line * get_text_line(fz_text_block *block, unsigned int line_index);
fz_text_span * get_text_span(fz_text_line *line, unsigned int span_index);
fz_text_char * get_text_char(fz_text_span *span, unsigned int char_index);

/* ********************************************************************************************** */

// Helper
char * get_meta_info(fz_document *doc, char *key, int buffer_size);

/* ********************************************************************************************** */

// Expose private API / Helper
char * fz_buffer_data(fz_buffer *buffer);

/* ********************************************************************************************** */

// Missing feature
// Return the Metadata XML stream
fz_buffer * pdf_metadata(fz_document *doc);

/* ********************************************************************************************** */

// Numpy interface
unsigned char * numpy_to_pixmap(uint8_t *buffer,
				size_t number_of_rows,
				size_t number_of_columns,
				size_t number_of_channels);

/* ********************************************************************************************** */

#endif /* __FITZ_EXTENSION_H__ */

/***************************************************************************************************
 * 
 * End
 * 
 **************************************************************************************************/
