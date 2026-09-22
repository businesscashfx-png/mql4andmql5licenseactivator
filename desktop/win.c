/* ============================================================================
 * BCFX Activador de Licencias MQL4/MQL5  —  Programa de escritorio Windows
 * ----------------------------------------------------------------------------
 * win.c : Interfaz moderna dibujada a medida (Win32 + GDI, sin frameworks).
 *         Diseño oscuro con tarjetas redondeadas, botones con degradado y
 *         controles segmentados. Windows 10 / 11 (x64).
 *
 *         Motor JS embebido: QuickJS. La lógica de firma/licencia es idéntica
 *         a la versión web (se evalúan crypto.js y mql.js en el runtime).
 *
 * Compilación: tools/build-desktop.sh  (zig cc -target x86_64-windows-gnu ...)
 * ==========================================================================*/
#ifndef UNICODE
#define UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shellapi.h>

#include "deps/quickjs/quickjs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ---------- JS embebido (crypto.js + mql.js) ---------- */
#include "js_embed.h"

#define APP_VERSION "1.2.0"

/* ------------------------- colores de diseño ------------------------------ */
#define CLR_BG     RGB(11, 16, 32)
#define CLR_BG2    RGB(15, 22, 48)
#define CLR_CARD   RGB(19, 27, 56)
#define CLR_CARD2  RGB(24, 34, 67)
#define CLR_EDGE   RGB(36, 48, 89)
#define CLR_TXT    RGB(232, 236, 248)
#define CLR_MUT    RGB(139, 150, 192)
#define CLR_ACC    RGB(34, 211, 238)
#define CLR_ACC2   RGB(59, 130, 246)
#define CLR_GOLD   RGB(251, 191, 36)
#define CLR_GREEN  RGB(52, 211, 153)
#define CLR_RED    RGB(248, 113, 113)
#define CLR_FIELD  RGB(13, 19, 48)
#define CLR_INK    RGB(4, 18, 26)

/* ------------------------- ids de controls -------------------------------- */
#define IDC_OPEN      1001
#define IDC_GENERATE  1002
#define IDC_LICENSEE  1003
#define IDC_DAYS      1004
#define IDC_UNTIL     1005
#define IDC_BROKER    1006
#define IDC_ACCOUNT   1007
#define IDC_LIC       1008
#define IDC_SAVE_EA   1009
#define IDC_SAVE_LIC  1010
#define IDC_COPY      1011

/* ------------------------- estado de la app ------------------------------- */
#define MAX_EA_SRC (4 * 1024 * 1024)

typedef struct {
    char ea_source[MAX_EA_SRC];
    int  ea_loaded;
    char ea_path[MAX_PATH];
    char ea_ext[8];

    char licensee[128];
    char broker[256];
    char account[128];
    char days[32];
    char until[64];

    int  mode;          /* 0 = por tiempo, 1 = ilimitado */
    int  broker_mode;   /* 0 = cualquier, 1 = especifico */
    int  account_mode;  /* 0 = todas, 1 = especifica */

    char license[256];
    char out_source[MAX_EA_SRC];
    int  have_result;

    char status[512];
} App;

static App   g_app;
static HWND  g_wnd;
static HINSTANCE g_hinst;

/* ------------------------- fuentes ---------------------------------------- */
static HFONT f_logo, f_big, f_h, f_b, f_s, f_btn, f_mono;

/* ------------------------- widget state ----------------------------------- */
typedef enum {
    Z_NONE = 0, Z_DROP, Z_GEN, Z_EA, Z_LIC, Z_COPY,
    Z_SEG_MODE, Z_SEG_BROKER, Z_SEG_ACCOUNT
} Zone;

static int g_hover = Z_NONE;      /* zona bajo el cursor */
static int g_press = Z_NONE;      /* zona presionada     */
static int g_focus = 0;           /* edit con foco       */
static int g_tracking = 0;        /* WM_MOUSELEAVE activo */

/* rectángulos fijos de la UI */
static RECT rcDrop, rcGen, rcEA, rcLic, rcCopy;
static RECT rcSegMode, rcSegBroker, rcSegAccount;
static RECT rcFName, rcFDays, rcFUntil, rcFBroker, rcFAccount, rcFLic;

/* ------------------------- gradiente (msimg32 dinámico) ------------------- */
typedef struct { LONG x, y; WORD r, g, b, a; } GTRI;
typedef struct { ULONG ul, lr; } GRECT;
typedef BOOL (__stdcall *gradf_t)(HDC, GTRI*, ULONG, void*, ULONG, ULONG);

static HMODULE g_msimg = NULL;
static gradf_t  g_grad = NULL;

#define C16(c) ((WORD)(((c) << 8) & 0xff00) | ((c) & 0xff))

static void grad_init(void) {
    g_msimg = LoadLibraryA("msimg32.dll");
    if (g_msimg) g_grad = (gradf_t)(void*)GetProcAddress(g_msimg, "GradientFill");
}

static void grad(HDC dc, RECT rc, COLORREF a, COLORREF b, BOOL vert) {
    if (g_grad) {
        GTRI v[2]; GRECT g;
        v[0].x = rc.left;  v[0].y = rc.top;
        v[0].r = C16(GetRValue(a)); v[0].g = C16(GetGValue(a)); v[0].b = C16(GetBValue(a)); v[0].a = 0;
        v[1].x = rc.right; v[1].y = rc.bottom;
        v[1].r = C16(GetRValue(b)); v[1].g = C16(GetGValue(b)); v[1].b = C16(GetBValue(b)); v[1].a = 0;
        g.ul = 0; g.lr = 1;
        g_grad(dc, v, 2, &g, 1, vert ? 0x00000001 : 0x00000000);
        return;
    }
    HBRUSH br = CreateSolidBrush(a);
    FillRect(dc, &rc, br);
    DeleteObject(br);
}

/* ------------------------- primitivas de dibujo --------------------------- */
static void rr_fill(HDC dc, RECT rc, int r, COLORREF c) {
    HRGN h = CreateRoundRectRgn(rc.left, rc.top, rc.right + 1, rc.bottom + 1, r * 2, r * 2);
    HBRUSH br = CreateSolidBrush(c);
    FillRgn(dc, h, br);
    DeleteObject(br); DeleteObject(h);
}

