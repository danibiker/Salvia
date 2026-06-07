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
bool TextArea::loadTextFile(std::string filepathToOpen){
    bool ret = false;

    if (this->filepath.empty() || this->filepath.compare(filepathToOpen) != 0){
        fstream fileRomTxt;

        fileRomTxt.open(filepathToOpen, ios::in);
        this->lastScroll = 0;

        if (fileRomTxt.is_open()){
            lines.clear();
            std::string txt;
            std::string fulltxt = "";
            while(getline(fileRomTxt, txt)){
                fulltxt.append(!fulltxt.empty() ? " " : "" + txt);
            }
            fileRomTxt.close();

            std::vector<std::string> words = Constant::splitChar(fulltxt, ' ');
            lines.push_back("");

            const int spaceW = Fonts::getSize(this->fontType, " ");
            for (int i=0; i < (int)words.size(); i++){
				std::string word = words.at(i);
                int wordW = Fonts::getSize(this->fontType, word.c_str());
                int lineW = Fonts::getSize(this->fontType, lines.at(lines.size()-1).c_str());
                if (lineW + wordW + spaceW >= this->getW() - this->marginX){
                    lines.push_back("");
                    lines.at(lines.size()-1).append(word);
                } else {
                    if (!lines.at(lines.size()-1).empty()){
                        lines.at(lines.size()-1).append(" ");
                    }
                    lines.at(lines.size()-1).append(word);
                }
            }
            this->filepath = filepathToOpen;
            ret = true;
        } else {
            lines.clear();
        }
    } else if (!this->filepath.empty() && this->filepath.compare(filepathToOpen) == 0){
        ret = true;
    }

	this->filepath = filepathToOpen;
    return ret;
}

bool TextArea::loadString(std::string fulltxt){
	lines.clear();
    std::vector<std::string> words = Constant::splitChar(fulltxt, ' ');
    lines.push_back("");

    const int spaceW = Fonts::getSize(this->fontType, " ");
    for (int i=0; i < (int)words.size(); i++){
		std::string word = words.at(i);
        int wordW = Fonts::getSize(this->fontType, word.c_str());
        int lineW = Fonts::getSize(this->fontType, lines.at(lines.size()-1).c_str());
        if (lineW + wordW + spaceW >= this->getW() - this->marginX){
            lines.push_back("");
            lines.at(lines.size()-1).append(word);
        } else {
            if (!lines.at(lines.size()-1).empty()){
                lines.at(lines.size()-1).append(" ");
            }
            lines.at(lines.size()-1).append(word);
        }
    }
    return true;
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