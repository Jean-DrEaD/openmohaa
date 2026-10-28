/* LG lote 18e: renderer nulo (tela preta) para validar o build cliente no nxdk. */
#include "../qcommon/q_shared.h"
#include "../renderercommon/tr_public.h"
#include <string.h>

/* LG 18m: fonte dummy nao-NULL (UI faz ERR_DROP se LoadFont retorna NULL) */
static fontheader_t lg_null_fonts[16];
static int lg_null_nfonts = 0;
static fontheader_sgl_t lg_null_sgl[16]; /* LG 18v */
static fontheader_t *lg_null_LoadFont(const char *name) {
    int i;
    for (i = 0; i < lg_null_nfonts; i++) { if (!Q_stricmp(lg_null_fonts[i].name, name)) return &lg_null_fonts[i]; }
    if (lg_null_nfonts >= 16) return &lg_null_fonts[0];
    Q_strncpyz(lg_null_fonts[lg_null_nfonts].name, name, sizeof(lg_null_fonts[0].name));
    { /* LG 18v */
        fontheader_t *f = &lg_null_fonts[lg_null_nfonts];
        fontheader_sgl_t *s = &lg_null_sgl[lg_null_nfonts];
        int c;
        memset(s, 0, sizeof(*s));
        for (c = 0; c < 256; c++) { s->indirection[c] = c; s->locations[c].size[0] = 8.0f / 256.0f; }
        s->height = 16.0f; s->aspectRatio = 1.0f;
        Q_strncpyz(s->name, name, sizeof(s->name));
        f->numPages = 0; f->sgl[0] = s;
    }
    return &lg_null_fonts[lg_null_nfonts++];
}
static refexport_t lgNullRe;
static refimport_t lgNullRi;

#define LG_SET(field, fn) do { void *lgp_ = (void *)(fn); memcpy(&lgNullRe.field, &lgp_, sizeof(lgp_)); } while (0)

