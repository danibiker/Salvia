#include <uiobjects/textarea.h>
#include <io/dirutil.h>

TextArea::TextArea(){
    init();
}

TextArea::~TextArea(){
	LOG_DEBUG("Deleting TextArea...\n");
	clear();
}

TextArea::TextArea(int x, int y, int w, int h){
    this->setX(x);
    this->setY(y);
    this->setW(w);
    this->setH(h);
    this->marginX = 0;
    init();
}

void TextArea::init(){
    this->filepath = "";
    this->lineSpace = 3;
    this->marginTop = 10;
    this->lastScroll = 0;
    setObjectType(GUITEXTAREA);

    this->enableScroll = true;
    this->pixelDesp = 0;
    this->timesWaiting = 0;
	this->timesWaitingEnd = 0;
    this->waiting = true;
	setFontType(Fonts::FONTSMALL);
}

void TextArea::setFontType(Fonts::enumFonts type){
	this->fontType = type;
	this->fontText = Fonts::getFont(type);
	this->face_h = TTF_FontLineSkip(this->fontText);
}

void TextArea::clear(){
	for (unsigned int i=0; i < lines.size(); i++){
		if (lines[i].lineSrf != NULL){
			SDL_FreeSurface(lines[i].lineSrf);
		}
	}
	lines.clear();
}

bool TextArea::isEmpty(){
	return lines.empty();
}
/**
    * 
    */
bool TextArea::loadTextFileFromGame(std::string baseDir, GameFile& game, std::string ext){
    dirutil dir;
    return loadTextFile(baseDir + dir.getFileNameNoExt(game.longFileName) + ext);
}

/**
* 
*/
bool TextArea::loadTextFile(std::string filepathToOpen) {
    // 1. Comprobación rápida para evitar recargar el mismo archivo
    if (!this->filepath.empty() && this->filepath == filepathToOpen) {
        return true;
    }

    std::ifstream fileRomTxt(filepathToOpen);
    if (!fileRomTxt.is_open()) {
        clear();
        return false;
    }

    this->filepath = filepathToOpen;
    this->lastScroll = 0;
    clear();

    const int spaceW = Fonts::getSize(this->fontType, " ");
    const int maxW = this->getW() - this->marginX;
    
    std::string rawLine;
    
    // Leemos el archivo linea por linea para respetar los retornos de carro originales
    while (std::getline(fileRomTxt, rawLine)) {
		t_line line;
        // Creamos una nueva linea en nuestro vector para este parrafo
        lines.push_back(std::move(line));
        int currentLineW = 0;

        std::stringstream ss(rawLine);
        std::string word;

        // Troceamos la linea actual por espacios
        while (ss >> word) {
            int wordW = Fonts::getSize(this->fontType, word.c_str());

            // Si es la primera palabra de la linea actual
            if (lines.back().text.empty()) {
                lines.back().text = std::move(word);
                currentLineW = wordW;
            } 
            // Si la palabra cabe en la linea actual junto con el espacio
            else if (currentLineW + spaceW + wordW < maxW) {
                lines.back().text.append(" ").append(word);
                currentLineW += spaceW + wordW;
            } 
            // Si no cabe, hacemos un salto de linea automático (ajuste de texto)
            else {
				line.text = std::move(word);
                lines.push_back(std::move(line));
                currentLineW = wordW;
            }
        }
    }

    fileRomTxt.close();
    return true;
}

bool TextArea::loadString(std::string fulltxt){
	this->lines = wrapString(fulltxt, this->fontType, this->getW() - this->marginX);
	this->filepath = "";
	this->lastScroll = 0;
	return true;
}

/* ---------- Helpers estaticos para carga asincrona ----------
 * Las versiones "WithFont" hacen todo el trabajo y son seguras desde
 * cualquier hilo si la TTF_Font* es exclusiva de ese hilo (ver
 * Fonts::createIndependentFont). */
