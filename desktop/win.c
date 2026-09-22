/* ============================================================================
 * BCFX Activador de Licencias MQL4/MQL5  (Programa de escritorio Windows)
 * ----------------------------------------------------------------------------
 * win.c   - Interfaz Win32 pura (Windows 10 / 11), sin dependencias externas.
 *           Motor JS embebido: QuickJS (compilado junto a esta app).
 *           La lógica de firma/licencia es exactamente la misma que la web:
 *           se evalúan crypto.js y mql.js dentro del runtime QuickJS.
 *
 * Compilación (ver tools/build-desktop.sh):
 *   zig cc -target x86_64-windows-gnu ...
 * ==========================================================================*/
#ifndef UNICODE
#define UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <shlobj.h>

#include "deps/quickjs/quickjs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ---------- JS embebido (crypto.js + mql.js) ---------- */
#include "js_embed.h"

/* ========================================================================== */
/* Recursos de diálogo: manejados por código (sin .rc obligatorio)            */
/* ========================================================================== */
#define IDC_FILE_STATUS  101
#define IDC_OPEN         102
#define IDC_LICENSEE     103
#define IDC_BROKER       104
#define IDC_ACCOUNT      105
#define IDC_DAYS         106
#define IDC_UNTIL        107
#define IDC_MODE_T       108
#define IDC_MODE_U       109
#define IDC_BRK_ANY      110
#define IDC_BRK_ONE      111
#define IDC_ACC_ALL      112
#define IDC_ACC_ONE      113
#define IDC_SUMMARY      114
#define IDC_GENERATE     115
#define IDC_LIC          116
#define IDC_COPY         117
#define IDC_SAVE_LIC     118
#define IDC_SAVE_EA      119
#define IDC_RESULT       120
#define IDC_TITLE        121
#define IDC_LIC_FILE     122

#define APP_VERSION "1.1.0"

/* -------------------------------------------------------------------------- */
/* Estado de la app                                                           */
/* -------------------------------------------------------------------------- */
#define MAX_EA_SRC  (4 * 1024 * 1024)

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

    int mode;          /* 0 = por tiempo, 1 = ilimitado */
    int broker_mode;   /* 0 = cualquier, 1 = especifico */
    int account_mode;  /* 0 = todas, 1 = especifica */

    char license[256];
    char lic_meta[512];
    char out_source[MAX_EA_SRC];
    int  have_result;
} App;

static App   g_app;
static HWND  g_wnd;
static HINSTANCE g_hinst;
static HFONT g_font_ui, g_font_title, g_font_mono;

/* Colores */
#define COL_BG      RGB(13, 17, 30)
#define COL_BG2     RGB(17, 23, 46)
#define COL_EDGE    RGB(40, 50, 90)
#define COL_TXT     RGB(232, 236, 248)
#define COL_MUTED   RGB(150, 160, 199)
#define COL_ACC     RGB(34, 211, 238)
#define COL_ACC2    RGB(59, 130, 246)
#define COL_GOLD    RGB(251, 191, 36)
#define COL_GREEN   RGB(52, 211, 153)
#define COL_RED     RGB(248, 113, 113)

/* ========================================================================== */
/* Módulo JS: evalúa crypto.js + mql.js y deja el objeto global Bcfx          */
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

/* ========================================================================== */
/* Llamadas al código JS (Bcfx.genLicense + Bcfx.inject)                       */
/* ========================================================================== */
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

    /* --- inyectar en el fuente del EA --- */
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

    snprintf(a->lic_meta, sizeof(a->lic_meta), "Licencia generada. Guarda el EA y el archivo .lic para tu cliente.");

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
/* Persistencia (BCFXActivator.ini en %APPDATA%)                              */
/* ========================================================================== */
static void prefs_path(char *out, size_t n) {
    if (FAILED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, out))) {
        strncpy(out, ".", n);
        return;
    }
    strncat(out, "\\BCFXActivator.ini", n - strlen(out) - 1);
}