static void rr_grad(HDC dc, RECT rc, int r, COLORREF a, COLORREF b, BOOL vert) {
    HRGN h = CreateRoundRectRgn(rc.left, rc.top, rc.right + 1, rc.bottom + 1, r * 2, r * 2);
    int saved = SaveDC(dc);
    SelectClipRgn(dc, h);
    grad(dc, rc, a, b, vert);
    RestoreDC(dc, saved);
    DeleteObject(h);
}

static void rr_frame(HDC dc, RECT rc, int r, int pw, COLORREF c) {
    HPEN pen = CreatePen(PS_SOLID, pw, c);
    HGDIOBJ op = SelectObject(dc, pen);
    HGDIOBJ ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
    RoundRect(dc, rc.left, rc.top, rc.right, rc.bottom, r * 2, r * 2);
    SelectObject(dc, op); SelectObject(dc, ob);
    DeleteObject(pen);
}

static void rr_frame_dash(HDC dc, RECT rc, int r, int pw, COLORREF c) {
    HPEN pen = CreatePen(PS_DASH, pw, c);
    HGDIOBJ op = SelectObject(dc, pen);
    SetBkMode(dc, TRANSPARENT);
    HGDIOBJ ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
    RoundRect(dc, rc.left, rc.top, rc.right, rc.bottom, r * 2, r * 2);
    SelectObject(dc, op); SelectObject(dc, ob);
    DeleteObject(pen);
}

static void text(HDC dc, RECT rc, HFONT font, COLORREF color, const char *s, UINT fmt) {
    HGDIOBJ of = SelectObject(dc, font);
    SetTextColor(dc, color);
    SetBkMode(dc, TRANSPARENT);
    DrawTextA(dc, s, -1, (LPRECT)&rc, fmt);
    SelectObject(dc, of);
}

/* ------------------------- layout (medidas) ------------------------------ */
static void Rect(RECT *r, int l, int t, int rr, int b) { r->left=l; r->top=t; r->right=rr; r->bottom=b; }

static void layout(void) {
    /* margen M=48, borde derecho=712 (ventana 760 de ancho) */
    Rect(&rcDrop, 48, 176, 712, 236);

    Rect(&rcSegMode, 424, 338, 712, 376);
    Rect(&rcSegBroker, 72, 498, 472, 536);
    Rect(&rcSegAccount, 72, 630, 472, 668);

    Rect(&rcFName,   72, 338, 392, 376);
    Rect(&rcFDays,   72, 418, 182, 456);
    Rect(&rcFUntil, 196, 418, 712, 456);
    Rect(&rcFBroker, 72, 544, 542, 582);
    Rect(&rcFAccount,72, 676, 242, 714);

    Rect(&rcGen, 72, 736, 392, 782);
    Rect(&rcEA,  72, 846, 332, 886);
    Rect(&rcLic, 348, 846, 568, 886);

    Rect(&rcFLic, 72, 798, 592, 834);
    Rect(&rcCopy, 600, 798, 712, 834);
}

static RECT *zone_rect(int z) {
    switch (z) {
        case Z_DROP: return &rcDrop;
        case Z_GEN:  return &rcGen;
        case Z_EA:   return &rcEA;
        case Z_LIC:  return &rcLic;
        case Z_COPY: return &rcCopy;
        case Z_SEG_MODE:   return &rcSegMode;
        case Z_SEG_BROKER: return &rcSegBroker;
        case Z_SEG_ACCOUNT:return &rcSegAccount;
    }
    return NULL;
}

static int hit_zone(POINT p) {
    int z;
    for (z = Z_DROP; z <= Z_SEG_ACCOUNT; z++) {
        RECT *r = zone_rect(z);
        if (r && PtInRect(r, p)) {
            /* botones secundarios solo activos si hay resultado */
            if (z == Z_EA || z == Z_LIC || z == Z_COPY) {
                if (!g_app.have_result && z != Z_COPY) continue;
                if (!g_app.have_result) continue;
            }
            if (z == Z_GEN && !g_app.ea_loaded) continue;
            return z;
        }
    }
    return Z_NONE;
}

