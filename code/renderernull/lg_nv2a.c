/* LG 18z8m-c: textura NV2A + quad texturizado (xgu/pbkit, shaders do ioQ3) */
#include <stdint.h>
#include <string.h>
#include <windows.h>
#include <xboxkrnl/xboxkrnl.h>
#include <pbkit/pbkit.h>
#include "xgu/xgu.h"
#include "xgu/xgux.h"

typedef struct { float pos[3]; uint8_t col[4]; float tc0[2]; float tc1[2]; } lgxVert;
typedef struct { uint32_t *mem; int w, h, lw, lh; } lgxTex;
#define LGX_MAXVERT 4096
#define LGX_MAXTEX 64
#define LGX_RAM 0x03FFAFFF
#define LGX_UB_OGL ((XguVertexArrayType)NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_UB_OGL)

static lgxTex lgx_tex[LGX_MAXTEX];
static int lgx_ntex, lgx_ok, lgx_white = -1;
static lgxVert *lgx_vb;
static unsigned int lgx_nv;

static int lgx_log2(int v) { int l = 0; while ((1 << l) < v) l++; return l; }
static void *lgx_cmem(size_t n) { return MmAllocateContiguousMemoryEx(n, 0, LGX_RAM, 0, PAGE_WRITECOMBINE | PAGE_READWRITE); }

/* Swizzle NV2A: bits de x e y intercalados, x no bit mais baixo. */
static uint32_t lgx_swz(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
uint32_t off = 0, bit = 1, mx = 1, my = 1;
while (mx < w || my < h) {
if (mx < w) { if (x & mx) off |= bit; bit <<= 1; mx <<= 1; }
if (my < h) { if (y & my) off |= bit; bit <<= 1; my <<= 1; }
}
return off;
}

/* argb linear (linha 0 = topo) -> textura swizzled; so potencias de 2. Retorna id ou -1. */
int lgx_upload(const uint32_t *argb, int w, int h)
{
lgxTex *t; int x, y;
if (lgx_ntex >= LGX_MAXTEX || w <= 0 || h <= 0 || (w & (w - 1)) || (h & (h - 1))) return -1;
t = &lgx_tex[lgx_ntex];
t->mem = (uint32_t *)lgx_cmem((size_t)w * h * 4);
if (!t->mem) return -1;
for (y = 0; y < h; y++)
for (x = 0; x < w; x++)
t->mem[lgx_swz(x, y, w, h)] = argb[y * w + x];
t->w = w; t->h = h; t->lw = lgx_log2(w); t->lh = lgx_log2(h);
return lgx_ntex++;
}

int lgx_init(void)
{
static const XguTransformProgramInstruction vp[] = {
#include "xbox_nv2a_vp.inl"
};
uint32_t white = 0xFFFFFFFFu, *p;
unsigned int i;
if (lgx_ok) return 1;
p = pb_begin();
p = xgu_set_transform_program_start(p, 0);
p = xgu_set_transform_execution_mode(p, XGU_PROGRAM, XGU_RANGE_MODE_PRIVATE);
p = xgu_set_transform_program_cxt_write_enable(p, false);
p = xgu_set_transform_program_load(p, 0);
for (i = 0; i < sizeof(vp) / sizeof(vp[0]); ++i) {
p = push_command(p, NV097_SET_TRANSFORM_PROGRAM, 4);
p = push_parameters(p, &vp[i].i[0], 4);
}
pb_end(p);
p = pb_begin();
#include "xbox_nv2a_fp.inl"
pb_end(p);
lgx_vb = (lgxVert *)lgx_cmem(LGX_MAXVERT * sizeof(lgxVert));
if (!lgx_vb) return 0;
lgx_white = lgx_upload(&white, 1, 1);
lgx_ok = lgx_white >= 0;
return lgx_ok;
}

