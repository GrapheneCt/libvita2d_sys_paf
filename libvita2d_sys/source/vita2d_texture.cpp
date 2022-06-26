#include <kernel.h>
#include <kernel/dmacmgr.h>
#include <libdbg.h>
#include <appmgr.h>
#include <paf.h>

#include "vita2d_sys_paf.h"
#include "utils.h"

using namespace paf;

#define GXM_TEX_MAX_SIZE 4096
static graph::SurfacePool::MemoryType heapType = (graph::SurfacePool::MemoryType)(0);

SceInt32 vita2d::Texture::TexFormat2Bytespp(SceGxmTextureFormat format)
{
	switch (format & 0x9f000000U) {
	case SCE_GXM_TEXTURE_BASE_FORMAT_U8:
	case SCE_GXM_TEXTURE_BASE_FORMAT_S8:
	case SCE_GXM_TEXTURE_BASE_FORMAT_P8:
		return 1;
	case SCE_GXM_TEXTURE_BASE_FORMAT_U4U4U4U4:
	case SCE_GXM_TEXTURE_BASE_FORMAT_U8U3U3U2:
	case SCE_GXM_TEXTURE_BASE_FORMAT_U1U5U5U5:
	case SCE_GXM_TEXTURE_BASE_FORMAT_U5U6U5:
	case SCE_GXM_TEXTURE_BASE_FORMAT_S5S5U6:
	case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8:
	case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8:
		return 2;
	case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8:
	case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8:
		return 3;
	case SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8:
	case SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8S8:
	case SCE_GXM_TEXTURE_BASE_FORMAT_F32:
	case SCE_GXM_TEXTURE_BASE_FORMAT_U32:
	case SCE_GXM_TEXTURE_BASE_FORMAT_S32:
	default:
		return 4;
	}
}

SceVoid vita2d::Texture::SetHeapType(graph::SurfacePool::MemoryType type)
{
		heapType = type;
}

graph::SurfacePool::MemoryType vita2d::Texture::GetHeapType()
{
	return heapType;
}

vita2d::Texture::Texture(SceUInt32 w, SceUInt32 h, SceGxmTextureFormat format, SceBool isRenderTarget)
{
	SceInt32 ret;

	if (w > GXM_TEX_MAX_SIZE || h > GXM_TEX_MAX_SIZE) {
		sceClibPrintf("[TEX] Texture is too big!");
	}

	texHeapType = heapType;

	const SceUInt32 tex_size = ((w + 7) & ~7) * h * TexFormat2Bytespp(format);

	/* Allocate a GPU buffer for the texture */

	dataMem = graph::SurfacePool::AllocSurfaceMemory(heapType, tex_size, "vita2d::TextureMemory");

	if (dataMem == SCE_NULL) {
		sceClibPrintf("[TEX] graphics::MemoryPool::AllocMemBlock() returned NULL");
	}

	/* Clear the texture */
	if (tex_size < 128 * 1024)
		sce_paf_memset(dataMem, 0, tex_size);
	else
		sceDmacMemset(dataMem, 0, tex_size);

	/* Create the gxm texture */
	sceGxmTextureInitLinear(
		&gxmTex,
		dataMem,
		format,
		w,
		h,
		1);

	if ((format & 0x9f000000U) == SCE_GXM_TEXTURE_BASE_FORMAT_P8) {

		const SceUInt32 pal_size = 256 * sizeof(SceUInt32);

		paletteMem = graph::SurfacePool::AllocSurfaceMemory(heapType, pal_size, "vita2d::TexturePaletteMemory");

		if (paletteMem == SCE_NULL) {
			sceClibPrintf("[TEX] graphics::MemoryPool::AllocMemBlock() returned NULL");
		}

		sce_paf_memset(paletteMem, 0, pal_size);

		sceGxmTextureSetPalette(&gxmTex, paletteMem);
	}
	else {
		paletteMem = SCE_NULL;
	}

	if (isRenderTarget) {

		ret = sceGxmColorSurfaceInit(
			&gxmSfc,
			SCE_GXM_COLOR_FORMAT_A8B8G8R8,
			SCE_GXM_COLOR_SURFACE_LINEAR,
			SCE_GXM_COLOR_SURFACE_SCALE_NONE,
			SCE_GXM_OUTPUT_REGISTER_SIZE_32BIT,
			w,
			h,
			w,
			dataMem
		);

		if (ret < 0) {
			sceClibPrintf("[TEX] sceGxmColorSurfaceInit(): 0x%X", ret);
		}

		// create the depth/stencil surface
		const SceUInt32 alignedWidth = ALIGN(w, SCE_GXM_TILE_SIZEX);
		const SceUInt32 alignedHeight = ALIGN(h, SCE_GXM_TILE_SIZEY);
		SceUInt32 sampleCount = alignedWidth * alignedHeight;
		SceUInt32 depthStrideInSamples = alignedWidth;

		// allocate it
		depthMem = graph::SurfacePool::AllocSurfaceMemory(heapType, 4 * sampleCount, "vita2d::TextureDepthMemory");

		// create the SceGxmDepthStencilSurface structure
		ret = sceGxmDepthStencilSurfaceInit(
			&gxmSfd,
			SCE_GXM_DEPTH_STENCIL_FORMAT_S8D24,
			SCE_GXM_DEPTH_STENCIL_SURFACE_TILED,
			depthStrideInSamples,
			depthMem,
			SCE_NULL);

		if (ret < 0) {
			sceClibPrintf("[TEX] sceGxmDepthStencilSurfaceInit(): 0x%X", ret);
		}

		SceGxmRenderTarget *tgt = SCE_NULL;

		// set up parameters
		SceGxmRenderTargetParams renderTargetParams;
		sceClibMemset(&renderTargetParams, 0, sizeof(SceGxmRenderTargetParams));
		renderTargetParams.flags = 0;
		renderTargetParams.width = w;
		renderTargetParams.height = h;
		renderTargetParams.scenesPerFrame = 1;
		renderTargetParams.multisampleMode = SCE_GXM_MULTISAMPLE_NONE;
		renderTargetParams.multisampleLocations = 0;
		renderTargetParams.driverMemBlock = -1;

		// create the render target
		ret = sceGxmCreateRenderTarget(&renderTargetParams, &tgt);

		gxmRtgt = tgt;

		if (ret < 0) {
			sceClibPrintf("[TEX] sceGxmCreateRenderTarget(): 0x%X", ret);
		}

	}
	else {
		gxmRtgt = SCE_NULL;
		depthMem = SCE_NULL;
	}
}

