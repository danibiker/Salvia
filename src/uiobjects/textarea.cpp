#include <uiobjects/textarea.h>
#include <font/fonts.h>
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
    this->waiting = true;
}

/**
    * 
    */
bool TextArea::loadTextFileFromGame(std::string baseDir, GameFile game, std::string ext){
    dirutil dir;
    std::string fileToOpen = baseDir + dir.getFileNameNoExt(game.shortFileName) + ext;

    if (dir.fileExists(fileToOpen.c_str())){
        return loadTextFile(fileToOpen);
    } else {
        return loadTextFile(baseDir + dir.getFileNameNoExt(game.longFileName) + ext);
    }
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

            const int spaceW = Fonts::getSize(Fonts::FONTSMALL, " ");
            for (unsigned int i=0; i < words.size(); i++){
				std::string word = words.at(i);
                int wordW = Fonts::getSize(Fonts::FONTSMALL, word.c_str());
                int lineW = Fonts::getSize(Fonts::FONTSMALL, lines.at(lines.size()-1).c_str());
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
    return ret;
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
void TextArea::calcTicks(GameTicks gameTicks, int &scrollDesp, float &pixelDesp){
            
    if (!enableScroll)
        return;

	TTF_Font *fontSmall = Fonts::getFont(Fonts::FONTSMALL);
	int face_h = TTF_FontLineSkip(fontSmall);

    const size_t maxLines = (this->getH() - marginTop) / (face_h + lineSpace);
    const int TICKSTOLINE = 40;
    const int TICKSTOLINEPIXEL = 1;
    const int LOOPSTOSTART = 1;
            
    //To wait some moments before start scrolling
    if (scrollDesp == 0 && timesWaiting < LOOPSTOSTART){
        if (abs(gameTicks.ticks - lastWaitTick) >= TICKSTOLINE){
            timesWaiting += 1;
            lastWaitTick = gameTicks.ticks;
            waiting = true;
        }
        return;
    } else if (waiting){
        lastTick = gameTicks.ticks;
        waiting = false;
    }

    //Move one element of the list if needed
    if (abs(gameTicks.ticks - lastTick) >= TICKSTOLINE ){
        lastTick = gameTicks.ticks;
        pixelDesp = 0;
                
        if (lines.size() > maxLines){
            scrollDesp = (scrollDesp + 1) % (lines.size() - maxLines + 1) ;
            //To reset the wait status when we reach the final position
            if (scrollDesp == 0){
                resetTicks(gameTicks);
            }
        } else {
            scrollDesp = 0;
        }
    }
            
    //Move the text line an amount of pixels relative to the font height
    if (abs(gameTicks.ticks - lastSubTick) >= TICKSTOLINEPIXEL){
        lastSubTick = gameTicks.ticks;
        if (lines.size() > maxLines){
            if (TICKSTOLINEPIXEL != 0 && TICKSTOLINE / TICKSTOLINEPIXEL != 0)
                pixelDesp += (face_h + lineSpace) / (float)(TICKSTOLINE / TICKSTOLINEPIXEL);
            else 
                pixelDesp = 0;
        }
    }
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
    TTF_Font *fontSmall = Fonts::getFont(Fonts::FONTSMALL);
	int face_h = TTF_FontLineSkip(fontSmall);

    do{
        std::string line = lines.at(i + this->lastScroll);
        //textout_justify_ex(video_page, font, line.c_str(), this->getX(), this->getX() + this->getW() -1,
        //    nextLineY - pixelDesp, this->getW() / 3, Constant::textColor, -1);
        //alfont_textout_ex(video_page, fontSmall, line.c_str(), this->getX() + this->marginX, nextLineY - pixelDesp, Constant::textColor, -1);
		Constant::drawText(video_page, fontSmall, line.c_str(), this->getX() + this->marginX, (int) (nextLineY - pixelDesp), white, 0);

        nextLineY = this->getY() + marginTop + (++i) * (face_h + lineSpace);
    } while ((size_t) (i + this->lastScroll) < lines.size() && nextLineY < this->getY() + this->getH() - face_h);
}

/**
    * 
    */
void TextArea::draw(SDL_Surface *video_page){
    this->enableScroll = false;
	GameTicks ticks = {0};
    draw(video_page, ticks);
    //int nextLineY = this->getY() + marginTop;
    //int i = 0;
    //
    //if (lines.empty() || lines.size() == 0){
    //    return;
    //}
    //
    //do{
    //    std::string line = lines.at(i);
    //    textout_justify_ex(video_page, font, line.c_str(), this->getX(), this->getX() + this->getW() -1,
    //        nextLineY, this->getW() / 3, Constant::textColor, -1);
    //    nextLineY = this->getY() + marginTop + (++i) * (fontSmall->face_h + lineSpace);
    //} while (i < lines.size() && nextLineY < this->getY() + this->getH() - fontSmall->face_h);
}