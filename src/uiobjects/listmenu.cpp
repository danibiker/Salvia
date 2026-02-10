#include "listmenu.h"

#include <io/dirutil.h>
#include <beans/structures.h>
//#include "utils/so/dosnames.h"
#include <font/fonts.h>
#include <gfx/SDL_gfxPrimitives.h>
#include <gfx/gfx_utils.h>
#include <image/icons.h>

SDL_Surface* ListMenu::imgText;

void ListMenu::clearSelectedText(){
    if (imgText != NULL){
		SDL_FreeSurface(imgText);
        imgText = NULL;
    }
}

ListMenu::ListMenu(int screenw, int screenh){
    iniPos = 0;
    endPos = 0;
    curPos = 0;
    listSize = 0;
    maxLines = 0;
    marginX = (int)floor((double)(screenw / 100));
    marginY = (int) (screenh / SCREENHDIV * 1.5);
    lastSel = -1;
    pixelShift = 0;
    keyUp = false;
    animateBkg = true;
    setObjectType(GUILISTBOX);
    setLayout(LAYSIMPLE, screenw, screenh);
    //set_trans_blender(255, 255, 255, 190);
}

ListMenu::~ListMenu(){
	LOG_DEBUG("Deleting ListMenu...");
    clear();
}

void ListMenu::clear(){
    if (listGames.size() > 0)
        listGames.clear();
}

size_t ListMenu::getNumGames(){
    return listGames.size();
}

int ListMenu::getScreenNumLines(){
	TTF_Font *fontMenu = Fonts::getFont(Fonts::FONTBIG);
	int face_h = TTF_FontLineSkip(fontMenu);
    return face_h != 0 ? (int)std::floor((double)getH() / face_h) : 0;
}

/**
    * 
    */
void ListMenu::setLayout(int layout, int screenw, int screenh){
    this->marginY = (int) (screenh / SCREENHDIV * 1.5);
    clearSelectedText();

    if (layout == LAYBOXES){
        this->setX(0);
        this->setY(marginY);
        this->setW(screenw / 2);
        this->setH(screenh - marginY);
        this->centerText = false;
        this->layout = layout;

        for (size_t i=0; i < listGames.size(); i++){
            auto file = listGames.at(i).get();
            file->cutTitleIdx = Fonts::idxToCutTTF(file->gameTitle, this->getW() - 2*this->marginX, Fonts::FONTBIG);
        }
    } else {
        this->setX(marginX);
        this->setY(marginY);
        this->setW(screenw - marginX);
        this->setH(screenh - marginY);
        this->centerText = true;
        this->layout = layout;
    }
}



/**
    * 
    */
