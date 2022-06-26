#include <math.h>
#include <sceconst.h>
#include <paf.h>

#include "vita2d_sys_paf.h"

using namespace paf;
using namespace vita2d;

SceVoid vita2d::SimpleDraw::Pixel(SceFloat x, SceFloat y, SceUInt32 color)
{
	Core *core = Core::GetCurrentCore();
	graph::GraphicsContext *gContext = graph::GraphicsContext::GetGraphicsContext();

	Core::ColorVertex *vertex = (Core::ColorVertex *)core->PoolMemalign(
		1 * sizeof(Core::ColorVertex), // 1 vertex
		sizeof(Core::ColorVertex));

	SceUInt16 *index = (SceUInt16 *)core->PoolMemalign(
		1 * sizeof(SceUInt16), // 1 index
		sizeof(SceUInt16));

	vertex->x = x;
	vertex->y = y;
	vertex->z = -0.5f;
	vertex->color = color;

	*index = 0;

	sceGxmSetVertexProgram(gContext->gxmContext, core->colorVertexProgram);
	sceGxmSetFragmentProgram(gContext->gxmContext, core->colorFragmentProgram);

	ScePVoid vertexDefaultBuffer;
	sceGxmReserveVertexDefaultUniformBuffer(gContext->gxmContext, &vertexDefaultBuffer);
	sceGxmSetUniformDataF(vertexDefaultBuffer, core->colorWvpParam, 0, 16, (SceFloat *)&core->orthoMatrix);

	sceGxmSetVertexStream(gContext->gxmContext, 0, vertex);
	sceGxmSetFrontPolygonMode(gContext->gxmContext, SCE_GXM_POLYGON_MODE_POINT);
	sceGxmDraw(gContext->gxmContext, SCE_GXM_PRIMITIVE_POINTS, SCE_GXM_INDEX_FORMAT_U16, index, 1);
	sceGxmSetFrontPolygonMode(gContext->gxmContext, SCE_GXM_POLYGON_MODE_TRIANGLE_FILL);
}

SceVoid vita2d::SimpleDraw::Line(SceFloat x0, SceFloat y0, SceFloat x1, SceFloat y1, SceUInt32 color)
{
	Core *core = Core::GetCurrentCore();
	graph::GraphicsContext *gContext = graph::GraphicsContext::GetGraphicsContext();

	Core::ColorVertex *vertices = (Core::ColorVertex *)core->PoolMemalign(
		2 * sizeof(Core::ColorVertex), // 2 vertices
		sizeof(Core::ColorVertex));

	vertices[0].x = x0;
	vertices[0].y = y0;
	vertices[0].z = -0.5f;
	vertices[0].color = color;

	vertices[1].x = x1;
	vertices[1].y = y1;
	vertices[1].z = -0.5f;
	vertices[1].color = color;

	sceGxmSetVertexProgram(gContext->gxmContext, core->colorVertexProgram);
	sceGxmSetFragmentProgram(gContext->gxmContext, core->colorFragmentProgram);

	ScePVoid vertexDefaultBuffer;
	sceGxmReserveVertexDefaultUniformBuffer(gContext->gxmContext, &vertexDefaultBuffer);
	sceGxmSetUniformDataF(vertexDefaultBuffer, core->colorWvpParam, 0, 16, (SceFloat *)&core->orthoMatrix);

	sceGxmSetVertexStream(gContext->gxmContext, 0, vertices);
	sceGxmSetFrontPolygonMode(gContext->gxmContext, SCE_GXM_POLYGON_MODE_LINE);
	sceGxmDraw(gContext->gxmContext, SCE_GXM_PRIMITIVE_LINES, SCE_GXM_INDEX_FORMAT_U16, core->GetLinearIndices(), 2);
	sceGxmSetFrontPolygonMode(gContext->gxmContext, SCE_GXM_POLYGON_MODE_TRIANGLE_FILL);
}

