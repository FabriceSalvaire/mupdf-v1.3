#include "fitz.h"
#include "mupdf.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_XFREE86_H

static char *base_font_names[14][7] =
{
	{ "Courier", "CourierNew", "CourierNewPSMT", NULL },
	{ "Courier-Bold", "CourierNew,Bold", "Courier,Bold",
		"CourierNewPS-BoldMT", "CourierNew-Bold", NULL },
	{ "Courier-Oblique", "CourierNew,Italic", "Courier,Italic",
		"CourierNewPS-ItalicMT", "CourierNew-Italic", NULL },
	{ "Courier-BoldOblique", "CourierNew,BoldItalic", "Courier,BoldItalic",
		"CourierNewPS-BoldItalicMT", "CourierNew-BoldItalic", NULL },
	{ "Helvetica", "ArialMT", "Arial", NULL },
	{ "Helvetica-Bold", "Arial-BoldMT", "Arial,Bold", "Arial-Bold",
		"Helvetica,Bold", NULL },
	{ "Helvetica-Oblique", "Arial-ItalicMT", "Arial,Italic", "Arial-Italic",
		"Helvetica,Italic", "Helvetica-Italic", NULL },
	{ "Helvetica-BoldOblique", "Arial-BoldItalicMT",
		"Arial,BoldItalic", "Arial-BoldItalic",
		"Helvetica,BoldItalic", "Helvetica-BoldItalic", NULL },
	{ "Times-Roman", "TimesNewRomanPSMT", "TimesNewRoman",
		"TimesNewRomanPS", NULL },
	{ "Times-Bold", "TimesNewRomanPS-BoldMT", "TimesNewRoman,Bold",
		"TimesNewRomanPS-Bold", "TimesNewRoman-Bold", NULL },
	{ "Times-Italic", "TimesNewRomanPS-ItalicMT", "TimesNewRoman,Italic",
		"TimesNewRomanPS-Italic", "TimesNewRoman-Italic", NULL },
	{ "Times-BoldItalic", "TimesNewRomanPS-BoldItalicMT",
		"TimesNewRoman,BoldItalic", "TimesNewRomanPS-BoldItalic",
		"TimesNewRoman-BoldItalic", NULL },
	{ "Symbol", NULL },
	{ "ZapfDingbats", NULL }
};

static int isdynalab(char *name)
{
	if (strstr(name, "HuaTian"))
		return 1;
	if (strstr(name, "MingLi"))
		return 1;
	if ((strstr(name, "DF") == name) || strstr(name, "+DF"))
		return 1;
	if ((strstr(name, "DLC") == name) || strstr(name, "+DLC"))
		return 1;
	return 0;
}

static int strcmp_ignore_space(char *a, char *b)
{
	while (1)
	{
		while (*a == ' ')
			a++;
		while (*b == ' ')
			b++;
		if (*a != *b)
			return 1;
		if (*a == 0)
			return *a != *b;
		if (*b == 0)
			return *a != *b;
		a++;
		b++;
	}
}

static char *clean_font_name(char *fontname)
{
	int i, k;
	for (i = 0; i < 14; i++)
		for (k = 0; base_font_names[i][k]; k++)
			if (!strcmp_ignore_space(base_font_names[i][k], fontname))
				return base_font_names[i][0];
	return fontname;
}

/*
 * FreeType and Rendering glue
 */

enum { UNKNOWN, TYPE1, TRUETYPE };

static int ft_kind(FT_Face face)
{
	const char *kind = FT_Get_X11_Font_Format(face);
	pdf_log_font("ft font format %s\n", kind);
	if (!strcmp(kind, "TrueType"))
		return TRUETYPE;
	if (!strcmp(kind, "Type 1"))
		return TYPE1;
	if (!strcmp(kind, "CFF"))
		return TYPE1;
	if (!strcmp(kind, "CID Type 1"))
		return TYPE1;
	return UNKNOWN;
}

static int ft_char_index(FT_Face face, int cid)
{
	int gid = FT_Get_Char_Index(face, cid);
	if (gid == 0)
		gid = FT_Get_Char_Index(face, 0xf000 + cid);
	return gid;
}

static inline int ft_cid_to_gid(pdf_font_desc *fontdesc, int cid)
{
	if (fontdesc->to_ttf_cmap)
	{
		cid = pdf_lookup_cmap(fontdesc->to_ttf_cmap, cid);
		return ft_char_index(fontdesc->font->ft_face, cid);
	}

	if (fontdesc->cid_to_gid)
		return fontdesc->cid_to_gid[cid];

	return cid;
}

