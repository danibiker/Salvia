/*
 * libgui/IPLFont.h  --  shim para el port de glN64_GX a Xenon64.
 *
 * En Wii64 es la fuente del IPL de la consola, usada para pintar texto sobre
 * el juego.  glN64_GX la usa en UN solo sitio: el contador de FPS en
 * pantalla de VI.cpp (VI_GX_showFPS, VI.cpp:278-292) y el volcado de debug
 * de VI.cpp:371-374.
 *
 * Aqui no hay ni fuente ni texto sobre el juego, y no debe haberlo: el
 * overlay de Salvia y las metricas del perfilador ([PERF], [FRAME], [PEAK])
 * ya cubren eso, y ademas dibujar desde el core sobre el device compartido
 * es justo lo que obliga a reestablecer el pipeline entero
 * (ver project_xenon64_libretro_port).
 *
 * Asi que las dos funciones son no-ops.  El llamante ya esta guardado por
 * `showFPSonScreen`, que vale 0, asi que en la practica ni se llega aqui.
 *
 * Se declara con la MISMA forma que en Wii64 -- singleton en el namespace
 * `menu` con `getInstance()` -- porque el sitio de llamada
 * (`menu::IplFont::getInstance().drawInit(...)`) esta en un fichero copiado
 * que no queremos tocar.
 */

#ifndef IPLFONT_H
#define IPLFONT_H

#include <gccore.h>   /* GXColor */

namespace menu {

class IplFont
{
public:
    static IplFont &getInstance()
    {
        static IplFont s_instance;
        return s_instance;
    }

    void drawInit(GXColor fontColor) { (void)fontColor; }

    void drawString(int x, int y, const char *string, float scale, bool centered)
    {
        (void)x; (void)y; (void)string; (void)scale; (void)centered;
    }

    void setColor(GXColor color) { (void)color; }
    void drawFinish(void) {}

private:
    IplFont() {}
    ~IplFont() {}
};

} /* namespace menu */

#endif /* IPLFONT_H */
