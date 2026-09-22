/* test_glue.c - Verifica en Linux el MISMO puente C->JS que usa win.c en Windows.
   Compila: gcc -I desktop/deps/quickjs -I desktop -o /tmp/test_glue test_glue.c \
                 desktop/deps/quickjs/quickjs.c desktop/deps/quickjs/cutils.c \
                 desktop/deps/quickjs/libregexp.c desktop/deps/quickjs/libunicode.c \
                 desktop/deps/quickjs/dtoa.c -lm -lpthread -ldl
*/
#define CONFIG_VERSION "test"
#include "deps/quickjs/quickjs.h"
#include "js_embed.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(void) {
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);
    JS_SetMaxStackSize(rt, 4*1024*1024);
    JS_SetMemoryLimit(rt, 256*1024*1024);

    JSValue r1 = JS_Eval(ctx, EMBED_CRYPTO_JS, EMBED_CRYPTO_JS_LEN, "crypto.js", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(r1)) { fprintf(stderr,"crypto eval fail\n"); return 2; }
    JS_FreeValue(ctx, r1);
    JSValue r2 = JS_Eval(ctx, EMBED_MQL_JS, EMBED_MQL_JS_LEN, "mql.js", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(r2)) { fprintf(stderr,"mql eval fail\n"); return 3; }
    JS_FreeValue(ctx, r2);

    /* --- generar licencia (igual que js_generate en win.c) --- */
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue bcfx = JS_GetPropertyStr(ctx, global, "Bcfx");
    JSValue fn = JS_GetPropertyStr(ctx, bcfx, "genLicense");
    JSValue opts = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, opts, "licensee", JS_NewString(ctx, "Juan Perez"));
    JS_SetPropertyStr(ctx, opts, "mode",     JS_NewString(ctx, "T"));
    JS_SetPropertyStr(ctx, opts, "broker",   JS_NewString(ctx, "ICMarkets"));
    JS_SetPropertyStr(ctx, opts, "account",  JS_NewString(ctx, "0"));
    JS_SetPropertyStr(ctx, opts, "days",     JS_NewString(ctx, "30"));
    JSValue licVal = JS_Call(ctx, fn, bcfx, 1, (JSValueConst *)&opts);
    JS_FreeValue(ctx, opts);
    if (JS_IsException(licVal)) { const char*s=JS_ToCString(ctx, JS_GetException(ctx)); fprintf(stderr, "JS exc: %s\n", s?s:"?"); return 4; }

    JSValue vLic = JS_GetPropertyStr(ctx, licVal, "lic");
    const char *lic = JS_ToCString(ctx, vLic);
    JS_FreeValue(ctx, vLic);
    printf("LIC=%s\n", lic ? lic : "(null)");
    printf("LEN=%zu\n", lic ? strlen(lic) : 0);

    /* --- inyectar --- */
    char ea[1024];
    snprintf(ea, sizeof(ea),
      "#property copyright \"Test\"\nint OnInit()\n  {\n   return(INIT_SUCCEEDED);\n  }\nvoid OnTick()\n  {\n   Comment(\"x\");\n  }\n");
    JSValue ifn = JS_GetPropertyStr(ctx, bcfx, "inject");
    JSValue args[3];
    args[0] = JS_NewString(ctx, ea);
    args[1] = licVal;
    args[2] = JS_NewString(ctx, lic);
    JSValue ir = JS_Call(ctx, ifn, bcfx, 3, args);
    JS_FreeValue(ctx, args[0]); JS_FreeValue(ctx, args[2]);
    if (JS_IsException(ir)) { fprintf(stderr, "inject exc\n"); return 5; }
    JSValue vS = JS_GetPropertyStr(ctx, ir, "source");
    const char *src = JS_ToCString(ctx, vS);
    JS_FreeValue(ctx, vS);
    printf("INJ_OK=%d\n", src && strstr(src, "if(BCFX_F60()!=0) return(-1);") != NULL);
    printf("INJ_MARKER=%d\n", src && strstr(src, "//BCFX_ACTIVADOR_INICIO") != NULL);
    printf("INJ_LIC=%d\n", src && lic && strstr(src, lic) != NULL);
    JS_FreeCString(ctx, src);
    JS_FreeValue(ctx, ir);
    JS_FreeValue(ctx, licVal);
    JS_FreeCString(ctx, lic);
    JS_FreeValue(ctx, fn); JS_FreeValue(ctx, ifn);
    JS_FreeValue(ctx, bcfx); JS_FreeValue(ctx, global);

    JS_FreeContext(ctx); JS_FreeRuntime(rt);
    return 0;
}