int
pdf_font_cid_to_gid(pdf_font_desc *fontdesc, int cid)
{
	if (fontdesc->font->ft_face)
		return ft_cid_to_gid(fontdesc, cid);
	return cid;
}

static int ft_width(pdf_font_desc *fontdesc, int cid)
{
	int gid, fterr;

	gid = ft_cid_to_gid(fontdesc, cid);

	fterr = FT_Load_Glyph(fontdesc->font->ft_face, gid,
			FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP | FT_LOAD_IGNORE_TRANSFORM);
	if (fterr)
	{
		fz_warn("freetype load glyph (gid %d): %s", gid, ft_error_string(fterr));
		return 0;
	}
	return ((FT_Face)fontdesc->font->ft_face)->glyph->advance.x;
}

/*
 * Basic encoding tables
 */

static int mre_code(char *name)
{
	int i;
	for (i = 0; i < 256; i++)
		if (pdf_mac_roman[i] && !strcmp(name, pdf_mac_roman[i]))
			return i;
	return -1;
}

/*
 * Create and destroy
 */

pdf_font_desc *
pdf_keep_font(pdf_font_desc *fontdesc)
{
	fontdesc->refs ++;
	return fontdesc;
}

void
pdf_drop_font(pdf_font_desc *fontdesc)
{
	if (fontdesc && --fontdesc->refs == 0)
	{
		if (fontdesc->font)
			fz_drop_font(fontdesc->font);
		if (fontdesc->encoding)
			pdf_drop_cmap(fontdesc->encoding);
		if (fontdesc->to_ttf_cmap)
			pdf_drop_cmap(fontdesc->to_ttf_cmap);
		if (fontdesc->to_unicode)
			pdf_drop_cmap(fontdesc->to_unicode);
		fz_free(fontdesc->cid_to_gid);
		fz_free(fontdesc->cid_to_ucs);
		fz_free(fontdesc->hmtx);
		fz_free(fontdesc->vmtx);
		fz_free(fontdesc);
	}
}

pdf_font_desc *
pdf_new_font_desc(void)
{
	pdf_font_desc *fontdesc;

	fontdesc = fz_malloc(sizeof(pdf_font_desc));
	fontdesc->refs = 1;

	fontdesc->font = NULL;

	fontdesc->flags = 0;
	fontdesc->italic_angle = 0;
	fontdesc->ascent = 0;
	fontdesc->descent = 0;
	fontdesc->cap_height = 0;
	fontdesc->x_height = 0;
	fontdesc->missing_width = 0;

	fontdesc->encoding = NULL;
	fontdesc->to_ttf_cmap = NULL;
	fontdesc->cid_to_gid_len = 0;
	fontdesc->cid_to_gid = NULL;

	fontdesc->to_unicode = NULL;
	fontdesc->cid_to_ucs_len = 0;
	fontdesc->cid_to_ucs = NULL;

	fontdesc->wmode = 0;

	fontdesc->hmtx_cap = 0;
	fontdesc->vmtx_cap = 0;
	fontdesc->hmtx_len = 0;
	fontdesc->vmtx_len = 0;
	fontdesc->hmtx = NULL;
	fontdesc->vmtx = NULL;

	fontdesc->dhmtx.lo = 0x0000;
	fontdesc->dhmtx.hi = 0xFFFF;
	fontdesc->dhmtx.w = 1000;

	fontdesc->dvmtx.lo = 0x0000;
	fontdesc->dvmtx.hi = 0xFFFF;
	fontdesc->dvmtx.x = 0;
	fontdesc->dvmtx.y = 880;
	fontdesc->dvmtx.w = -1000;

	fontdesc->isembedded = 0;

	return fontdesc;
}

/*
 * Simple fonts (Type1 and TrueType)
 */

