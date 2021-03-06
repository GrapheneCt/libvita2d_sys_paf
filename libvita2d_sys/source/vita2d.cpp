#include <display.h>
#include <gxm.h>
#include <kernel.h>
#include <kernel/dmacmgr.h>
#include <message_dialog.h>
#include <libsysmodule.h>
#include <appmgr.h>
#include <libdbg.h>
#include <vectormath.h>
#include <paf.h>

#include "vita2d_sys_paf.h"
#include "utils.h"

/* Shader binaries */

#include "shader/compiled/color_v_gxp.h"
#include "shader/compiled/color_f_gxp.h"
#include "shader/compiled/texture_v_gxp.h"
//#include "shader/compiled/texture_f_gxp.h"
#include "shader/compiled/texture_tint_f_gxp.h"

/* Defines */

using namespace paf;
using namespace vita2d;

#define DEFAULT_TEMP_POOL_SIZE		(1 * 1024 * 1024)
#define UINT16_MAX	0xffff

/* Static variables */

static Core *s_currentCore = SCE_NULL;

static const SceGxmProgram *const colorVertexProgramGxp = (const SceGxmProgram*)color_v_gxp;
static const SceGxmProgram *const colorFragmentProgramGxp = (const SceGxmProgram*)color_f_gxp;
static const SceGxmProgram *const textureVertexProgramGxp = (const SceGxmProgram*)texture_v_gxp;
//static const SceGxmProgram *const textureFragmentProgramGxp = (const SceGxmProgram*)texture_f_gxp;
static const SceGxmProgram *const textureTintFragmentProgramGxp = (const SceGxmProgram*)texture_tint_f_gxp;

vita2d::Core::Core(SceUInt32 v2dTempPoolSize, SceUInt32 width, SceUInt32 height)
{
	if (!sceSysmoduleIsLoadedInternal(SCE_SYSMODULE_INTERNAL_PAF)) {

		msaa = SCE_GXM_MULTISAMPLE_NONE;
		tempPoolSize = v2dTempPoolSize;
		screenWidth = width;
		screenHeight = height;
		if (screenWidth == 0 || screenHeight == 0) {
			screenWidth = 960;
			screenHeight = 544;
		}

		poolIndex = 0;

		SetupShaders();
	}
}

vita2d::Core::~Core()
{
	s_currentCore = SCE_NULL;

	graph::GraphicsContext *gContext = graph::GraphicsContext::GetGraphicsContext();

	sceGxmShaderPatcherReleaseVertexProgram(gContext->shaderPatcher->shaderPatcher, colorVertexProgram);

	FreeFragmentPrograms(&fragmentProgramsList.blend_mode_normal);
	FreeFragmentPrograms(&fragmentProgramsList.blend_mode_add);

	graph::SurfacePool::FreeSurfaceMemory(graph::SurfacePool::MemoryType_UserNC, linearIndicesMem);

	// unregister programs and destroy shader patcher
	sceGxmShaderPatcherUnregisterProgram(gContext->shaderPatcher->shaderPatcher, colorFragmentProgramId);
	sceGxmShaderPatcherUnregisterProgram(gContext->shaderPatcher->shaderPatcher, colorVertexProgramId);
	//sceGxmShaderPatcherUnregisterProgram(gContext->shaderPatcher->shaderPatcher, textureFragmentProgramId);
	sceGxmShaderPatcherUnregisterProgram(gContext->shaderPatcher->shaderPatcher, textureTintFragmentProgramId);
	sceGxmShaderPatcherUnregisterProgram(gContext->shaderPatcher->shaderPatcher, textureVertexProgramId);

	graph::SurfacePool::FreeSurfaceMemory(graph::SurfacePool::MemoryType_UserNC, poolMem);
}

SceVoid vita2d::Core::FreeFragmentPrograms(FragmentProgram *out)
{
	SceInt32 err;

	graph::GraphicsContext *gContext = graph::GraphicsContext::GetGraphicsContext();

	err = sceGxmShaderPatcherReleaseFragmentProgram(gContext->shaderPatcher->shaderPatcher, out->color);
	if (err != SCE_OK)
		sceClibPrintf("sceGxmShaderPatcherReleaseFragmentProgram(): 0x%X", err);

	err = sceGxmShaderPatcherReleaseFragmentProgram(gContext->shaderPatcher->shaderPatcher, out->textureTint);
	if (err != SCE_OK)
		sceClibPrintf("sceGxmShaderPatcherReleaseFragmentProgram(): 0x%X", err);
}