SceVoid vita2d::SimpleDraw::Rectangle(SceFloat x, SceFloat y, SceFloat w, SceFloat h, SceUInt32 color)
{
	Core *core = Core::GetCurrentCore();
	graph::GraphicsContext *gContext = graph::GraphicsContext::GetGraphicsContext();

	Core::ColorVertex *vertices = (Core::ColorVertex *)core->PoolMemalign(
		4 * sizeof(Core::ColorVertex), // 4 vertices
		sizeof(Core::ColorVertex));

	vertices[0].x = x;
	vertices[0].y = y;
	vertices[0].z = -0.5f;
	vertices[0].color = color;

	vertices[1].x = x + w;
	vertices[1].y = y;
	vertices[1].z = -0.5f;
	vertices[1].color = color;

	vertices[2].x = x;
	vertices[2].y = y + h;
	vertices[2].z = -0.5f;
	vertices[2].color = color;

	vertices[3].x = x + w;
	vertices[3].y = y + h;
	vertices[3].z = -0.5f;
	vertices[3].color = color;

	sceGxmSetVertexProgram(gContext->gxmContext, core->colorVertexProgram);
	sceGxmSetFragmentProgram(gContext->gxmContext, core->colorFragmentProgram);

	ScePVoid vertexDefaultBuffer;
	sceGxmReserveVertexDefaultUniformBuffer(gContext->gxmContext, &vertexDefaultBuffer);
	sceGxmSetUniformDataF(vertexDefaultBuffer, core->colorWvpParam, 0, 16, (SceFloat *)&core->orthoMatrix);

	sceGxmSetVertexStream(gContext->gxmContext, 0, vertices);
	sceGxmDraw(gContext->gxmContext, SCE_GXM_PRIMITIVE_TRIANGLE_STRIP, SCE_GXM_INDEX_FORMAT_U16, core->GetLinearIndices(), 4);
}

SceVoid vita2d::SimpleDraw::Circle(SceFloat x, SceFloat y, SceFloat radius, SceUInt32 color)
{
	Core *core = Core::GetCurrentCore();
	graph::GraphicsContext *gContext = graph::GraphicsContext::GetGraphicsContext();

	static const SceInt32 num_segments = 100;

	Core::ColorVertex *vertices = (Core::ColorVertex *)core->PoolMemalign(
		(num_segments + 1) * sizeof(Core::ColorVertex),
		sizeof(Core::ColorVertex));

	SceUInt16 *indices = (SceUInt16 *)core->PoolMemalign(
		(num_segments + 2) * sizeof(SceUInt16),
		sizeof(SceUInt16));

	vertices[0].x = x;
	vertices[0].y = y;
	vertices[0].z = -0.5f;
	vertices[0].color = color;
	indices[0] = 0;

	SceFloat theta = 2 * SCE_MATH_PI / (float)num_segments;
	SceFloat c = sce_paf_cosf(theta);
	SceFloat s = sce_paf_sinf(theta);
	SceFloat t;

	SceFloat xx = radius;
	SceFloat yy = 0;
	SceInt32 i;

	for (i = 1; i <= num_segments; i++) {
		vertices[i].x = x + xx;
		vertices[i].y = y + yy;
		vertices[i].z = -0.5f;
		vertices[i].color = color;
		indices[i] = i;

		t = xx;
		xx = c * xx - s * yy;
		yy = s * t + c * yy;
	}

	indices[num_segments + 1] = 1;

	sceGxmSetVertexProgram(gContext->gxmContext, core->colorVertexProgram);
	sceGxmSetFragmentProgram(gContext->gxmContext, core->colorFragmentProgram);

	void *vertexDefaultBuffer;
	sceGxmReserveVertexDefaultUniformBuffer(gContext->gxmContext, &vertexDefaultBuffer);
	sceGxmSetUniformDataF(vertexDefaultBuffer, core->colorWvpParam, 0, 16, (SceFloat *)&core->orthoMatrix);

	sceGxmSetVertexStream(gContext->gxmContext, 0, vertices);
	sceGxmDraw(gContext->gxmContext, SCE_GXM_PRIMITIVE_TRIANGLE_FAN, SCE_GXM_INDEX_FORMAT_U16, indices, num_segments + 2);
}

SceVoid vita2d::SimpleDraw::Array(SceGxmPrimitiveType mode, const Core::ColorVertex *vertices, SceSize count)
{
	Core *core = Core::GetCurrentCore();
	graph::GraphicsContext *gContext = graph::GraphicsContext::GetGraphicsContext();

	sceGxmSetVertexProgram(gContext->gxmContext, core->colorVertexProgram);
	sceGxmSetFragmentProgram(gContext->gxmContext, core->colorFragmentProgram);

	void *vertexDefaultBuffer;
	sceGxmReserveVertexDefaultUniformBuffer(gContext->gxmContext, &vertexDefaultBuffer);
	sceGxmSetUniformDataF(vertexDefaultBuffer, core->colorWvpParam, 0, 16, (SceFloat *)&core->orthoMatrix);

	sceGxmSetBackPolygonMode(gContext->gxmContext, SCE_GXM_POLYGON_MODE_TRIANGLE_FILL);

	sceGxmSetVertexStream(gContext->gxmContext, 0, vertices);
	sceGxmDraw(gContext->gxmContext, mode, SCE_GXM_INDEX_FORMAT_U16, core->GetLinearIndices(), count);
}
