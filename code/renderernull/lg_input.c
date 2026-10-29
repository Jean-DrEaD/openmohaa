#define LG_WD_NOPRINT(...) ((void)0) /* LG 18z9j: watchdog sem tela (corrida pbkit) */
/* LG 18z9f watchdog */
#include <windows.h>
#include <hal/debug.h>
#include <pbkit/pbkit.h>
extern volatile int lg_st[8]; /* LG 18z9g */
volatile int lg_wd;
static int lg_wd_on;
static DWORD WINAPI lg_wd_thread(LPVOID p) {
  int last = -1, still = 0; (void)p;
  for (;;) {
    Sleep(500);
    if (lg_wd != last) { if (still >= 6) ((void)0); last = lg_wd; still = 0; continue; }
    still++;
    if (still == 6) ((void)0);
    if (still == 6 || (still > 6 && (still % 8) == 0))
      LG_WD_NOPRINT("LG WD frame %d cm %d ev %d ps %d pl %d pm %d s5 %d s6 %d\n",
        lg_wd, lg_st[0], lg_st[1], lg_st[3], lg_st[5], lg_st[4], lg_st[6], lg_st[7]);
  }
  return 0;
}
/* LG 18z9b: pad Xbox (XID) -> eventos OpenMoHAA. Base ioQ3-Xbox xbox_input.c, sem HID (nxdk_usb.lib sem usbh_hid). */
#include <string.h>
#include <usb.h>
#include <usbh_lib.h>
#include <xid_driver.h>
#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../client/keycodes.h"

extern int Key_GetCatcher(void);
extern void Key_SetBinding(int keynum, const char *binding);
extern int lg_dbg[];

#define LGI_PORTS 4
#define LGI_THR 30
#define LGI_DZ 7849
#define LGI_MAX 32767
#define LGI_CUR 400.0f

enum { B_UP, B_DOWN, B_LEFT, B_RIGHT, B_START, B_BACK, B_LT_CLK, B_RT_CLK,
B_A, B_B, B_X, B_Y, B_BLACK, B_WHITE, B_LT, B_RT, B_COUNT };

static const int lgi_keys[B_COUNT] = {
K_UPARROW, K_DOWNARROW, K_LEFTARROW, K_RIGHTARROW,
K_ESCAPE, K_JOY11, K_JOY9, K_JOY10,
K_JOY1, K_JOY2, K_JOY3, K_JOY4, K_JOY5, K_JOY6, K_JOY7, K_JOY8 };

typedef struct {
int connected;
uint16_t buttons;
uint8_t an[8]; /* a b x y black white l r */
int16_t lx, ly, rx, ry;
} lgi_pad_t;

static lgi_pad_t lgi_pad[LGI_PORTS];
static int lgi_sent[LGI_PORTS][B_COUNT];
static int lgi_axis[LGI_PORTS][4];
static float lgi_cx, lgi_cy;
static int lgi_last, lgi_started, lgi_stopping, lgi_presses;

static void lgi_read(UTR_T *t)
{
xid_dev_t *d = t ? (xid_dev_t *)t->context : NULL;
lgi_pad_t *p;
xid_gamepad_in r;
if (!d || t->status != USBH_OK || t->xfer_len < sizeof(r)) return;
if (d->xid_desc.bType != XID_TYPE_GAMECONTROLLER) return;
p = (lgi_pad_t *)d->user_data;
if (!p) return;
memcpy(&r, t->buff, sizeof(r));
p->buttons = r.dButtons;
p->an[0] = r.a; p->an[1] = r.b; p->an[2] = r.x; p->an[3] = r.y;
p->an[4] = r.black; p->an[5] = r.white; p->an[6] = r.l; p->an[7] = r.r;
p->lx = r.leftStickX; p->ly = r.leftStickY; p->rx = r.rightStickX; p->ry = r.rightStickY;
if (!lgi_stopping) { t->xfer_len = 0; t->bIsTransferDone = 0; (void)usbh_int_xfer(t); }
}

static void lgi_conn(xid_dev_t *d, int param)
{
int i;
(void)param;
if (!d || usbh_xid_get_type(d) == XID_UNKNOWN) return;
if (!d->user_data)
for (i = 0; i < LGI_PORTS; ++i)
if (!lgi_pad[i].connected) { lgi_pad[i].connected = 1; d->user_data = &lgi_pad[i]; break; }
if (d->user_data) (void)usbh_xid_read(d, 0, (void *)lgi_read);
}

static void lgi_disc(xid_dev_t *d, int param)
{
(void)param;
if (!d) return;
if (d->user_data) memset(d->user_data, 0, sizeof(lgi_pad_t));
d->user_data = NULL;
}

static int lgi_stick(int v)
{
int m = v < 0 ? -v : v;
if (m <= LGI_DZ) return 0;
if (m > LGI_MAX) m = LGI_MAX;
m = (m - LGI_DZ) * LGI_MAX / (LGI_MAX - LGI_DZ);
return v < 0 ? -m : m;
}

