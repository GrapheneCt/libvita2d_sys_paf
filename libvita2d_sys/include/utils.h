#ifndef UTILS_H
#define UTILS_H

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

#endif