/* LG 18z8c: 2D minimo via pbkit (CPU fill no back buffer; cor por shader) */
#ifdef NXDK
#include <pbkit/pbkit.h>
static int lg2d_ok = 0, lg2d_in = 0, lg2d_w = 640, lg2d_h = 480;
static unsigned int lg2d_frame = 0;
static float lg2d_rgba[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
static refimport_t *lg2d_ri = 0;
/* LG 18z8d: contadores por frame + tabela de shaders */
/* LG 18z8e: tempos (ms) */
static DWORD lg2d_msvbl = 0, lg2d_msfin = 0, lg2d_msgame = 0, lg2d_tend = 0; static unsigned int lg2d_nto = 0;
static unsigned int lg2d_nfill = 0, lg2d_ndraw = 0, lg2d_nalpha = 0, lg2d_nsh0 = 0, lg2d_ncol = 0;
static unsigned int lg2d_fps = 0, lg2d_fpsf = 0; static DWORD lg2d_fpst = 0;
static char lg2d_shn[512][64]; static int lg2d_nsh = 0;
/* LG 18z8k: probe + decode TGA (CPU) no registro de shader */
static refimport_t lg_ri; static int lg_ri_ok = 0;
static unsigned int lg_tga_ok = 0, lg_tga_fail = 0;
static char lg_tga_last[48] = "-"; static int lg_tga_w = 0, lg_tga_h = 0, lg_tga_t = 0, lg_tga_bpp = 0;
static char lg_sh_st[512];
static unsigned int *lg_img[512]; static int lg_iw[512], lg_ih[512]; static unsigned int lg_ndec = 0;
static unsigned int lg2d_px(const unsigned char *p, int bp) {
    unsigned int a = (bp == 4) ? p[3] : 255;
    return (a << 24) | ((unsigned int)p[2] << 16) | ((unsigned int)p[1] << 8) | p[0];
}
static void lg2d_decode(int idx, const unsigned char *b, long len) {
    int t = b[2], w = b[12] | (b[13] << 8), h = b[14] | (b[15] << 8), bp = b[16] / 8;
    int top = (b[17] & 0x20) != 0, i = 0, n, y, x;
    const unsigned char *p = b + 18 + b[0], *end = b + len; unsigned int *o;
    if ((t != 2 && t != 10) || (bp != 3 && bp != 4) || w <= 0 || h <= 0 || w > 256 || h > 256 || idx < 0 || idx >= 512) return;
    n = w * h; o = (unsigned int *)malloc((size_t)n * 4); if (!o) return;
    while (i < n && p < end) {
        int cnt = 1, rle = 0;
        if (t == 10) { cnt = (*p & 0x7f) + 1; rle = *p & 0x80; p++; }
        if (rle) { unsigned int px; if (p + bp > end) break; px = lg2d_px(p, bp); p += bp; while (cnt-- && i < n) o[i++] = px; }
        else { while (cnt-- && i < n) { if (p + bp > end) { i = n; break; } o[i++] = lg2d_px(p, bp); p += bp; } }
    }
    if (!top) for (y = 0; y < h / 2; y++) for (x = 0; x < w; x++) {
        unsigned int tmp = o[y * w + x]; o[y * w + x] = o[(h - 1 - y) * w + x]; o[(h - 1 - y) * w + x] = tmp; }
    lg_img[idx] = o; lg_iw[idx] = w; lg_ih[idx] = h; lg_ndec++;
}
static long lg2d_try(const char *base, const char *ext, void **buf) {
    char fn[80];
    Q_strncpyz(fn, base, sizeof(fn)); Q_strcat(fn, sizeof(fn), ext);
    *buf = 0; return (long)lg_ri.FS_ReadFile(fn, buf);
}
static char lg2d_probe(const char *name, int idx) {
    char base[80]; void *buf = 0; long len; const unsigned char *b; size_t n;
    if (!lg_ri_ok || !lg_ri.FS_ReadFile || !name) return '?';
    Q_strncpyz(base, name, sizeof(base)); n = strlen(base);
    if (n >= 4 && base[n - 4] == '.') base[n - 4] = 0;
    Q_strncpyz(lg_tga_last, name, sizeof(lg_tga_last));
    len = lg2d_try(base, ".tga", &buf);
    if (len >= 18 && buf) {
        b = (const unsigned char *)buf; lg_tga_ok++;
        lg_tga_t = b[2]; lg_tga_w = b[12] | (b[13] << 8); lg_tga_h = b[14] | (b[15] << 8); lg_tga_bpp = b[16];
        lg2d_decode(idx, b, len);
        lg_ri.FS_FreeFile(buf); return 'T';
    }
    if (buf) lg_ri.FS_FreeFile(buf);
    len = lg2d_try(base, ".jpg", &buf);
    if (len > 0 && buf) { lg_tga_ok++; lg_ri.FS_FreeFile(buf); return 'J'; }
    if (buf) lg_ri.FS_FreeFile(buf);
    lg_tga_fail++; return '-';
}
static void lg2d_dbgblit(int nsh) {
    unsigned int *fb = (unsigned int *)pb_back_buffer(); int k, x, y, x0 = 632, y0 = 400;
    if (!fb) return;
    for (k = 0; k < nsh && k < 512; k++) {
        if (!lg_img[k]) continue;
        x0 -= lg_iw[k]; if (x0 < 0) break;
        for (y = 0; y < lg_ih[k] && y0 + y < 480; y++) for (x = 0; x < lg_iw[k]; x++) {
            unsigned int px = lg_img[k][y * lg_iw[k] + x];
            if ((px >> 24) >= 0x80) fb[(y0 + y) * 640 + x0 + x] = px | 0xFF000000u;
        }
        x0 -= 8;
    }
}
static qhandle_t LG2D_RegisterShader(const char *name) {
    int i;
    if (!name || !name[0]) return 0;
    for (i = 0; i < lg2d_nsh; i++) if (!Q_stricmp(lg2d_shn[i], name)) return (qhandle_t)(i + 1);
    if (lg2d_nsh >= 512) return 1;
    Q_strncpyz(lg2d_shn[lg2d_nsh], name, 64); lg_sh_st[lg2d_nsh] = lg2d_probe(name, (int)lg2d_nsh);
    return (qhandle_t)(++lg2d_nsh);
}
static void lg2d_init(void) {
    if (lg2d_ok) return;
    if (pb_init() != 0) { if (lg2d_ri) lg2d_ri->Printf(PRINT_ALL, "LG: pb_init falhou\n"); return; }
    pb_show_front_screen();
    lg2d_w = (int)pb_back_buffer_width(); lg2d_h = (int)pb_back_buffer_height();
    if (lg2d_w <= 0 || lg2d_h <= 0) { lg2d_w = 640; lg2d_h = 480; }
    lg2d_ok = 1;
}
static float lg2d_c01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
static DWORD lg2d_mk(int shader) {
    float r = 1.0f, g = 1.0f, b = 1.0f, a = lg2d_c01(lg2d_rgba[3]);
    if (shader <= 0) lg2d_nsh0++;
    if (shader > 0) {
        unsigned int h = (unsigned int)shader * 2654435761u;
        r = 0.3f + (float)((h >> 8) & 0xFF) / 365.0f;
        g = 0.3f + (float)((h >> 16) & 0xFF) / 365.0f;
        b = 0.3f + (float)((h >> 24) & 0xFF) / 365.0f;
    }
    r = lg2d_c01(r * lg2d_rgba[0]); g = lg2d_c01(g * lg2d_rgba[1]); b = lg2d_c01(b * lg2d_rgba[2]);
    return ((DWORD)(a * 255.0f) << 24) | ((DWORD)(r * 255.0f) << 16) | ((DWORD)(g * 255.0f) << 8) | (DWORD)(b * 255.0f);
}
/* LG 18z8g: clear por GPU (cor, retangulo) */
extern uint32_t *pb_begin(void);
extern void pb_end(uint32_t *pEnd);
static void lg2d_gpufill(int x0, int y0, int x1, int y1, DWORD col) {
    uint32_t *p = pb_begin();
    *(p++) = (2u << 18) | (0u << 13) | 0x1D98u; *(p++) = ((uint32_t)(x1 - 1) << 16) | (uint32_t)x0; *(p++) = ((uint32_t)(y1 - 1) << 16) | (uint32_t)y0;
    *(p++) = (3u << 18) | (0u << 13) | 0x1D8Cu; *(p++) = 0xffffff00u; *(p++) = (uint32_t)col; *(p++) = 0xF0u;
    pb_end(p);
}
static void lg2d_fill(float x, float y, float w, float h, DWORD col) {
    int x0, y0, x1, y1, i, j; DWORD *fb;
    lg2d_nfill++; if (!lg2d_in) return; if (((col >> 24) & 0xFF) < 128) { lg2d_nalpha++; return; }
    x0 = (int)x; y0 = (int)y; x1 = (int)(x + w); y1 = (int)(y + h);
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0;
    if (x1 > lg2d_w) x1 = lg2d_w; if (y1 > lg2d_h) y1 = lg2d_h;
    if (x0 >= x1 || y0 >= y1) return;
    fb = (DWORD *)pb_back_buffer();
    lg2d_ndraw++; (void)fb; (void)i; (void)j; lg2d_gpufill(x0, y0, x1, y1, col);
}
static void LG2D_SetColor(const float *rgba) {
    int i; lg2d_ncol++; for (i = 0; i < 4; i++) lg2d_rgba[i] = rgba ? rgba[i] : 1.0f;
}
static void LG2D_BeginFrame(int stereo) {
    DWORD *fb; int i, n;
    (void)stereo;
    if (!lg2d_ok || lg2d_in) return;
    { DWORD lgv = GetTickCount(); if (lg2d_tend) lg2d_msgame = lgv - lg2d_tend; pb_wait_for_vbl(); lg2d_msvbl = GetTickCount() - lgv; }
    pb_reset();
    pb_target_back_buffer();
    fb = 0; n = 0;
    (void)fb; (void)i; (void)n; lg2d_gpufill(0, 0, lg2d_w, lg2d_h, 0xFF101020);
    lg2d_in = 1; lg2d_frame++;
}
static void LG2D_EndFrame(int *fe, int *be) {
    DWORD t;
    if (fe) *fe = 0; if (be) *be = 0;
    if (!lg2d_in) return;
    lg2d_nfill--; lg2d_ndraw--; lg2d_fill((float)(lg2d_frame % (unsigned int)(lg2d_w - 40)), (float)(lg2d_h - 12), 40.0f, 10.0f, 0xFF00FF00);
    pb_erase_text_screen();
    lg2d_fpsf++; if (GetTickCount() - lg2d_fpst >= 1000) { lg2d_fps = lg2d_fpsf; lg2d_fpsf = 0; lg2d_fpst = GetTickCount(); }
    lg2d_dbgblit((int)lg2d_nsh); /* LG 18z8k */
    pb_printat(0, 0, "LG 18z8k frame %u fps %u", lg2d_frame, lg2d_fps);
    pb_printat(1, 0, "fill %u draw %u alpha %u sh0 %u", lg2d_nfill, lg2d_ndraw, lg2d_nalpha, lg2d_nsh0);
    pb_printat(2, 0, "setcolor %u shaders %u", lg2d_ncol, (unsigned int)lg2d_nsh); pb_printat(4, 0, "tga ok %u fail %u dec %u t%d %dx%d b%d", lg_tga_ok, lg_tga_fail, lg_ndec, lg_tga_t, lg_tga_w, lg_tga_h, lg_tga_bpp); if (0) pb_printat(4, 0, "%u", lg_tga_ok, lg_tga_fail, lg_tga_t, lg_tga_w, lg_tga_h, lg_tga_bpp); { int i0 = lg2d_nsh > 18 ? lg2d_nsh - 18 : 0, r; for (r = 0; i0 + r < lg2d_nsh; r++) pb_printat(5 + r, 0, "%c %-34.34s", lg_sh_st[i0 + r], lg2d_shn[i0 + r]); }
    pb_printat(3, 0, "vbl %u fin %u game %u to %u", (unsigned int)lg2d_msvbl, (unsigned int)lg2d_msfin, (unsigned int)lg2d_msgame, lg2d_nto);
    lg2d_nfill = lg2d_ndraw = lg2d_nalpha = lg2d_nsh0 = lg2d_ncol = 0;
    pb_draw_text_screen();
    __asm__ __volatile__("sfence" ::: "memory");
    t = GetTickCount();
    while (pb_finished()) { if (GetTickCount() - t > 50) { lg2d_nto++; break; } } lg2d_msfin = GetTickCount() - t; lg2d_tend = GetTickCount();
    lg2d_in = 0;
}
static void LG2D_DrawStretchPic(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t shader) {
    (void)s1; (void)t1; (void)s2; (void)t2;
    lg2d_fill(x, y, w, h, lg2d_mk((int)shader));
}
static void LG2D_DrawStretchPic2(float x, float y, float w, float h, float s1, float t1, float s2, float t2, float sx, float sy, qhandle_t shader) {
    (void)s1; (void)t1; (void)s2; (void)t2; (void)sx; (void)sy;
    lg2d_fill(x, y, w, h, lg2d_mk((int)shader));
}
static void LG2D_DrawTilePic(float x, float y, float w, float h, qhandle_t shader) {
    lg2d_fill(x, y, w, h, lg2d_mk((int)shader));
}
static void LG2D_DrawBox(float x, float y, float w, float h) {
    lg2d_fill(x, y, w, h, lg2d_mk(0));
}
#endif
/* cdecl: o chamador limpa a pilha, entao um stub int serve p/ qualquer assinatura sem float/struct. */
static int LG_NullStub(void) { return 0; }
static int LG_NullOne(void) { return 1; }
static float LG_NullFloat(void) { return 0.0f; }
static const char *LG_NullName(void) { return ""; }
static qboolean LG_NullTrue(void) { return qtrue; }

static void LG_NullBeginRegistration(glconfig_t *config)
{
memset(config, 0, sizeof(*config));
config->vidWidth = 640;
config->vidHeight = 480;
}

static void LG_NullRegisterFont(const char *name, int size, fontInfo_t *font)
{
(void)name; (void)size;
memset(font, 0, sizeof(*font));
}

static orientation_t LG_NullOrientation(refEntity_t *model, int tagNum)
{
orientation_t o;
(void)model; (void)tagNum;
memset(&o, 0, sizeof(o));
AxisClear(o.axis);
return o;
}

refexport_t *GetRefAPI(int apiVersion, refimport_t *rimp)
{
    lg_ri = *rimp; lg_ri_ok = 1; /* LG 18z8i */
void *stub = (void *)LG_NullStub;
size_t i;

if (apiVersion != REF_API_VERSION || !rimp)
return NULL;
lgNullRi = *rimp;
for (i = 0; i < sizeof(lgNullRe) / sizeof(void *); ++i)
memcpy((char *)&lgNullRe + i * sizeof(void *), &stub, sizeof(stub));

LG_SET(BeginRegistration, LG_NullBeginRegistration);
LG_SET(RegisterFont, LG_NullRegisterFont);
LG_SET(TIKI_Orientation, LG_NullOrientation);
LG_SET(ModelRadius, LG_NullFloat);
LG_SET(GetFontHeight, LG_NullFloat);
LG_SET(GetFontStringWidth, LG_NullFloat);
LG_SET(Noise, LG_NullFloat);
LG_SET(GetShaderName, LG_NullName);
LG_SET(GetModelName, LG_NullName);
LG_SET(GetGraphicsInfo, LG_NullName);
LG_SET(SetMode, LG_NullTrue);
LG_SET(GetShaderWidth, LG_NullOne);
LG_SET(GetShaderHeight, LG_NullOne);
#ifdef NXDK /* LG 18z8c */
lg2d_ri = &lgNullRi;
LG_SET(SetColor, LG2D_SetColor);
LG_SET(BeginFrame, LG2D_BeginFrame);
LG_SET(EndFrame, LG2D_EndFrame);
LG_SET(DrawStretchPic, LG2D_DrawStretchPic);
LG_SET(DrawStretchPic2, LG2D_DrawStretchPic2);
LG_SET(DrawTilePic, LG2D_DrawTilePic);
LG_SET(DrawBox, LG2D_DrawBox);
LG_SET(RegisterShader, LG2D_RegisterShader);
LG_SET(RegisterShaderNoMip, LG2D_RegisterShader);
lg2d_init();
#endif

lgNullRi.Printf(PRINT_ALL, "LG: renderer nulo (18e)\n");
lgNullRe.LoadFont = lg_null_LoadFont; /* LG 18m */
return &lgNullRe;
}

qboolean R_ImageExists(const char *name)
{
(void)name;
return qfalse;
}
#ifdef NXDK
#include "lg_nv2a.c"
#endif