SceVoid vita2d::Core::MakeFragmentPrograms(FragmentProgram *out,
	const SceGxmBlendInfo *blend_info, SceGxmMultisampleMode msaa)
{
	SceInt32 err;

	graph::GraphicsContext *gContext = graph::GraphicsContext::GetGraphicsContext();

	err = sceGxmShaderPatcherCreateFragmentProgram(
		gContext->shaderPatcher->shaderPatcher,
		colorFragmentProgramId,
		SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
		msaa,
		blend_info,
		colorVertexProgramGxp,
		&out->color);

	if (err != SCE_OK)
		sceClibPrintf("color sceGxmShaderPatcherCreateFragmentProgram(): 0x%X", err);

	err = sceGxmShaderPatcherCreateFragmentProgram(
		gContext->shaderPatcher->shaderPatcher,
		textureTintFragmentProgramId,
		SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
		msaa,
		blend_info,
		textureVertexProgramGxp,
		&out->textureTint);

	if (err != SCE_OK)
		sceClibPrintf("texture_tint sceGxmShaderPatcherCreateFragmentProgram(): 0x%X", err);
}

SceVoid vita2d::Core::SetupShaders()
{
	SceInt32 err;
	UNUSED(err);

	graph::GraphicsContext *gContext = graph::GraphicsContext::GetGraphicsContext();

	err = sceGxmShaderPatcherRegisterProgram(gContext->shaderPatcher->shaderPatcher, colorVertexProgramGxp, &colorVertexProgramId);
	if (err != SCE_OK) {
		sceClibPrintf("color_v sceGxmShaderPatcherRegisterProgram(): 0x%X", err);
	}

	err = sceGxmShaderPatcherRegisterProgram(gContext->shaderPatcher->shaderPatcher, colorFragmentProgramGxp, &colorFragmentProgramId);
	if (err != SCE_OK) {
		sceClibPrintf("color_f sceGxmShaderPatcherRegisterProgram(): 0x%X", err);
	}

	err = sceGxmShaderPatcherRegisterProgram(gContext->shaderPatcher->shaderPatcher, textureVertexProgramGxp, &textureVertexProgramId);
	if (err != SCE_OK) {
		sceClibPrintf("texture_v sceGxmShaderPatcherRegisterProgram(): 0x%X", err);
	}

	/*err = sceGxmShaderPatcherRegisterProgram(gContext->shaderPatcher->shaderPatcher, textureFragmentProgramGxp, &textureFragmentProgramId);
	if (err != SCE_OK) {
		sceClibPrintf("texture_f sceGxmShaderPatcherRegisterProgram(): 0x%X", err);
	}*/

	err = sceGxmShaderPatcherRegisterProgram(gContext->shaderPatcher->shaderPatcher, textureTintFragmentProgramGxp, &textureTintFragmentProgramId);
	if (err != SCE_OK) {
		sceClibPrintf("texture_tint_f sceGxmShaderPatcherRegisterProgram(): 0x%X", err);
	}

	// Fill SceGxmBlendInfo
	static const SceGxmBlendInfo blend_info = {
		.colorFunc = SCE_GXM_BLEND_FUNC_ADD,
		.alphaFunc = SCE_GXM_BLEND_FUNC_ADD,
		.colorSrc = SCE_GXM_BLEND_FACTOR_SRC_ALPHA,
		.colorDst = SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
		.alphaSrc = SCE_GXM_BLEND_FACTOR_SRC_ALPHA,
		.alphaDst = SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
		.colorMask = SCE_GXM_COLOR_MASK_ALL
	};

	static const SceGxmBlendInfo blend_info_add = {
		.colorFunc = SCE_GXM_BLEND_FUNC_ADD,
		.alphaFunc = SCE_GXM_BLEND_FUNC_ADD,
		.colorSrc = SCE_GXM_BLEND_FACTOR_ONE,
		.colorDst = SCE_GXM_BLEND_FACTOR_ONE,
		.alphaSrc = SCE_GXM_BLEND_FACTOR_ONE,
		.alphaDst = SCE_GXM_BLEND_FACTOR_ONE,
		.colorMask = SCE_GXM_COLOR_MASK_ALL
	};

	linearIndicesMem = graph::SurfacePool::AllocSurfaceMemory(graph::SurfacePool::MemoryType_UserNC, UINT16_MAX * sizeof(SceUInt16), "vita2d::LinearIndices");

	if (linearIndicesMem == SCE_NULL) {
		sceClibPrintf("graphics::MemoryPool::AllocMemBlock() returned NULL", err);
	}

	SceUInt16 *linearIndices16 = (SceUInt16 *)(linearIndicesMem);

	// Range of i must be greater than uint16_t, this doesn't endless-loop
	for (SceUInt32 i = 0; i <= UINT16_MAX; ++i) {
		linearIndices16[i] = i;
	}

	const SceGxmProgramParameter *paramColorPositionAttribute = sceGxmProgramFindParameterByName(colorVertexProgramGxp, "aPosition");
	const SceGxmProgramParameter *paramColorColorAttribute = sceGxmProgramFindParameterByName(colorVertexProgramGxp, "aColor");

	// create color vertex format
	SceGxmVertexAttribute colorVertexAttributes[2];
	SceGxmVertexStream colorVertexStreams[1];
	/* x,y,z: 3 float 32 bits */
	colorVertexAttributes[0].streamIndex = 0;
	colorVertexAttributes[0].offset = 0;
	colorVertexAttributes[0].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
	colorVertexAttributes[0].componentCount = 3; // (x, y, z)
	colorVertexAttributes[0].regIndex = sceGxmProgramParameterGetResourceIndex(paramColorPositionAttribute);
	/* color: 4 unsigned char  = 32 bits */
	colorVertexAttributes[1].streamIndex = 0;
	colorVertexAttributes[1].offset = 12; // (x, y, z) * 4 = 12 bytes
	colorVertexAttributes[1].format = SCE_GXM_ATTRIBUTE_FORMAT_U8N;
	colorVertexAttributes[1].componentCount = 4; // (color)
	colorVertexAttributes[1].regIndex = sceGxmProgramParameterGetResourceIndex(paramColorColorAttribute);
	// 16 bit (short) indices
	colorVertexStreams[0].stride = sizeof(ColorVertex);
	colorVertexStreams[0].indexSource = SCE_GXM_INDEX_SOURCE_INDEX_16BIT;

	// create color shaders
	err = sceGxmShaderPatcherCreateVertexProgram(
		gContext->shaderPatcher->shaderPatcher,
		colorVertexProgramId,
		colorVertexAttributes,
		2,
		colorVertexStreams,
		1,
		&colorVertexProgram);

	if (err != SCE_OK) {
		sceClibPrintf("color sceGxmShaderPatcherCreateVertexProgram(): 0x%X", err);
	}

	const SceGxmProgramParameter *paramTexturePositionAttribute = sceGxmProgramFindParameterByName(textureVertexProgramGxp, "aPosition");
	const SceGxmProgramParameter *paramTextureTexcoordAttribute = sceGxmProgramFindParameterByName(textureVertexProgramGxp, "aTexcoord");

	// create texture vertex format
	SceGxmVertexAttribute textureVertexAttributes[2];
	SceGxmVertexStream textureVertexStreams[1];
	/* x,y,z: 3 float 32 bits */
	textureVertexAttributes[0].streamIndex = 0;
	textureVertexAttributes[0].offset = 0;
	textureVertexAttributes[0].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
	textureVertexAttributes[0].componentCount = 3; // (x, y, z)
	textureVertexAttributes[0].regIndex = sceGxmProgramParameterGetResourceIndex(paramTexturePositionAttribute);
	/* u,v: 2 floats 32 bits */
	textureVertexAttributes[1].streamIndex = 0;
	textureVertexAttributes[1].offset = 12; // (x, y, z) * 4 = 12 bytes
	textureVertexAttributes[1].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
	textureVertexAttributes[1].componentCount = 2; // (u, v)
	textureVertexAttributes[1].regIndex = sceGxmProgramParameterGetResourceIndex(paramTextureTexcoordAttribute);
	// 16 bit (short) indices
	textureVertexStreams[0].stride = sizeof(TextureVertex);
	textureVertexStreams[0].indexSource = SCE_GXM_INDEX_SOURCE_INDEX_16BIT;

	// create texture shaders
	err = sceGxmShaderPatcherCreateVertexProgram(
		gContext->shaderPatcher->shaderPatcher,
		textureVertexProgramId,
		textureVertexAttributes,
		2,
		textureVertexStreams,
		1,
		&textureVertexProgram);

	if (err != SCE_OK) {
		sceClibPrintf("texture sceGxmShaderPatcherCreateVertexProgram(): 0x%X", err);
	}

	// Create variations of the fragment program based on blending mode
	MakeFragmentPrograms(&fragmentProgramsList.blend_mode_normal, &blend_info, msaa);
	MakeFragmentPrograms(&fragmentProgramsList.blend_mode_add, &blend_info_add, msaa);

	// Default to "normal" blending mode (non-additive)
	SetBlendModeAdd(SCE_FALSE);

	// find vertex uniforms by name and cache parameter information
	colorWvpParam = sceGxmProgramFindParameterByName(colorVertexProgramGxp, "wvp");
	textureWvpParam = sceGxmProgramFindParameterByName(textureVertexProgramGxp, "wvp");
	textureTintColorParam = sceGxmProgramFindParameterByName(textureTintFragmentProgramGxp, "uTintColor");

	// Allocate memory for the memory pool
	poolMem = graph::SurfacePool::AllocSurfaceMemory(graph::SurfacePool::MemoryType_UserNC, tempPoolSize, "vita2d::TempPool");

	if (poolMem == SCE_NULL) {
		sceClibPrintf("graphics::MemoryPool::AllocMemBlock() returned NULL", err);
	}

	orthoMatrix = sce::Vectormath::Simd::Aos::Matrix4::orthographic(0.0f, 960.0f, 544.0f, 0.0f, 0.0f, 1.0f);

	orthoMatrix.setElem(3, 1, 1.0f);

	s_currentCore = this;
}