static fz_error
load_simple_font(pdf_font_desc **fontdescp, pdf_xref *xref, fz_obj *dict)
{
	fz_error error;
	fz_obj *descriptor;
	fz_obj *encoding;
	fz_obj *widths;
	unsigned short *etable = NULL;
	pdf_font_desc *fontdesc;
	FT_Face face;
	FT_CharMap cmap;
	int kind;
	int symbolic;

	char *basefont;
	char *fontname;
	char *estrings[256];
	char ebuffer[256][32];
	int i, k, n;
	int fterr;

	basefont = fz_to_name(fz_dict_gets(dict, "BaseFont"));
	fontname = clean_font_name(basefont);

	/* Load font file */

	fontdesc = pdf_new_font_desc();

	pdf_log_font("load simple font (%d %d R) ptr=%p {\n", fz_to_num(dict), fz_to_gen(dict), fontdesc);
	pdf_log_font("basefont %s -> %s\n", basefont, fontname);

	descriptor = fz_dict_gets(dict, "FontDescriptor");
	if (descriptor)
		error = pdf_load_font_descriptor(fontdesc, xref, descriptor, NULL, basefont);
	else
		error = pdf_load_builtin_font(fontdesc, fontname);
	if (error)
		goto cleanup;

	/* Some chinese documents mistakenly consider WinAnsiEncoding to be codepage 936 */
	if (!*fontdesc->font->name &&
		!fz_dict_gets(dict, "ToUnicode") &&
		!strcmp(fz_to_name(fz_dict_gets(dict, "Encoding")), "WinAnsiEncoding") &&
		fz_to_int(fz_dict_gets(descriptor, "Flags")) == 4)
	{
		/* note: without the comma, pdf_load_font_descriptor would prefer /FontName over /BaseFont */
		char *cp936fonts[] = {
			"\xCB\xCE\xCC\xE5", "SimSun,Regular",
			"\xBA\xDA\xCC\xE5", "SimHei,Regular",
			"\xBF\xAC\xCC\xE5_GB2312", "SimKai,Regular",
			"\xB7\xC2\xCB\xCE_GB2312", "SimFang,Regular",
			"\xC1\xA5\xCA\xE9", "SimLi,Regular",
			NULL
		};
		for (i = 0; cp936fonts[i]; i += 2)
			if (!strcmp(basefont, cp936fonts[i]))
				break;
		if (cp936fonts[i])
		{
			fz_warn("workaround for S22PDF lying about chinese font encodings");
			pdf_drop_font(fontdesc);
			fontdesc = pdf_new_font_desc();
			error = pdf_load_font_descriptor(fontdesc, xref, descriptor, "Adobe-GB1", cp936fonts[i+1]);
			error |= pdf_load_system_cmap(&fontdesc->encoding, "GBK-EUC-H");
			error |= pdf_load_system_cmap(&fontdesc->to_unicode, "Adobe-GB1-UCS2");
			error |= pdf_load_system_cmap(&fontdesc->to_ttf_cmap, "Adobe-GB1-UCS2");
			if (error)
				return fz_rethrow(error, "cannot load font");

			face = fontdesc->font->ft_face;
			kind = ft_kind(face);
			goto skip_encoding;
		}
	}

	face = fontdesc->font->ft_face;
	kind = ft_kind(face);

	/* Encoding */

	pdf_log_font("ft name '%s' '%s'\n", face->family_name, face->style_name);

	symbolic = fontdesc->flags & 4;

	if (face->num_charmaps > 0)
		cmap = face->charmaps[0];
	else
		cmap = NULL;

	for (i = 0; i < face->num_charmaps; i++)
	{
		FT_CharMap test = face->charmaps[i];

		if (kind == TYPE1)
		{
			if (test->platform_id == 7)
				cmap = test;
		}

		if (kind == TRUETYPE)
		{
			if (test->platform_id == 1 && test->encoding_id == 0)
				cmap = test;
			if (test->platform_id == 3 && test->encoding_id == 1)
				cmap = test;
		}
	}

	if (cmap)
	{
		fterr = FT_Set_Charmap(face, cmap);
		if (fterr)
			fz_warn("freetype could not set cmap: %s", ft_error_string(fterr));
	}
	else
		fz_warn("freetype could not find any cmaps");

	etable = fz_calloc(256, sizeof(unsigned short));
	for (i = 0; i < 256; i++)
	{
		estrings[i] = NULL;
		etable[i] = 0;
	}

	encoding = fz_dict_gets(dict, "Encoding");
	if (encoding)
	{
		if (fz_is_name(encoding))
			pdf_load_encoding(estrings, fz_to_name(encoding));

		if (fz_is_dict(encoding))
		{
			fz_obj *base, *diff, *item;

			base = fz_dict_gets(encoding, "BaseEncoding");
			if (fz_is_name(base))
				pdf_load_encoding(estrings, fz_to_name(base));
			else if (!fontdesc->isembedded && !symbolic)
				pdf_load_encoding(estrings, "StandardEncoding");

			diff = fz_dict_gets(encoding, "Differences");
			if (fz_is_array(diff))
			{
				n = fz_array_len(diff);
				k = 0;
				for (i = 0; i < n; i++)
				{
					item = fz_array_get(diff, i);
					if (fz_is_int(item))
						k = fz_to_int(item);
					if (fz_is_name(item))
						estrings[k++] = fz_to_name(item);
					if (k < 0) k = 0;
					if (k > 255) k = 255;
				}
			}
		}
	}

	/* start with the builtin encoding */
	for (i = 0; i < 256; i++)
		etable[i] = ft_char_index(face, i);

	/* encode by glyph name where we can */
	if (kind == TYPE1)
	{
		pdf_log_font("encode type1/cff by strings\n");
		for (i = 0; i < 256; i++)
		{
			if (estrings[i])
			{
				etable[i] = FT_Get_Name_Index(face, estrings[i]);
				if (etable[i] == 0)
				{
					int aglcode = pdf_lookup_agl(estrings[i]);
					const char **dupnames = pdf_lookup_agl_duplicates(aglcode);
					while (*dupnames)
					{
						etable[i] = FT_Get_Name_Index(face, (char*)*dupnames);
						if (etable[i])
							break;
						dupnames++;
					}
				}
			}
		}
	}

	/* encode by glyph name where we can */
	if (kind == TRUETYPE)
	{
		/* Unicode cmap */
		if (!symbolic && face->charmap && face->charmap->platform_id == 3)
		{
			pdf_log_font("encode truetype via unicode\n");
			for (i = 0; i < 256; i++)
			{
				if (estrings[i])
				{
					int aglcode = pdf_lookup_agl(estrings[i]);
					if (!aglcode)
						etable[i] = FT_Get_Name_Index(face, estrings[i]);
					else
						etable[i] = ft_char_index(face, aglcode);
				}
			}
		}

		/* MacRoman cmap */
		else if (!symbolic && face->charmap && face->charmap->platform_id == 1)
		{
			pdf_log_font("encode truetype via macroman\n");
			for (i = 0; i < 256; i++)
			{
				if (estrings[i])
				{
					k = mre_code(estrings[i]);
					if (k <= 0)
						etable[i] = FT_Get_Name_Index(face, estrings[i]);
					else
						etable[i] = ft_char_index(face, k);
				}
			}
		}

		/* Symbolic cmap */
		else
		{
			pdf_log_font("encode truetype symbolic\n");
			for (i = 0; i < 256; i++)
			{
				if (estrings[i])
				{
					etable[i] = FT_Get_Name_Index(face, estrings[i]);
					if (etable[i] == 0)
						etable[i] = ft_char_index(face, i);
				}
			}
		}
	}

	/* try to reverse the glyph names from the builtin encoding */
	for (i = 0; i < 256; i++)
	{
		if (etable[i] && !estrings[i])
		{
			if (FT_HAS_GLYPH_NAMES(face))
			{
				fterr = FT_Get_Glyph_Name(face, etable[i], ebuffer[i], 32);
				if (fterr)
					fz_warn("freetype get glyph name (gid %d): %s", etable[i], ft_error_string(fterr));
				if (ebuffer[i][0])
					estrings[i] = ebuffer[i];
			}
			else
			{
				estrings[i] = (char*) pdf_win_ansi[i]; /* discard const */
			}
		}
	}

	fontdesc->encoding = pdf_new_identity_cmap(0, 1);
	fontdesc->cid_to_gid_len = 256;
	fontdesc->cid_to_gid = etable;

	error = pdf_load_to_unicode(fontdesc, xref, estrings, NULL, fz_dict_gets(dict, "ToUnicode"));
	if (error)
		fz_catch(error, "cannot load to_unicode");

skip_encoding:

	/* Widths */

	pdf_set_default_hmtx(fontdesc, fontdesc->missing_width);

	widths = fz_dict_gets(dict, "Widths");
	if (widths)
	{
		int first, last;

		first = fz_to_int(fz_dict_gets(dict, "FirstChar"));
		last = fz_to_int(fz_dict_gets(dict, "LastChar"));

		if (first < 0 || last > 255 || first > last)
			first = last = 0;

		for (i = 0; i < last - first + 1; i++)
		{
			int wid = fz_to_int(fz_array_get(widths, i));
			pdf_add_hmtx(fontdesc, i + first, i + first, wid);
		}
	}
	else
	{
		fterr = FT_Set_Char_Size(face, 1000, 1000, 72, 72);
		if (fterr)
			fz_warn("freetype set character size: %s", ft_error_string(fterr));
		for (i = 0; i < 256; i++)
		{
			pdf_add_hmtx(fontdesc, i, i, ft_width(fontdesc, i));
		}
	}

	pdf_end_hmtx(fontdesc);

	pdf_log_font("}\n");

	*fontdescp = fontdesc;
	return fz_okay;

cleanup:
	if (etable != fontdesc->cid_to_gid)
		fz_free(etable);
	pdf_drop_font(fontdesc);
	return fz_rethrow(error, "cannot load simple font (%d %d R)", fz_to_num(dict), fz_to_gen(dict));
}

