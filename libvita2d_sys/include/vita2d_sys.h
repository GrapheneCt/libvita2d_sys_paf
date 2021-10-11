#ifndef VITA2D_H
#define VITA2D_H

#include <stddef.h>
#include <gxm.h>
#include <kernel.h>
#include <font/libpgf.h>
#include <font/libpvf.h>
#include <paf.h>
#include <vectormath.h>
#include <scebase_common/scebase_target.h>

#define VITA2D_SYS_VERSION_INTERNAL 0146

#ifndef VITA2D_SYS_VERSION
#define VITA2D_SYS_VERSION VITA2D_SYS_VERSION_INTERNAL
#endif

//#define VITA2D_SYS_PRX
#undef VITA2D_SYS_PRX

#if _SCE_HOST_COMPILER_SNC && defined(VITA2D_SYS_PRX)
#define PRX_INTERFACE __declspec (dllexport)
#else
#define PRX_INTERFACE
#endif

#define RGBA8(r,g,b,a) ((((a)&0xFF)<<24) | (((b)&0xFF)<<16) | (((g)&0xFF)<<8) | (((r)&0xFF)<<0))

#define VITA2D_SYS_ERROR_NOT_INITIALIZED		-1000
#define VITA2D_SYS_ERROR_ALREADY_INITIALIZED	-1001
#define VITA2D_SYS_ERROR_VERSION_MISMATCH		-1002
#define VITA2D_SYS_ERROR_INVALID_ARGUMENT		-1003
#define VITA2D_SYS_ERROR_INVALID_POINTER		-1004

namespace vita2d {

	class Core
	{
	public:

		class ColorVertex
		{
		public:

			SceFloat x;
			SceFloat y;
			SceFloat z;
			SceUInt32 color;
		};

		class TextureVertex
		{
		public:

			SceFloat x;
			SceFloat y;
			SceFloat z;
			SceFloat u;
			SceFloat v;
		};

		Core(SceUInt32 v2dTempPoolSize = 1 * 1024 * 1024, SceUInt32 width = 960, SceUInt32 height = 544);

		~Core();

		static Core *GetCurrentCore();

		const SceUInt16 *GetLinearIndices();

		ScePVoid PoolMalloc(SceUInt32 size);

		ScePVoid PoolMemalign(SceUInt32 size, SceUInt32 alignment);

		SceUInt32 PoolFreeSpace();

		SceVoid PoolReset();

		SceVoid SetBlendModeAdd(SceBool enable);

		SceInt32 CheckVersion(SceInt32 vita2d_version);

		sce::Vectormath::Simd::Aos::Matrix4 orthoMatrix;
		SceGxmVertexProgram *colorVertexProgram;
		SceGxmFragmentProgram *colorFragmentProgram;
		const SceGxmProgramParameter *colorWvpParam;

	private:

		class FragmentPrograms
		{
		public:

			SceGxmFragmentProgram *blend_mode_normal;
			SceGxmFragmentProgram *blend_mode_add;
		};

		SceVoid SetupShaders();

		SceVoid MakeFragmentPrograms(SceGxmFragmentProgram **out,
			const SceGxmBlendInfo *blend_info, SceGxmMultisampleMode msaa);

		SceVoid FreeFragmentPrograms(SceGxmFragmentProgram *out);

		SceUInt32 tempPoolSize;
		SceGxmMultisampleMode msaa;
		SceUInt32 screenWidth;
		SceUInt32 screenHeight;
		SceGxmShaderPatcherId colorVertexProgramId;
		SceGxmShaderPatcherId colorFragmentProgramId;
		FragmentPrograms fragmentProgramsList;
		ScePVoid linearIndicesMem;
		ScePVoid poolMem;
		SceUInt32 poolIndex;
	};


	class SimpleDraw
	{
	public:

		static SceVoid Pixel(SceFloat x, SceFloat y, SceUInt32 color);

		static SceVoid Line(SceFloat x0, SceFloat y0, SceFloat x1, SceFloat y1, SceUInt32 color);

		static SceVoid Rectangle(SceFloat x, SceFloat y, SceFloat w, SceFloat h, SceUInt32 color);
		
		static SceVoid Circle(SceFloat x, SceFloat y, SceFloat radius, SceUInt32 color);

		static SceVoid Array(SceGxmPrimitiveType mode, const Core::ColorVertex *vertices, SceSize count);
	};

	class Texture
	{
	public:

		Texture(SceUInt32 w, SceUInt32 h, SceGxmTextureFormat format = SCE_GXM_TEXTURE_FORMAT_A8B8G8R8, SceBool isRenderTarget = SCE_FALSE);

		~Texture();

		static SceVoid SetHeapType(paf::graphics::MemoryPool::MemoryType type);

		static paf::graphics::MemoryPool::MemoryType GetHeapType();

		SceVoid SetFilters(SceGxmTextureFilter min_filter, SceGxmTextureFilter mag_filter);

		SceVoid Draw(SceFloat x, SceFloat y, SceFloat tex_x, SceFloat tex_y, SceFloat tex_w, SceFloat tex_h, SceFloat x_scale, SceFloat y_scale, SceUInt32 color);

	private:

		static SceInt32 TexFormat2Bytespp(SceGxmTextureFormat format);

		SceVoid Draw(SceFloat x, SceFloat y, SceFloat tex_x, SceFloat tex_y, SceFloat tex_w, SceFloat tex_h, SceFloat x_scale, SceFloat y_scale);

		ScePVoid dataMem;
		ScePVoid paletteMem;
		ScePVoid depthMem;
		SceGxmTexture gxmTex;
		SceGxmColorSurface gxmSfc;
		SceGxmDepthStencilSurface gxmSfd;
		SceGxmRenderTarget *gxmRtgt;
		graphics::MemoryPool::MemoryType texHeapType;

	};

	class Pvf
	{
	public:

		Pvf(const char *path, SceFloat hSize, SceFloat vSize, ScePvfDataAccessMode accessMode);

		Pvf(ScePVoid buf, SceSize bufSize, SceFloat hSize, SceFloat vSize);

		~Pvf();

		SceInt32 Draw(SceInt32 x, SceInt32 y,
			SceUInt32 color, SceFloat scale,
			const char *text);

		SceInt32 Drawf(SceInt32 x, SceInt32 y,
			SceUInt32 color, SceFloat scale,
			const char *text, ...);

		SceVoid Dimensions(SceFloat scale,
			const char *text, SceInt32 *width, SceInt32 *height);

		SceInt32 Width(SceFloat scale, const char *text);

		SceInt32 Height(SceFloat scale, const char *text);

		SceInt32 IrectMaxHeight();

		SceInt32 IrectMaxWidth();

		SceVoid EmboldenRate(SceFloat em);

		SceVoid SkewRate(SceFloat ax, SceFloat ay);

		SceVoid CharSize(SceFloat hs, SceFloat vs);

		SceVoid Linespace(SceFloat ls);

		SceVoid Charspace(SceFloat cs);

	private:

		SceInt32 Draw(SceBool draw, SceInt32 *height,
			SceInt32 x, SceInt32 y, SceUInt32 color, SceFloat scale,
			const char *text);

		ScePvf_t_libId libHandle;
		ScePvf_t_fontId fontHandle;
		texture_atlas *atlas;
		paf::thread::Mutex *mutex;
		SceFloat vsize;
		SceFloat prLinespace;
		SceFloat prCharspace;
		SceFloat xCorr;
		SceFloat yCorr;
	};
}

#endif
