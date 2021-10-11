#include <math.h>
#include <kernel.h>
#include <libdbg.h>
#include <fios2.h>
#include <appmgr.h>
#include <paf.h>

#include "utils.h"

void vita2d::utils::Matrix::Copy(SceFloat *dst, const SceFloat *src)
{
	sce_paf_memcpy(dst, src, sizeof(SceFloat)*4*4);
}

void vita2d::utils::Matrix::Identity4x4(SceFloat *m)
{
	m[0] = m[5] = m[10] = m[15] = 1.0f;
	m[1] = m[2] = m[3] = 0.0f;
	m[4] = m[6] = m[7] = 0.0f;
	m[8] = m[9] = m[11] = 0.0f;
	m[12] = m[13] = m[14] = 0.0f;
}

void vita2d::utils::Matrix::Mult4x4(const SceFloat *src1, const SceFloat *src2, SceFloat *dst)
{
	int i, j, k;
	for (i = 0; i < 4; i++) {
		for (j = 0; j < 4; j++) {
			dst[i*4 + j] = 0.0f;
			for (k = 0; k < 4; k++) {
				dst[i*4 + j] += src1[i*4 + k]*src2[k*4 + j];
			}
		}
	}
}

void vita2d::utils::Matrix::SetXRotation(SceFloat *m, SceFloat rad)
{
	SceFloat c = sce_paf_cosf(rad);
	SceFloat s = sce_paf_sinf(rad);

	Identity4x4(m);

	m[0] = c;
	m[2] = -s;
	m[8] = s;
	m[10] = c;
}

void vita2d::utils::Matrix::SetYRotation(SceFloat *m, SceFloat rad)
{
	SceFloat c = sce_paf_cosf(rad);
	SceFloat s = sce_paf_sinf(rad);

	Identity4x4(m);

	m[5] = c;
	m[6] = s;
	m[9] = -s;
	m[10] = c;
}

void vita2d::utils::Matrix::SetZRotation(SceFloat *m, SceFloat rad)
{
	SceFloat c = sce_paf_cosf(rad);
	SceFloat s = sce_paf_sinf(rad);

	Identity4x4(m);

	m[0] = c;
	m[1] = s;
	m[4] = -s;
	m[5] = c;
}

void vita2d::utils::Matrix::RotateX(SceFloat *m, SceFloat rad)
{
	SceFloat mr[4*4], mt[4*4];
	SetYRotation(mr, rad);
	Mult4x4(m, mr, mt);
	Copy(m, mt);
}


void vita2d::utils::Matrix::RotateY(SceFloat *m, SceFloat rad)
{
	SceFloat mr[4*4], mt[4*4];
	SetXRotation(mr, rad);
	Mult4x4(m, mr, mt);
	Copy(m, mt);
}

void vita2d::utils::Matrix::RotateZ(SceFloat *m, SceFloat rad)
{
	SceFloat mr[4*4], mt[4*4];
	SetZRotation(mr, rad);
	Mult4x4(m, mr, mt);
	Copy(m, mt);
}

void vita2d::utils::Matrix::SetXYZTranslation(SceFloat *m, SceFloat x, SceFloat y, SceFloat z)
{
	Identity4x4(m);

	m[12] = x;
	m[13] = y;
	m[14] = z;
}

void vita2d::utils::Matrix::TranslateXYZ(SceFloat *m, SceFloat x, SceFloat y, SceFloat z)
{
	SceFloat mr[4*4], mt[4*4];
	SetXYZTranslation(mr, x, y, z);
	Mult4x4(m, mr, mt);
	Copy(m, mt);
}

void vita2d::utils::Matrix::SetScaling(SceFloat *m, SceFloat x_scale, SceFloat y_scale, SceFloat z_scale)
{
	Identity4x4(m);
	m[0] = x_scale;
	m[5] = y_scale;
	m[10] = z_scale;
}

void vita2d::utils::Matrix::SwapXY(SceFloat *m)
{
	SceFloat ms[4*4], mt[4*4];
	Identity4x4(ms);

	ms[0] = 0.0f;
	ms[1] = 1.0f;
	ms[4] = 1.0f;
	ms[5] = 0.0f;

	Mult4x4(m, ms, mt);
	Copy(m, mt);
}

void vita2d::utils::Matrix::InitOrthographic(SceFloat *m, SceFloat left, SceFloat right, SceFloat bottom, SceFloat top, SceFloat near, SceFloat far)
{
	m[0x0] = 2.0f/(right-left);
	m[0x4] = 0.0f;
	m[0x8] = 0.0f;
	m[0xC] = -(right+left)/(right-left);

	m[0x1] = 0.0f;
	m[0x5] = 2.0f/(top-bottom);
	m[0x9] = 0.0f;
	m[0xD] = -(top+bottom)/(top-bottom);

	m[0x2] = 0.0f;
	m[0x6] = 0.0f;
	m[0xA] = -2.0f/(far-near);
	m[0xE] = (far+near)/(far-near);

	m[0x3] = 0.0f;
	m[0x7] = 0.0f;
	m[0xB] = 0.0f;
	m[0xF] = 1.0f;
}

void vita2d::utils::Matrix::InitFrustum(SceFloat *m, SceFloat left, SceFloat right, SceFloat bottom, SceFloat top, SceFloat near, SceFloat far)
{
	m[0x0] = (2.0f*near)/(right-left);
	m[0x4] = 0.0f;
	m[0x8] = (right+left)/(right-left);
	m[0xC] = 0.0f;

	m[0x1] = 0.0f;
	m[0x5] = (2.0f*near)/(top-bottom);
	m[0x9] = (top+bottom)/(top-bottom);
	m[0xD] = 0.0f;

	m[0x2] = 0.0f;
	m[0x6] = 0.0f;
	m[0xA] = -(far+near)/(far-near);
	m[0xE] = (-2.0f*far*near)/(far-near);

	m[0x3] = 0.0f;
	m[0x7] = 0.0f;
	m[0xB] = -1.0f;
	m[0xF] = 0.0f;
}

void vita2d::utils::Matrix::InitPerspective(SceFloat *m, SceFloat fov, SceFloat aspect, SceFloat near, SceFloat far)
{
	SceFloat half_height = near * sce_paf_tanf(DEG_TO_RAD(fov) * 0.5f);
	SceFloat half_width = half_height * aspect;

	InitFrustum(m, -half_width, half_width, -half_height, half_height, near, far);
}