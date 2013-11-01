####################################################################################################

import mupdf as cmupdf

####################################################################################################

class GenericIterator(object):

    ##############################################

    def __init__(self, obj):

        self._obj = obj
        self._index = 0
        self._size = self._obj.len

    ##############################################

    def __iter__(self):

        return self

    ##############################################

    def next(self):

        if self._index < self._size:
            item = self._getter(self._obj, self._index)
            self._index += 1
            return item
        else:
            raise StopIteration

####################################################################################################

class TextBlockIterator(GenericIterator):
    _getter = cmupdf.get_text_block

class TextLineIterator(GenericIterator):
    _getter = cmupdf.get_text_line

class TextCharIterator(GenericIterator):
    _getter = cmupdf.get_text_char

####################################################################################################

class TextSpanIterator(object):

    ##############################################

    def __init__(self, text_line):

        self._text_line = text_line
        self._span = self._text_line.first_span

    ##############################################

    def __iter__(self):

        return self

    ##############################################

    def next(self):

        if self._span:
            span = self._span
            self._span = span.next
            return span
        else:
            raise StopIteration

####################################################################################################

def rect_width_height(rect):
    return  (rect.x1 - rect.x0,
             rect.y1 - rect.y0)

####################################################################################################
# 
# End
# 
####################################################################################################
