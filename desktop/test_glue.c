/* test_glue.c - Verifica en Linux el MISMO puente C->JS que usa win.c en Windows.
   Compila: gcc -I desktop/deps/quickjs -o /tmp/test_glue test_glue.c \
            desktop/deps/quickjs/libquickjs.a -lm -lpthread -ldl -I desktop
   Incluye js_embed.h (generado) y emula js_init + js_generate de win.c.
*/
#include "quickjs.h"
#include "js_embed.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef struct {
    char ea_source[1024*1024];
    char licensee[128];
    char broker[256];
    char account[128];
    char days[32];
    char until[64];
    int mode, broker_mode, account_mode;
    char license[256];
    char out_source[1024*1024];
    int have_result;
} App;

static JSRuntime *rt;
static JSContext *ctx;

static int gen(App *a) {
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue bcfx = JS_GetPropertyStr(ctx, global, "Bcfx");
    if (JS_IsUndefined(bcfx)) { fprintf(stderr, "Bcfx undefined\n"); return -1; }

    JSValue fn_gen = JS_GetPropertyStr(ctx, bcfx, "genLicense");
    JSValue fn_inj = JS_GetPropertyStr(ctx, bcfx, "inject");
    JSValue opts = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, opts, "licensee", JS_NewString(ctx, a->licensee));
    JS_SetPropertyStr(ctx, opts, "mode",     JS_NewString(ctx, a->mode ? "U" : "T"));
    JS_SetPropertyStr(ctx, opts, "broker",   JS_NewString(ctx, a->broker_mode ? a->broker : "**"));
    JS_SetPropertyStr(ctx, opts, "account",  JS_NewString(ctx, a->account_mode ? a->account : "0"));
    if (!a->mode) {
        JS_SetPropertyStr(ctx, opts, "until", JS_NewString(ctx, a->until));
        JS_SetPropertyStr(ctx, opts, "days",  JS_NewString(ctx, a->days));
    }

    JSValue licVal = JS_Call(ctx, fn_gen, bcfx, 1, (JSValueConst *)&opts);
    JS_FreeValue(ctx, opts);
    if (JS_IsException(licVal)) { JSValue e=JS_GetException(ctx); const char*s=JS_ToCString(ctx,e); fprintf(stderr,"JS exc: %s\n", s?s:"?"); return -1; }

    const char *lic = JS_ToCString(ctx, JS_GetPropertyStr(ctx, licVal, "lic"));
    strncpy(a->license, lic, 255);
    JS_FreeCString(ctx, lic);

    JSValue args[3];
    args[0] = JS_NewString(ctx, a->ea_source);
    args[1] = licVal;
    args[2] = JS_NewString(ctx, a->license);
    JSValue injRes = JS_Call(ctx, fn_inj, bcfx, 3, args);
    JS_FreeValue(ctx, args[0]); JS_FreeValue(ctx, args[2]);
    if (JS_IsException(injRes)) { fprintf(stderr, "inject exc\n"); return -1; }

    const char *src = JS_ToCString(ctx, JS_GetPropertyStr(ctx, injRes, "source"));
    size_t sl = strlen(src); if (sl > sizeof(a->out_source)-1) sl = sizeof(a->out_source)-1;
    memcpy(a->out_source, src, sl); a->out_source[sl]=0;
    JS_FreeCString(ctx, src);
    a->have_result = 1;
    JS_FreeValue(ctx, injRes); JS_FreeValue(ctx, licVal);
    JS_FreeValue(ctx, fn_gen); JS_FreeValue(ctx, fn_inj);
    JS_FreeValue(ctx, bcfx); JS_FreeValue(ctx, global);
    return 0;
}

int main(void) {
    rt = JS_NewRuntime();
    ctx = JS_NewContext(rt);
    JS_SetMaxStackSize(rt, 4*1024*1024);
    JS_SetMemoryLimit(rt, 256*1024*1024);

    JSValue r1 = JS_Eval(ctx, EMBED_CRYPTO_JS, EMBED_CRYPTO_JS_LEN, "crypto.js", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(r1)) { fprintf(stderr,"crypto eval fail\n"); return 2; }
    JS_FreeValue(ctx, r1);
    JSValue r2 = JS_Eval(ctx, EMBED_MQL_JS, EMBED_MQL_JS_LEN, "mql.js", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(r2)) { fprintf(stderr,"mql eval fail\n"); return 3; }
    JS_FreeValue(ctx, r2);

    App a; memset(&a,0,sizeof(a));
    strcpy(a.licensee, "Juan Perez");
    strcpy(a.broker, "ICMarkets");
    strcpy(a.account, "12345678");
    strcpy(a.days, "30");
    a.mode = 0; a.broker_mode = 1; a.account_mode = 1;
    strcpy(a.ea_source,
      "//+----------------------------------+\n"
      "#property copyright \"Test\"\n"
      "int OnInit()\n  {\n   return(INIT_SUCCEEDED);\n  }\n"
      "void OnTick()\n  {\n   Comment(\"x\");\n  }\n");

    if (gen(&a) != 0) { fprintf(stderr, "gen fail\n"); return 4; }

    printf("LIC=%s\n", a.license);
    printf("LEN_LIC=%zu\n", strlen(a.license));
    printf("INJ_OK=%d\n", strstr(a.out_source, "if(BCFX_F60()!=0) return(-1);") != NULL);
    printf("INJ_LIC_EMBEDED=%d\n", strstr(a.out_source, a.license) != NULL);
    printf("INJ_MARKER=%d\n", strstr(a.out_source, "//BCFX_ACTIVADOR_INICIO") != NULL);

    JS_FreeContext(ctx); JS_FreeRuntime(rt);
    return 0;
}
