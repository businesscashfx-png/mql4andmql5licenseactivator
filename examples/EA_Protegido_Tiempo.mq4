//+------------------------------------------------------------------+
//|                                        BCFX_Ejemplo_Protegido.mq4 |
//|                                  Demo protegida por BCFX Activator |
//+------------------------------------------------------------------+
#property copyright "BCFX Demo"
#property version   "1.00"
#property strict

//BCFX_ACTIVADOR_INICIO
//=================================================================
//   BCFX License Activator  (MQL4 / MQL5)
//   Proteccion por: TIEMPO / BROKER / CUENTA / ILIMITADO
//   Si el EA se manipula o la licencia no corresponde, se desactiva.
//=================================================================
input string BCFX_LICENCIA = "B-6AD91E3E-7F28FB492F98F1AF3BF8Juan Perez******************25E3C4BA";   // Licencia entregada (o pegar aqui el .lic)

string BCFX_K   = "424346582D376B39587132547A38764C6D346E502D32303234";   // llave (hex)
string BCFX_MM  = "BCFX2024";   // magic
string BCFX_X1  = "65346264343265353030353033636437";   // salt
string BCFX_X3  = "34373632353138384335433337393743454244323538394338303335433037313239443433323939433941353132343944354236324536464237334638454234";   // firma A
string BCFX_X4  = "3235453343344241";   // firma B
string BCFX_X5  = "37333945304237414335323135453638463942333839433245413942423737463533343836434530443545343737413744374636343732413445323034383839";   // integridad interna
string BCFX_X6  = "54";   // modo T / U
string BCFX_X7  = "69636D61726B657473";   // broker (o 2A2A = **)
string BCFX_X8  = "30";   // cuenta (o 30 = 0)

//=================== utilidades (sin dependencias de platforma) ===================
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

int OnInit()
  {
   if(BCFX_F60()!=0) return(-1);   // BCFX
   // aqui se inyecta automaticamente la comprobacion de licencia
   return(INIT_SUCCEEDED);
  }

void OnTick()
  {
   if(BCFX_F60()!=0) return;   // BCFX
   // aqui se inyecta automaticamente la comprobacion de licencia
   // --- logica de trading de ejemplo ---
   Comment("EA protegido - licencia activa");
  }
