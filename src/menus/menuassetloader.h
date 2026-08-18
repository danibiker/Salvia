#pragma once

#include <string>

#include <SDL.h>
#include <SDL_ttf.h>

#ifdef _XBOX
#include <xtl.h>
#else
#include <windows.h>
#endif

class GameMenu;

/* MenuAssetLoader: worker dedicado a cargar de forma asincrona el panel
 * derecho del menu (snap/box2d/snaptit + synopsis).
 * Permite que la navegacion del menu (lista izquierda) responda
 * inmediatamente aunque el PNG/text de la rom seleccionada tarde en cargar.
 *
 * Cancelacion: cada submit() incrementa un contador. Entre cada paso
 * de carga el worker compara y, si llego una peticion mas reciente,
 * abandona la actual y reentra con la nueva. */
class MenuAssetLoader {
public:
    MenuAssetLoader();
    ~MenuAssetLoader();

    void start(GameMenu* owner);
    void stop();

    void submit(const std::string& fileNoExt,
                const std::string& assetsDir,
                SDL_PixelFormat* format,
                int overlayW,
                int synopsisMaxW);

private:
    static DWORD WINAPI WorkerProc(LPVOID self_ptr);
    void run();

    void loadMenuImage(const std::string& menuImgId,
                       const std::string& assetsDir,
                       const std::string& fileNoExt,
                       SDL_PixelFormat* format);
    void loadMenuText(const std::string& menuTxtId,
                      const std::string& assetsDir,
                      const std::string& fileNoExt);

    GameMenu*           m_owner;
    CRITICAL_SECTION    m_reqCS;
    HANDLE              m_event;
    HANDLE              m_thread;
    volatile LONG       m_seqSubmitted;
    volatile bool       m_stop;
    bool                m_started;
    bool                m_reqCSInited;
    TTF_Font*           m_workerFont;

    std::string         m_pendFileNoExt;
    std::string         m_pendAssetsDir;
    SDL_PixelFormat*    m_pendFormat;
    int                 m_pendOverlayW;
    int                 m_pendSynopsisMaxW;

    MenuAssetLoader(const MenuAssetLoader&);
    MenuAssetLoader& operator=(const MenuAssetLoader&);
};