static void prefs_load(void) {
    char path[MAX_PATH];
    prefs_path(path, sizeof(path));
    FILE *f = fopen(path, "rb");
    if (!f) return;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char *nl = strpbrk(line, "\r\n"); if (nl) *nl = 0;
        char *eq = strchr(line, '='); if (!eq) continue;
        *eq = 0;
        if      (!strcmp(line, "licensee"))    strncpy(g_app.licensee, eq + 1, sizeof(g_app.licensee)-1);
        else if (!strcmp(line, "broker"))      strncpy(g_app.broker, eq + 1, sizeof(g_app.broker)-1);
        else if (!strcmp(line, "account"))     strncpy(g_app.account, eq + 1, sizeof(g_app.account)-1);
        else if (!strcmp(line, "days"))        strncpy(g_app.days, eq + 1, sizeof(g_app.days)-1);
        else if (!strcmp(line, "until"))       strncpy(g_app.until, eq + 1, sizeof(g_app.until)-1);
        else if (!strcmp(line, "mode"))        g_app.mode = atoi(eq + 1);
        else if (!strcmp(line, "broker_mode")) g_app.broker_mode = atoi(eq + 1);
        else if (!strcmp(line, "account_mode"))g_app.account_mode = atoi(eq + 1);
    }
    fclose(f);
}

static void prefs_save(void) {
    char path[MAX_PATH];
    prefs_path(path, sizeof(path));
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "licensee=%s\n", g_app.licensee);
    fprintf(f, "broker=%s\n", g_app.broker);
    fprintf(f, "account=%s\n", g_app.account);
    fprintf(f, "days=%s\n", g_app.days);
    fprintf(f, "until=%s\n", g_app.until);
    fprintf(f, "mode=%d\n", g_app.mode);
    fprintf(f, "broker_mode=%d\n", g_app.broker_mode);
    fprintf(f, "account_mode=%d\n", g_app.account_mode);
    fclose(f);
}

/* ========================================================================== */
/* UI helpers                                                                 */
/* ========================================================================== */
static void set_text(int id, const char *txt)  { SetDlgItemTextA(g_wnd, id, txt); }
static void get_text(int id, char *out, int n) { GetDlgItemTextA(g_wnd, id, out, n); }

static void paint_bg(HDC dc, RECT *rc) {
    HBRUSH br = CreateSolidBrush(COL_BG);
    FillRect(dc, rc, br);
    DeleteObject(br);
}

static void summary_render(void) {
    const App *a = &g_app;
    char buf[1024];
    const char *typ = a->mode ? "ILIMITADA" : "POR TIEMPO";
    snprintf(buf, sizeof(buf),
        "PERSONA : %s\r\n"
        "TIPO    : %s\r\n"
        "BROKER  : %s\r\n"
        "CUENTA  : %s",
        a->licensee[0] ? a->licensee : "(escribe el nombre)",
        typ,
        a->broker_mode ? a->broker : "Cualquier broker (ilimitado abierto)",
        a->account_mode ? a->account : "Todas las cuentas");
    set_text(IDC_SUMMARY, buf);
}

/* ---------- abrir archivo ---------- */
static void do_open(void) {
    OPENFILENAMEA ofn; char file[MAX_PATH] = {0};
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_wnd;
    ofn.lpstrFilter = "Asesores Expertos MQL (*.mq4;*.mq5)\0*.mq4;*.mq5\0Todos (*.*)\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    if (!GetOpenFileNameA(&ofn)) return;

    FILE *f = fopen(file, "rb");
    if (!f) { MessageBoxA(g_wnd, "No se pudo abrir el archivo.", "BCFX", MB_ICONERROR); return; }
    size_t n = fread(g_app.ea_source, 1, sizeof(g_app.ea_source) - 1, f);
    fclose(f);
    if (n == 0) { MessageBoxA(g_wnd, "El archivo está vacío.", "BCFX", MB_ICONWARNING); return; }
    if (memchr(g_app.ea_source, 0, n)) {
        MessageBoxA(g_wnd, "El archivo parece binario (compilado .ex4/.ex5). Usa el fuente .mq4/.mq5.", "BCFX", MB_ICONWARNING);
        return;
    }
    g_app.ea_source[n] = 0;
    g_app.ea_loaded = 1;
    strncpy(g_app.ea_path, file, MAX_PATH - 1);
    const char *dot = strrchr(file, '.');
    if (dot && (!_stricmp(dot, ".mq4") || !_stricmp(dot, ".mq5")))
        strncpy(g_app.ea_ext, dot + 1, sizeof(g_app.ea_ext) - 1);
    else
        strcpy(g_app.ea_ext, "mq4");

    char info[512];
    snprintf(info, sizeof(info), "EA cargado : %s  (%zu caracteres)", g_app.ea_path, n);
    set_text(IDC_FILE_STATUS, info);
    EnableWindow(GetDlgItem(g_wnd, IDC_GENERATE), TRUE);
}

