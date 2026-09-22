/* mql.js - Generador del Activador de Licencias BCFX (MQL4 / MQL5)
   ----------------------------------------------------------------
   Produce el bloque de proteccion & activacion (SHA-256 + HMAC-SHA256)
   que se inyecta en el EA, mas el archivo de licencia (.lic) y el
   texto a pegar en el parametro BCFX_LICENCIA.

   Toda la criptografia es SHA-256/HMAC-SHA256 identica entre JS (web)
   y MQL4/5 (impl. local del EA), por lo que la firma calculada aqui se
   verifica dentro del EA con exactamente el mismo resultado.

   La plantilla del bloque usa marcadores @PLACEHOLDER@ para que pueda
   embeberse también en la aplicacion de escritorio (desktop/), que es
   una sola fuente de verdad compartida (ver tools/gen-desktop.js).
*/
'use strict';

(function (root) {
  const c = (typeof require === 'function' && typeof module !== 'undefined')
    ? require('./crypto.js')
    : (root.BcfxCrypto || (typeof globalThis !== 'undefined' ? globalThis.BcfxCrypto : null));

  // ---------- constantes compartidas ----------
  const MAGIC = 'BCFX2024';
  const KEY   = 'BCFX-7k9Xq2Tz8vLm4nP-2024';   // llave maestra del activador (ASCII)
  const M_ACT = '//BCFX_ACTIVADOR_INICIO';      // marcador del bloque del activador
  const M_ACT_END = '//BCFX_ACTIVADOR_FIN';
  const NAME_MAX = 28;                          // máximo de caracteres del nombre

  // ---------- utilidades JS ----------
  function sha256Hex(str) { return c.sha256Hex(str); }
  function hmacHex(key, msg) { return c.hmacSha256Hex(key, msg); }

  function crc8(s) {
    let r = 0;
    for (let i = 0; i < s.length; i++) {
      r ^= (s.charCodeAt(i) & 0xff);
      for (let b = 0; b < 8; b++) r = (r & 1) ? ((r >> 1) ^ 0x8c) : (r >> 1);
      r &= 0xff;
    }
    return r;
  }

  function hexEnc(s) {
    let o = '';
    for (let i = 0; i < s.length; i++) {
      o += ('0' + (s.charCodeAt(i) & 0xff).toString(16)).slice(-2);
    }
    return o.toUpperCase();
  }

  function hex8(n) {
    return ('0000000' + (n >>> 0).toString(16)).slice(-8).toUpperCase();
  }
  function hex2(n) {
    return ('0' + (n & 0xff).toString(16)).slice(-2).toUpperCase();
  }
  function padRight(s, n, ch) {
    while (s.length < n) s += ch;
    return s.slice(0, n);
  }

  // Pliega a ASCII de forma determinista (igual en web y en el .exe/QuickJS).
  // Reemplaza acentos y caracteres latinos comunes; sin depender de
  // String.prototype.normalize (que no existe en todos los motores).
  const FOLD = {
    'á':'a', 'à':'a', 'â':'a', 'ä':'a', 'ã':'a', 'å':'a', 'ā':'a',
    'é':'e', 'è':'e', 'ê':'e', 'ë':'e', 'ē':'e',
    'í':'i', 'ì':'i', 'î':'i', 'ï':'i', 'ī':'i',
    'ó':'o', 'ò':'o', 'ô':'o', 'ö':'o', 'õ':'o', 'ø':'o', 'ō':'o',
    'ú':'u', 'ù':'u', 'û':'u', 'ü':'u', 'ū':'u',
    'ñ':'n', 'Ñ':'N', 'ç':'c', 'Ç':'C', 'ý':'y',
    'Á':'A', 'À':'A', 'Â':'A', 'Ä':'A', 'Ã':'A', 'Å':'A', 'Ā':'A',
    'É':'E', 'È':'E', 'Ê':'E', 'Ë':'E', 'Ē':'E',
    'Í':'I', 'Ì':'I', 'Î':'I', 'Ï':'I', 'Ī':'I',
    'Ó':'O', 'Ò':'O', 'Ô':'O', 'Ö':'O', 'Õ':'O', 'Ø':'O', 'Ō':'O',
    'Ú':'U', 'Ù':'U', 'Û':'U', 'Ü':'U', 'Ū':'U',
    'Ý':'Y',
  };
  function asciiFold(s) {
    let out = '';
    const str = String(s);
    for (let i = 0; i < str.length; i++) {
      const ch = str[i];
      out += Object.prototype.hasOwnProperty.call(FOLD, ch) ? FOLD[ch] : ch;
    }
    return out;
  }

  function replaceAll(str, find, repl) {
    return str.split(find).join(repl);
  }

  // ---------- generación de la licencia ----------
  function calcExpiry(utcNow, opts) {
    if (opts.mode === 'U') return 0xffffffff;
    let e;
    if (opts.until) e = Math.floor(Date.parse(opts.until) / 1000);
    else if (opts.days) e = utcNow + Math.max(3600, Math.round(Number(opts.days) * 86400));
    else throw new Error('Falta la vigencia (días o fecha límite)');
    return Math.min(e, 0xfffffffe);
  }

  /**
   * opts = {
   *   mode:      'T' | 'U'
   *   licensee:  nombre de la persona (máx. 28 caracteres)
   *   broker:    '**' (cualquier broker) o el nombre del broker
   *   account:   '' / 0 => todas las cuentas | numero => cuenta única
   *   days:      nº de días (solo modo T)
   *   until:     fecha límite ISO (solo modo T)
   *   salt:      hex del salt (opcional)
   *   utcNow:    timestamp de referencia (tests deterministas)
   * }
   */
  function genLicense(opts) {
    const utcNow = Math.floor(opts.utcNow || Date.now() / 1000);
    const licensee = asciiFold((opts.licensee || '').trim()).replace(/[^A-Za-z0-9 ._-]/g, '');
    if (!licensee) throw new Error('Se requiere el nombre del licenciatario');
    if (/[*;=]/.test(licensee)) throw new Error('El nombre no puede contener * ; =');
    if (licensee.length > NAME_MAX) throw new Error('El nombre no puede superar ' + NAME_MAX + ' caracteres');

    const mode = opts.mode === 'U' ? 'U' : 'T';
    const salt = (opts.salt && /^[0-9a-fA-F]+$/.test(opts.salt)) ? opts.salt : randomSalt();

    let broker = '**';
    if (opts.broker && opts.broker !== '**') {
      broker = asciiFold(opts.broker).trim().toLowerCase();
      if (!broker) broker = '**';
    }

    let account = '0';
    if (opts.account && String(opts.account) !== '0' && String(opts.account) !== '') {
      account = String(opts.account).trim();
    }

    const exp = calcExpiry(utcNow, opts);
    const tHex = hex8(exp);

    const inner = tHex + licensee;
    const id20 = hex2(crc8(inner)) + sha256Hex(inner).toUpperCase().slice(0, 18);

    const pad = 'tl=' + tHex + ';tm=' + mode + ';ac=' + account + ';br=' + broker +
                ';nm=' + licensee + ';sl=' + salt + ';mg=' + MAGIC;

    const sigA = hmacHex(KEY, pad).toUpperCase();
    const sigB = hmacHex(KEY, pad + id20).toUpperCase().slice(0, 8);
    const idd  = sha256Hex(pad + id20).toUpperCase();

    const name28 = padRight(licensee, 28, '*');
    const lic = 'B-' + tHex + '-' + id20 + name28 + sigB;

    return {
      mode, licensee, broker, account, exp, tHex, salt,
      id20, pad, sigA, sigB, idd,
      lic, name28,
    };
  }

  function randomSalt() {
    let s = '';
    for (let i = 0; i < 16; i++) s += '0123456789abcdef'[Math.floor(Math.random() * 16)];
    return s;
  }

  // ---------- plantilla del bloque activador (fuente única de verdad) ----------
  //
  // Marcadores que sustituye tanto la web como el .exe:
  //   @K@  llave          @X1@ salt   @X3@ firma A   @X4@ firma B
  //   @MM@ magic          @X5@ idd    @X6@ modo      @X7@ broker
  //   @X8@ cuenta         @LIC@ licencia (sin comillas) o cadena vacía
  function buildActivatorTemplate() {
    return `//BCFX_ACTIVADOR_INICIO
//=================================================================
//   BCFX License Activator  (MQL4 / MQL5)
//   Proteccion por: TIEMPO / BROKER / CUENTA / ILIMITADO
//   Si el EA se manipula o la licencia no corresponde, se desactiva.
//=================================================================
input string BCFX_LICENCIA = "@LIC@";   // Licencia entregada (o pegar aqui el .lic)

string BCFX_K   = "@K@";   // llave (hex)
string BCFX_MM  = "@MM@";   // magic
string BCFX_X1  = "@X1@";   // salt
string BCFX_X3  = "@X3@";   // firma A
string BCFX_X4  = "@X4@";   // firma B
string BCFX_X5  = "@X5@";   // integridad interna
string BCFX_X6  = "@X6@";   // modo T / U
string BCFX_X7  = "@X7@";   // broker (o 2A2A = **)
string BCFX_X8  = "@X8@";   // cuenta (o 30 = 0)

//=================== utilidades (sin dependencias de plataforma) ===================
long BCFX_HL(string h){   // hex -> long  (soporta FFFFFFFF sin desborde de int)
   long v=0;
   int n=StringLen(h);
   for(int i=0;i<n;i++){
      int cc=StringGetCharacter(h,i);
      int d=0;
      if(cc>=48 && cc<=57) d=cc-48;
      else if(cc>=65 && cc<=70) d=cc-55;
      else if(cc>=97 && cc<=102) d=cc-87;
      v=v*16+d;
   }
   return(v);
}

int BCFX_HI(string h){ return((int)BCFX_HL(h)); }   // hex -> int (byte)

string BCFX_DH(string h){   // hex -> string (bytes ASCII)
   string r="";
   int n=StringLen(h);
   for(int i=0;i+1<n;i=i+2){
      r = r + CharToString((short)(BCFX_HI(StringSubstr(h,i,2)) & 0xFF));
   }
   return(r);
}

string BCFX_HX(){ return("0123456789ABCDEF"); }

string BCFX_H2X(uint x){   // word (32 bits) -> 8 hex
   string h=BCFX_HX();
   string r="";
   for(int s=28;s>=0;s=s-4)
      r = r + CharToString((short)StringGetCharacter(h,(int)((x>>s)&15)));
   return(r);
}

string BCFX_H2B(int x){    // byte -> 2 hex
   string h=BCFX_HX();
   return(CharToString((short)StringGetCharacter(h,((x>>4)&15))) +
          CharToString((short)StringGetCharacter(h,(x&15))));
}

string BCFX_H2(uint &w[], int cnt){   // array de words -> hex
   string s="";
   for(int i=0;i<cnt;i++) s=s+BCFX_H2X(w[i]);
   return(s);
}

string BCFX_UC(string s){  // ASCII -> mayusculas (MQL4/5 seguro)
   string r="";
   int n=StringLen(s);
   for(int i=0;i<n;i++){
      int cc=StringGetCharacter(s,i);
      if(cc>=97 && cc<=122) cc=cc-32;
      r=r+CharToString((short)cc);
   }
   return(r);
}

string BCFX_LC(string s){  // ASCII -> minusculas
   string r="";
   int n=StringLen(s);
   for(int i=0;i<n;i++){
      int cc=StringGetCharacter(s,i);
      if(cc>=65 && cc<=90) cc=cc+32;
      r=r+CharToString((short)cc);
   }
   return(r);
}

string BCFX_TR(string s){  // recorta espacios al inicio y final
   int a=0;
   int b=StringLen(s)-1;
   while(a<=b && StringGetCharacter(s,a)<=32) a++;
   while(b>=a && StringGetCharacter(s,b)<=32) b--;
   if(a>b) return("");
   return(StringSubstr(s,a,b-a+1));
}

uint BCFX_RR(uint x,int n){ return((x >> n) | (x << (32 - n))); }

uint BCFX_K0[64] = {
 0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
 0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
 0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
 0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
 0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
 0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
 0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
 0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2 };

// hash núcleo SHA-256 sobre bytes (con padding estandar)
void BCFX_SHC(uchar &b[], int n, uint &h8[]){
   int blks = (int)((n + 8) / 64) + 1;
   uchar m[];
   ArrayResize(m, blks*64);
   for(int i=0;i<blks*64;i++) m[i]=0;   // cero explicito (MQL4 no inicializa)
   for(int i=0;i<n;i++) m[i]=b[i];
   m[n]=0x80;
   uint bl=(uint)(n*8);
   int last=blks*64;
   m[last-1]=(uchar)(bl & 0xFF);
   m[last-2]=(uchar)((bl>>8) & 0xFF);
   m[last-3]=(uchar)((bl>>16) & 0xFF);
   m[last-4]=(uchar)((bl>>24) & 0xFF);

   uint h0=0x6a09e667,h1=0xbb67ae85,h2=0x3c6ef372,h3=0xa54ff53a;
   uint h4=0x510e527f,h5=0x9b05688c,h6=0x1f83d9ab,h7=0x5be0cd19;

   for(int off=0; off<blks*64; off+=64){
      uint w[64];
      for(int i=0;i<16;i++){
         w[i]=((uint)m[off+i*4]<<24)|((uint)m[off+i*4+1]<<16)|((uint)m[off+i*4+2]<<8)|((uint)m[off+i*4+3]);
      }
      for(int i=16;i<64;i++){
         uint s0=BCFX_RR(w[i-15],7) ^ BCFX_RR(w[i-15],18) ^ (w[i-15]>>3);
         uint s1=BCFX_RR(w[i-2],17) ^ BCFX_RR(w[i-2],19) ^ (w[i-2]>>10);
         w[i]=w[i-16]+s0+w[i-7]+s1;
      }
      uint a=h0,b=h1,c=h2,d=h3,e=h4,f=h5,g=h6,h=h7;
      for(int i=0;i<64;i++){
         uint S1=BCFX_RR(e,6) ^ BCFX_RR(e,11) ^ BCFX_RR(e,25);
         uint ch=(e & f) ^ (~e & g);
         uint t1=h+S1+ch+BCFX_K0[i]+w[i];
         uint S0=BCFX_RR(a,2) ^ BCFX_RR(a,13) ^ BCFX_RR(a,22);
         uint mj=(a & b) ^ (a & c) ^ (b & c);
         uint t2=S0+mj;
         h=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
      }
      h0+=a; h1+=b; h2+=c; h3+=d; h4+=e; h5+=f; h6+=g; h7+=h;
   }
   h8[0]=h0; h8[1]=h1; h8[2]=h2; h8[3]=h3;
   h8[4]=h4; h8[5]=h5; h8[6]=h6; h8[7]=h7;
}

void BCFX_SHS(string s, uint &h8[]){   // SHA-256 de un string
   uchar b[];
   int n=StringToCharArray(s, b, 0, StringLen(s));
   BCFX_SHC(b, n, h8);
}

void BCFX_HMC(string key, string msg, uint &o8[]){   // HMAC-SHA256
   int BL=64;
   uchar kk[64];
   for(int i=0;i<64;i++) kk[i]=0;
   uchar kb[];
   int kl=StringToCharArray(key, kb, 0, StringLen(key));
   if(kl > 64){
      uint t8[8];
      BCFX_SHC(kb, kl, t8);
      for(int i=0;i<32;i++) kk[i]=(uchar)((t8[i/4] >> ((3-(i%4))*8)) & 0xFF);
   } else {
      for(int i=0;i<kl;i++) kk[i]=kb[i];
   }
   uchar ip[64], op[64];
   for(int i=0;i<64;i++){ ip[i]=(uchar)(kk[i] ^ 0x36); op[i]=(uchar)(kk[i] ^ 0x5c); }

   uchar mb[];
   int ml=StringToCharArray(msg, mb, 0, StringLen(msg));

   uchar mid[];
   ArrayResize(mid, 64+ml);
   for(int i=0;i<64;i++) mid[i]=ip[i];
   for(int i=0;i<ml;i++) mid[64+i]=mb[i];

   uint ih[8];
   BCFX_SHC(mid, 64+ml, ih);

   uchar fin[];
   ArrayResize(fin, 96);
   for(int i=0;i<64;i++) fin[i]=op[i];
   for(int i=0;i<32;i++) fin[64+i]=(uchar)((ih[i/4] >> ((3-(i%4))*8)) & 0xFF);

   BCFX_SHC(fin, 96, o8);
}

int BCFX_CR8(string s){   // CRC-8 (polinomio 0x8C)
   int c1=0;
   int n=StringLen(s);
   for(int i=0;i<n;i++){
      c1 = c1 ^ (int)StringGetCharacter(s,i);
      for(int b=0;b<8;b++) c1 = ((c1 & 1)!=0) ? ((c1>>1) ^ 0x8C) : (c1>>1);
      c1 = c1 & 0xFFFF;
   }
   return((int)(c1 & 0xFF));
}

datetime BCFX_RES(long v){   // timestamp (long) -> datetime
   if(v >= 0xFFFFFFFF) return(0xFFFFFFFF);
   return((datetime)v);
}

//===================== estado =====================
int   BCFX_HITS=0;
bool  BCFX_BOOTED=false;
bool  BCFX_OK=false;
bool  BCFX_CHRONO=false;
bool  BCFX_TOLD=false;
long  BCFX_EXPD=0;

void BCFX_DIE(){
   // Autodestruccion ante manipulacion del codigo -> error critico / detiene el EA
   int zz=0;
   int vv=100/zz;         // "zero divide" -> error critico
   int aa[];
   vv = aa[5];            // acceso fuera de rango -> error critico
   while(true){ vv=vv+zz; }
}

void BCFX_NOTE(string rz){
   if(BCFX_TOLD) return;
   BCFX_TOLD=true;
   Alert("BCFX: " + rz);
}

int BCFX_G(string why){
   BCFX_HITS++;
   if(BCFX_HITS > 2) BCFX_DIE();   // fallo reiterado -> autodestruccion
   return(-1);
}

void BCFX_BOOT(){
   string salt = BCFX_DH(BCFX_X1);
   string sigA = BCFX_DH(BCFX_X3);
   string sigB = BCFX_DH(BCFX_X4);
   string idd  = BCFX_DH(BCFX_X5);
   string mode = BCFX_DH(BCFX_X6);
   string bkr  = BCFX_DH(BCFX_X7);
   string acc  = BCFX_DH(BCFX_X8);
   string key  = BCFX_DH(BCFX_K);

   string lic = BCFX_LICENCIA;
   if(StringLen(lic) < 67){ BCFX_OK=false; BCFX_NOTE("licencia ausente o danada"); return; }

   // ---- analisis de la licencia (offsets fijos) ----
   string ltl   = StringSubstr(lic,  2,  8);
   string lid   = StringSubstr(lic, 11, 20);
   string lname = StringSubstr(lic, 31, 28);
   string lgrd  = StringSubstr(lic, 59,  8);

   string nm = lname;
   while(StringLen(nm)>0 && StringSubstr(nm,StringLen(nm)-1,1)=="*")
      nm=StringSubstr(nm,0,StringLen(nm)-1);

   long tlexp = BCFX_HL(ltl);

   // ---- identidad: editar nombre/fecha rompe el id ----
   string inner = ltl + nm;
   uint tmp[8];
   BCFX_SHS(inner, tmp);
   string id_chk = BCFX_H2B(BCFX_CR8(inner)) + StringSubstr(BCFX_H2(tmp, 8), 0, 18);
   if(BCFX_UC(id_chk) != BCFX_UC(lid)){ BCFX_DIE(); BCFX_OK=false; BCFX_NOTE("nombre/fecha alterados"); return; }

   // ---- reconstruccion del pad (fuente unica de verdad) ----
   string pad = "tl=" + ltl + ";tm=" + mode + ";ac=" + acc + ";br=" + bkr +
                ";nm=" + nm + ";sl=" + salt + ";mg=" + BCFX_MM;

   // ---- firma A ----
   uint ha[8];
   BCFX_HMC(key, pad, ha);
   string sa = BCFX_H2(ha, 8);
   if(sa != sigA){ BCFX_DIE(); BCFX_OK=false; BCFX_NOTE("firma invalida"); return; }

   // ---- firma B (liga el id de la licencia) ----
   uint hb[8];
   BCFX_HMC(key, pad + lid, hb);
   string sb = StringSubstr(BCFX_H2(hb, 8), 0, 8);
   if(sb != sigB){ BCFX_DIE(); BCFX_OK=false; BCFX_NOTE("firma invalida"); return; }

   // ---- guard anti-edicion del .lic ----
   if(BCFX_UC(lgrd) != BCFX_UC(sb)){ BCFX_OK=false; BCFX_NOTE("licencia modificada"); return; }

   // ---- integridad interna ----
   uint hi[8];
   BCFX_SHS(pad + lid, hi);
   string si = BCFX_H2(hi, 8);
   if(si != idd){ BCFX_DIE(); BCFX_OK=false; BCFX_NOTE("codigo alterado"); return; }

   // ---- comprobaciones en vivo ----
   if(mode == "T"){
      BCFX_CHRONO=true;
      BCFX_EXPD=tlexp;
      if((long)TimeCurrent() >= tlexp){ BCFX_OK=false; BCFX_NOTE("licencia caducada"); return; }
   }

   if(bkr != "**"){
      string comp="";
#ifdef __MQL5__
      comp = AccountInfoString(ACCOUNT_COMPANY);
#else
      comp = AccountCompany();
#endif
      if(BCFX_LC(BCFX_TR(comp)) != bkr){ BCFX_OK=false; BCFX_NOTE("broker no autorizado"); return; }
   }

   if(acc != "0"){
      string acn="";
#ifdef __MQL5__
      acn = IntegerToString(AccountInfoInteger(ACCOUNT_LOGIN));
#else
      acn = IntegerToString(AccountNumber());
#endif
      if(acn != acc){ BCFX_OK=false; BCFX_NOTE("cuenta no autorizada"); return; }
   }

   BCFX_OK=true;
}

int BCFX_F60(){
   if(!BCFX_BOOTED){ BCFX_BOOT(); BCFX_BOOTED=true; }
   if(BCFX_OK){
      if(BCFX_CHRONO && ((long)TimeCurrent() >= BCFX_EXPD)) return(BCFX_G("licencia caducada"));
      return(0);
   }
   return(BCFX_G("licencia invalida"));
}
//=================================================================
//BCFX_ACTIVADOR_FIN
`;
  }

  function buildActivator(f, licValue) {
    let t = buildActivatorTemplate();
    t = replaceAll(t, '@K@', hexEnc(KEY));
    t = replaceAll(t, '@MM@', MAGIC);
    const lic = licValue ? String(licValue) : '';
    t = replaceAll(t, '@LIC@', lic);
    t = replaceAll(t, '@X1@', hexEnc(f.salt));
    t = replaceAll(t, '@X3@', hexEnc(f.sigA));
    t = replaceAll(t, '@X4@', hexEnc(f.sigB));
    t = replaceAll(t, '@X5@', hexEnc(f.idd));
    t = replaceAll(t, '@X6@', hexEnc(f.mode));
    t = replaceAll(t, '@X7@', hexEnc(f.broker));
    t = replaceAll(t, '@X8@', hexEnc(f.account));
    return t;
  }

  // ---------- inyección ----------
  function findBraceAfter(src, idx) {
    return src.indexOf('{', idx);
  }

  function injectOnce(src, pattern, line) {
    if (src.indexOf(line.trim()) >= 0) return src;
    const m = pattern.exec(src);
    if (!m) return src;
    const brace = findBraceAfter(src, m.index + m[0].length);
    if (brace < 0) return src;
    return src.slice(0, brace + 1) + '\n' + line + src.slice(brace + 1);
  }

  /**
   * opts: { source, f, lic }  ->  lic = valor a incrustar por defecto (opcional)
   * Devuelve { source, injected }.
   */
  function inject(source, f, lic) {
    let out = String(source);
    const block = buildActivator(f, lic);
    const starti = out.indexOf(M_ACT);
    const endi = out.indexOf(M_ACT_END);

    if (starti >= 0 && endi > starti) {
      out = out.slice(0, starti) + block + out.slice(endi + M_ACT_END.length);
    } else {
      const m = out.match(/\b(int\s+OnInit\s*\(|int\s+init\s*\()/);
      const pos = m ? m.index : out.indexOf('#import');
      out = out.slice(0, pos) + block + '\n' + out.slice(pos);
    }

    const before = out.includes('BCFX_F60()');
    const initLine = '   if(BCFX_F60()!=0) return(-1);   // BCFX';
    out = injectOnce(out, /OnInit\s*\(/, initLine);
    out = injectOnce(out, /(^|[^A-Za-z0-9_])init\s*\(/, initLine);
    out = injectOnce(out, /OnTick\s*\(/, '   if(BCFX_F60()!=0) return;   // BCFX');

    return { source: out, injected: !before && out.includes('BCFX_F60()') };
  }

  // ---------- EA de ejemplo ----------
  function sampleEA(f) {
    return `//+------------------------------------------------------------------+
//|                                        BCFX_Ejemplo_Protegido.mq4 |
//|                                  Demo protegida por BCFX Activator |
//+------------------------------------------------------------------+
#property copyright "BCFX Demo"
#property version   "1.00"
#property strict

int OnInit()
  {
   // aqui se inyecta automaticamente la comprobacion de licencia
   return(INIT_SUCCEEDED);
  }

void OnTick()
  {
   // aqui se inyecta automaticamente la comprobacion de licencia
   // --- logica de trading de ejemplo ---
   Comment("EA protegido - licencia activa");
  }
`;
  }

  // ---------- autotest ----------
  function selfTest() {
    const checks = [];
    function eq(name, got, want) {
      checks.push({ name, ok: got === want, got, want });
    }
    eq('SHA-256("")',
      sha256Hex(''),
      'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855');
    eq('SHA-256("abc")',
      sha256Hex('abc'),
      'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad');
    eq('HMAC-SHA256("key", fox)',
      hmacHex('key', 'The quick brown fox jumps over the lazy dog'),
      'f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8');

    const f = genLicense({
      mode: 'T', licensee: 'Juan Perez', broker: 'ICMarkets', account: '12345678',
      days: 30, salt: 'a1b2c3d4e5f60718', utcNow: 1750000000,
    });
    eq('lic len = 67', String(f.lic.length), '67');
    eq('lic prefijo B-', f.lic.slice(0, 2), 'B-');
    eq('id20 len', String(f.id20.length), '20');
    eq('sigA len 64', String(f.sigA.length), '64');
    eq('sigB len 8', String(f.sigB.length), '8');
    eq('idd len 64', String(f.idd.length), '64');
    eq('mode T', f.mode, 'T');
    eq('broker norm', f.broker, 'icmarkets');
    eq('account', f.account, '12345678');

    const rebuilt = 'tl=' + f.tHex + ';tm=' + f.mode + ';ac=' + f.account + ';br=' + f.broker +
                    ';nm=' + f.licensee + ';sl=' + f.salt + ';mg=' + MAGIC;
    eq('pad rebuild == pad', rebuilt === f.pad ? '1' : '0', '1');

    eq('idd == sha(pad+id20)',
      sha256Hex(f.pad + f.id20).toUpperCase() === f.idd ? '1' : '0', '1');
    eq('sigB == hmac(pad+id20)[0:8]',
      hmacHex(KEY, f.pad + f.id20).toUpperCase().slice(0, 8) === f.sigB ? '1' : '0', '1');
    eq('id20 == crc+sha',
      (hex2(crc8(f.tHex + f.licensee)) + sha256Hex(f.tHex + f.licensee).toUpperCase().slice(0, 18)) === f.id20 ? '1' : '0', '1');

    const u = genLicense({ mode: 'U', licensee: 'Maria Lopez', broker: '**', utcNow: 1750000000 });
    eq('ilimitado tHex', u.tHex, 'FFFFFFFF');
    eq('ilimitado broker **', u.broker, '**');
    eq('ilimitado account 0', u.account, '0');

    let longErr = '';
    try { genLicense({ mode: 'U', licensee: 'Nombre Demasiado Largo Para La Licencia De Prueba', broker: '**' }); }
    catch (e) { longErr = e.message; }
    eq('nombre > 28 lanza error', longErr ? '1' : '0', '1');

    const inj = inject(sampleEA(f), f, f.lic);
    eq('inyeccion marcador', inj.source.includes(M_ACT) ? '1' : '0', '1');
    eq('inyeccion def + 2 llamadas F60', (inj.source.match(/BCFX_F60/g) || []).length >= 3 ? '1' : '0', '1');
    eq('licencia incrustada', inj.source.includes('BCFX_LICENCIA = "' + f.lic + '"') ? '1' : '0', '1');
    eq('guard __MQL5__', inj.source.includes('__MQL5__') ? '1' : '0', '1');

    for (const k of ['BCFX_X1', 'BCFX_X3', 'BCFX_X7', 'BCFX_K']) {
      const re = new RegExp(k + '\\s*=\\s*"([^"]*)"');
      const m = inj.source.match(re);
      eq(k + ' literal valido sin " interno', m ? (m[1].includes('"') ? '0' : '1') : '0', m ? '1' : '1');
    }

    // la plantilla debe contener todos los marcadores esperados
    const tpl = buildActivatorTemplate();
    for (const ph of ['@K@', '@MM@', '@X1@', '@X3@', '@X4@', '@X5@', '@X6@', '@X7@', '@X8@', '@LIC@']) {
      eq('plantilla tiene ' + ph, tpl.includes(ph) ? '1' : '0', '1');
    }
    eq('buildActivator no deja marcadores',
      /@(K|MM|X[1-8]|LIC)@/.test(buildActivator(f, f.lic)) ? '0' : '1', '1');

    return checks;
  }

  // ---------- export ----------
  const api = {
    MAGIC, KEY, NAME_MAX,
    genLicense, buildActivatorTemplate, buildActivator, inject, sampleEA, selfTest,
    hexEnc, crc8, hex8, hex2, asciiFold, padRight, replaceAll,
  };

  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  else root.Bcfx = api;
  // también bajo globalThis para QuickJS embebido
  if (typeof globalThis !== 'undefined' && !globalThis.Bcfx) globalThis.Bcfx = api;
})(typeof self !== 'undefined' ? self : this);