std::vector<t_line> TextArea::wrapStringWithFont(const std::string& fulltxt, TTF_Font* font, int maxW)
{
    std::vector<t_line> out;
    if (fulltxt.empty()) return out;

    // 1. Preasignamos memoria estimada para evitar realojamientos del vector
    out.reserve(fulltxt.size() / 20 + 1); 
    out.push_back(t_line());

    int currentLineW = 0;
    const int spaceW = Fonts::getSize(font, " ");
    
    std::size_t start = 0;
    std::size_t length = fulltxt.length();

    // Reutilizamos un único buffer dinámico para extraer palabras sin reservar memoria continuamente
    std::string word;
    word.reserve(32); 

    while (start < length) {
        // Saltar espacios iniciales pero marcar que venimos de uno
        bool preSpace = (start > 0 && fulltxt[start - 1] == ' ');
        
        // 2. Encontrar el final de la palabra actual de forma eficiente
        std::size_t end = start;
        bool hasBreak = false;
        std::size_t breakPos = std::string::npos;

        while (end < length && fulltxt[end] != ' ') {
            if (fulltxt[end] == '\n' || fulltxt[end] == '\r') {
                hasBreak = true;
                breakPos = end;
                break; // Paramos en el salto de línea para procesarlo
            }
            ++end;
        }

        // 3. Extraer la palabra reutilizando la capacidad del string (sin hacer New/Delete de memoria)
        word.assign(fulltxt, start, (hasBreak ? breakPos : end) - start);

        int wordW = word.empty() ? 0 : Fonts::getSize(font, word.c_str());
        bool hasSpace = !out.back().text.empty();
        int addedW = wordW + (hasSpace ? spaceW : 0);

        // 4. Evaluar si cabe en la línea actual
        if (currentLineW + addedW >= maxW) {
            out.push_back(t_line());
            out.back().text = word;
            currentLineW = wordW;
        } else {
            if (!word.empty()) {
                if (hasSpace) out.back().text += " ";
                out.back().text += word;
                currentLineW += addedW;
            }
        }

        // 5. Avanzar los índices según lo que hemos encontrado
        if (hasBreak) {
            // Saltamos el '\n' o '\r'
            start = breakPos + 1;
            // Si el siguiente carácter es el compañero (\r\n), lo saltamos también
            if (start < length && ((fulltxt[breakPos] == '\r' && fulltxt[start] == '\n') || 
                                   (fulltxt[breakPos] == '\n' && fulltxt[start] == '\r'))) {
                ++start;
            }
            // Forzamos la creación de una nueva línea debido al salto explícito
            out.push_back(t_line());
            currentLineW = 0;
        } else {
            // Avanzamos al siguiente carácter después del espacio
            start = end + 1; 
        }
    }

    return out;
}