/* ---------- generar ---------- */
static void do_generate(void) {
    App *a = &g_app;
    get_text(IDC_LICENSEE, a->licensee, sizeof(a->licensee));
    get_text(IDC_BROKER,   a->broker,   sizeof(a->broker));
    get_text(IDC_ACCOUNT,  a->account,  sizeof(a->account));
    get_text(IDC_DAYS,     a->days,     sizeof(a->days));
    get_text(IDC_UNTIL,    a->until,    sizeof(a->until));

    if (!a->licensee[0]) { MessageBoxA(g_wnd, "Escribe el nombre de la persona.", "BCFX", MB_ICONWARNING); SetFocus(GetDlgItem(g_wnd, IDC_LICENSEE)); return; }
    if (a->broker_mode && !a->broker[0]) { MessageBoxA(g_wnd, "Escribe el nombre exacto del broker.", "BCFX", MB_ICONWARNING); SetFocus(GetDlgItem(g_wnd, IDC_BROKER)); return; }
    if (a->account_mode && !a->account[0]) { MessageBoxA(g_wnd, "Escribe el número de cuenta.", "BCFX", MB_ICONWARNING); SetFocus(GetDlgItem(g_wnd, IDC_ACCOUNT)); return; }
    if (!a->ea_loaded) { MessageBoxA(g_wnd, "Primero carga el EA (.mq4/.mq5).", "BCFX", MB_ICONWARNING); return; }

    if (js_generate(a) != 0) return;

    set_text(IDC_LIC, a->license);
    set_text(IDC_LIC_FILE, a->license);
    set_text(IDC_RESULT, "Licencia generada. Guarda el EA y el archivo .lic para tu cliente.");
    EnableWindow(GetDlgItem(g_wnd, IDC_SAVE_EA), TRUE);
    EnableWindow(GetDlgItem(g_wnd, IDC_SAVE_LIC), TRUE);
    EnableWindow(GetDlgItem(g_wnd, IDC_COPY), TRUE);
    prefs_save();
}

/* ---------- guardar EA ---------- */
static void do_save_ea(void) {
    if (!g_app.have_result) { MessageBoxA(g_wnd, "Genera primero la licencia.", "BCFX", MB_ICONWARNING); return; }
    OPENFILENAMEA ofn; char file[MAX_PATH] = {0};
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_wnd;
    ofn.lpstrFilter = _stricmp(g_app.ea_ext, "mq5") == 0
        ? "Asesor Experto MQL5 (*.mq5)\0*.mq5\0"
        : "Asesor Experto MQL4 (*.mq4)\0*.mq4\0";
    strncpy(file, "EA_Protegido", MAX_PATH - 1);
    strncat(file, _stricmp(g_app.ea_ext, "mq5") == 0 ? ".mq5" : ".mq4", MAX_PATH - strlen(file) - 1);
    ofn.lpstrFile = file; ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = g_app.ea_ext;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameA(&ofn)) return;
    FILE *f = fopen(file, "wb");
    if (!f) { MessageBoxA(g_wnd, "No se pudo guardar el EA.", "BCFX", MB_ICONERROR); return; }
    size_t w = fwrite(g_app.out_source, 1, strlen(g_app.out_source), f);
    fclose(f);
    char msg[256];
    snprintf(msg, sizeof(msg), "EA protegido guardado (%zu bytes).", w);
    MessageBoxA(g_wnd, msg, "BCFX", MB_ICONINFORMATION);
}