void ListMenu::draw(SDL_Surface *video_page){
	static const int bkg = SDL_MapRGB(video_page->format, bkgMenu.r, bkgMenu.g, bkgMenu.b);
	TTF_Font *fontMenu = Fonts::getFont(Fonts::FONTBIG);
	int face_h = TTF_FontLineSkip(fontMenu);

    //To scroll one letter in one second. We use the face_h because the width of 
    //a letter is not fixed.
    const float pixelsScrollFps = max(ceil(face_h / (float)textFps), 1.0f);

	Icons icons;
	const int marginTextIcon = icons.icon_w_add + 14;

    for (int i=this->iniPos; i < this->endPos; i++){
        auto game = this->listGames.at(i).get();
        const int screenPos = i - this->iniPos;
        const int fontHeightRect = screenPos * face_h;
        const int lineBackground = -1;
        SDL_Color lineTextColor = i == this->curPos ? black : white;
        string line = game->gameTitle.empty() ? game->shortFileName : game->gameTitle;

        //Drawing a faded background selection rectangle
        if (i == this->curPos){
            int y = this->getY() + fontHeightRect;
            //Gaining some extra fps when the screen resolution is low
			SDL_Rect rectElem = {this->getX() + marginX, y, this->getW() - 2 * marginX, face_h};
            if (video_page->h >= 480){
                //Weird things happen if this line is not used here
                //when using antialiased text
                //set_trans_blender(255, 255, 255, 190);
                //drawing_mode(DRAW_MODE_TRANS, video_page, this->getX(), this->getY());
                //rectfill(video_page, this->getX() + marginX, y, this->getW() - marginX, y + fontMenu->face_h, colorTrans);
                //drawing_mode(DRAW_MODE_SOLID, video_page, this->getX(), this->getY());
				//SDL_FillRect(video_page, &rectElem, bkg);
				DrawRectAlpha(video_page, rectElem, bkgMenu, 190);
            } else {
                lineTextColor = white;
            }
			rect(video_page, rectElem.x - 1, rectElem.y - 1, rectElem.x + rectElem.w, rectElem.y + rectElem.h, bkgMenu);
        }
                
        //Drawing the selected option in a separate bitmap to allow scrolling
        if (layout == LAYBOXES){
            static int txtDifWidth = 0;
			static uint32_t lastTick = SDL_GetTicks();

            if (lastSel != this->curPos && i == this->curPos){
                clearSelectedText();
				const int txtMaxWidth = Fonts::getSize(Fonts::FONTBIG, line.substr(0, game->cutTitleIdx).c_str());
                const int txtTotalWidth = Fonts::getSize(Fonts::FONTBIG, line.c_str());
                txtDifWidth = txtTotalWidth - txtMaxWidth;
				imgText = TTF_RenderText_Blended(fontMenu, line.c_str(), lineTextColor);
                lastSel = this->curPos;
            }
                    
            //Scrolling the text if it's big enough to not fill on the screen 
			if (game->cutTitleIdx < game->gameTitle.length() && txtDifWidth > 0 && SDL_GetTicks() > lastTick + frameTimeText){
                //Waiting at the beginning and the end of the scrolling
                if (pixelShift == 0 || pixelShift + pixelsScrollFps >= txtDifWidth){
                    //Adding a decimal to not enter again while we should be waiting
                    pixelShift += 0.1f;
                    lastTick += waitTitleMove;
                } else {
                    //pixelShift = (int)floor(pixelShift + pixelsScrollFps) % txtDifWidth;
					// fmod permite hacer el "módulo" con números float/double
					pixelShift = fmod(pixelShift + pixelsScrollFps, (float)txtDifWidth);
					lastTick = SDL_GetTicks();
                }
            }
            line = game->gameTitle.substr(0, game->cutTitleIdx);
        }

        //Finally drawing the text
        if (this->centerText){
            //alfont_textout_centre_ex(video_page, fontMenu, line.c_str(), centerPos, 
            //    this->getY() + fontHeightRect, lineTextColor, lineBackground);
			Constant::drawTextCent(video_page, fontMenu, line.c_str(), 0, this->getY() + fontHeightRect, true, false, lineTextColor, lineBackground);
        } else {
			SDL_Rect dstRect;
			dstRect.x = this->getX() + marginX;
			dstRect.y = this->getY() + fontHeightRect;

            if (layout == LAYBOXES && i == this->curPos && imgText != NULL){
				SDL_Rect srcRect;
				srcRect.x = (Sint16)pixelShift;
				srcRect.y = 0;
				srcRect.w = this->getW() - 2 * this->marginX;
				srcRect.h = face_h; // Usando la equivalencia de face_h

				SDL_Rect dstRectWithMargin = dstRect;
				dstRectWithMargin.x += marginTextIcon;
				// imgText es la superficie con el texto
				SDL_BlitSurface(imgText, &srcRect, video_page, &dstRectWithMargin);

			} else {
                Constant::drawTextTransparent(video_page, fontMenu, line.c_str(), this->getX() + marginX + marginTextIcon, 
                    this->getY() + fontHeightRect, lineTextColor, lineBackground);
            }

			if (icons.icons.size() > page_white){
				//dstRect.y = icons.icon_w_add / 2;
				dstRect.y += 2;
				//dstRect.x = 4;
				//SDL_BlitSurface(icons.icons[page_white], NULL, video_page, &dstRect);
				SDL_BlitSurface(icons.icons_carts[getCartForSystem(game->systemid)], NULL, video_page, &dstRect);
			}
        }
    }
}

int ListMenu::getCartForSystem(int systemid){
	switch(systemid){
		case 1:
			return cart_genesis;
		case 2: 
			return cart_sms;
		case 3:
			return cart_nes;
		case 4:
			return cart_snes;
		case 9:
			return cart_gb;
		case 12:
			return cart_gba;
		case 19:
			return cart_32x;
		case 20:
			return cart_mcd;
		case 21:
			return cart_gg;
		case 31:
			return cart_pce;
		case 57:
			return cart_psx;
		default:
			return cart_nes;
	}
}

/**
    * 
    */
