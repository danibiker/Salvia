# hidmouse.xex — plugin residente de ratón USB HID para Salvia (Xbox 360)

Plugin de DashLaunch que lee un ratón USB HID (boot protocol) hookeando la pila
USB del kernel y **publica** los deltas crudos (X/Y/rueda + botones) en una struct
global. **Salvia solo lee** esa struct y empuja los eventos a SDL.

Como el plugin es **residente** (cargado por DashLaunch al arranque, nunca se
descarga), el ratón sobrevive a los `XLaunchNewImage` de Salvia (cambio de core):
**sin crash y sin replug** — que era el problema de hacerlo dentro del `.xex` de
Salvia.

Requisitos: consola CFW (RGH/JTAG), build de dashboard **17559** (retail) o **17489**
(devkit). Otras builds → no instala (fail-safe); añade sus direcciones en
`g_builds[]` de `main.cpp`.

## Ficheros

- `main.cpp` — driver (hooks USB + callback de interrupción + publicación en `g_hidMouseShared`).
- `HidMouseShared.h` — contrato compartido (struct + magic + VA).
- `Detours.h` — detour PPC (iMoD1998).
- `xex.xml` — config del XEX (**sysdll**, base `0x81F00000`).
- `hidmouse.vcxproj` — proyecto (DynamicLibrary `/dll`, XDK VS2010).

## Compilar

1. Abre `hidmouse.vcxproj` con Visual Studio + XDK de Xbox 360 (toolset 2010).
2. Necesitas `xextool.exe` en esta carpeta (cópialo de `x360remap\hiddriver` o de tu
   XDK) para el post-build (`-r a -m r`, fija privilegios del `.xex`).
3. No hace falta `xkelib` (resolvemos el kernel por ordinal con `GetProcAddress`). Si
   tu entorno exige `xkelib` para construir un sysdll, copia esa carpeta de x360remap
   y añádela al include/lib del proyecto.
4. Compila → `hidmouse.xex`.

## Localización del canal — SIN ajustes (nada de `.map`)

No hay que buscar ninguna dirección ni tocar constantes. El plugin es sysdll a base
**fija `0x81F00000`** (xex.xml), y Salvia **escanea** esa imagen buscando la firma
de la struct (`magic` + `version`). Así encuentra `g_hidMouseShared` esté donde esté
dentro de la imagen del plugin, y **funciona aunque recompiles el plugin y cambie el
layout**. Si el plugin no está cargado, el escaneo (protegido con SEH) simplemente no
encuentra la firma → no-op, sin crash.

> Detalle: si algún día cambias `baseaddr` en `xex.xml`, actualiza `HIDMOUSE_PLUGIN_BASE`
> en `HidMouseShared.h` y en el reader de Salvia (y `HIDMOUSE_SHARED_VERSION` si cambia
> el layout de la struct). Con la base por defecto no hay nada que tocar.

## Desplegar

1. Copia `hidmouse.xex` a `Hdd1:\` (o donde tengas los plugins).
2. En **DashLaunch**, añádelo como plugin (`plugin1`…`plugin5` = ruta al `.xex`) y guarda.
3. **Reinicia la consola** (los plugins se cargan al arranque).
4. Conecta el ratón (si estaba antes del arranque, reconéctalo: hay una ventana de
   *blackout* de ~15 s al inicio para no reclamar durante la enumeración de boot).

## Probar

- En Salvia, mueve el ratón → responde en el menú.
- **Lanza cores y vuelve, varias veces, sin tocar el ratón** → el ratón sigue
  funcionando (el plugin es residente), sin crash ni replug. Es la prueba clave.
- Sin el plugin cargado → Salvia arranca normal y el ratón no va (fail-safe).
- Debug: pon `HIDMOUSE_PLUGIN_DEBUG 1` en `main.cpp` para trazas por `OutputDebugStringA`
  (visibles por XBDM).