std::vector<t_line> TextArea::wrapTextFileWithFont(const std::string& filepathToOpen, TTF_Font* font, int maxW)
{
    std::vector<t_line> out;
    
    // Abrir el archivo en modo binario para controlar manualmente \r y \n
    std::ifstream file(filepathToOpen.c_str(), std::ios::binary);
    if (!file.is_open()) return out;

    // Preasignamos un tamaño estimado inicial para evitar realojamientos del vector
    out.reserve(50); 
    out.push_back(t_line());

    int currentLineW = 0;
    const int spaceW = Fonts::getSize(font, " ");

    // Buffer en el Stack (memoria ultrarrápida) para leer bloques del disco
    const std::size_t BUFFER_SIZE = 4096;
    char buffer[BUFFER_SIZE];

    std::string word;
    word.reserve(32); // Evita allocs constantes al crecer la palabra

    while (file.read(buffer, BUFFER_SIZE) || file.gcount() > 0) {
        std::streamsize bytesRead = file.gcount();
        
        for (std::streamsize i = 0; i < bytesRead; ++i) {
            char c = buffer[i];

            // 1. Detectar delimitadores (Espacio o Saltos de línea)
            if (c == ' ' || c == '\n' || c == '\r') {
                
                // Si teníamos una palabra acumulada, la procesamos
                if (!word.empty()) {
                    int wordW = Fonts::getSize(font, word.c_str());
                    bool hasSpace = !out.back().text.empty();
                    int addedW = wordW + (hasSpace ? spaceW : 0);

                    if (currentLineW + addedW >= maxW) {
                        out.push_back(t_line());
                        out.back().text = word;
                        currentLineW = wordW;
                    } else {
                        if (hasSpace) out.back().text += " ";
                        out.back().text += word;
                        currentLineW += addedW;
                    }
                    word.clear(); // Vacía el string sin liberar su memoria interna
                }

                // 2. Gestión estricta de saltos de línea nativos (\n, \r, \r\n)
                if (c == '\n' || c == '\r') {
                    // Si es un \r y el siguiente es \n, nos lo saltamos para no duplicar
                    if (c == '\r' && (i + 1 < bytesRead) && buffer[i + 1] == '\n') {
                        ++i; 
                    } else if (c == '\r' && (i + 1 == bytesRead) && file.peek() == '\n') {
                        file.get(); // Saltamos el \n si quedó justo en el límite del buffer
                    }

                    // Forzar nueva línea en el ajuste de texto
                    out.push_back(t_line());
                    currentLineW = 0;
                }
            } 
            else {
                // Acumular carácter en la palabra actual
                word.push_back(c);
            }
        }
    }

    // Procesar la última palabra si el fichero no terminaba en espacio/salto de línea
    if (!word.empty()) {
        int wordW = Fonts::getSize(font, word.c_str());
        bool hasSpace = !out.back().text.empty();
        if (currentLineW + wordW + (hasSpace ? spaceW : 0) >= maxW) {
            out.push_back(t_line());
            out.back().text = word;
        } else {
            if (hasSpace) out.back().text += " ";
            out.back().text += word;
        }
    }

    file.close();
    return out;
}

/* Variantes por fontType delegan en la version WithFont resolviendo
 * la TTF_Font compartida (uso desde el main thread). */

std::vector<t_line> TextArea::wrapString(const std::string& fulltxt,
                                                Fonts::enumFonts fontType, int maxW)
{
	return wrapStringWithFont(fulltxt, Fonts::getFont(fontType), maxW);
}

std::vector<t_line> TextArea::wrapTextFile(const std::string& filepathToOpen,
                                                  Fonts::enumFonts fontType, int maxW)
{
	return wrapTextFileWithFont(filepathToOpen, Fonts::getFont(fontType), maxW);
}

void TextArea::adoptLines(const std::vector<t_line>& newLines, const std::string& newPath){
	this->lines = newLines;
	this->filepath = newPath;
	this->lastScroll = 0;
}

/**
    * 
    */
void TextArea::resetTicks(GameTicks gameTicks){
    pixelDesp = 0;
    lastTick = gameTicks.ticks;
    lastSubTick = gameTicks.ticks;
    lastWaitTick = gameTicks.ticks;
    timesWaiting = 0;
    waiting = true;
}