/* Estado 2D por frame: chamar depois do target/clear do BeginFrame. */
void lgx_frame(void)
{
XguVec4 m[4];
const lgxVert *v = lgx_vb;
uint32_t *p;
int i;
if (!lgx_ok) return;
lgx_nv = 0;
memset(m, 0, sizeof(m));
for (i = 0; i < 4; ++i) m[i].f[i] = 1.0f;
p = pb_begin();
p = xgu_set_front_face(p, XGU_FRONT_CCW);
p = xgu_set_cull_face_enable(p, false);
p = xgu_set_depth_test_enable(p, false);
p = xgu_set_depth_mask(p, false);
p = xgu_set_stencil_test_enable(p, false);
p = xgu_set_alpha_test_enable(p, false);
p = xgu_set_blend_enable(p, true);
p = xgu_set_blend_func_sfactor(p, XGU_FACTOR_SRC_ALPHA);
p = xgu_set_blend_func_dfactor(p, XGU_FACTOR_ONE_MINUS_SRC_ALPHA);
p = xgu_set_transform_constant_load(p, 96);
p = xgu_set_transform_constant(p, m, 4);
pb_end(p);
for (i = 0; i < XGU_ATTRIBUTE_COUNT; ++i)
xgux_set_attrib_pointer((XguVertexArray)i, XGU_FLOAT, 0, 0, NULL);
xgux_set_attrib_pointer(XGU_VERTEX_ARRAY, XGU_FLOAT, 3, sizeof(*v), &v[0].pos[0]);
xgux_set_attrib_pointer(XGU_COLOR_ARRAY, LGX_UB_OGL, 4, sizeof(*v), &v[0].col[0]);
xgux_set_attrib_pointer(XGU_TEXCOORD0_ARRAY, XGU_FLOAT, 2, sizeof(*v), &v[0].tc0[0]);
xgux_set_attrib_pointer(XGU_TEXCOORD1_ARRAY, XGU_FLOAT, 2, sizeof(*v), &v[0].tc1[0]);
}

static uint32_t *lgx_bind(uint32_t *p, unsigned int unit, int id)
{
const lgxTex *t = &lgx_tex[id];
p = xgu_set_texture_offset(p, unit, (void *)((uint32_t)t->mem & 0x03ffffff));
p = xgu_set_texture_format(p, unit, 2, false, XGU_SOURCE_COLOR, 2,
XGU_TEXTURE_FORMAT_A8R8G8B8_SWIZZLED, 1, (uint8_t)t->lw, (uint8_t)t->lh, 0);
p = xgu_set_texture_address(p, unit, XGU_WRAP, false, XGU_WRAP, false, XGU_WRAP, false, false);
p = xgu_set_texture_control0(p, unit, true, 0, 4095);
p = xgu_set_texture_filter(p, unit, 0, XGU_TEXTURE_CONVOLUTION_QUINCUNX, 2, 2,
false, false, false, false);
return p;
}

static void lgx_vtx(lgxVert *v, float x, float y, float s, float t, const uint8_t *c)
{
v->pos[0] = x; v->pos[1] = y; v->pos[2] = 0.0f;
v->col[0] = c[0]; v->col[1] = c[1]; v->col[2] = c[2]; v->col[3] = c[3];
v->tc0[0] = s; v->tc0[1] = t; v->tc1[0] = 0.0f; v->tc1[1] = 0.0f;
}

/* Quad texturizado em pixels de tela; rgba = SetColor do jogo. */
int lgx_quad(int id, float x, float y, float w, float h,
float s1, float t1, float s2, float t2, const uint8_t *rgba)
{
lgxVert *v;
uint32_t *p;
if (!lgx_ok || id < 0 || id >= lgx_ntex || lgx_nv + 6 > LGX_MAXVERT) return 0;
v = lgx_vb + lgx_nv;
lgx_vtx(&v[0], x, y, s1, t1, rgba);
lgx_vtx(&v[1], x + w, y, s2, t1, rgba);
lgx_vtx(&v[2], x + w, y + h, s2, t2, rgba);
lgx_vtx(&v[3], x, y, s1, t1, rgba);
lgx_vtx(&v[4], x + w, y + h, s2, t2, rgba);
lgx_vtx(&v[5], x, y + h, s1, t2, rgba);
p = pb_begin();
p = lgx_bind(p, 0, id);
p = lgx_bind(p, 1, lgx_white);
p = push_command_parameter(p, NV097_BREAK_VERTEX_BUFFER_CACHE, 0);
pb_end(p);
xgux_draw_arrays(XGU_TRIANGLES, lgx_nv, 6);
lgx_nv += 6;
return 1;
}