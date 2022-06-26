#include <font/libpvf.h>
#include <kernel.h>
#include <libdbg.h>
#include <paf.h>
#include <ces.h>

#include "vita2d_sys_paf.h"
#include "bin_packing_2d.h"
#include "utils.h"

#define ATLAS_DEFAULT_W 512
#define ATLAS_DEFAULT_H 512

#define PVF_GLYPH_MARGIN 2

using namespace paf;

SceInt32 vita2d::Pvf::Utf8ToUcs2(const char *utf8, SceUInt32 *character)
{
	if (((utf8[0] & 0xF0) == 0xE0) && ((utf8[1] & 0xC0) == 0x80) && ((utf8[2] & 0xC0) == 0x80)) {
		*character = ((utf8[0] & 0x0F) << 12) | ((utf8[1] & 0x3F) << 6) | (utf8[2] & 0x3F);
		return 3;
	}
	else if (((utf8[0] & 0xE0) == 0xC0) && ((utf8[1] & 0xC0) == 0x80)) {
		*character = ((utf8[0] & 0x1F) << 6) | (utf8[1] & 0x3F);
		return 2;
	}
	else {
		*character = utf8[0];
		return 1;
	}
}

ScePvf_t_libId vita2d::Pvf::GetLibId()
{
	ScePVoid tempPtr = &ui::s_widget19999DA5;
	return *(ScePvf_t_libId *)(tempPtr + 0x108);
}

ScePvf_t_fontId vita2d::Pvf::GetDefaultLatinFont()
{
	ScePVoid tempPtr = &ui::s_widget91871D2D;
	return *(ScePvf_t_fontId *)(tempPtr + 0x1E3C);
}

ScePvf_t_fontId vita2d::Pvf::GetDefaultJapaneseFont()
{
	ScePVoid tempPtr = &ui::s_widget91871D2D;
	return *(ScePvf_t_fontId *)(tempPtr + 0x1E0C);
}

SceVoid vita2d::Pvf::SetDefaultLatinFont(ScePvf_t_fontId font)
{
	ScePVoid tempPtr = &ui::s_widget91871D2D;
	thread::Mutex *mutex = (thread::Mutex *)(tempPtr + 0x1F24);

	mutex->Lock();

	*(ScePvf_t_fontId *)(tempPtr + 0x1E3C) = font;

	mutex->Unlock();
}

SceVoid vita2d::Pvf::SetDefaultJapaneseFont(ScePvf_t_fontId font)
{
	ScePVoid tempPtr = &ui::s_widget91871D2D;
	thread::Mutex *mutex = (thread::Mutex *)(tempPtr + 0x1F24);

	mutex->Lock();

	*(ScePvf_t_fontId *)(tempPtr + 0x1E0C) = font;

	mutex->Unlock();
}

vita2d::Pvf::Pvf(const char *path, SceFloat hSize, SceFloat vSize, ScePvfDataAccessMode accessMode)
{
	ScePvf_t_error error;
	ScePvf_t_irect irectinfo;
	ScePVoid tempPtr = &ui::s_widget19999DA5;
	libHandle = *(ScePvf_t_libId *)(tempPtr + 0x108);

	tempPtr = &ui::s_widget91871D2D;
	mutex = (thread::Mutex *)(tempPtr + 0x1F24);

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

	atlas = new TextureAtlas(ATLAS_DEFAULT_W, ATLAS_DEFAULT_H, SCE_GXM_TEXTURE_FORMAT_U8_R111);
}

vita2d::Pvf::Pvf(ScePVoid buf, SceSize bufSize, SceFloat hSize, SceFloat vSize)
{
	ScePvf_t_error error;
	ScePvf_t_irect irectinfo;
	ScePVoid tempPtr = &ui::s_widget19999DA5;
	libHandle = *(ScePvf_t_libId *)(tempPtr + 0x108);

	tempPtr = &ui::s_widget91871D2D;
	mutex = *(thread::Mutex **)(tempPtr + 0x1F24);

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

	atlas = new TextureAtlas(ATLAS_DEFAULT_W, ATLAS_DEFAULT_H, SCE_GXM_TEXTURE_FORMAT_U8_R111);
}

vita2d::Pvf::~Pvf()
{
	scePvfClose(fontHandle);
	delete atlas;
}

SceInt32 vita2d::Pvf::AtlasAddGlyph(SceUInt32 character)
{
	ScePvf_t_charInfo char_info;
	ScePvf_t_irect char_image_rect;
	bp2d_position position;
	Texture *tex = atlas->texture;

	if (scePvfGetCharInfo(fontHandle, character, &char_info) < 0)
		return 0;

	if (scePvfGetCharImageRect(fontHandle, character, &char_image_rect) < 0)
		return 0;

	bp2d_size size = {
		char_image_rect.width + 2 * PVF_GLYPH_MARGIN,
		char_image_rect.height + 2 * PVF_GLYPH_MARGIN
	};

	TextureAtlas::EntryData data = {
		char_info.glyphMetrics.horizontalBearingX64 >> 6,
		char_info.glyphMetrics.horizontalBearingY64 >> 6,
		char_info.glyphMetrics.horizontalAdvance64,
		char_info.glyphMetrics.verticalAdvance64,
		0
	};

	if (!atlas->Insert(character, &size, &data,
		&position))
		return 0;

	ScePvf_t_userImageBufferRec glyph_image;
	glyph_image.pixelFormat = SCE_PVF_USERIMAGE_DIRECT8;
	glyph_image.xPos64 = ((position.x + PVF_GLYPH_MARGIN) << 6) - char_info.glyphMetrics.horizontalBearingX64;
	glyph_image.yPos64 = ((position.y + PVF_GLYPH_MARGIN) << 6) + char_info.glyphMetrics.horizontalBearingY64;
	glyph_image.rect.width = sceGxmTextureGetWidth(&tex->gxmTex);
	glyph_image.rect.height = sceGxmTextureGetHeight(&tex->gxmTex);
	glyph_image.bytesPerLine = sceGxmTextureGetWidth(&tex->gxmTex);
	glyph_image.reserved = 0;
	glyph_image.buffer = (ScePvf_t_u8 *)sceGxmTextureGetData(&tex->gxmTex);

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
	TextureAtlas::EntryData data;
	ScePvf_t_kerningInfo kerning_info;
	SceUInt32 old_character = 0;
	Texture *tex = atlas->texture;
	SceFloat start_x = x;
	SceFloat max_x = 0;
	SceFloat pen_x = x;
	SceFloat pen_y = y;

	for (i = 0; text[i];) {
		i += Utf8ToUcs2(&text[i], &character);

		if (character == '\n') {
			if (pen_x > max_x)
				max_x = pen_x;
			pen_x = start_x;
			pen_y += (vsize + (vsize / 2) + prLinespace) * scale;
			continue;
		}

		if (!atlas->Get(character, &rect, &data)) {
			if (!AtlasAddGlyph(character))
				continue;

			if (!atlas->Get(character,
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
			tex->Draw(
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