/**
* 
*/
void TextArea::calcTicks(GameTicks gameTicks, int& scrollDesp, float& pixelDesp)
{
    if (!enableScroll)
        return;

    const int   TICKS_PER_LINE  = 120;
    const int   TICKS_PER_PIXEL = 1;
    const int   LOOPS_TO_START  = 1;
    const int   LOOPS_TO_END    = LOOPS_TO_START * 6;
    const float PIXEL_STEP      = static_cast<float>(face_h + lineSpace)
                                             / (TICKS_PER_LINE / TICKS_PER_PIXEL);
    const float PIXEL_MAX       = static_cast<float>(face_h + lineSpace);

    const std::size_t maxLines = (getH() - marginTop) / (face_h + lineSpace);
    const bool        hasScroll = lines.size() > maxLines;
    const int         lastLine  = hasScroll ? static_cast<int>(lines.size() - maxLines) : 0;

    auto elapsed = [&](uint32_t ref) -> uint32_t {
        return (gameTicks.ticks >= ref) ? gameTicks.ticks - ref : ref - gameTicks.ticks;
    };

    auto advancePixels = [&]() {
        if (hasScroll && (int)elapsed(lastSubTick) >= TICKS_PER_PIXEL) {
            lastSubTick = gameTicks.ticks;
            pixelDesp  += PIXEL_STEP;
        }
    };

    // --- 1. Espera inicial ---
    if (scrollDesp == 0 && timesWaiting < LOOPS_TO_START) {
        if (elapsed(lastWaitTick) >= TICKS_PER_LINE) {
            ++timesWaiting;
            lastWaitTick = gameTicks.ticks;
            waiting      = true;
        }
        return;
    }

    if (waiting) {
        lastTick = lastSubTick = gameTicks.ticks;
        waiting  = false;
    }

    // --- 2. Espera final ---
    if (hasScroll && scrollDesp == lastLine) {
        if (pixelDesp < PIXEL_MAX) {           // completar desplazamiento de última línea
            advancePixels();
            return;
        }
        if (timesWaitingEnd >= LOOPS_TO_END) { // espera terminada: volver al inicio
            resetTicks(gameTicks);
            timesWaitingEnd = scrollDesp = 0;
            pixelDesp       = 0;
            return;
        }
        if (elapsed(lastWaitTick) >= TICKS_PER_LINE) { // contar ciclos de espera
            ++timesWaitingEnd;
            lastWaitTick = gameTicks.ticks;
        }

        pixelDesp = PIXEL_MAX;
        lastTick  = lastSubTick = gameTicks.ticks;
        return;
    }

    // --- 3. Avance de línea completa ---
    if (elapsed(lastTick) >= TICKS_PER_LINE) {
        lastTick   = gameTicks.ticks;
        pixelDesp  = 0;
        scrollDesp = hasScroll ? scrollDesp + 1 : 0;
    }

    // --- 4. Desplazamiento suavizado por píxeles ---
    advancePixels();
}

/**
* 
*/
void TextArea::draw(SDL_Surface *video_page, GameTicks gameTicks){
    int nextLineY = this->getY() + marginTop;
    int i = 0;
    if (lines.empty() || lines.size() == 0){
        return;
    }
    calcTicks(gameTicks, this->lastScroll, pixelDesp);

    do{
		t_line& line = lines.at(i + this->lastScroll);
		drawTextAreaTransparent(video_page, this->fontText, line, this->getX() + this->marginX, (int) (nextLineY - pixelDesp), Constant::colors[clWhite].sdlColor, 0);
        nextLineY = this->getY() + marginTop + (++i) * (face_h + lineSpace);
    } while ((std::size_t) (i + this->lastScroll) < lines.size() && nextLineY < this->getY() + this->getH() - face_h);
}

/**
* 
*/
void TextArea::draw(SDL_Surface *video_page){
    this->enableScroll = false;
	GameTicks ticks = {0};
    draw(video_page, ticks);
}

SDL_Rect TextArea::drawTextAreaTransparent(SDL_Surface* surface, TTF_Font* font, t_line& line, int x, int y, SDL_Color color, int bg, JFY_TYPE& justifyHelper){
	if (font && !line.text.empty()) {
		if (line.lineSrf == NULL){
			#ifdef _XBOX 
			line.lineSrf = TTF_RenderUTF8_Solid(font, line.text.c_str(), color);
			#else
			line.lineSrf = TTF_RenderUTF8_Blended(font, line.text.c_str(), color);
			#endif
			if (line.lineSrf){
				line.dest.w = line.lineSrf->w;
				line.dest.h = line.lineSrf->h;
				line.dest.x = x + justifyHelper.getJustification(line.lineSrf->w);
			}
		}

		if (line.lineSrf) {
			line.dest.y = y;	
			SDL_BlitSurface(line.lineSrf, NULL, surface, &line.dest);
		}
	}
	return line.dest;
}