/*
 * CID Fonts
 */

static fz_error
load_cid_font(pdf_font_desc **fontdescp, pdf_xref *xref, fz_obj *dict, fz_obj *encoding, fz_obj *to_unicode)
{
	fz_error error;
	fz_obj *widths;
	fz_obj *descriptor;
	pdf_font_desc *fontdesc;
	FT_Face face;
	int kind;
	char collection[256];
	char *basefont;
	int i, k, fterr;
	fz_obj *obj;
	int dw;

	/* Get font name and CID collection */

	basefont = fz_to_name(fz_dict_gets(dict, "BaseFont"));

	{
		fz_obj *cidinfo;
		char tmpstr[64];
		int tmplen;

		cidinfo = fz_dict_gets(dict, "CIDSystemInfo");
		if (!cidinfo)
			return fz_throw("cid font is missing info");

		obj = fz_dict_gets(cidinfo, "Registry");
		tmplen = MIN(sizeof tmpstr - 1, fz_to_str_len(obj));
		memcpy(tmpstr, fz_to_str_buf(obj), tmplen);
		tmpstr[tmplen] = '\0';
		fz_strlcpy(collection, tmpstr, sizeof collection);

		fz_strlcat(collection, "-", sizeof collection);

		obj = fz_dict_gets(cidinfo, "Ordering");
		tmplen = MIN(sizeof tmpstr - 1, fz_to_str_len(obj));
		memcpy(tmpstr, fz_to_str_buf(obj), tmplen);
		tmpstr[tmplen] = '\0';
		fz_strlcat(collection, tmpstr, sizeof collection);
	}

	/* Load font file */

	fontdesc = pdf_new_font_desc();

	pdf_log_font("load cid font (%d %d R) ptr=%p {\n", fz_to_num(dict), fz_to_gen(dict), fontdesc);
	pdf_log_font("basefont %s\n", basefont);
	pdf_log_font("collection %s\n", collection);

	descriptor = fz_dict_gets(dict, "FontDescriptor");
	if (descriptor)
		error = pdf_load_font_descriptor(fontdesc, xref, descriptor, collection, basefont);
	else
		error = fz_throw("syntaxerror: missing font descriptor");
	if (error)
		goto cleanup;

	face = fontdesc->font->ft_face;
	kind = ft_kind(face);

	/* Check for DynaLab fonts that must use hinting */
	if (kind == TRUETYPE)
	{
		if (FT_IS_TRICKY(face) || isdynalab(fontdesc->font->name))
		{
			fontdesc->font->ft_hint = 1;
			pdf_log_font("forced hinting for dynalab font\n");
		}
	}

	/* Encoding */

	error = fz_okay;
	if (fz_is_name(encoding))
	{
		pdf_log_font("encoding /%s\n", fz_to_name(encoding));
		if (!strcmp(fz_to_name(encoding), "Identity-H"))
			fontdesc->encoding = pdf_new_identity_cmap(0, 2);
		else if (!strcmp(fz_to_name(encoding), "Identity-V"))
			fontdesc->encoding = pdf_new_identity_cmap(1, 2);
		else
			error = pdf_load_system_cmap(&fontdesc->encoding, fz_to_name(encoding));
	}
	else if (fz_is_indirect(encoding))
	{
		pdf_log_font("encoding %d %d R\n", fz_to_num(encoding), fz_to_gen(encoding));
		error = pdf_load_embedded_cmap(&fontdesc->encoding, xref, encoding);
	}
	else
	{
		error = fz_throw("syntaxerror: font missing encoding");
	}
	if (error)
		goto cleanup;

	pdf_set_font_wmode(fontdesc, pdf_get_wmode(fontdesc->encoding));
	pdf_log_font("wmode %d\n", pdf_get_wmode(fontdesc->encoding));

	if (kind == TRUETYPE)
	{
		fz_obj *cidtogidmap;

		cidtogidmap = fz_dict_gets(dict, "CIDToGIDMap");
		if (fz_is_indirect(cidtogidmap))
		{
			fz_buffer *buf;

			pdf_log_font("cidtogidmap stream\n");

			error = pdf_load_stream(&buf, xref, fz_to_num(cidtogidmap), fz_to_gen(cidtogidmap));
			if (error)
				goto cleanup;

			fontdesc->cid_to_gid_len = (buf->len) / 2;
			fontdesc->cid_to_gid = fz_calloc(fontdesc->cid_to_gid_len, sizeof(unsigned short));
			for (i = 0; i < fontdesc->cid_to_gid_len; i++)
				fontdesc->cid_to_gid[i] = (buf->data[i * 2] << 8) + buf->data[i * 2 + 1];

			fz_drop_buffer(buf);
		}

		/* if truetype font is external, cidtogidmap should not be identity */
		/* so we map from cid to unicode and then map that through the (3 1) */
		/* unicode cmap to get a glyph id */
		else if (fontdesc->font->ft_substitute)
		{
			pdf_log_font("emulate ttf cidfont\n");

			fterr = FT_Select_Charmap(face, ft_encoding_unicode);
			if (fterr)
			{
				error = fz_throw("fonterror: no unicode cmap when emulating CID font: %s", ft_error_string(fterr));
				goto cleanup;
			}

			if (!strcmp(collection, "Adobe-CNS1"))
				error = pdf_load_system_cmap(&fontdesc->to_ttf_cmap, "Adobe-CNS1-UCS2");
			else if (!strcmp(collection, "Adobe-GB1"))
				error = pdf_load_system_cmap(&fontdesc->to_ttf_cmap, "Adobe-GB1-UCS2");
			else if (!strcmp(collection, "Adobe-Japan1"))
				error = pdf_load_system_cmap(&fontdesc->to_ttf_cmap, "Adobe-Japan1-UCS2");
			else if (!strcmp(collection, "Adobe-Japan2"))
				error = pdf_load_system_cmap(&fontdesc->to_ttf_cmap, "Adobe-Japan2-UCS2");
			else if (!strcmp(collection, "Adobe-Korea1"))
				error = pdf_load_system_cmap(&fontdesc->to_ttf_cmap, "Adobe-Korea1-UCS2");
			else
				error = fz_okay;

			if (error)
			{
				error = fz_rethrow(error, "cannot load system cmap %s", collection);
				goto cleanup;
			}
		}
	}

	error = pdf_load_to_unicode(fontdesc, xref, NULL, collection, to_unicode);
	if (error)
		fz_catch(error, "cannot load to_unicode");

	/* Horizontal */

	dw = 1000;
	obj = fz_dict_gets(dict, "DW");
	if (obj)
		dw = fz_to_int(obj);
	pdf_set_default_hmtx(fontdesc, dw);

	widths = fz_dict_gets(dict, "W");
	if (widths)
	{
		int c0, c1, w;

		for (i = 0; i < fz_array_len(widths); )
		{
			c0 = fz_to_int(fz_array_get(widths, i));
			obj = fz_array_get(widths, i + 1);
			if (fz_is_array(obj))
			{
				for (k = 0; k < fz_array_len(obj); k++)
				{
					w = fz_to_int(fz_array_get(obj, k));
					pdf_add_hmtx(fontdesc, c0 + k, c0 + k, w);
				}
				i += 2;
			}
			else
			{
				c1 = fz_to_int(obj);
				w = fz_to_int(fz_array_get(widths, i + 2));
				pdf_add_hmtx(fontdesc, c0, c1, w);
				i += 3;
			}
		}
	}

	pdf_end_hmtx(fontdesc);

	/* Vertical */

	if (pdf_get_wmode(fontdesc->encoding) == 1)
	{
		int dw2y = 880;
		int dw2w = -1000;

		obj = fz_dict_gets(dict, "DW2");
		if (obj)
		{
			dw2y = fz_to_int(fz_array_get(obj, 0));
			dw2w = fz_to_int(fz_array_get(obj, 1));
		}

		pdf_set_default_vmtx(fontdesc, dw2y, dw2w);

		widths = fz_dict_gets(dict, "W2");
		if (widths)
		{
			int c0, c1, w, x, y;

			for (i = 0; i < fz_array_len(widths); )
			{
				c0 = fz_to_int(fz_array_get(widths, i));
				obj = fz_array_get(widths, i + 1);
				if (fz_is_array(obj))
				{
					for (k = 0; k * 3 < fz_array_len(obj); k ++)
					{
						w = fz_to_int(fz_array_get(obj, k * 3 + 0));
						x = fz_to_int(fz_array_get(obj, k * 3 + 1));
						y = fz_to_int(fz_array_get(obj, k * 3 + 2));
						pdf_add_vmtx(fontdesc, c0 + k, c0 + k, x, y, w);
					}
					i += 2;
				}
				else
				{
					c1 = fz_to_int(obj);
					w = fz_to_int(fz_array_get(widths, i + 2));
					x = fz_to_int(fz_array_get(widths, i + 3));
					y = fz_to_int(fz_array_get(widths, i + 4));
					pdf_add_vmtx(fontdesc, c0, c1, x, y, w);
					i += 5;
				}
			}
		}

		pdf_end_vmtx(fontdesc);
	}

	pdf_log_font("}\n");

	*fontdescp = fontdesc;
	return fz_okay;

cleanup:
	pdf_drop_font(fontdesc);
	return fz_rethrow(error, "cannot load cid font (%d %d R)", fz_to_num(dict), fz_to_gen(dict));
}