void ListMenu::mapFileToList(string filepath) {
    dirutil dir;
    //if (dir.fileExists(filepath.c_str()) && !dir.isDir(filepath.c_str())){
        fstream fileRomList;
        fileRomList.open(filepath, ios::in);

        if (fileRomList.is_open()){
            this->clear();
            string uri;
            string filelong;
            while(getline(fileRomList, uri)){
                if (uri.length() > 1){
                    GameFile gameFile;
                    std::size_t found = uri.find_first_of(" ");
                    if (found != string::npos){
                        gameFile.shortFileName = uri.substr(0, found);
                        gameFile.longFileName = gameFile.shortFileName;

                        filelong = Constant::Trim(Constant::replaceAll(uri.substr(found + 1), "\"", ""));
                        //gameFile.gameTitle = Constant::cutToLength(filelong, this->getW());
                        gameFile.gameTitle = filelong;
                        gameFile.cutTitleIdx = Fonts::idxToCutTTF(filelong, this->getW() - 2*this->marginX, Fonts::FONTBIG);
                    }
					listGames.push_back(std::unique_ptr<GameFile>(new GameFile(gameFile)));
                }
            }
            std::sort(listGames.begin(), listGames.end(), ListMenu::compareUniquePtrs);
            resetIndexPos();
        }
        fileRomList.close();
    //}
}

// Define the comparison function
bool ListMenu::compareUniquePtrs(const std::unique_ptr<GameFile>& a,
                        const std::unique_ptr<GameFile>& b) {
    string sA = !a->gameTitle.empty() ? a->gameTitle : a->shortFileName;
    string sB = !b->gameTitle.empty() ? b->gameTitle : b->shortFileName;
    Constant::lowerCase(&sA);
    Constant::lowerCase(&sB);
    return sA.compare(sB) < 0;
}

/**
    * 
    */
void ListMenu::filesToList(vector<unique_ptr<FileProps>> &files, ConfigEmu emu) {
    this->clear();
    dirutil dir;

	vector<string> v = Constant::splitChar(emu.system, '_');
	int system = 0;
	if (v.size() > 0){
		system = Constant::strToTipo<int>(v.at(0));
	}

    //string filelong;
    for (size_t i=0; i < files.size(); i++){
        GameFile gameFile;
		gameFile.systemid = system;
        auto file = files.at(i).get();
        //filelong = emu.use_extension ? file->filename : dir.getFileNameNoExt(file->filename);
        gameFile.shortFileName = file->filename;
        gameFile.longFileName = file->filename;
        //gameFile.gameTitle = Constant::cutToLength(dir.getFileNameNoExt(file->filename), this->getW());
        gameFile.gameTitle = dir.getFileNameNoExt(file->filename);
        gameFile.cutTitleIdx = Fonts::idxToCutTTF(gameFile.gameTitle, this->getW() - 2*this->marginX, Fonts::FONTBIG);
		listGames.push_back(std::unique_ptr<GameFile>(new GameFile(gameFile)));
    }
            
    /*#ifndef DOS
        //This is to be able of showing the images or the synopsis
        //if the download was in dos and we want to use another Operating System
        //but not having to download all again
        vector<string> v = Constant::splitChar(emu.system, '_');
        if (v.size() > 0 && Constant::MAME_SYS_ID.compare(v.at(0)) != 0){
            //For mame there is no need, because names are already short
            DosNames dosnames;
            dosnames.convertirNombresCortos(this->listGames);
        }
    #endif*/

    // Sort the vector
    std::sort(this->listGames.begin(), this->listGames.end(), ListMenu::compareUniquePtrs);
    resetIndexPos();
}

/**
    * 
    */
void ListMenu::resetIndexPos(){
    this->listSize = this->listGames.size();
    this->maxLines = this->getScreenNumLines();
    /*To go to the bottom of the list*/
    //this->endPos = getListGames()->size();
    //this->iniPos = (int)getListGames()->size() >= this->maxLines ? getListGames()->size() - this->maxLines : 0;
    //this->curPos = this->endPos - 1;
    /*To go to the init of the list*/
    this->iniPos = 0;
    this->curPos = 0;
    this->endPos = (int)this->listGames.size() > this->maxLines ? this->maxLines : this->listGames.size();
    this->pixelShift = 0;
    this->lastSel = -1;
}

void ListMenu::nextPos(){
    if (this->curPos < this->listSize - 1){
        this->curPos++;
        int posCursorInScreen = this->curPos - this->iniPos;

        if (posCursorInScreen > this->maxLines - 1){
            this->iniPos++;
            this->endPos++;
        }
        this->pixelShift = 0;
        this->lastSel = -1;
    }
}

void ListMenu::prevPos(){
    if (this->curPos > 0){
        this->curPos--;
        if (this->curPos < this->iniPos && this->curPos >= 0){
            this->iniPos--;
            this->endPos--;
        }
        this->pixelShift = 0;
        this->lastSel = -1;
    }
}

void ListMenu::nextPage(){
    for (int i=0; i < this->maxLines -1; i++){
        nextPos();
    }
}

void ListMenu::prevPage(){
    for (int i=0; i < this->maxLines -1; i++){
        prevPos();
    }
}