vita2d::Texture::~Texture()
{
	if (gxmRtgt) {
		sceGxmDestroyRenderTarget(gxmRtgt);
	}
	if (depthMem) {
		graph::SurfacePool::FreeSurfaceMemory(texHeapType, depthMem);
	}
	if (paletteMem) {
		graph::SurfacePool::FreeSurfaceMemory(texHeapType, paletteMem);
	}
	if (dataMem) {
		graph::SurfacePool::FreeSurfaceMemory(texHeapType, dataMem);
	}
}

SceVoid vita2d::Texture::SetFilters(SceGxmTextureFilter min_filter, SceGxmTextureFilter mag_filter)
{
	sceGxmTextureSetMinFilter(&gxmTex, min_filter);
	sceGxmTextureSetMagFilter(&gxmTex, mag_filter);
}

SceVoid vita2d::Texture::Draw(SceFloat x, SceFloat y, SceFloat tex_x, SceFloat tex_y, SceFloat tex_w, SceFloat tex_h, SceFloat x_scale, SceFloat y_scale)
{
	graph::GraphicsContext *gContext = graph::GraphicsContext::GetGraphicsContext();
	Core *core = Core::GetCurrentCore();

	Core::TextureVertex *vertices = (Core::TextureVertex *)core->PoolMemalign(
		4 * sizeof(Core::TextureVertex), // 4 vertices
		sizeof(Core::TextureVertex));

	const SceFloat w = sceGxmTextureGetWidth(&gxmTex);
	const SceFloat h = sceGxmTextureGetHeight(&gxmTex);

	const SceFloat u0 = tex_x / w;
	const SceFloat v0 = tex_y / h;
	const SceFloat u1 = (tex_x + tex_w) / w;
	const SceFloat v1 = (tex_y + tex_h) / h;

	tex_w *= x_scale;
	tex_h *= y_scale;

	vertices[0].x = x;
	vertices[0].y = y;
	vertices[0].z = -0.5f;
	vertices[0].u = u0;
	vertices[0].v = v0;

	vertices[1].x = x + tex_w;
	vertices[1].y = y;
	vertices[1].z = -0.5f;
	vertices[1].u = u1;
	vertices[1].v = v0;

	vertices[2].x = x;
	vertices[2].y = y + tex_h;
	vertices[2].z = -0.5f;
	vertices[2].u = u0;
	vertices[2].v = v1;

	vertices[3].x = x + tex_w;
	vertices[3].y = y + tex_h;
	vertices[3].z = -0.5f;
	vertices[3].u = u1;
	vertices[3].v = v1;

	// Set the texture to the TEXUNIT0
	sceGxmSetFragmentTexture(gContext->gxmContext, 0, &gxmTex);

	sceGxmSetVertexStream(gContext->gxmContext, 0, vertices);
	sceGxmDraw(gContext->gxmContext, SCE_GXM_PRIMITIVE_TRIANGLE_STRIP, SCE_GXM_INDEX_FORMAT_U16, core->GetLinearIndices(), 4);
}

SceVoid vita2d::Texture::Draw(SceFloat x, SceFloat y, SceFloat tex_x, SceFloat tex_y, SceFloat tex_w, SceFloat tex_h, SceFloat x_scale, SceFloat y_scale, SceUInt32 color)
{
	graph::GraphicsContext *gContext = graph::GraphicsContext::GetGraphicsContext();
	Core *core = Core::GetCurrentCore();

	sceGxmSetVertexProgram(gContext->gxmContext, core->textureVertexProgram);
	sceGxmSetFragmentProgram(gContext->gxmContext, core->textureTintFragmentProgram);

	ScePVoid vertex_wvp_buffer;
	sceGxmReserveVertexDefaultUniformBuffer(gContext->gxmContext, &vertex_wvp_buffer);
	sceGxmSetUniformDataF(vertex_wvp_buffer, core->textureWvpParam, 0, 16, (SceFloat *)&core->orthoMatrix);

	ScePVoid texture_tint_color_buffer;
	sceGxmReserveFragmentDefaultUniformBuffer(gContext->gxmContext, &texture_tint_color_buffer);

	SceFloat *tint_color = (SceFloat *)core->PoolMemalign(
		4 * sizeof(SceFloat), // RGBA
		sizeof(SceFloat));

	tint_color[0] = ((color >> 8 * 0) & 0xFF) / 255.0f;
	tint_color[1] = ((color >> 8 * 1) & 0xFF) / 255.0f;
	tint_color[2] = ((color >> 8 * 2) & 0xFF) / 255.0f;
	tint_color[3] = ((color >> 8 * 3) & 0xFF) / 255.0f;

	sceGxmSetUniformDataF(texture_tint_color_buffer, core->textureTintColorParam, 0, 4, tint_color);

	Draw(x, y, tex_x, tex_y, tex_w, tex_h, x_scale, y_scale);
}