#include <font/libpvf.h>
#include <kernel.h>
#include <libdbg.h>
#include <paf.h>
#include "vita2d_sys.h"

#include "texture_atlas.h"
#include "bin_packing_2d.h"
#include "utils.h"
#include "shared.h"
#include "heap.h"

#define ATLAS_DEFAULT_W 512
#define ATLAS_DEFAULT_H 512

#define PVF_GLYPH_MARGIN 2

using namespace paf;

extern void* vita2d_heap_internal;

typedef struct vita2d_pvf_font_handle {
	ScePvf_t_fontId font_handle;
	int (*in_font_group)(unsigned int c);
	struct vita2d_pvf_font_handle *next;
} vita2d_pvf_font_handle;

typedef struct vita2d_pvf {
	ScePvf_t_fontId lib_handle;
	vita2d_pvf_font_handle *font_handle_list;
	texture_atlas *atlas;
	SceKernelLwMutexWork mutex;
	float vsize;
	float pr_linespace;
	float pr_charspace;
	float x_corr;
	float y_corr;
} vita2d_pvf;

vita2d::Pvf::Pvf(const char *path, SceFloat hSize, SceFloat vSize, ScePvfDataAccessMode accessMode)
{
	ScePvf_t_error error;
	ScePvf_t_irect irectinfo;
	ScePVoid tempPtr = &widget::Widget::s_widget19999DA5;
	libHandle = *(ScePvf_t_libId *)(tempPtr + 0x108);

	vsize = 0.0f;
	prLinespace = 0.0f;
	prCharspace = 0.0f;
	xCorr = 0.0f;
	yCorr = 0.0f;

	fontHandle = scePvfOpenUserFile(libHandle, (ScePvf_t_pointer)path, accessMode, &error);
	if (error != SCE_OK) {
		sceClibPrintf("[PVF] scePvfOpenUserFile(): 0x%X", error);
	}

	scePvfSetCharSize(fontHandle, hSize, vSize);

	scePvfGetCharImageRect(fontHandle, 0x0057, &irectinfo);
	vsize = irectinfo.height;

	atlas = texture_atlas_create(ATLAS_DEFAULT_W, ATLAS_DEFAULT_H, SCE_GXM_TEXTURE_FORMAT_U8_R111);

	mutex = new thread::Mutex("vita2d::PvfMutex", SCE_TRUE);
}

vita2d::Pvf::Pvf(ScePVoid buf, SceSize bufSize, SceFloat hSize, SceFloat vSize)
{
	ScePvf_t_error error;
	ScePvf_t_irect irectinfo;
	ScePVoid tempPtr = &widget::Widget::s_widget19999DA5;
	libHandle = *(ScePvf_t_libId *)(tempPtr + 0x108);

	vsize = 0.0f;
	prLinespace = 0.0f;
	prCharspace = 0.0f;
	xCorr = 0.0f;
	yCorr = 0.0f;

	fontHandle = scePvfOpenUserMemory(libHandle, buf, bufSize, &error);
	if (error != SCE_OK) {
		sceClibPrintf("[PVF] scePvfOpenUserMemory(): 0x%X", error);
	}

	scePvfSetCharSize(fontHandle, hSize, vSize);

	scePvfGetCharImageRect(fontHandle, 0x0057, &irectinfo);
	vsize = irectinfo.height;

	atlas = texture_atlas_create(ATLAS_DEFAULT_W, ATLAS_DEFAULT_H, SCE_GXM_TEXTURE_FORMAT_U8_R111);

	mutex = new thread::Mutex("vita2d::PvfMutex", SCE_TRUE);
}

vita2d::Pvf::~Pvf()
{
	delete mutex;
	scePvfClose(fontHandle);
	texture_atlas_free(atlas);
}

SceInt32 vita2d::Pvf::AtlasAddGlyph(unsigned int character)
{
	ScePvf_t_charInfo char_info;
	ScePvf_t_irect char_image_rect;
	bp2d_position position;
	ScePVoid texture_data;
	vita2d_texture *tex = font->atlas->texture;

	if (scePvfGetCharInfo(fontHandle, character, &char_info) < 0)
		return 0;

	if (scePvfGetCharImageRect(fontHandle, character, &char_image_rect) < 0)
		return 0;

	bp2d_size size = {
		char_image_rect.width + 2 * PVF_GLYPH_MARGIN,
		char_image_rect.height + 2 * PVF_GLYPH_MARGIN
	};

	texture_atlas_entry_data data = {
		char_info.glyphMetrics.horizontalBearingX64 >> 6,
		char_info.glyphMetrics.horizontalBearingY64 >> 6,
		char_info.glyphMetrics.horizontalAdvance64,
		char_info.glyphMetrics.verticalAdvance64,
		0
	};

	if (!texture_atlas_insert(atlas, character, &size, &data,
		&position))
		return 0;

	texture_data = vita2d_texture_get_datap(tex);

	ScePvf_t_userImageBufferRec glyph_image;
	glyph_image.pixelFormat = SCE_PVF_USERIMAGE_DIRECT8;
	glyph_image.xPos64 = ((position.x + PVF_GLYPH_MARGIN) << 6) - char_info.glyphMetrics.horizontalBearingX64;
	glyph_image.yPos64 = ((position.y + PVF_GLYPH_MARGIN) << 6) + char_info.glyphMetrics.horizontalBearingY64;
	glyph_image.rect.width = vita2d_texture_get_width(tex);
	glyph_image.rect.height = vita2d_texture_get_height(tex);
	glyph_image.bytesPerLine = vita2d_texture_get_stride(tex);
	glyph_image.reserved = 0;
	glyph_image.buffer = (ScePvf_t_u8 *)texture_data;

	return scePvfGetCharGlyphImage(fontHandle, character, &glyph_image) == 0;
}