static fz_error
load_type0_font(pdf_font_desc **fontdescp, pdf_xref *xref, fz_obj *dict)
{
	fz_error error;
	fz_obj *dfonts;
	fz_obj *dfont;
	fz_obj *subtype;
	fz_obj *encoding;
	fz_obj *to_unicode;

	dfonts = fz_dict_gets(dict, "DescendantFonts");
	if (!dfonts)
		return fz_throw("cid font is missing descendant fonts");

	dfont = fz_array_get(dfonts, 0);

	subtype = fz_dict_gets(dfont, "Subtype");
	encoding = fz_dict_gets(dict, "Encoding");
	to_unicode = fz_dict_gets(dict, "ToUnicode");

	if (fz_is_name(subtype) && !strcmp(fz_to_name(subtype), "CIDFontType0"))
		error = load_cid_font(fontdescp, xref, dfont, encoding, to_unicode);
	else if (fz_is_name(subtype) && !strcmp(fz_to_name(subtype), "CIDFontType2"))
		error = load_cid_font(fontdescp, xref, dfont, encoding, to_unicode);
	else
		error = fz_throw("syntaxerror: unknown cid font type");
	if (error)
		return fz_rethrow(error, "cannot load descendant font (%d %d R)", fz_to_num(dfont), fz_to_gen(dfont));

	return fz_okay;
}