const SceUInt16 *vita2d::Core::GetLinearIndices()
{
	return (SceUInt16 *)linearIndicesMem;
}

ScePVoid vita2d::Core::PoolMalloc(SceUInt32 size)
{
	if ((poolIndex + size) < tempPoolSize) {
		ScePVoid addr = (ScePVoid)((SceUInt32)poolMem + poolIndex);
		poolIndex += size;
		return addr;
	}
	return NULL;
}

ScePVoid vita2d::Core::PoolMemalign(SceUInt32 size, SceUInt32 alignment)
{
	SceUInt32 new_index = (poolIndex + alignment - 1) & ~(alignment - 1);
	if ((new_index + size) < tempPoolSize) {
		ScePVoid addr = (ScePVoid)((SceUInt32)poolMem + poolIndex);
		poolIndex = new_index + size;
		return addr;
	}
	return NULL;
}

SceUInt32 vita2d::Core::PoolFreeSpace()
{
	return tempPoolSize - poolIndex;
}

SceVoid vita2d::Core::PoolReset()
{
	poolIndex = 0;
}

SceVoid vita2d::Core::SetBlendModeAdd(SceBool enable)
{
	FragmentProgram *in = enable ? &fragmentProgramsList.blend_mode_add
		: &fragmentProgramsList.blend_mode_normal;

	colorFragmentProgram = in->color;
	textureFragmentProgram = in->texture;
	textureTintFragmentProgram = in->textureTint;
}

SceInt32 vita2d::Core::CheckVersion(SceInt32 vita2d_version)
{
	if (vita2d_version == VITA2D_SYS_VERSION_INTERNAL)
		return SCE_OK;
	else
		return VITA2D_SYS_ERROR_VERSION_MISMATCH;
}

Core *vita2d::Core::GetCurrentCore()
{
	return s_currentCore;
}

#ifdef VITA2D_SYS_PRX

extern "C" {

	int __module_stop(SceSize argc, const void *args) {
		sceClibPrintf("vita2d_sys module stop\n");
		return SCE_KERNEL_STOP_SUCCESS;
	}

	int __module_exit() {
		sceClibPrintf("vita2d_sys module exit\n");
		return SCE_KERNEL_STOP_SUCCESS;
	}

	int __module_start(SceSize argc, void *args) {
		sceClibPrintf("vita2d_sys module start, ver. %d\n", VITA2D_SYS_VERSION_INTERNAL);
		return SCE_KERNEL_START_SUCCESS;
	}

}

#endif