/* ---------- guardar licencia ---------- */
static void do_save_lic(void) {
    if (!g_app.license[0]) { MessageBoxA(g_wnd, "Genera primero la licencia.", "BCFX", MB_ICONWARNING); return; }
    OPENFILENAMEA ofn; char file[MAX_PATH] = {0};
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_wnd;
    ofn.lpstrFilter = "Licencia BCFX (*.lic)\0*.lic\0";
    strncpy(file, "LICENCIA.lic", MAX_PATH - 1);
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

/* ---------- copiar licencia ---------- */
static void do_copy(void) {
    if (!g_app.license[0]) { MessageBoxA(g_wnd, "Genera primero la licencia.", "BCFX", MB_ICONWARNING); return; }
    if (OpenClipboard(g_wnd)) {
        EmptyClipboard();
        size_t n = strlen(g_app.license);
        HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, n + 1);
        if (h) {
            char *p = (char *)GlobalLock(h);
            if (p) { memcpy(p, g_app.license, n + 1); GlobalUnlock(h); SetClipboardData(CF_TEXT, h); }
        }
        CloseClipboard();
        MessageBoxA(g_wnd, "Licencia copiada al portapapeles.", "BCFX", MB_ICONINFORMATION);
    }
}

/* ---------- radios ---------- */
static void cmd_mode(int id) {
    g_app.mode = (id == IDC_MODE_U);
    CheckRadioButton(g_wnd, IDC_MODE_T, IDC_MODE_U, id);
    EnableWindow(GetDlgItem(g_wnd, IDC_DAYS),  g_app.mode == 0);
    EnableWindow(GetDlgItem(g_wnd, IDC_UNTIL), g_app.mode == 0);
    summary_render();
}
static void cmd_broker(int id) {
    g_app.broker_mode = (id == IDC_BRK_ONE);
    CheckRadioButton(g_wnd, IDC_BRK_ANY, IDC_BRK_ONE, id);
    EnableWindow(GetDlgItem(g_wnd, IDC_BROKER), g_app.broker_mode == 1);
    summary_render();
}
static void cmd_account(int id) {
    g_app.account_mode = (id == IDC_ACC_ONE);
    CheckRadioButton(g_wnd, IDC_ACC_ALL, IDC_ACC_ONE, id);
    EnableWindow(GetDlgItem(g_wnd, IDC_ACCOUNT), g_app.account_mode == 1);
    summary_render();
}

static void on_edit(int id) {
    char buf[300];
    GetDlgItemTextA(g_wnd, id, buf, sizeof(buf));
    switch (id) {
        case IDC_LICENSEE: strncpy(g_app.licensee, buf, sizeof(g_app.licensee)-1); break;
        case IDC_BROKER:   strncpy(g_app.broker, buf, sizeof(g_app.broker)-1);   break;
        case IDC_ACCOUNT:  strncpy(g_app.account, buf, sizeof(g_app.account)-1); break;
        case IDC_DAYS:     strncpy(g_app.days, buf, sizeof(g_app.days)-1);       break;
        case IDC_UNTIL:    strncpy(g_app.until, buf, sizeof(g_app.until)-1);     break;
    }
    summary_render();
}

/* ========================================================================== */
/* Creación de la UI (todo por código)                                        */
/* ========================================================================== */
static void add_label(const char *text, int x, int y, int w, int ht, int bold) {
    HWND hL = CreateWindowA("STATIC", text, WS_CHILD | WS_VISIBLE, x, y, w, ht,
                           g_wnd, NULL, g_hinst, NULL);
    SendMessage(hL, WM_SETFONT, (WPARAM)(bold ? g_font_title : g_font_ui), TRUE);
}

static HWND add_edit(int id, const char *def, int x, int y, int w, int ht, BOOL multi) {
    DWORD style = WS_CHILD | WS_VISIBLE | (multi ? (ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL) : ES_AUTOHSCROLL);
    HWND hE = CreateWindowA("EDIT", def, style | WS_BORDER, x, y, w, ht,
                           g_wnd, (HMENU)(INT_PTR)id, g_hinst, NULL);
    SendMessage(hE, WM_SETFONT, (WPARAM)g_font_ui, TRUE);
    return hE;
}

static HWND add_button(const char *text, int id, int x, int y, int w, int ht, int accent) {
    HWND hB = CreateWindowA("BUTTON", text, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, x, y, w, ht,
                           g_wnd, (HMENU)(INT_PTR)id, g_hinst, NULL);
    SendMessage(hB, WM_SETFONT, (WPARAM)g_font_ui, TRUE);
    (void)accent;
    return hB;
}