/*
 * FontDescriptor
 */

fz_error
pdf_load_font_descriptor(pdf_font_desc *fontdesc, pdf_xref *xref, fz_obj *dict, char *collection, char *basefont)
{
	fz_error error;
	fz_obj *obj1, *obj2, *obj3, *obj;
	char *fontname;
	char *origname;

	pdf_log_font("load fontdescriptor {\n");

	if (!strchr(basefont, ',') || strchr(basefont, '+'))
		origname = fz_to_name(fz_dict_gets(dict, "FontName"));
	else
		origname = basefont;
	fontname = clean_font_name(origname);

	pdf_log_font("fontname %s -> %s\n", origname, fontname);

	fontdesc->flags = fz_to_int(fz_dict_gets(dict, "Flags"));
	fontdesc->italic_angle = fz_to_real(fz_dict_gets(dict, "ItalicAngle"));
	fontdesc->ascent = fz_to_real(fz_dict_gets(dict, "Ascent"));
	fontdesc->descent = fz_to_real(fz_dict_gets(dict, "Descent"));
	fontdesc->cap_height = fz_to_real(fz_dict_gets(dict, "CapHeight"));
	fontdesc->x_height = fz_to_real(fz_dict_gets(dict, "XHeight"));
	fontdesc->missing_width = fz_to_real(fz_dict_gets(dict, "MissingWidth"));

	pdf_log_font("flags %d\n", fontdesc->flags);

	obj1 = fz_dict_gets(dict, "FontFile");
	obj2 = fz_dict_gets(dict, "FontFile2");
	obj3 = fz_dict_gets(dict, "FontFile3");
	obj = obj1 ? obj1 : obj2 ? obj2 : obj3;

	if (getenv("NOFONT"))
		obj = NULL;

	if (fz_is_indirect(obj))
	{
		error = pdf_load_embedded_font(fontdesc, xref, obj);
		if (error)
		{
			fz_catch(error, "ignored error when loading embedded font, attempting to load system font");
			if (origname != fontname)
				error = pdf_load_builtin_font(fontdesc, fontname);
			else
				error = pdf_load_system_font(fontdesc, fontname, collection);
			if (error)
				return fz_rethrow(error, "cannot load font descriptor (%d %d R)", fz_to_num(dict), fz_to_gen(dict));
		}
	}
	else
	{
		if (origname != fontname)
			error = pdf_load_builtin_font(fontdesc, fontname);
		else
			error = pdf_load_system_font(fontdesc, fontname, collection);
		if (error)
			return fz_rethrow(error, "cannot load font descriptor (%d %d R)", fz_to_num(dict), fz_to_gen(dict));
	}

	fz_strlcpy(fontdesc->font->name, fontname, sizeof fontdesc->font->name);

	pdf_log_font("}\n");

	return fz_okay;

}