/* ------------------------- dibujo de widgets ----------------------------- */
static void draw_logo(HDC dc) {
    RECT r; Rect(&r, 32, 26, 96, 90);
    rr_grad(dc, r, 14, CLR_ACC, CLR_ACC2, FALSE);
    text(dc, r, f_logo, CLR_INK, "BCFX", DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void draw_badge(HDC dc) {
    RECT r; Rect(&r, 520, 40, 712, 78);
    rr_fill(dc, r, 18, RGB(9, 36, 28));
    rr_frame(dc, r, 18, 1, RGB(38, 92, 72));
    text(dc, r, f_s, CLR_GREEN, "100% local - sin internet", DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void draw_dropzone(HDC dc) {
    int hov = (g_hover == Z_DROP);
    int prs = (g_press == Z_DROP);
    COLORREF bg = CLR_FIELD, edge = RGB(58, 76, 130);
    if (hov) { bg = RGB(16, 26, 58); edge = CLR_ACC; }
    if (prs) bg = RGB(12, 20, 46);
    rr_fill(dc, rcDrop, 12, bg);
    rr_frame_dash(dc, rcDrop, 12, 1, edge);

    RECT icon = { rcDrop.left + (rcDrop.right - rcDrop.left) / 2 - 90,
                  rcDrop.top + 18, rcDrop.left + (rcDrop.right - rcDrop.left) / 2 + 90,
                  rcDrop.top + 34 };
    /* flecha abajo dibujada con GDI */
    {
        int cx = rcDrop.left + (rcDrop.right - rcDrop.left) / 2;
        int ay = rcDrop.top + 22;
        HPEN pen = CreatePen(PS_SOLID, 2, CLR_ACC);
        HGDIOBJ op = SelectObject(dc, pen);
        MoveToEx(dc, cx, ay, NULL); LineTo(dc, cx, ay + 14);
        LineTo(dc, cx - 6, ay + 8); MoveToEx(dc, cx, ay + 14, NULL); LineTo(dc, cx + 6, ay + 8);
        SelectObject(dc, op); DeleteObject(pen);
    }
    (void)icon;
    text(dc, (RECT){ rcDrop.left, rcDrop.top + 44, rcDrop.right, rcDrop.bottom }, f_btn, hov ? CLR_TXT : RGB(205,214,238),
         g_app.ea_loaded ? "Haz clic para cambiar el EA" : "Arrastra aqui el .mq4 / .mq5", DT_CENTER | DT_TOP | DT_SINGLELINE);
}

static void draw_seg(HDC dc, RECT rc, const char *o1, const char *o2, int active) {
    rr_fill(dc, rc, 9, CLR_FIELD);
    rr_frame(dc, rc, 9, 1, CLR_EDGE);
    int w = (rc.right - rc.left) / 2;
    RECT a = { rc.left + 3, rc.top + 3, rc.left + w,     rc.bottom - 3 };
    RECT b = { rc.left + w,  rc.top + 3, rc.right - 3,   rc.bottom - 3 };
    if (active == 0) rr_grad(dc, a, 6, CLR_ACC, CLR_ACC2, FALSE);
    else             rr_grad(dc, b, 6, CLR_ACC, CLR_ACC2, FALSE);
    text(dc, a, f_btn, active == 0 ? CLR_INK : CLR_MUT, o1, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    text(dc, b, f_btn, active == 1 ? CLR_INK : CLR_MUT, o2, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void draw_button(HDC dc, RECT rc, const char *label, int primary, int disabled) {
    int z = (rc.left == rcGen.left && rc.top == rcGen.top) ? Z_GEN :
            (rc.left == rcEA.left  && rc.top == rcEA.top)  ? Z_EA :
            (rc.left == rcLic.left && rc.top == rcLic.top) ? Z_LIC :
            (rc.left == rcCopy.left && rc.top == rcCopy.top) ? Z_COPY : Z_NONE;
    int hov = (g_hover == z), prs = (g_press == z);

    if (primary) {
        if (disabled) { rr_fill(dc, rc, 10, RGB(28, 38, 66)); }
        else if (prs) { rr_grad(dc, rc, 10, CLR_ACC2, CLR_ACC, FALSE); }
        else if (hov) { rr_grad(dc, rc, 10, RGB(64, 214, 242), RGB(60, 140, 255), FALSE); }
        else          { rr_grad(dc, rc, 10, CLR_ACC, CLR_ACC2, FALSE); }
        text(dc, rc, f_btn, disabled ? CLR_MUT : CLR_INK, label, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    } else {
        if (disabled) { rr_fill(dc, rc, 10, RGB(20, 28, 55)); }
        else if (prs) { rr_fill(dc, rc, 10, RGB(16, 24, 50)); }
        else if (hov) { rr_fill(dc, rc, 10, RGB(24, 33, 66)); }
        else          { rr_fill(dc, rc, 10, RGB(19, 27, 56)); }
        rr_frame(dc, rc, 10, 1, hov && !disabled ? CLR_ACC : CLR_EDGE);
        text(dc, rc, f_btn, disabled ? CLR_MUT : CLR_TXT, label, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

static void draw_field(HDC dc, RECT rc, int focused) {
    rr_fill(dc, rc, 9, CLR_FIELD);
    rr_frame(dc, rc, 9, 1, focused ? CLR_ACC : CLR_EDGE);
}

/* ========================================================================== */
/* Motor JS (idéntico al de la versión anterior)                              */
/* ========================================================================== */
static JSRuntime *g_js_rt = NULL;
static JSContext *g_js_ctx = NULL;

static void js_report_error(JSContext *ctx) {
    JSValue exc = JS_GetException(ctx);
    const char *s = JS_ToCString(ctx, exc);
    char msg[512];
    snprintf(msg, sizeof(msg), "Error interno: %s", s ? s : "(desconocido)");
    MessageBoxA(g_wnd, msg, "BCFX", MB_ICONERROR);
    if (s) JS_FreeCString(ctx, s);
    JS_FreeValue(ctx, exc);
}

static int js_init(void) {
    g_js_rt = JS_NewRuntime();
    if (!g_js_rt) return -1;
    g_js_ctx = JS_NewContext(g_js_rt);
    if (!g_js_ctx) return -1;
    JS_SetMaxStackSize(g_js_rt, 4 * 1024 * 1024);
    JS_SetMemoryLimit(g_js_rt, 256 * 1024 * 1024);

    JSValue r1 = JS_Eval(g_js_ctx, EMBED_CRYPTO_JS, EMBED_CRYPTO_JS_LEN, "crypto.js", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(r1)) { js_report_error(g_js_ctx); JS_FreeValue(g_js_ctx, r1); return -1; }
    JS_FreeValue(g_js_ctx, r1);

    JSValue r2 = JS_Eval(g_js_ctx, EMBED_MQL_JS, EMBED_MQL_JS_LEN, "mql.js", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(r2)) { js_report_error(g_js_ctx); JS_FreeValue(g_js_ctx, r2); return -1; }
    JS_FreeValue(g_js_ctx, r2);
    return 0;
}

static void js_close(void) {
    if (g_js_ctx) { JS_FreeContext(g_js_ctx); g_js_ctx = NULL; }
    if (g_js_rt)  { JS_FreeRuntime(g_js_rt);  g_js_rt = NULL; }
}

static int js_generate(App *a) {
    JSValue global = JS_GetGlobalObject(g_js_ctx);
    JSValue bcfx = JS_GetPropertyStr(g_js_ctx, global, "Bcfx");
    if (JS_IsUndefined(bcfx)) {
        MessageBoxA(g_wnd, "No se pudo inicializar el motor interno (Bcfx).", "BCFX", MB_ICONERROR);
        JS_FreeValue(g_js_ctx, global);
        return -1;
    }
    JSValue fn_gen = JS_GetPropertyStr(g_js_ctx, bcfx, "genLicense");
    JSValue fn_inj = JS_GetPropertyStr(g_js_ctx, bcfx, "inject");
    JSValue opts = JS_NewObject(g_js_ctx);
    JS_SetPropertyStr(g_js_ctx, opts, "licensee", JS_NewString(g_js_ctx, a->licensee));
    JS_SetPropertyStr(g_js_ctx, opts, "mode",     JS_NewString(g_js_ctx, a->mode ? "U" : "T"));
    JS_SetPropertyStr(g_js_ctx, opts, "broker",   JS_NewString(g_js_ctx, a->broker_mode ? a->broker : "**"));
    JS_SetPropertyStr(g_js_ctx, opts, "account",  JS_NewString(g_js_ctx, a->account_mode ? a->account : "0"));
    if (!a->mode) {
        JS_SetPropertyStr(g_js_ctx, opts, "until", JS_NewString(g_js_ctx, a->until));
        JS_SetPropertyStr(g_js_ctx, opts, "days",  JS_NewString(g_js_ctx, a->days));
    }
    JSValue licVal = JS_Call(g_js_ctx, fn_gen, bcfx, 1, (JSValueConst *)&opts);
    JS_FreeValue(g_js_ctx, opts);
    if (JS_IsException(licVal)) { js_report_error(g_js_ctx); goto fail; }

    JSValue vLic = JS_GetPropertyStr(g_js_ctx, licVal, "lic");
    const char *lic = JS_ToCString(g_js_ctx, vLic);
    JS_FreeValue(g_js_ctx, vLic);
    if (!lic || strlen(lic) < 67) {
        MessageBoxA(g_wnd, "No se pudo generar la licencia. Revisa los datos.", "BCFX", MB_ICONERROR);
        if (lic) JS_FreeCString(g_js_ctx, lic);
        JS_FreeValue(g_js_ctx, licVal);
        goto fail;
    }
    strncpy(a->license, lic, sizeof(a->license) - 1);
    JS_FreeCString(g_js_ctx, lic);

    JSValue args[3];
    args[0] = JS_NewString(g_js_ctx, a->ea_source);
    args[1] = licVal;
    args[2] = JS_NewString(g_js_ctx, a->license);
    JSValue injRes = JS_Call(g_js_ctx, fn_inj, bcfx, 3, args);
    JS_FreeValue(g_js_ctx, args[0]);
    JS_FreeValue(g_js_ctx, args[2]);
    if (JS_IsException(injRes)) { js_report_error(g_js_ctx); JS_FreeValue(g_js_ctx, licVal); goto fail; }

    JSValue vSrc = JS_GetPropertyStr(g_js_ctx, injRes, "source");
    const char *src = JS_ToCString(g_js_ctx, vSrc);
    JS_FreeValue(g_js_ctx, vSrc);
    size_t slen = src ? strlen(src) : 0;
    if (src && slen > sizeof(a->out_source) - 1) slen = sizeof(a->out_source) - 1;
    if (src) { memcpy(a->out_source, src, slen); a->out_source[slen] = 0; a->have_result = 1; }
    if (src) JS_FreeCString(g_js_ctx, src);

    JS_FreeValue(g_js_ctx, injRes);
    JS_FreeValue(g_js_ctx, licVal);
    JS_FreeValue(g_js_ctx, fn_gen);
    JS_FreeValue(g_js_ctx, fn_inj);
    JS_FreeValue(g_js_ctx, bcfx);
    JS_FreeValue(g_js_ctx, global);
    return 0;

fail:
    JS_FreeValue(g_js_ctx, fn_gen);
    JS_FreeValue(g_js_ctx, fn_inj);
    JS_FreeValue(g_js_ctx, bcfx);
    JS_FreeValue(g_js_ctx, global);
    return -1;
}

/* ========================================================================== */
/* Preferencias                                                               */
/* ========================================================================== */
static void prefs_path(char *out, size_t n) {
    if (FAILED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, out))) { strncpy(out, ".", n); return; }
    strncat(out, "\\BCFXActivator.ini", n - strlen(out) - 1);
}
static void prefs_load(void) {
    char path[MAX_PATH]; prefs_path(path, sizeof(path));
    FILE *f = fopen(path, "rb");
    if (!f) return;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char *nl = strpbrk(line, "\r\n"); if (nl) *nl = 0;
        char *eq = strchr(line, '='); if (!eq) continue;
        *eq = 0;
        if      (!strcmp(line, "licensee"))    strncpy(g_app.licensee, eq+1, sizeof(g_app.licensee)-1);
        else if (!strcmp(line, "broker"))      strncpy(g_app.broker, eq+1, sizeof(g_app.broker)-1);
        else if (!strcmp(line, "account"))     strncpy(g_app.account, eq+1, sizeof(g_app.account)-1);
        else if (!strcmp(line, "days"))        strncpy(g_app.days, eq+1, sizeof(g_app.days)-1);
        else if (!strcmp(line, "until"))       strncpy(g_app.until, eq+1, sizeof(g_app.until)-1);
        else if (!strcmp(line, "mode"))        g_app.mode = atoi(eq+1);
        else if (!strcmp(line, "broker_mode")) g_app.broker_mode = atoi(eq+1);
        else if (!strcmp(line, "account_mode"))g_app.account_mode = atoi(eq+1);
    }
    fclose(f);
}
static void prefs_save(void) {
    char path[MAX_PATH]; prefs_path(path, sizeof(path));
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "licensee=%s\nbroker=%s\naccount=%s\ndays=%s\nuntil=%s\nmode=%d\nbroker_mode=%d\naccount_mode=%d\n",
            g_app.licensee, g_app.broker, g_app.account, g_app.days, g_app.until,
            g_app.mode, g_app.broker_mode, g_app.account_mode);
    fclose(f);
}

/* ========================================================================== */
/* Acciones                                                                   */
/* ========================================================================== */
static void do_open(void) {
    OPENFILENAMEA ofn; char file[MAX_PATH] = {0};
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_wnd;
    ofn.lpstrFilter = "Asesores Expertos MQL (*.mq4;*.mq5)\0*.mq4;*.mq5\0Todos (*.*)\0*.*\0";
    ofn.lpstrFile = file; ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    if (!GetOpenFileNameA(&ofn)) return;

    FILE *f = fopen(file, "rb");
    if (!f) { MessageBoxA(g_wnd, "No se pudo abrir el archivo.", "BCFX", MB_ICONERROR); return; }
    size_t n = fread(g_app.ea_source, 1, sizeof(g_app.ea_source)-1, f);
    fclose(f);
    if (n == 0) { MessageBoxA(g_wnd, "El archivo esta vacio.", "BCFX", MB_ICONWARNING); return; }
    if (memchr(g_app.ea_source, 0, n)) {
        MessageBoxA(g_wnd, "El archivo parece binario (.ex4/.ex5). Usa el fuente .mq4/.mq5.", "BCFX", MB_ICONWARNING);
        return;
    }
    g_app.ea_source[n] = 0;
    g_app.ea_loaded = 1;
    strncpy(g_app.ea_path, file, MAX_PATH-1);
    const char *dot = strrchr(file, '.');
    if (dot && (!_stricmp(dot,".mq4") || !_stricmp(dot,".mq5"))) strncpy(g_app.ea_ext, dot+1, sizeof(g_app.ea_ext)-1);
    else strcpy(g_app.ea_ext, "mq4");
    snprintf(g_app.status, sizeof(g_app.status), "EA cargado : %s  (%zu caracteres)", g_app.ea_path, n);
    InvalidateRect(g_wnd, NULL, TRUE);
}

/* aceptar archivo soltado (drag & drop) */
static void load_path(const char *file) {
    FILE *f = fopen(file, "rb");
    if (!f) return;
    size_t n = fread(g_app.ea_source, 1, sizeof(g_app.ea_source)-1, f);
    fclose(f);
    const char *dot = strrchr(file, '.');
    if (!dot || (_stricmp(dot,".mq4") && _stricmp(dot,".mq5"))) {
        MessageBoxA(g_wnd, "Solo se aceptan .mq4 o .mq5", "BCFX", MB_ICONWARNING); return;
    }
    g_app.ea_source[n] = 0;
    g_app.ea_loaded = 1;
    strncpy(g_app.ea_path, file, MAX_PATH-1);
    strncpy(g_app.ea_ext, dot+1, sizeof(g_app.ea_ext)-1);
    snprintf(g_app.status, sizeof(g_app.status), "EA cargado : %s  (%zu caracteres)", g_app.ea_path, n);
    InvalidateRect(g_wnd, NULL, TRUE);
}

static void do_generate(void) {
    App *a = &g_app;
    GetDlgItemTextA(g_wnd, IDC_LICENSEE, a->licensee, sizeof(a->licensee));
    GetDlgItemTextA(g_wnd, IDC_DAYS,     a->days,     sizeof(a->days));
    GetDlgItemTextA(g_wnd, IDC_UNTIL,    a->until,    sizeof(a->until));
    GetDlgItemTextA(g_wnd, IDC_BROKER,   a->broker,   sizeof(a->broker));
    GetDlgItemTextA(g_wnd, IDC_ACCOUNT,  a->account,  sizeof(a->account));

    if (!a->licensee[0]) { MessageBoxA(g_wnd, "Escribe el nombre de la persona.", "BCFX", MB_ICONWARNING); return; }
    if (a->broker_mode && !a->broker[0]) { MessageBoxA(g_wnd, "Escribe el nombre exacto del broker.", "BCFX", MB_ICONWARNING); return; }
    if (a->account_mode && !a->account[0]) { MessageBoxA(g_wnd, "Escribe el numero de cuenta.", "BCFX", MB_ICONWARNING); return; }
    if (!a->ea_loaded) { MessageBoxA(g_wnd, "Primero carga el EA (.mq4/.mq5).", "BCFX", MB_ICONWARNING); return; }

    if (js_generate(a) != 0) return;

    SetDlgItemTextA(g_wnd, IDC_LIC, a->license);
    snprintf(a->status, sizeof(a->status), "Listo. Guarda el EA protegido y el archivo .lic para tu cliente.");
    InvalidateRect(g_wnd, NULL, TRUE);
    prefs_save();
}

static void do_save_ea(void) {
    if (!g_app.have_result) { MessageBoxA(g_wnd, "Genera primero la licencia.", "BCFX", MB_ICONWARNING); return; }
    OPENFILENAMEA ofn; char file[MAX_PATH] = {0};
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_wnd;
    ofn.lpstrFilter = _stricmp(g_app.ea_ext, "mq5")==0 ? "Asesor Experto MQL5 (*.mq5)\0*.mq5\0" : "Asesor Experto MQL4 (*.mq4)\0*.mq4\0";
    strncpy(file, _stricmp(g_app.ea_ext,"mq5")==0 ? "EA_Protegido.mq5" : "EA_Protegido.mq4", MAX_PATH-1);
    ofn.lpstrFile = file; ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = g_app.ea_ext;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameA(&ofn)) return;
    FILE *f = fopen(file, "wb");
    if (!f) { MessageBoxA(g_wnd, "No se pudo guardar el EA.", "BCFX", MB_ICONERROR); return; }
    size_t w = fwrite(g_app.out_source, 1, strlen(g_app.out_source), f);
    fclose(f);
    char msg[256]; snprintf(msg, sizeof(msg), "EA protegido guardado (%zu bytes).", w);
    MessageBoxA(g_wnd, msg, "BCFX", MB_ICONINFORMATION);
}

static void do_save_lic(void) {
    if (!g_app.license[0]) { MessageBoxA(g_wnd, "Genera primero la licencia.", "BCFX", MB_ICONWARNING); return; }
    OPENFILENAMEA ofn; char file[MAX_PATH] = {0};
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_wnd;
    ofn.lpstrFilter = "Licencia BCFX (*.lic)\0*.lic\0";
    strncpy(file, "LICENCIA.lic", MAX_PATH-1);
    ofn.lpstrFile = file; ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = "lic";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameA(&ofn)) return;
    FILE *f = fopen(file, "wb");
    if (!f) { MessageBoxA(g_wnd, "No se pudo guardar la licencia.", "BCFX", MB_ICONERROR); return; }
    fwrite(g_app.license, 1, strlen(g_app.license), f);
    fclose(f);
    MessageBoxA(g_wnd, "Licencia guardada.", "BCFX", MB_ICONINFORMATION);
}

static void do_copy(void) {
    if (!g_app.license[0]) { MessageBoxA(g_wnd, "Genera primero la licencia.", "BCFX", MB_ICONWARNING); return; }
    if (OpenClipboard(g_wnd)) {
        EmptyClipboard();
        size_t n = strlen(g_app.license);
        HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, n+1);
        if (h) { char *p = (char*)GlobalLock(h); if (p) { memcpy(p, g_app.license, n+1); GlobalUnlock(h); SetClipboardData(CF_TEXT, h); } }
        CloseClipboard();
        MessageBoxA(g_wnd, "Licencia copiada al portapapeles.", "BCFX", MB_ICONINFORMATION);
    }
}

/* ========================================================================== */
/* Dibujo principal                                                           */
/* ========================================================================== */
static void paint_all(HDC dc, RECT *rc) {
    /* fondo */
    grad(dc, *rc, CLR_BG2, CLR_BG, TRUE);

    /* encabezado */
    RECT hdr = { 0, 0, rc->right, 104 };
    grad(dc, hdr, RGB(20, 30, 66), CLR_BG, TRUE);
    draw_logo(dc);
    text(dc, (RECT){116, 30, 500, 58}, f_big, CLR_TXT, "BCFX  -  Activador de Licencias", DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    text(dc, (RECT){116, 62, 500, 84}, f_s, CLR_MUT, "MQL4 / MQL5 / MT4 / MT5   -   Windows 10/11   -   v" APP_VERSION, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    draw_badge(dc);

    /* ---- tarjeta 1: EA ---- */
    RECT card1 = { 48, 120, 712, 258 };
    rr_fill(dc, card1, 14, CLR_CARD);
    rr_frame(dc, card1, 14, 1, CLR_EDGE);

    RECT circle1 = { 72, 146, 108, 182 };
    rr_grad(dc, circle1, 18, CLR_ACC, CLR_ACC2, FALSE);
    text(dc, circle1, f_h, CLR_INK, "1", DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    text(dc, (RECT){122, 142, 700, 164}, f_h, CLR_TXT, "Archivo del Asesor Experto (.mq4 / .mq5)", DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    draw_dropzone(dc);
    if (!g_app.ea_loaded)
        text(dc, (RECT){48, 242, 712, 258}, f_s, CLR_MUT, "Ningun EA cargado", DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    else
        text(dc, (RECT){60, 240, 700, 257}, f_s, CLR_GREEN, g_app.status, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    /* ---- tarjeta 2: datos ---- */
    RECT card2 = { 48, 274, 712, 726 };
    rr_fill(dc, card2, 14, CLR_CARD);
    rr_frame(dc, card2, 14, 1, CLR_EDGE);

    RECT circle2 = { 72, 300, 108, 336 };
    rr_grad(dc, circle2, 18, CLR_ACC, CLR_ACC2, FALSE);
    text(dc, circle2, f_h, CLR_INK, "2", DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    text(dc, (RECT){122, 296, 700, 318}, f_h, CLR_TXT, "Datos de la activacion", DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    /* etiquetas */
    text(dc, (RECT){72, 318, 400, 336}, f_s, CLR_MUT, "Nombre de la persona", DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    text(dc, (RECT){424, 318, 712, 336}, f_s, CLR_MUT, "Tipo de licencia", DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    text(dc, (RECT){72, 398, 400, 416}, f_s, CLR_MUT, "Vigencia:  dias  /  fecha limite (AAAA-MM-DD)", DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    text(dc, (RECT){72, 478, 472, 496}, f_s, CLR_MUT, "Restriccion de broker", DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    text(dc, (RECT){72, 610, 472, 628}, f_s, CLR_MUT, "Restriccion de cuenta", DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    /* campos */
    draw_field(dc, rcFName,    g_focus == IDC_LICENSEE);
    draw_field(dc, rcFDays,    g_focus == IDC_DAYS);
    draw_field(dc, rcFUntil,   g_focus == IDC_UNTIL);
    draw_field(dc, rcFBroker,  g_focus == IDC_BROKER);
    draw_field(dc, rcFAccount, g_focus == IDC_ACCOUNT);

    /* flecha entre "dias" y "fecha limite" */
    {
        int mx = rcFDays.right + 12, my = (rcFDays.top + rcFDays.bottom) / 2;
        int fx = rcFUntil.left - 12;
        HPEN pen = CreatePen(PS_SOLID, 2, CLR_MUT);
        HGDIOBJ op = SelectObject(dc, pen);
        MoveToEx(dc, mx, my, NULL); LineTo(dc, fx - 8, my);
        LineTo(dc, fx - 14, my - 5); MoveToEx(dc, fx - 8, my, NULL); LineTo(dc, fx - 14, my + 5);
        SelectObject(dc, op); DeleteObject(pen);
    }

    /* segmentos */
    draw_seg(dc, rcSegMode,    "Por tiempo", "Ilimitada", g_app.mode);
    draw_seg(dc, rcSegBroker,  "Cualquier broker", "Un broker especifico", g_app.broker_mode);
    draw_seg(dc, rcSegAccount, "Todas las cuentas", "Una cuenta especifica", g_app.account_mode);

    text(dc, (RECT){72, 586, 712, 608}, f_s, CLR_MUT,
         "El broker debe coincidir con AccountCompany() en MT4 / ACCOUNT_COMPANY en MT5 (se ignoran mayusculas y espacios).",
         DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    /* ---- botón generar ---- */
    draw_button(dc, rcGen, "ACTIVAR LICENCIA Y PROTEGER EA", 1, !g_app.ea_loaded);
    if (!g_app.ea_loaded)
        text(dc, (RECT){408, 742, 712, 782}, f_s, CLR_MUT, "Carga primero un EA para activar la licencia.",
             DT_LEFT | DT_VCENTER | DT_WORDBREAK);
    else
        text(dc, (RECT){408, 742, 712, 782}, f_s, CLR_MUT, "Genera la licencia y protege el codigo del EA.",
             DT_LEFT | DT_VCENTER | DT_WORDBREAK);

    /* ---- tarjeta 3: resultado ---- */
    RECT card3 = { 48, 792, 712, 892 };
    rr_fill(dc, card3, 14, CLR_CARD);
    rr_frame(dc, card3, 14, 1, CLR_EDGE);

    RECT circle3 = { 72, 812, 108, 848 };
    rr_grad(dc, circle3, 18, CLR_ACC, CLR_ACC2, FALSE);
    text(dc, circle3, f_h, CLR_INK, "3", DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    text(dc, (RECT){122, 808, 700, 830}, f_h, CLR_TXT, "Licencia generada", DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    text(dc, (RECT){122, 830, 700, 850}, f_s, CLR_MUT, "Descarga el EA protegido y el archivo .lic para tu cliente.", DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    draw_field(dc, rcFLic, g_focus == IDC_LIC);
    draw_button(dc, rcCopy, "Copiar", 0, !g_app.have_result);
    draw_button(dc, rcEA, "Guardar EA protegido", 1, !g_app.have_result);
    draw_button(dc, rcLic, "Guardar .lic", 0, !g_app.have_result);

    if (g_app.have_result)
        text(dc, (RECT){72, 848, 712, 892}, f_s, CLR_GREEN, g_app.status, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

/* ========================================================================== */
/* Window proc                                                                */
/* ========================================================================== */
static void track_leave(void) {
    if (g_tracking) return;
    TRACKMOUSEEVENT tme; ZeroMemory(&tme, sizeof(tme));
    tme.cbSize = sizeof(tme); tme.dwFlags = TME_LEAVE; tme.hwndTrack = g_wnd;
    TrackMouseEvent(&tme); g_tracking = 1;
}

static void set_hover(int z) {
    if (g_hover != z) { g_hover = z; InvalidateRect(g_wnd, NULL, FALSE); }
}

static void click_zone(int z) {
    switch (z) {
        case Z_DROP: do_open(); break;
        case Z_GEN:  do_generate(); break;
        case Z_EA:   do_save_ea(); break;
        case Z_LIC:  do_save_lic(); break;
        case Z_COPY: do_copy(); break;
        case Z_SEG_MODE: {
            g_app.mode = (g_app.mode == 0) ? 1 : 0;
            EnableWindow(GetDlgItem(g_wnd, IDC_DAYS),  g_app.mode == 0);
            EnableWindow(GetDlgItem(g_wnd, IDC_UNTIL), g_app.mode == 0);
            InvalidateRect(g_wnd, NULL, FALSE);
            break;
        }
        case Z_SEG_BROKER: {
            g_app.broker_mode = (g_app.broker_mode == 0) ? 1 : 0;
            EnableWindow(GetDlgItem(g_wnd, IDC_BROKER), g_app.broker_mode == 1);
            InvalidateRect(g_wnd, NULL, FALSE);
            break;
        }
        case Z_SEG_ACCOUNT: {
            g_app.account_mode = (g_app.account_mode == 0) ? 1 : 0;
            EnableWindow(GetDlgItem(g_wnd, IDC_ACCOUNT), g_app.account_mode == 1);
            InvalidateRect(g_wnd, NULL, FALSE);
            break;
        }
    }
}

/* lugar donde se incrusta un EDIT sobre su campo */
static void place_edit(int id, RECT *field) {
    int h = field->bottom - field->top;
    SetWindowPos(GetDlgItem(g_wnd, id), NULL,
                 field->left + 12, field->top + (h - 26) / 2,
                 (field->right - field->left) - 24, 26, SWP_NOZORDER | SWP_NOACTIVATE);
}

static void create_edits(void) {
    DWORD st = WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL;
    HWND e;
    e = CreateWindowA("EDIT", g_app.licensee, st, 0,0,10,10, g_wnd, (HMENU)(INT_PTR)IDC_LICENSEE, g_hinst, NULL);
    e = CreateWindowA("EDIT", g_app.days, st, 0,0,10,10, g_wnd, (HMENU)(INT_PTR)IDC_DAYS, g_hinst, NULL);
    e = CreateWindowA("EDIT", g_app.until, st, 0,0,10,10, g_wnd, (HMENU)(INT_PTR)IDC_UNTIL, g_hinst, NULL);
    e = CreateWindowA("EDIT", g_app.broker, st, 0,0,10,10, g_wnd, (HMENU)(INT_PTR)IDC_BROKER, g_hinst, NULL);
    e = CreateWindowA("EDIT", g_app.account, st, 0,0,10,10, g_wnd, (HMENU)(INT_PTR)IDC_ACCOUNT, g_hinst, NULL);
    e = CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | ES_READONLY | ES_AUTOHSCROLL, 0,0,10,10, g_wnd, (HMENU)(INT_PTR)IDC_LIC, g_hinst, NULL);
    (void)e;

    SendMessage(GetDlgItem(g_wnd, IDC_LICENSEE), WM_SETFONT, (WPARAM)f_b, TRUE);
    SendMessage(GetDlgItem(g_wnd, IDC_DAYS),     WM_SETFONT, (WPARAM)f_b, TRUE);
    SendMessage(GetDlgItem(g_wnd, IDC_UNTIL),    WM_SETFONT, (WPARAM)f_b, TRUE);
    SendMessage(GetDlgItem(g_wnd, IDC_BROKER),   WM_SETFONT, (WPARAM)f_b, TRUE);
    SendMessage(GetDlgItem(g_wnd, IDC_ACCOUNT),  WM_SETFONT, (WPARAM)f_b, TRUE);
    SendMessage(GetDlgItem(g_wnd, IDC_LIC),      WM_SETFONT, (WPARAM)f_mono, TRUE);

    place_edit(IDC_LICENSEE, &rcFName);
    place_edit(IDC_DAYS,     &rcFDays);
    place_edit(IDC_UNTIL,    &rcFUntil);
    place_edit(IDC_BROKER,   &rcFBroker);
    place_edit(IDC_ACCOUNT,  &rcFAccount);
    place_edit(IDC_LIC,      &rcFLic);

    EnableWindow(GetDlgItem(g_wnd, IDC_DAYS),   g_app.mode == 0);
    EnableWindow(GetDlgItem(g_wnd, IDC_UNTIL),  g_app.mode == 0);
    EnableWindow(GetDlgItem(g_wnd, IDC_BROKER), g_app.broker_mode == 1);
    EnableWindow(GetDlgItem(g_wnd, IDC_ACCOUNT), g_app.account_mode == 1);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE:
            layout();
            create_edits();
            DragAcceptFiles(hwnd, TRUE);
            return 0;

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd, &ps);
            RECT rc; GetClientRect(hwnd, &rc);
            HDC mem = CreateCompatibleDC(dc);
            HBITMAP bmp = CreateCompatibleBitmap(dc, rc.right, rc.bottom);
            HBITMAP old = (HBITMAP)SelectObject(mem, bmp);
            paint_all(mem, &rc);
            BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
            SelectObject(mem, old);
            DeleteObject(bmp);
            DeleteDC(mem);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_MOUSEMOVE: {
            POINT p = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            set_hover(hit_zone(p));
            track_leave();
            return 0;
        }
        case WM_MOUSELEAVE:
            g_tracking = 0;
            set_hover(Z_NONE);
            return 0;

        case WM_LBUTTONDOWN: {
            POINT p = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            g_press = hit_zone(p);
            if (g_press != Z_NONE) { SetCapture(hwnd); InvalidateRect(hwnd, NULL, FALSE); }
            return 0;
        }
        case WM_LBUTTONUP: {
            ReleaseCapture();
            POINT p = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            int z = hit_zone(p);
            if (g_press != Z_NONE && z == g_press) click_zone(z);
            g_press = Z_NONE;
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }

        case WM_DROPFILES: {
            HDROP drop = (HDROP)wp;
            char file[MAX_PATH];
            if (DragQueryFileA(drop, 0, file, MAX_PATH)) load_path(file);
            DragFinish(drop);
            return 0;
        }

        case WM_SETCURSOR: {
            POINT p; GetCursorPos(&p); ScreenToClient(hwnd, &p);
            int z = hit_zone(p);
            if (z != Z_NONE) { SetCursor(LoadCursor(NULL, IDC_HAND)); return TRUE; }
            break;
        }

        case WM_CTLCOLOREDIT: {
            HDC dc = (HDC)wp;
            HWND ctl = (HWND)lp;
            int id = GetDlgCtrlID(ctl);
            SetBkColor(dc, CLR_FIELD);
            SetTextColor(dc, IsWindowEnabled(ctl) ? CLR_TXT : CLR_MUT);
            if (id == IDC_LIC) SetTextColor(dc, CLR_GOLD);
            static HBRUSH br;
            if (!br) br = CreateSolidBrush(CLR_FIELD);
            return (LRESULT)br;
        }

        case WM_COMMAND: {
            int id = LOWORD(wp), code = HIWORD(wp);
            if (code == EN_SETFOCUS)  { g_focus = id; InvalidateRect(hwnd, NULL, FALSE); }
            else if (code == EN_KILLFOCUS) { g_focus = 0; InvalidateRect(hwnd, NULL, FALSE); }
            return 0;
        }

        case WM_DESTROY:
            DragAcceptFiles(hwnd, FALSE);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

/* ========================================================================== */
/* Entry point                                                                 */
/* ========================================================================== */
static void enable_dark_titlebar(HWND hwnd) {
    /* mejor esfuerzo: título oscuro en Windows 10 (1903+) / 11 */
    HMODULE d = LoadLibraryA("dwmapi.dll");
    if (!d) return;
    typedef HRESULT (__stdcall *dwa_t)(HWND, DWORD, LPCVOID, DWORD);
    dwa_t f = (dwa_t)(void*)GetProcAddress(d, "DwmSetWindowAttribute");
    if (f) {
        DWORD dark = 1;
        f(hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof(dark));
        f(hwnd, 19 /* DWMWA_BORDER_COLOR */, &dark, sizeof(dark));
    }
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrev; (void)lpCmdLine;
    g_hinst = hInst;

    ZeroMemory(&g_app, sizeof(g_app));
    strcpy(g_app.days, "30");
    prefs_load();
    grad_init();

    /* fuentes */
    f_logo = CreateFontA(-18, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0,0, CLEARTYPE_QUALITY, 0, "Segoe UI");
    f_big  = CreateFontA(-21, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0,0, CLEARTYPE_QUALITY, 0, "Segoe UI");
    f_h    = CreateFontA(-15, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0,0, CLEARTYPE_QUALITY, 0, "Segoe UI");
    f_b    = CreateFontA(-14, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0,0, CLEARTYPE_QUALITY, 0, "Segoe UI");
    f_s    = CreateFontA(-12, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0,0, CLEARTYPE_QUALITY, 0, "Segoe UI");
    f_btn  = CreateFontA(-14, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0,0, CLEARTYPE_QUALITY, 0, "Segoe UI");
    f_mono = CreateFontA(-13, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0,0, CLEARTYPE_QUALITY, 0, "Consolas");

    WNDCLASSA wc; ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = "BCFXActivatorWnd";
    wc.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(1));
    RegisterClassA(&wc);

    int w = 760, h = 936;
    RECT r = { 0, 0, w, h };
    AdjustWindowRect(&r, WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);
    int cx = (GetSystemMetrics(SM_CXSCREEN) - (r.right - r.left)) / 2;
    int cy = (GetSystemMetrics(SM_CYSCREEN) - (r.bottom - r.top)) / 2;
    if (cx < 0) cx = 0; if (cy < 0) cy = 0;

    g_wnd = CreateWindowA("BCFXActivatorWnd", "BCFX - Activador de Licencias MQL4/MQL5",
        WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        cx, cy, r.right - r.left, r.bottom - r.top,
        NULL, NULL, hInst, NULL);
    if (!g_wnd) return 1;

    enable_dark_titlebar(g_wnd);

    if (js_init() != 0) {
        MessageBoxA(g_wnd, "No se pudo iniciar el motor interno.", "BCFX", MB_ICONERROR);
        return 1;
    }

    ShowWindow(g_wnd, nCmdShow);
    UpdateWindow(g_wnd);

    MSG m;
    while (GetMessage(&m, NULL, 0, 0) > 0) {
        TranslateMessage(&m);
        DispatchMessage(&m);
    }
    js_close();
    return (int)m.wParam;
}
