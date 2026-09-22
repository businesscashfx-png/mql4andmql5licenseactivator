# BCFX Activador de Licencias — Aplicación de escritorio Windows (10 / 11)

El mismo activador de licencias que la versión web, ahora como **programa `.exe`** de
escritorio para Windows 10 y 11. No requiere internet, no requiere instalación y no
guarda ni envía ninguna información: todo se procesa en tu equipo.

## Uso

1. Ejecuta `BCFX_Activador_Windows.exe`.
2. Pulsa **ABRIR EA...** y carga tu `.mq4` (MT4) o `.mq5` (MT5).
3. Configura la licencia:
   - **Nombre** de la persona.
   - **Tipo**: *Por tiempo* (días o fecha límite) o *Ilimitada*.
   - **Broker**: cualquier broker (ilimitado abierto) o un broker específico (bloquea otros).
   - **Cuenta**: todas o una cuenta específica.
4. Pulsa **ACTIVAR LICENCIA Y PROTEGER EA**.
5. **Guardar EA protegido** y **Guardar archivo .lic** para entregar a tu cliente.

## Limitaciones / notas

- Es una aplicación **x64** (64 bits) para Windows 10/11.
- Compila siempre el EA a `.ex4` / `.ex5` en MetaEditor antes de distribuirlo.
- Windows SmartScreen puede mostrar un aviso por ser un binario sin firma de código.

## Compilar el .exe desde cero (Linux)

```bash
# 1) instalar un compilador cruzado a Windows (vía Python)
python3 -m venv .venv-zig && .venv-zig/bin/pip install ziglang

# 2) clonar el motor JS
./tools/clone-quickjs.sh

# 3) compilar
./tools/build-desktop.sh
```

Genera `desktop/BCFX_Activador_Windows.exe`.

## Estructura

- `win.c` — interfaz Win32 pura (sin frameworks) + puente al motor JS.
- `resource.rc`, `app.ico`, `app.manifest` — icono, versión y manifiesto (DPI aware).
- `deps/quickjs/` — motor JavaScript embebido (QuickJS; clonar con tools/clone-quickjs.sh).
- `js_embed.h` — `crypto.js` + `mql.js` embebidos (auto-generado por `tools/embed-js.js`).
- `test_glue.c` — prueba del puente C→JS en Linux (sin GUI).
