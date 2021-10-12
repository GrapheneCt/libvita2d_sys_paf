#include <kernel.h>
#include <paf.h>

#include "vita2d_sys.h"
#include "int_htab.h"


vita2d::TextureAtlas::TextureAtlas(SceInt32 width, SceInt32 height, SceGxmTextureFormat format)
{
	bp2d_rectangle rect;
	rect.x = 0;
	rect.y = 0;
	rect.w = width;
	rect.h = height;

	texture = new Texture(width, height, format);

	bpRoot = bp2d_create(&rect);
	htab = int_htab_create(256);

	texture->SetFilters(SCE_GXM_TEXTURE_FILTER_POINT, SCE_GXM_TEXTURE_FILTER_LINEAR);
}

vita2d::TextureAtlas::~TextureAtlas()
{
	delete texture;
	bp2d_free(bpRoot);
	int_htab_free(htab);
}

SceInt32 vita2d::TextureAtlas::Insert(SceUInt32 character,
	const bp2d_size *size,
	const EntryData *data,
	bp2d_position *inserted_pos)
{
	HtabEntry *entry;
	bp2d_node *new_node;

	if (!bp2d_insert(bpRoot, size, inserted_pos, &new_node))
		return 0;

	entry = new HtabEntry();

	entry->rect.x = inserted_pos->x;
	entry->rect.y = inserted_pos->y;
	entry->rect.w = size->w;
	entry->rect.h = size->h;
	entry->data = *data;

	if (!int_htab_insert(htab, character, entry)) {
		bp2d_delete(bpRoot, new_node);
		return 0;
	}

	return 1;
}

SceBool vita2d::TextureAtlas::Exists(SceUInt32 character)
{
	return int_htab_find(htab, character) != NULL;
}

SceInt32 vita2d::TextureAtlas::Get(SceUInt32 character,
	bp2d_rectangle *rect, EntryData *data)
{
	HtabEntry *entry = (HtabEntry *)int_htab_find(htab, character);
	if (!entry)
		return 0;

	*rect = entry->rect;
	*data = entry->data;

	return 1;
}