static HWND add_radio(const char *text, int id, int x, int y, int w, int ht, int group) {
    HWND hR = CreateWindowA("BUTTON", text,
                           WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | (group ? WS_GROUP : 0),
                           x, y, w, ht, g_wnd, (HMENU)(INT_PTR)id, g_hinst, NULL);
    SendMessage(hR, WM_SETFONT, (WPARAM)g_font_ui, TRUE);
    return hR;
}

static void CreateUI(void) {
    int y;

    /* encabezado */
    HWND hT = CreateWindowA("STATIC", "BCFX  ·  Activador de Licencias MQL4 / MQL5",
                            WS_CHILD | WS_VISIBLE, 28, 18, 620, 34, g_wnd, NULL, g_hinst, NULL);
    SendMessage(hT, WM_SETFONT, (WPARAM)g_font_title, TRUE);
    add_label("Windows 10 / 11   ·   100 % local, sin internet   ·   v" APP_VERSION, 28, 52, 620, 20, 0);

    /* -- 1) EA -- */
    y = 84;
    add_label("1)  ARCHIVO DEL ASESOR EXPERTO (.mq4 / .mq5)", 28, y, 620, 20, 1);
    add_button("ABRIR EA ...", IDC_OPEN, 28, y + 26, 150, 36, 0);
    CreateWindowA("STATIC", "Ningún EA cargado", WS_CHILD | WS_VISIBLE,
                  196, y + 33, 452, 22, g_wnd, (HMENU)(INT_PTR)IDC_FILE_STATUS, g_hinst, NULL);

    /* -- separador -- */
    y = 162;
    CreateWindowA("STATIC", "", WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ, 28, y, 620, 2, g_wnd, NULL, g_hinst, NULL);

    /* -- 2) datos -- */
    y = 176;
    add_label("2)  DATOS DE LA ACTIVACIÓN", 28, y, 620, 20, 1);

    add_label("Nombre de la persona:", 28, y + 30, 200, 18, 0);
    add_edit(IDC_LICENSEE, g_app.licensee, 28, y + 48, 300, 26, FALSE);

    add_label("Tipo de licencia:", 356, y + 30, 200, 18, 0);
    add_radio("Por tiempo", IDC_MODE_T, 356, y + 46, 130, 20, 1);
    add_radio("Ilimitada",  IDC_MODE_U, 496, y + 46, 130, 20, 0);

    add_label("Vigencia (días):", 28, y + 84, 200, 18, 0);
    add_edit(IDC_DAYS, g_app.days, 28, y + 102, 140, 26, FALSE);
    add_label("o fecha límite (AAAA-MM-DD):", 196, y + 84, 260, 18, 0);
    add_edit(IDC_UNTIL, g_app.until, 196, y + 102, 180, 26, FALSE);

    add_label("Broker:", 28, y + 140, 200, 18, 0);
    add_radio("Cualquier broker", IDC_BRK_ANY, 28, y + 158, 180, 20, 1);
    add_radio("Un broker específico", IDC_BRK_ONE, 216, y + 158, 200, 20, 0);
    add_edit(IDC_BROKER, g_app.broker, 28, y + 184, 380, 26, FALSE);
    add_label("(Mismo nombre que muestra AccountCompany() en MT4 / ACCOUNT_COMPANY en MT5)", 28, y + 214, 620, 16, 0);

    add_label("Cuenta:", 28, y + 236, 200, 18, 0);
    add_radio("Todas las cuentas", IDC_ACC_ALL, 28, y + 254, 180, 20, 1);
    add_radio("Una cuenta específica", IDC_ACC_ONE, 216, y + 254, 200, 20, 0);
    add_edit(IDC_ACCOUNT, g_app.account, 28, y + 280, 200, 26, FALSE);

    /* -- resumen -- */
    CreateWindowA("STATIC", "(resumen)", WS_CHILD | WS_VISIBLE,
                  440, y + 84, 208, 220, g_wnd, (HMENU)(INT_PTR)IDC_SUMMARY, g_hinst, NULL);

    /* -- botón generar -- */
    add_button("ACTIVAR LICENCIA Y PROTEGER EA", IDC_GENERATE, 28, y + 320, 340, 44, 1);

    /* -- 3) resultado -- */
    y = 560;
    add_label("3)  LICENCIA GENERADA", 28, y, 620, 20, 1);
    add_label("Licencia (para pegar en BCFX_LICENCIA):", 28, y + 28, 620, 18, 0);
    CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_READONLY,
                  28, y + 48, 560, 30, g_wnd, (HMENU)(INT_PTR)IDC_LIC, g_hinst, NULL);
    add_button("Copiar licencia", IDC_COPY, 600, y + 48, 48, 30, 0);
    CreateWindowA("STATIC", "", WS_CHILD | WS_VISIBLE,
                  28, y + 84, 620, 20, g_wnd, (HMENU)(INT_PTR)IDC_RESULT, g_hinst, NULL);
    add_button("Guardar EA protegido", IDC_SAVE_EA, 28, y + 112, 300, 40, 1);
    add_button("Guardar archivo .lic", IDC_SAVE_LIC, 348, y + 112, 300, 40, 0);

    summary_render();
}

