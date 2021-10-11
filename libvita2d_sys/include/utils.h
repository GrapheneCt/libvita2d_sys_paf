#ifndef UTILS_H
#define UTILS_H

#include <gxm.h>
#include <kernel.h>
#include <fios2.h>

#define GXM_TEX_MAX_SIZE 4096

/* Misc utils */
#define ALIGN(x, a)	(((x) + ((a) - 1)) & ~((a) - 1))
#define	UNUSED(a)	(void)(a)
#define SCREEN_DPI	220

/* Math utils */

#define _PI_OVER_180 0.0174532925199432957692369076849f
#define _180_OVER_PI 57.2957795130823208767981548141f

#define DEG_TO_RAD(x) (x * _PI_OVER_180)
#define RAD_TO_DEG(x) (x * _180_OVER_PI)

namespace vita2d {
	namespace utils {

		class Matrix
		{
		public:

			static void Copy(SceFloat *dst, const SceFloat *src);
			static void Identity4x4(SceFloat *m);
			static void Mult4x4(const SceFloat *src1, const SceFloat *src2, SceFloat *dst);
			static void SetXRotation(SceFloat *m, SceFloat rad);
			static void SetYRotation(SceFloat *m, SceFloat rad);
			static void SetZRotation(SceFloat *m, SceFloat rad);
			static void RotateX(SceFloat *m, SceFloat rad);
			static void RotateY(SceFloat *m, SceFloat rad);
			static void RotateZ(SceFloat *m, SceFloat rad);
			static void SetXYZTranslation(SceFloat *m, SceFloat x, SceFloat y, SceFloat z);
			static void TranslateXYZ(SceFloat *m, SceFloat x, SceFloat y, SceFloat z);
			static void SetScaling(SceFloat *m, SceFloat x_scale, SceFloat y_scale, SceFloat z_scale);
			static void SwapXY(SceFloat *m);
			static void InitOrthographic(SceFloat *m, SceFloat left, SceFloat right, SceFloat bottom, SceFloat top, SceFloat near, SceFloat far);
			static void InitFrustum(SceFloat *m, SceFloat left, SceFloat right, SceFloat bottom, SceFloat top, SceFloat near, SceFloat far);
			static void InitPerspective(SceFloat *m, SceFloat fov, SceFloat aspect, SceFloat near, SceFloat far);
		};


	}
}

#endif
