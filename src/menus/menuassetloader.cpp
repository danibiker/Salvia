#include "menuassetloader.h"
#include "gamemenu.h"

#include <SDL_Image.h>
#include <gfx/SDL_rotozoom.h>

/* ======================================================================
 * MenuAssetLoader implementacion
 *
 * Patron de un thread persistente con peticion + numero de secuencia.
 * Submit() copia los datos necesarios e incrementa la secuencia; el worker
 * compara la secuencia entre pasos para cancelar cargas obsoletas en
 * cuanto el usuario sigue navegando.
 *
 * El worker lee filepath + isAttempted sin lock (solo el propio worker
 * los escribe bajo CS, o createMenuImages adquiriendo los CS). El lock
 * se toma unicamente durante addAttempt/adoptSurface (~microsegundos)
 * para que el main thread pueda dibujar bajo TryEnterCriticalSection
 * sin bloquearse.
 * ====================================================================== */
MenuAssetLoader::MenuAssetLoader()
    : m_owner(NULL), m_event(NULL), m_thread(NULL), m_seqSubmitted(0),
      m_stop(false), m_started(false), m_reqCSInited(false),
      m_workerFont(NULL),
      m_pendFormat(NULL), m_pendOverlayW(0), m_pendSynopsisMaxW(0)
{
}

MenuAssetLoader::~MenuAssetLoader()
{
    stop();
}

void MenuAssetLoader::start(GameMenu* owner)
{
    if (m_started) return;

    m_owner = owner;
    InitializeCriticalSection(&m_reqCS);
    m_reqCSInited = true;
    m_event  = CreateEvent(NULL, FALSE, FALSE, NULL);
    m_stop   = false;

    m_workerFont = Fonts::createIndependentFont(Fonts::FONTBIG);
    if (!m_workerFont) {
        LOG_ERROR("MenuAssetLoader: no se pudo crear TTF_Font independiente");
    }

    m_thread = CreateThread(NULL, 0, &MenuAssetLoader::WorkerProc, this, CREATE_SUSPENDED, NULL);

    if (!m_thread) {
        LOG_ERROR("MenuAssetLoader: CreateThread FAILED");
        if (m_workerFont) { TTF_CloseFont(m_workerFont); m_workerFont = NULL; }
        if (m_event) { CloseHandle(m_event); m_event = NULL; }
        if (m_reqCSInited) { DeleteCriticalSection(&m_reqCS); m_reqCSInited = false; }
        return;
    }

#ifdef _XBOX
    XSetThreadProcessor(m_thread, IO_THREAD);
#endif

    SetThreadPriority(m_thread, THREAD_PRIORITY_BELOW_NORMAL);
    ResumeThread(m_thread);

    m_started = true;
    LOG_DEBUG("MenuAssetLoader: started");
}

void MenuAssetLoader::stop()
{
    if (!m_started) return;

    m_stop = true;
    if (m_event) SetEvent(m_event);

    if (m_thread) {
        DWORD wr = WaitForSingleObject(m_thread, 3000);
        if (wr == WAIT_TIMEOUT) {
            LOG_ERROR("MenuAssetLoader: worker did not exit in 3s");
        }
        CloseHandle(m_thread);
        m_thread = NULL;
    }
    if (m_event) { CloseHandle(m_event); m_event = NULL; }
    if (m_reqCSInited) { DeleteCriticalSection(&m_reqCS); m_reqCSInited = false; }
    if (m_workerFont) { TTF_CloseFont(m_workerFont); m_workerFont = NULL; }

    m_started = false;
    LOG_DEBUG("MenuAssetLoader: stopped");
}

void MenuAssetLoader::submit(const std::string& fileNoExt,
                              const std::string& assetsDir,
                              SDL_PixelFormat* format,
                              int overlayW,
                              int synopsisMaxW)
{
    if (!m_started) return;

    EnterCriticalSection(&m_reqCS);
    m_pendFileNoExt    = fileNoExt;
    m_pendAssetsDir    = assetsDir;
    m_pendFormat       = format;
    m_pendOverlayW     = overlayW;
    m_pendSynopsisMaxW = synopsisMaxW;
    InterlockedIncrement(&m_seqSubmitted);
    LeaveCriticalSection(&m_reqCS);

    SetEvent(m_event);
}

DWORD WINAPI MenuAssetLoader::WorkerProc(LPVOID self_ptr)
{
    MenuAssetLoader* self = static_cast<MenuAssetLoader*>(self_ptr);
    self->run();
    return 0;
}