/* ========================================================================== */
/* Window proc                                                                 */
/* ========================================================================== */
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE:
            g_wnd = hwnd;
            CreateUI();
            return 0;

        case WM_CTLCOLORSTATIC: {
            HDC dc = (HDC)wp;
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, COL_TXT);
            return (LRESULT)GetStockObject(HOLLOW_BRUSH);
        }
        case WM_CTLCOLOREDIT: {
            HDC dc = (HDC)wp;
            SetBkColor(dc, COL_BG2);
            SetTextColor(dc, COL_TXT);
            return (LRESULT)CreateSolidBrush(COL_BG2);
        }
        case WM_ERASEBKGND: {
            RECT rc; GetClientRect(hwnd, &rc);
            paint_bg((HDC)wp, &rc);
            return 1;
        }
        case WM_COMMAND: {
            int id = LOWORD(wp), code = HIWORD(wp);
            switch (id) {
                case IDC_OPEN:       do_open(); break;
                case IDC_GENERATE:   do_generate(); break;
                case IDC_SAVE_EA:    do_save_ea(); break;
                case IDC_SAVE_LIC:   do_save_lic(); break;
                case IDC_COPY:       do_copy(); break;
                case IDC_MODE_T: case IDC_MODE_U:      cmd_mode(id); break;
                case IDC_BRK_ANY: case IDC_BRK_ONE:    cmd_broker(id); break;
                case IDC_ACC_ALL: case IDC_ACC_ONE:    cmd_account(id); break;
                case IDC_LICENSEE: case IDC_BROKER: case IDC_ACCOUNT:
                case IDC_DAYS: case IDC_UNTIL:
                    if (code == EN_CHANGE) on_edit(id);
                    break;
            }
            return 0;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

/* ========================================================================== */
/* Entry point                                                                 */
/* ========================================================================== */
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrev; (void)lpCmdLine;
    g_hinst = hInst;

    ZeroMemory(&g_app, sizeof(g_app));
    strcpy(g_app.days, "30");
    prefs_load();

    /* fuentes */
    g_font_title = CreateFontA(-22, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, "Segoe UI");
    g_font_ui    = CreateFontA(-15, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, "Segoe UI");
    g_font_mono  = CreateFontA(-13, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, "Consolas");

    /* clase de ventana */
    WNDCLASSA wc; ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = "BCFXActivatorWnd";
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    RegisterClassA(&wc);

    int w = 676, hgt = 800;
    RECT rc = {0, 0, w, hgt};
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME ^ WS_MAXIMIZEBOX, FALSE);
    g_wnd = CreateWindowA("BCFXActivatorWnd", "BCFX Activador de Licencias MQL4/MQL5",
        WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME ^ WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top,
        NULL, NULL, hInst, NULL);
    if (!g_wnd) return 1;

    /* motor JS */
    if (js_init() != 0) {
        MessageBoxA(g_wnd, "No se pudo iniciar el motor interno.", "BCFX", MB_ICONERROR);
        return 1;
    }

    ShowWindow(g_wnd, nCmdShow);
    UpdateWindow(g_wnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    js_close();
    return (int)msg.wParam;
}