static int lgi_down(const lgi_pad_t *p, int b)
{
if (b >= B_A) return p->an[b - B_A] >= LGI_THR;
return (p->buttons & (1 << b)) != 0;
}

void IN_Init(void *windowData)
{
int t0;
(void)windowData;
if (lgi_started) return;
memset(lgi_pad, 0, sizeof(lgi_pad));
memset(lgi_sent, 0, sizeof(lgi_sent));
memset(lgi_axis, 0, sizeof(lgi_axis));
lgi_cx = lgi_cy = 0.0f;
lgi_stopping = 0;
usbh_core_init();
usbh_xid_init();
usbh_install_xid_conn_callback(lgi_conn, lgi_disc);
t0 = Sys_Milliseconds();
while (Sys_Milliseconds() - t0 < 500) usbh_pooling_hubs();
Key_SetBinding(K_JOY1, "+moveup");
Key_SetBinding(K_JOY2, "+movedown");
Key_SetBinding(K_JOY3, "+use");
Key_SetBinding(K_JOY4, "weapnext");
Key_SetBinding(K_JOY5, "weapprev");
Key_SetBinding(K_JOY6, "+speed");
Key_SetBinding(K_JOY7, "+attacksecondary");
Key_SetBinding(K_JOY8, "+attackprimary");
Key_SetBinding(K_UPARROW, "+forward"); /* LG 18z9d */
Key_SetBinding(K_DOWNARROW, "+back");
Key_SetBinding(K_LEFTARROW, "+moveleft");
Key_SetBinding(K_RIGHTARROW, "+moveright");
Cvar_Set("j_yaw", "-0.006");
Cvar_Set("j_pitch", "0.005");
Cvar_Set("j_forward", "-0.0039");
Cvar_Set("j_side", "0.0039");
lgi_last = Sys_Milliseconds();
lgi_started = 1;
}

void IN_Frame(void)
{
  lg_wd++; if (!lg_wd_on) { lg_wd_on = 1; CreateThread(NULL, 0, lg_wd_thread, NULL, 0, NULL); }
int port, i, time, ms, menu, npads = 0;
if (!lgi_started) IN_Init(NULL);
usbh_pooling_hubs();
time = Sys_Milliseconds();
ms = time - lgi_last; lgi_last = time;
if (ms < 0) ms = 0;
if (ms > 50) ms = 50;
menu = (Key_GetCatcher() & KEYCATCH_UI) != 0;
if (!menu) lgi_cx = lgi_cy = 0.0f;
for (port = 0; port < LGI_PORTS; ++port)
{
const lgi_pad_t *p = &lgi_pad[port];
int st[4];
if (p->connected) npads++;
for (i = 0; i < B_COUNT; ++i)
{
int dn = lgi_down(p, i);
int *s = &lgi_sent[port][i];
if (dn && !*s) {
*s = (menu && i == B_A) ? K_ENTER : (menu && i == B_B) ? K_ESCAPE : lgi_keys[i];
Com_QueueEvent(time, SE_KEY, *s, qtrue, 0, NULL);
lgi_presses++;
} else if (!dn && *s) {
Com_QueueEvent(time, SE_KEY, *s, qfalse, 0, NULL);
*s = 0;
}
}
st[0] = lgi_stick(p->lx); st[1] = -lgi_stick(p->ly);
st[2] = lgi_stick(p->rx); st[3] = -lgi_stick(p->ry);
if (menu)
{
float fx = (float)st[0] / LGI_MAX, fy = (float)st[1] / LGI_MAX;
int dx, dy;
lgi_cx += fx * (fx < 0 ? -fx : fx) * LGI_CUR * ms * 0.001f;
lgi_cy += fy * (fy < 0 ? -fy : fy) * LGI_CUR * ms * 0.001f;
dx = (int)lgi_cx; dy = (int)lgi_cy;
lgi_cx -= (float)dx; lgi_cy -= (float)dy;
if (dx || dy) Com_QueueEvent(time, SE_MOUSE, dx, dy, 0, NULL);
memset(st, 0, sizeof(st));
}
for (i = 0; i < 4; ++i)
if (st[i] != lgi_axis[port][i]) {
lgi_axis[port][i] = st[i];
Com_QueueEvent(time, SE_JOYSTICK_AXIS, i, st[i], 0, NULL);
}
}
lg_dbg[2] = npads * 1000 + (lgi_presses % 1000);
}

void IN_Shutdown(void)
{
if (!lgi_started) return;
lgi_stopping = 1;
usbh_install_xid_conn_callback(NULL, NULL);
usbh_core_deinit();
memset(lgi_pad, 0, sizeof(lgi_pad));
memset(lgi_sent, 0, sizeof(lgi_sent));
memset(lgi_axis, 0, sizeof(lgi_axis));
lgi_started = 0;
}

void IN_Restart(void)
{
IN_Shutdown();
IN_Init(NULL);
}