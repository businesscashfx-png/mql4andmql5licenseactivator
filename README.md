# BCFX — Activador de Licencias para MQL4 / MQL5 (MT4 / MT5)

Herramienta web **100 % local** (sin conexión, sin servidor de guardado) que:

1. Recibe un EA en código fuente (`.mq4` para MT4 o `.mq5` para MT5).
2. Con el botón **“Agregar el sistema”** inserta el activador de licencias dentro del código.
3. Te deja elegir **nombre de la persona**, **tiempo / fecha límite** o **ilimitado**,
   y restricción de **broker** y/o **cuenta**.
4. Genera el **EA protegido** + el archivo de **licencia `.lic`** (o el texto a pegar en
   el parámetro `BCFX_LICENCIA`).

## Versiones disponibles

| Versión | Cómo usarla |
|---|---|
| **Web** | `node server.js` → `http://localhost:3000` |
| **Windows (.exe)** | `desktop/BCFX_Activador_Windows.exe` — de escritorio para Windows 10/11 (ver `desktop/README.md`) |

Ambas comparten **exactamente la misma lógica** (SHA-256 + HMAC-SHA256), por lo que una
licencia generada en el `.exe` es idéntica a la de la web.

---

## ▶ Cómo ejecutar (web)

```bash
node server.js
```

Abre `http://localhost:3000` (o el enlace de Vista Previa que te da el entorno).

> No requiere `npm install`: usa solo Node estándar y JavaScript puro.

---

## Flujo de uso (3 pasos)

### Paso 1 — Sube el EA
- Arrastra o selecciona tu `.mq4` / `.mq5`.
- Pulsa **“⚙ Agregar el sistema”**. El activador se inserta automáticamente antes
  de `OnInit`/`init` y se llaman las comprobaciones en `OnInit` y `OnTick`.

### Paso 2 — Configura la activación
- **Nombre de la persona** (obligatorio, máx. 28 caracteres, sin acentos residuales).
- **Tipo**: `⏱ Por tiempo` (días / meses / años / horas o fecha límite exacta) o
  `∞ Ilimitada`.
- **Broker**:
  - `🌐 Cualquier broker` → ilimitado abierto (funciona en cualquier broker/cuenta que use la persona).
  - `🔒 Un broker específico` → uso como “bloqueado” por broker: solo cuentas de ese broker.
- **Cuenta**: `👥 Todas` o `🔢 Una cuenta específica`.

### Paso 3 — Genera y entrega
- `⬇ Descargar EA protegido` (ya incluye la licencia incrustada) y `⬇ Descargar .lic`.
- El cliente compila el EA en MetaEditor y lo usa en su MT: **solo funcionará** para
  el **nombre**, **broker** y **cuenta** activados, y hasta la **fecha** indicada.

---

## Los dos tipos de ilimitado (según tu especificación)

| Variante | Persona | Broker | Cuenta | Resultado |
|---|---|---|---|---|
| **Ilimitado por broker** | fija (nombre) | un broker fijo | todas (`0`) | Funciona en **todas las cuentas de ese broker**; en otro broker queda bloqueado. |
| **Ilimitado abierto** | fija (nombre) | `**` (cualquiera) | todas (`0`) | Funciona en **cualquier broker y cualquier cuenta**, siempre bajo el nombre activado. |

---

## Formato de la licencia (67 caracteres)

```
B-AAAAAAAA-BBCCCCCCCCCCCCCCCCCCNNNNNNNNNNNNNNNNNNNNNNNNNNNNGGGGGGGG
└ prefix 1
  └ tHex 8 (expiracion en hex; FFFFFFFF = ilimitado)
      └ id20 (CRC8 + SHA-256[nombre+fecha])
          └ nombre (28, relleno con '*')
              └ guard 8 (HMAC)
```

- El **id** liga el nombre y la fecha: editar el `.lic` rompe el id.
- **Firmas A/B** (HMAC-SHA256) e **integridad interna** (SHA-256): editar el bloque
  oculto del EA rompe la verificación.
- **Ofuscación**: todos los campos críticos viajan en hexadecimal dentro del fuente.

---

## Protección / anti-descompilación

- El activador está **firmado**: si alguien altera el código (por ejemplo tras
  descompilar y recompilar con cambios) la firma/el hash interno ya no cuadran y
  el EA **se desactiva**.
- Ante manipulación detectada el código lanza errores críticos (autodestrucción).
- **Importante**: compila siempre a `.ex4` / `.ex5` antes de distribuir. Repartir el
  `.mq4`/`.mq5` entrega la lógica de negocio; el binario compilado es lo que debes entregar.

> ⚠ Aviso honesto: **ninguna** protección es 100 % inviolable frente a un atacante
> con tiempo y herramientas. Este activador dificulta mucho la copia/el desvío de
> licencias (restricciones por nombre/tiempo/broker/cuenta + firmas + ofuscación),
> pero no puede garantizar protección absoluta.

---

## Detalles técnicos

| Aspecto | Detalle |
|---|---|
| Criptografía | SHA-256 + HMAC-SHA256 implementados en JS (web) **y** en el bloque MQL (idénticos). |
| Puente de datos | El EA reconstruye la licencia desde offsets fijos y verifica contra los campos ocultos. |
| Compatibilidad | `#ifdef __MQL5__` para `AccountInfoString(ACCOUNT_COMPANY)` / `ACCOUNT_LOGIN`; MQL4 usa `AccountCompany()` / `AccountNumber()`. |
| Vigencia | Timestamp UTC en hex de 8 dígitos (`long`). Máx. año 2106. Caduca por `TimeCurrent()`. |
| Compilación MQL4/MQL5 | Solo usa funciones comunes a ambos lenguajes (`StringToCharArray`, `StringGetCharacter`, `CharToString`, `ArrayResize`), con lógica de hashing de 32 bits portable. |

### Archivos

- `crypto.js` — SHA-256 y HMAC-SHA256 en JS puro (verificado con vectores NIST/RFC 4231).
- `mql.js` — generador del bloque activador + inyección + licencia + autotests.
- `app.js` / `index.html` / `styles.css` — interfaz web (español).
- `server.js` — servidor estático mínimo para la vista previa.
- `desktop/` — aplicación de escritorio Windows 10/11 (`BCFX_Activador_Windows.exe`).
- `examples/` — EAs protegidos de demostración y sus licencias.

### Autotest

```bash
node -e "const r=require('./mql.js').selfTest(); console.log(r.every(c=>c.ok)?'OK':r)"
```
