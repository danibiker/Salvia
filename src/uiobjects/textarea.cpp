#include <uiobjects/textarea.h>
#include <io/dirutil.h>

TextArea::TextArea(){
    init();
}

TextArea::~TextArea(){
	LOG_DEBUG("Deleting TextArea...\n");
    lines.clear();
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
        lines.clear();
        return false;
    }

    this->filepath = filepathToOpen;
    this->lastScroll = 0;
    lines.clear();

    const int spaceW = Fonts::getSize(this->fontType, " ");
    const int maxW = this->getW() - this->marginX;
    
    std::string rawLine;
    
    // Leemos el archivo linea por linea para respetar los retornos de carro originales
    while (std::getline(fileRomTxt, rawLine)) {
        // Creamos una nueva linea en nuestro vector para este parrafo
        lines.push_back("");
        int currentLineW = 0;

        std::stringstream ss(rawLine);
        std::string word;

        // Troceamos la linea actual por espacios
        while (ss >> word) {
            int wordW = Fonts::getSize(this->fontType, word.c_str());

            // Si es la primera palabra de la linea actual
            if (lines.back().empty()) {
                lines.back() = std::move(word);
                currentLineW = wordW;
            } 
            // Si la palabra cabe en la linea actual junto con el espacio
            else if (currentLineW + spaceW + wordW < maxW) {
                lines.back().append(" ").append(word);
                currentLineW += spaceW + wordW;
            } 
            // Si no cabe, hacemos un salto de linea automático (ajuste de texto)
            else {
                lines.push_back(std::move(word));
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
std::vector<std::string> TextArea::wrapStringWithFont(const std::string& fulltxt,
                                                       TTF_Font* font, int maxW)
{
	std::vector<std::string> out;
	std::vector<std::string> words = Constant::splitChar(fulltxt, ' ');
	out.push_back("");

	const int spaceW = Fonts::getSize(font, " ");
	for (int i=0; i < (int)words.size(); i++){
		const std::string& word = words.at(i);
		int wordW = Fonts::getSize(font, word.c_str());
		int lineW = Fonts::getSize(font, out.back().c_str());
		if (lineW + wordW + spaceW >= maxW){
			out.push_back("");
			out.back().append(word);
		} else {
			if (!out.back().empty()){
				out.back().append(" ");
			}
			out.back().append(word);
		}
	}
	return out;
}

std::vector<std::string> TextArea::wrapTextFileWithFont(const std::string& filepathToOpen,
                                                          TTF_Font* font, int maxW)
{
	std::vector<std::string> out;
	std::ifstream fileRomTxt(filepathToOpen);
	if (!fileRomTxt.is_open()) return out;

	const int spaceW = Fonts::getSize(font, " ");
	std::string rawLine;
	while (std::getline(fileRomTxt, rawLine)) {
		out.push_back("");
		int currentLineW = 0;
		std::stringstream ss(rawLine);
		std::string word;
		while (ss >> word) {
			int wordW = Fonts::getSize(font, word.c_str());
			if (out.back().empty()) {
				out.back() = std::move(word);
				currentLineW = wordW;
			}
			else if (currentLineW + spaceW + wordW < maxW) {
				out.back().append(" ").append(word);
				currentLineW += spaceW + wordW;
			}
			else {
				out.push_back(std::move(word));
				currentLineW = wordW;
			}
		}
	}
	return out;
}

/* Variantes por fontType â€” delegan en la version WithFont resolviendo
 * la TTF_Font compartida (uso desde el main thread). */
std::vector<std::string> TextArea::wrapString(const std::string& fulltxt,
                                                Fonts::enumFonts fontType, int maxW)
{
	return wrapStringWithFont(fulltxt, Fonts::getFont(fontType), maxW);
}

std::vector<std::string> TextArea::wrapTextFile(const std::string& filepathToOpen,
                                                  Fonts::enumFonts fontType, int maxW)
{
	return wrapTextFileWithFont(filepathToOpen, Fonts::getFont(fontType), maxW);
}

void TextArea::adoptLines(const std::vector<std::string>& newLines, const std::string& newPath){
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
        std::string line = lines.at(i + this->lastScroll);
		Constant::drawTextTransparent(video_page, this->fontText, line.c_str(), this->getX() + this->marginX, (int) (nextLineY - pixelDesp), white, 0);
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