SceInt32 vita2d::Pvf::Draw(SceBool draw, SceInt32 *height,
	SceInt32 x, SceInt32 y, SceUInt32 color, SceFloat scale,
	const char *text)
{
	mutex->Lock();

	SceInt32 i;
	SceUInt32 character;
	bp2d_rectangle rect;
	texture_atlas_entry_data data;
	ScePvf_t_kerningInfo kerning_info;
	SceUInt32 old_character = 0;
	vita2d_texture *tex = font->atlas->texture;
	SceFloat start_x = x;
	SceFloat max_x = 0;
	SceFloat pen_x = x;
	SceFloat pen_y = y;

	for (i = 0; text[i];) {
		i += utf8_to_ucs2(&text[i], &character);

		if (character == '\n') {
			if (pen_x > max_x)
				max_x = pen_x;
			pen_x = start_x;
			pen_y += (vsize + (vsize / 2) + prLinespace) * scale;
			continue;
		}

		if (!texture_atlas_get(atlas, character, &rect, &data)) {
			if (!atlas_add_glyph(character))
				continue;

			if (!texture_atlas_get(atlas, character,
				&rect, &data))
				continue;
		}

		if (old_character) {
			if (scePvfGetKerningInfo(fontHandle, old_character, character, &kerning_info) >= 0) {
				pen_x += kerning_info.fKerningInfo.xOffset;
				pen_y += kerning_info.fKerningInfo.yOffset;
			}
		}

		if (draw) {
			vita2d_draw_texture_tint_part_scale(tex,
				pen_x + data.bitmap_left * scale,
				pen_y - data.bitmap_top * scale,
				rect.x + PVF_GLYPH_MARGIN / 2.0f, (rect.y + PVF_GLYPH_MARGIN / 2.0f) - yCorr,
				rect.w - PVF_GLYPH_MARGIN / 2.0f, (rect.h - PVF_GLYPH_MARGIN / 2.0f) - yCorr,
				scale,
				scale,
				color);
		}

		pen_x += ((data.advance_x >> 6) + prCharspace) * scale;
		old_character = character;
	}

	if (pen_x > max_x)
		max_x = pen_x;

	if (height)
		*height = pen_y + vsize * scale - y;

	mutex->Unlock();

	return max_x - x;
}

SceInt32 vita2d::Pvf::Draw(SceInt32 x, SceInt32 y,
	SceUInt32 color, SceFloat scale,
	const char *text)
{
	return Draw(SCE_TRUE, SCE_NULL, x, y, color, scale, text);
}

SceInt32 vita2d::Pvf::Drawf(SceInt32 x, SceInt32 y,
	SceUInt32 color, SceFloat scale,
	const char *text, ...)
{
	char buf[1024];
	va_list argptr;
	va_start(argptr, text);
	sceClibVsnprintf(buf, sizeof(buf), text, argptr);
	va_end(argptr);
	return Draw(x, y, color, scale, buf);
}

SceVoid vita2d::Pvf::Dimensions(SceFloat scale,
	const char *text, SceInt32 *width, SceInt32 *height)
{
	SceInt32 w;
	w = Draw(SCE_FALSE, height, 0, 0, 0, scale, text);

	if (width)
		*width = w;
}

SceInt32 vita2d::Pvf::Width(SceFloat scale, const char *text)
{
	SceInt32 width;
	Dimensions(scale, text, &width, SCE_NULL);
	return width;
}

SceInt32 vita2d::Pvf::Height(SceFloat scale, const char *text)
{
	SceInt32 height;
	Dimensions(scale, text, SCE_NULL, &height);
	return height;
}

SceInt32 vita2d::Pvf::IrectMaxHeight()
{
	ScePvf_t_irect irectinfo;
	scePvfGetCharImageRect(fontHandle, 0x0057, &irectinfo);

	return irectinfo.height;
}

SceInt32 vita2d::Pvf::IrectMaxWidth()
{
	ScePvf_t_irect irectinfo;
	scePvfGetCharImageRect(fontHandle, 0x0057, &irectinfo);

	return irectinfo.width;
}

SceVoid vita2d::Pvf::EmboldenRate(SceFloat em)
{
	scePvfSetEmboldenRate(fontHandle, em);
	yCorr = em / 6.666f;
}

SceVoid vita2d::Pvf::SkewRate(SceFloat ax, SceFloat ay)
{
	scePvfSetSkewValue(fontHandle, ax, ay);
}

SceVoid vita2d::Pvf::CharSize(SceFloat hs, SceFloat vs)
{
	scePvfSetCharSize(fontHandle, hs, vs);
}

SceVoid vita2d::Pvf::Linespace(SceFloat ls)
{
	prLinespace = ls;
}

SceVoid vita2d::Pvf::Charspace(SceFloat cs)
{
	prCharspace = cs;
}