static void
pdf_make_width_table(pdf_font_desc *fontdesc)
{
	fz_font *font = fontdesc->font;
	int i, k, cid, gid;

	font->width_count = 0;
	for (i = 0; i < fontdesc->hmtx_len; i++)
	{
		for (k = fontdesc->hmtx[i].lo; k <= fontdesc->hmtx[i].hi; k++)
		{
			cid = pdf_lookup_cmap(fontdesc->encoding, k);
			gid = pdf_font_cid_to_gid(fontdesc, cid);
			if (gid > font->width_count)
				font->width_count = gid;
		}
	}
	font->width_count ++;

	font->width_table = fz_calloc(font->width_count, sizeof(int));
	memset(font->width_table, 0, sizeof(int) * font->width_count);

	for (i = 0; i < fontdesc->hmtx_len; i++)
	{
		for (k = fontdesc->hmtx[i].lo; k <= fontdesc->hmtx[i].hi; k++)
		{
			cid = pdf_lookup_cmap(fontdesc->encoding, k);
			gid = pdf_font_cid_to_gid(fontdesc, cid);
			if (gid >= 0 && gid < font->width_count)
				font->width_table[gid] = fontdesc->hmtx[i].w;
		}
	}
}

fz_error
pdf_load_font(pdf_font_desc **fontdescp, pdf_xref *xref, fz_obj *rdb, fz_obj *dict)
{
	fz_error error;
	char *subtype;
	fz_obj *dfonts;
	fz_obj *charprocs;

	if ((*fontdescp = pdf_find_item(xref->store, pdf_drop_font, dict)))
	{
		pdf_keep_font(*fontdescp);
		return fz_okay;
	}

	subtype = fz_to_name(fz_dict_gets(dict, "Subtype"));
	dfonts = fz_dict_gets(dict, "DescendantFonts");
	charprocs = fz_dict_gets(dict, "CharProcs");

	if (subtype && !strcmp(subtype, "Type0"))
		error = load_type0_font(fontdescp, xref, dict);
	else if (subtype && !strcmp(subtype, "Type1"))
		error = load_simple_font(fontdescp, xref, dict);
	else if (subtype && !strcmp(subtype, "MMType1"))
		error = load_simple_font(fontdescp, xref, dict);
	else if (subtype && !strcmp(subtype, "TrueType"))
		error = load_simple_font(fontdescp, xref, dict);
	else if (subtype && !strcmp(subtype, "Type3"))
		error = pdf_load_type3_font(fontdescp, xref, rdb, dict);
	else if (charprocs)
	{
		fz_warn("unknown font format, guessing type3.");
		error = pdf_load_type3_font(fontdescp, xref, rdb, dict);
	}
	else if (dfonts)
	{
		fz_warn("unknown font format, guessing type0.");
		error = load_type0_font(fontdescp, xref, dict);
	}
	else
	{
		fz_warn("unknown font format, guessing type1 or truetype.");
		error = load_simple_font(fontdescp, xref, dict);
	}
	if (error)
		return fz_rethrow(error, "cannot load font (%d %d R)", fz_to_num(dict), fz_to_gen(dict));

	/* Save the widths to stretch non-CJK substitute fonts */
	if ((*fontdescp)->font->ft_substitute && !(*fontdescp)->to_ttf_cmap)
		pdf_make_width_table(*fontdescp);

	pdf_store_item(xref->store, pdf_keep_font, pdf_drop_font, dict, *fontdescp);

	return fz_okay;
}

