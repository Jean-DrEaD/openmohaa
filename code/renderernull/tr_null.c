/* LG lote 18e: renderer nulo (tela preta) para validar o build cliente no nxdk. */
#include "../qcommon/q_shared.h"
#include "../renderercommon/tr_public.h"
#include <string.h>

/* LG 18m: fonte dummy nao-NULL (UI faz ERR_DROP se LoadFont retorna NULL) */
static fontheader_t lg_null_fonts[16];
static int lg_null_nfonts = 0;
static fontheader_t *lg_null_LoadFont(const char *name) {
    int i;
    for (i = 0; i < lg_null_nfonts; i++) { if (!Q_stricmp(lg_null_fonts[i].name, name)) return &lg_null_fonts[i]; }
    if (lg_null_nfonts >= 16) return &lg_null_fonts[0];
    Q_strncpyz(lg_null_fonts[lg_null_nfonts].name, name, sizeof(lg_null_fonts[0].name));
    return &lg_null_fonts[lg_null_nfonts++];
}
static refexport_t lgNullRe;
static refimport_t lgNullRi;

#define LG_SET(field, fn) do { void *lgp_ = (void *)(fn); memcpy(&lgNullRe.field, &lgp_, sizeof(lgp_)); } while (0)

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

lgNullRi.Printf(PRINT_ALL, "LG: renderer nulo (18e)\n");
lgNullRe.LoadFont = lg_null_LoadFont; /* LG 18m */
return &lgNullRe;
}

qboolean R_ImageExists(const char *name)
{
(void)name;
return qfalse;
}