void MenuAssetLoader::loadMenuImage(const std::string& menuImgId, const std::string& assetsDir, const std::string& fileNoExt, SDL_PixelFormat *format)
{
    LONG mySeq = m_seqSubmitted;
    const std::string sep = std::string(Constant::tempFileSep);

    std::string relPath = menuImgId + sep + fileNoExt + ".png";
    std::string assetsPath = assetsDir + menuImgId + sep + fileNoExt + ".png";

    const std::string currentPath = m_owner->menuImages[menuImgId].getFilepath();
    const bool prevLoadFailed = m_owner->menuImages[menuImgId].isAttempted(relPath) || m_owner->menuImages[menuImgId].isAttempted(assetsPath);

    if (currentPath != relPath && currentPath != assetsPath && !prevLoadFailed) {
        std::size_t outSize = 0;
        const unsigned char* filePtr = m_owner->filePackage.GetFile(relPath, outSize);
        const bool binaryMiss = (filePtr == NULL || outSize == 0);

        SDL_Surface* surface = NULL;
        const std::string binaryPath = relPath;
        if (!binaryMiss) {
            surface = Image::loadConvertedSurfaceFromMem(filePtr, outSize, format);
        } else {
            relPath = assetsDir + menuImgId + sep + fileNoExt + ".png";
            surface = Image::loadConvertedSurface(relPath, format);
        }

        if (m_stop || mySeq != m_seqSubmitted) {
            if (surface) SDL_FreeSurface(surface);
            return;
        }

        EnterCriticalSection(m_owner->menuImages[menuImgId].m_objCS);
        if (binaryMiss) m_owner->menuImages[menuImgId].addAttempt(binaryPath);
        if (surface) {
            m_owner->menuImages[menuImgId].adoptSurface(surface, relPath);
            m_owner->menuImages[menuImgId].clearAttempts();
        } else {
            m_owner->menuImages[menuImgId].addAttempt(relPath);
            m_owner->menuImages[menuImgId].closeImage();
        }
        LeaveCriticalSection(m_owner->menuImages[menuImgId].m_objCS);
    }
}

void MenuAssetLoader::loadMenuText(const std::string& menuTxtId,
                                   const std::string& assetsDir,
                                   const std::string& fileNoExt)
{
    const LONG       mySeq        = m_seqSubmitted;
    const std::string sep         = std::string(Constant::tempFileSep);
    const int        maxW         = m_pendSynopsisMaxW;
    const std::string relPath     = menuTxtId + sep + fileNoExt + ".txt";
    const std::string assetsPath  = assetsDir + menuTxtId + sep + fileNoExt + ".txt";

    TextArea& ta       = m_owner->menuTextAreas[menuTxtId];
    const std::string currentPath    = ta.getFilepath();
    const bool        prevLoadFailed = ta.isAttempted(relPath) || ta.isAttempted(assetsPath);

    if (!m_workerFont || maxW <= 0
        || currentPath == relPath || currentPath == assetsPath
        || prevLoadFailed)
    {
        return;
    }

    std::string txt = m_owner->filePackage.GetFileText(relPath);
    const bool   useAssets = txt.empty();
    const std::string& activePath = useAssets ? assetsPath : relPath;

    std::vector<t_line> wrapped = useAssets
        ? TextArea::wrapTextFileWithFont(assetsPath, m_workerFont, maxW)
        : TextArea::wrapStringWithFont(txt, m_workerFont, maxW);

    if (m_stop || mySeq != m_seqSubmitted)
        return;

    {
        EnterCriticalSection(m_owner->menuTextAreas[menuTxtId].m_objCS);
        TextArea& ta = m_owner->menuTextAreas[menuTxtId];
        if (useAssets) ta.addAttempt(relPath);
        ta.setFilepath(activePath);
        if (wrapped.empty()) {
            ta.addAttempt(assetsPath);
            ta.clear();
        } else {
            ta.adoptLines(wrapped, assetsPath);
            ta.resetTicks(m_owner->gameTicks);
            ta.clearAttempts();
        }
        LeaveCriticalSection(m_owner->menuTextAreas[menuTxtId].m_objCS);
    }
}

void MenuAssetLoader::run()
{
    while (!m_stop) {
        WaitForSingleObject(m_event, 5000);
        if (m_stop) break;

        while (!m_stop && m_owner->status == EMU_MENU) {
            std::string fileNoExt, assetsDir;
            SDL_PixelFormat* format;
            int overlayW, synopsisMaxW;
            LONG mySeq;

            // 1. Bloqueamos y copiamos los datos
            EnterCriticalSection(&m_reqCS);
            fileNoExt    = m_pendFileNoExt;
            assetsDir    = m_pendAssetsDir;
            format       = m_pendFormat;
            overlayW     = m_pendOverlayW;
            synopsisMaxW = m_pendSynopsisMaxW;
            mySeq        = m_seqSubmitted;

            // 2. CLAVE: Limpiamos la peticion original para que no se procese dos veces
            m_pendFileNoExt.clear(); 
            LeaveCriticalSection(&m_reqCS);

            // Si no hay archivo nuevo que cargar, salimos del bucle interno de inmediato
            if (fileNoExt.empty()) break;

            const std::string sep = std::string(Constant::tempFileSep);

            #define MAL_CANCELLED() (m_seqSubmitted != mySeq || m_stop)

            // Proceso de carga por pasos
            loadMenuText(SYNOPSIS, assetsDir, fileNoExt);
            if (MAL_CANCELLED()) continue; // Si se cancela, vuelve arriba (pero fileNoExt ya estara vacio gracias al paso 2 y saldra)

            loadMenuImage(SNAP, assetsDir, fileNoExt, format);
            if (overlayW < 640) break; // Termina la carga de este juego de forma prematura y segura
            if (MAL_CANCELLED()) continue;

            loadMenuImage(BOX2D, assetsDir, fileNoExt, format);
            if (MAL_CANCELLED()) continue;

            loadMenuImage(SNAPTIT, assetsDir, fileNoExt, format);

            #undef MAL_CANCELLED

            // Si el usuario cambió de juego en el último milisegundo, repetimos para verificar
            if (m_seqSubmitted != mySeq) continue;
            
            // Todo se cargó con éxito para este juego, salimos del bucle interno
            break; 
        }
    }
}