void
pdf_debug_font(pdf_font_desc *fontdesc)
{
	int i;

	printf("fontdesc {\n");

	if (fontdesc->font->ft_face)
		printf("\tfreetype font\n");
	if (fontdesc->font->t3procs)
		printf("\ttype3 font\n");

	printf("\twmode %d\n", fontdesc->wmode);
	printf("\tDW %d\n", fontdesc->dhmtx.w);

	printf("\tW {\n");
	for (i = 0; i < fontdesc->hmtx_len; i++)
		printf("\t\t<%04x> <%04x> %d\n",
			fontdesc->hmtx[i].lo, fontdesc->hmtx[i].hi, fontdesc->hmtx[i].w);
	printf("\t}\n");

	if (fontdesc->wmode)
	{
		printf("\tDW2 [%d %d]\n", fontdesc->dvmtx.y, fontdesc->dvmtx.w);
		printf("\tW2 {\n");
		for (i = 0; i < fontdesc->vmtx_len; i++)
			printf("\t\t<%04x> <%04x> %d %d %d\n", fontdesc->vmtx[i].lo, fontdesc->vmtx[i].hi,
				fontdesc->vmtx[i].x, fontdesc->vmtx[i].y, fontdesc->vmtx[i].w);
		printf("\t}\n");
	}
}