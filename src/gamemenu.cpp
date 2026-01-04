#include "gamemenu.h"

#include "gfx/SDL_gfxPrimitives.h"
#include <gfx/gfx_utils.h>
#include <io/dirutil.h>
#include <beans/structures.h>

GameMenu::GameMenu(CfgLoader *cfgLoader){
    emuCfgPos = 0;
	gameTicks.ticks = 0;
	video_page = NULL;
	dblBufferEnabled = true;
	this->cfgLoader = cfgLoader;
	this->initEngine(*cfgLoader);
};

GameMenu::~GameMenu(){
	LOG_DEBUG("Deleting GameMenu...");
    this->stopEngine();
}

bool GameMenu::initDblBuffer(int w, int h){
#ifdef SW_DBL_BUFFER
    if (video_page != NULL){
		SDL_FreeSurface(video_page);
    }

	/* Create bitmap for page flipping */
	video_page = SDL_CreateRGBSurface(
        screen->flags,          // Mismos flags (SWSURFACE/HWSURFACE)
        screen->w,              // Mismo ancho
        screen->h,              // Mismo alto
        screen->format->BitsPerPixel, // Misma profundidad de color
        screen->format->Rmask,  // Máscara Roja
        screen->format->Gmask,  // Máscara Verde
        screen->format->Bmask,  // Máscara Azul
        screen->format->Amask   // Máscara Alfa
    );
	dblBufferEnabled = true;
#else 
	video_page = screen;
	dblBufferEnabled = false;
#endif
    return video_page != NULL;
}

ConfigEmu GameMenu::getNextCfgEmu(){
    emuCfgPos++;
    emuCfgPos = emuCfgPos % cfgLoader->configEmus.size();
    return cfgLoader->configEmus.at(emuCfgPos);
}

ConfigEmu GameMenu::getPrevCfgEmu(){
    if (emuCfgPos <= 0 && cfgLoader->configEmus.size() > 0)
        emuCfgPos = cfgLoader->configEmus.size() - 1;
    else 
        emuCfgPos--;
    return cfgLoader->configEmus.at(emuCfgPos);
}

bool GameMenu::isDebug(){
    return cfgLoader->configMain.debug;
}

void GameMenu::setCfgLoader(CfgLoader *cfgLoader){
    this->cfgLoader = cfgLoader;
}


/**
 * 
 */
void GameMenu::createMenuImages(ListMenu &listMenu){
    /** snap */
    Image imageSnap;
    const int snapW = screen->w / 2;
    const int snapH = listMenu.getH() / 2;
    const int snapOffset = screen->w / 10;
    //const int snapOffset = 5;
    menuImages.clear();
    menuTextAreas.clear();

    if (screen->w / 2 >= 320){
        imageSnap.setX(screen->w / 2 + snapOffset);
        imageSnap.setY(listMenu.getY());
        imageSnap.setW(snapW - snapOffset * 2);
        imageSnap.setH(snapH - snapOffset);
    } else {
        imageSnap.setX(screen->w / 2);
        imageSnap.setY(listMenu.getY());
        imageSnap.setW(snapW);
        imageSnap.setH(snapH);
    }
    imageSnap.vAlign = ALIGN_TOP;
    menuImages.insert(make_pair(SNAP, imageSnap));

    /** Box2d */
    Image imageBox2d;
    const int box2dH = listMenu.getH() / 4;
    const int box2dW = screen->w / 8;
    imageBox2d.setX(screen->w / 2);
    imageBox2d.setY(screen->h / 2 - box2dH);
    imageBox2d.setW(box2dW);
    imageBox2d.setH(box2dH);
    menuImages.insert(make_pair(BOX2D, imageBox2d));

    /** snaptit*/
    Image imageSnaptit;
    const int snapTitH = listMenu.getH() / 4;
    const int snapTitW = screen->w / 6;
    imageSnaptit.setX(screen->w - snapTitW);
    imageSnaptit.setY(screen->h / 2 - snapTitH);
    imageSnaptit.setW(snapTitW);
    imageSnaptit.setH(snapTitH);
    menuImages.insert(make_pair(SNAPTIT, imageSnaptit));

    Image imageSnapFs(0, 0, screen->w, screen->h);
    imageSnapFs.drawfaded = true;
    menuImages.insert(make_pair(SNAPFS, imageSnapFs));
    
    const int sectionGap = 0;
    const int textAreaY = listMenu.getH() / 2 + listMenu.getY() + sectionGap;
    TextArea textarea(screen->w / 2, textAreaY, screen->w / 2, screen->h - textAreaY);
    textarea.marginX = floor((double)screen->w / 100);
    menuTextAreas.insert(make_pair(SYNOPSIS, textarea));
}

/**
 * 
 */
void GameMenu::refreshScreen(ListMenu &listMenu){
    ConfigEmu emu = this->cfgLoader->configEmus.at(this->emuCfgPos);
    //Drawing the emulator name
    TTF_Font *fontBig = Fonts::getFont(Fonts::FONTBIG);
    TTF_Font *fontsmall = Fonts::getFont(Fonts::FONTSMALL);
    const int sepVertX = listMenu.getW();
    const int halfWidth = screen->w / 2;
	int face_h_big = TTF_FontLineSkip(fontBig);
	int face_h_small = TTF_FontLineSkip(fontsmall);
	bool debug = true;

    //Drawing the rest of list and images
    if (listMenu.getNumGames() > (std::size_t)listMenu.curPos){
        auto game = listMenu.listGames.at(listMenu.curPos).get();
        
        if (!game->shortFileName.empty()){
            if (listMenu.layout == LAYBOXES) {
				Constant::drawTextCent(video_page, fontBig, emu.name.c_str(), 0, face_h_big < listMenu.marginY ? (listMenu.marginY - face_h_big) / 2 : 0 , 
					true, false, textColor, 0);
                
				fastline(this->video_page, listMenu.marginX, listMenu.marginY - 1 , screen->w - listMenu.marginX, listMenu.marginY - 1, menuBars);
                fastline(this->video_page, sepVertX, listMenu.marginY , sepVertX, listMenu.getH() + listMenu.marginY - 1, menuBars);
                listMenu.draw(this->video_page);

                //Drawing a transparent rectangle
                if (screen->w >= 640){
                    static const int transBGText = SDL_MapRGBA(this->video_page->format, 255, 255, 255, 160);
					SDL_Rect rec = {halfWidth + 1, listMenu.marginY, screen->w - (halfWidth + 2), screen->h - listMenu.marginY - 1};
					DrawRectAlpha(this->video_page, rec, white, 160);
                }

                //Draw and update the screen because the loading of images can take a long time
                if (listMenu.keyUp){
                    string assetsDir = getPathPrefix(emu.assets) + string(Constant::tempFileSep);
                    //Drawing the rom's synopsis text
                    menuTextAreas[SYNOPSIS].loadTextFileFromGame(assetsDir + "synopsis" + string(Constant::tempFileSep), *game, ".txt");
                    menuTextAreas[SYNOPSIS].resetTicks(this->gameTicks);
                    menuTextAreas[SYNOPSIS].draw(this->video_page, this->gameTicks);

                    //Snapshot picture
                    menuImages[SNAP].loadImageFromGame(assetsDir + "snap" + string(Constant::tempFileSep), *game, ".png");
                    menuImages[SNAP].printImage(this->video_page);
                    blit(this->video_page, screen, 0, 0, 0, 0, this->video_page->w, this->video_page->h);

                    if (screen->w < 640){
                        //If it's so small, only show the snapshot
                        return;
                    }

                    //Box picture
                    menuImages[BOX2D].loadImageFromGame(assetsDir + "box2d" + string(Constant::tempFileSep), *game, ".png");
                    menuImages[BOX2D].printImage(this->video_page);
                    blit(this->video_page, screen, 0, 0, 0, 0, this->video_page->w, this->video_page->h);

                    //Title picture
                    menuImages[SNAPTIT].loadImageFromGame(assetsDir + "snaptit" + string(Constant::tempFileSep), *game, ".png");
                    menuImages[SNAPTIT].printImage(this->video_page);
                    blit(this->video_page, screen, 0, 0, 0, 0, this->video_page->w, this->video_page->h);
                } else {
                    menuImages[SNAP].printImage(this->video_page);
                    menuImages[BOX2D].printImage(this->video_page);
                    menuImages[SNAPTIT].printImage(this->video_page);
                    menuTextAreas[SYNOPSIS].draw(this->video_page, this->gameTicks);
                }

            } else if (listMenu.layout == LAYSIMPLE) {
                if (listMenu.keyUp){
                    //Snapshot picture
                    menuImages[SNAPFS].loadImageFromGame(getPathPrefix(emu.assets) + string(Constant::tempFileSep)
                        + "snap" + string(Constant::tempFileSep), *game, ".png");
                }
                menuImages[SNAPFS].printImage(this->video_page);
                //Draw the menu element after the image
                Constant::drawTextCent(video_page, fontBig, emu.name.c_str(), 
					halfWidth, face_h_big < listMenu.marginY ? (listMenu.marginY - face_h_big) / 2 : 0 , 
					true, false, textColor, 0);

                fastline(this->video_page, listMenu.marginX, listMenu.marginY - 1, listMenu.getW(), listMenu.marginY - 1, textColor);
                listMenu.draw(this->video_page);

            } else if (listMenu.layout == LAYTEXT) {

				Constant::drawTextCent(video_page, fontBig, emu.name.c_str(), 
					halfWidth, face_h_big < listMenu.marginY ? (listMenu.marginY - face_h_big) / 2 : 0 , 
					true, false, textColor, 0);

                fastline(this->video_page, listMenu.marginX, listMenu.marginY - 1, listMenu.getW(), listMenu.marginY - 1, textColor);
                listMenu.draw(this->video_page);
            }
        }

    } else if (listMenu.getNumGames() == 0){
		Constant::drawTextCent(video_page, fontBig, emu.name.c_str(), 
					cfgLoader->getWidth(), face_h_big < listMenu.marginY ? (listMenu.marginY - face_h_big) / 2 : 0 , 
					true, false, textColor, 0);

        fastline(this->video_page, listMenu.marginX, listMenu.marginY - 1, screen->w - listMenu.marginX, listMenu.marginY - 1, textColor);
		Constant::drawTextCent(video_page, fontsmall, "No roms found", 0, 0, true, true, textColor, 0);
    } else {
		Constant::drawTextCent(video_page, fontsmall, "The configuration is not valid", 0, 0, true, true, textColor, 0);
		Constant::drawTextCent(video_page, fontsmall, "Press TAB to select the next entry or", 0, face_h_small + 3, true, true, textColor, 0);
		Constant::drawTextCent(video_page, fontsmall, "Press ESC to exit", 0, (face_h_small + 3) * 2, true, true, textColor, 0);
    }
}

/**
*
*/
void GameMenu::blit(SDL_Surface * src, SDL_Surface * dst, int x1, int y1, int w1, int h1, int w2, int h2){
	if (this->dblBufferEnabled){
		SDL_Rect srcRect = {x1, y1, w1, h1};
		SDL_Rect dstRect = {0, 0, w2, h2};
		SDL_BlitSurface(this->video_page, &srcRect, screen, &dstRect);
	} 
}

/**
 * 
 */
void GameMenu::showMessage(string msg){
    int startGray = 240;
    static const int bkg = SDL_MapRGB(this->video_page->format, startGray, startGray, startGray);
    TTF_Font *fontsmall = Fonts::getFont(Fonts::FONTSMALL);
	int face_h_small = TTF_FontLineSkip(fontsmall);
    
    int rw = Fonts::getSize(Fonts::FONTSMALL, msg.c_str()) + 5; 
    //int rh = this->video_page->h / 3;
    int rh = face_h_small * 2;
    int rx = (this->video_page->w - rw) / 2;
    int ry = (this->video_page->h - rh) / 2 + face_h_small / 2;

	SDL_Rect rect = {rx, ry, rw, rh};
    //drawing_mode(DRAW_MODE_TRANS, this->video_page, rx, ry);
    SDL_FillRect(this->video_page, &rect, bkg);
    
    const int step = 40;
    for (int i=1; i < 5; i++){
        int fadingBkg = SDL_MapRGB(this->video_page->format, startGray - i*step, startGray - i*step, startGray - i*step);
		//drawing_mode(DRAW_MODE_TRANS, this->video_page, rx, ry);
		rectangleColor(this->video_page, rx - i, ry - i, rx + rw + i, ry + rh + i, fadingBkg);
    }

    //drawing_mode(DRAW_MODE_SOLID, this->video_page, rx, ry);
	Constant::drawTextCent(video_page, fontsmall, msg.c_str(), 
		this->video_page->w / 2, this->video_page->h / 2, true, true, black, -1);

	SDL_BlitSurface(this->video_page, NULL, screen, NULL);
}

/**
 * 
 */
void GameMenu::loadEmuCfg(ListMenu &menuData){
    TTF_Font *fontsmall = Fonts::getFont(Fonts::FONTSMALL);
	int face_h_small = TTF_FontLineSkip(fontsmall);
	static const int cblack = SDL_MapRGB(this->video_page->format, backgroundColor.r, backgroundColor.g, backgroundColor.b);

    if (this->cfgLoader->configEmus.size() == 0){
        SDL_FillRect(screen, NULL, cblack);
        string msg = "There are no emulators configured. Exiting..."; 
		Constant::drawTextCent(screen, fontsmall, msg.c_str(), 0, 0, true, true,  white, -1);
		Constant::drawTextCent(screen, fontsmall, "Press a key to continue", 0, face_h_small + 3, true, true, white, -1);
		SDL_Flip(screen);
        SDL_Delay(3000);
        exit(0);
    }

    if (this->cfgLoader->configEmus.size() <= (std::size_t)this->emuCfgPos){
        this->emuCfgPos = 0;
    } 

    dirutil dir;
    ConfigEmu emu = this->cfgLoader->configEmus.at(this->emuCfgPos);
    string mapfilepath = Constant::getAppDir() //+ string(Constant::tempFileSep) + "gmenu" 
            + string(Constant::tempFileSep) + "config" + string(Constant::tempFileSep) + emu.map_file;
    
    if (emu.use_rom_file && !emu.map_file.empty() && dir.fileExists(mapfilepath.c_str())){
        menuData.mapFileToList(mapfilepath);
    } else {
        mapfilepath = getPathPrefix(emu.rom_directory);
        vector<unique_ptr<FileProps>> files;
        emu.rom_extension = " " + emu.rom_extension;
        string extFilter = Constant::replaceAll(emu.rom_extension, " ", ".");

        if (cfgLoader->configMain.debug){
            SDL_FillRect(screen, NULL, cblack);
            string msg = "searching " + mapfilepath; 
			Constant::drawTextCent(screen, fontsmall, msg.c_str(), screen->w / 2, screen->h / 2, true, true,  white, -1);
        }

        dir.listarFilesSuperFast(mapfilepath.c_str(), files, extFilter, true, false);

        ConfigEmu emu = this->cfgLoader->configEmus.at(this->emuCfgPos);
        string mapfilepath = getPathPrefix(emu.rom_directory);

        if (cfgLoader->configMain.debug){
            SDL_FillRect(screen, NULL, cblack);
            string msg = "roms found: " + Constant::TipoToStr(files.size()); 
            string msg2 = "In dir " + mapfilepath;
			Constant::drawTextCent(screen, fontsmall, msg.c_str(), 0, 0, true, true,  white, -1);
            Constant::drawTextCent(screen, fontsmall, msg2.c_str(), 0, face_h_small + 3, true, true,  white, -1);
			Constant::drawTextCent(screen, fontsmall, "Press a key to continue", 0, (face_h_small + 3) * 2, true, true,  white, -1);
            SDL_Delay(3000);
        }

        menuData.filesToList(files, emu);
        files.clear();
    }
}

/**
 * 
 */
string GameMenu::getPathPrefix(string filepath){
    ConfigEmu emu = this->cfgLoader->configEmus.at(this->emuCfgPos);
    string finalpath = cfgLoader->configMain.path_prefix + filepath;

    string drivestr = string(":") + string(Constant::tempFileSep);
    //Checking if the path to the roms is absolute
    if (!filepath.empty() && (filepath.at(0) == Constant::FILE_SEPARATOR || filepath.find(drivestr) != string::npos)){
        finalpath = filepath;
    }
    #if defined(WIN) || defined(DOS)
        finalpath = Constant::replaceAll(finalpath, "/", "\\");
    #endif
    return finalpath;
}

/**
 * 
 */
string GameMenu::encloseWithCharIfSpaces(string str, string encloseChar){
    str = Constant::Trim(str);
    return str.find(" ") != string::npos ? encloseChar + str + encloseChar : str;
}

/**
 * 
 */
void GameMenu::launchProgram(ListMenu &menuData){
    //Launcher launcher;
    dirutil dir;
    vector<string> commands;

    if (this->cfgLoader->configEmus.size() <= (std::size_t)this->emuCfgPos)
        return;

    ConfigEmu emu = this->cfgLoader->configEmus.at(this->emuCfgPos);

    commands.emplace_back(getPathPrefix(emu.directory) + string(Constant::tempFileSep)
        + emu.executable);
    
    if (emu.options_before_rom){
        vector<string> v = Constant::splitChar(emu.global_options, ' ');
        //for (auto s : v){
		for (int i=0; i < v.size(); i++){
			std::string s = v.at(i);
            commands.emplace_back(s);
        }
    }

    if (menuData.listGames.size() <= (std::size_t)menuData.curPos)
        return;

    auto game = menuData.listGames.at(menuData.curPos).get();
    
    //Ignoring the fields if a rom file is used
    if (emu.use_rom_file){
        commands.emplace_back(encloseWithCharIfSpaces(game->shortFileName, "\"")); 
    } else {
        string romdir = emu.use_rom_directory ? getPathPrefix(emu.rom_directory) + string(Constant::tempFileSep) : "";
        string romFile = game->longFileName;

        #ifdef DOS
            //Maybe the long filename support is activated on msdos,
            //Otherwise just pick up the shortfilename
            string fileabslongname = encloseWithCharIfSpaces(romdir + game->longFileName, "\"");
            string msg = "romdir: " + romdir;
            string msg1;
            string msg2;

            if (!dir.fileExists( fileabslongname.c_str()  )){
                romFile = game->shortFileName;
                msg1 = "file " + game->longFileName + "doesn't exists.";
                msg2 = "launching " + romFile;
            } else {
                msg1 = "launching " + romFile;
            }

            if (cfgLoader->configMain.debug){
                clear(screen);
                TTF_Font *fontsmall = Fonts::getFont(Fonts::FONTSMALL);
                Constant::drawTextCentre(screen, fontsmall, msg.c_str(), screen->w / 2, screen->h / 2, textColor, -1);
                Constant::drawTextCentre(screen, fontsmall, msg1.c_str(), screen->w / 2, screen->h / 2 + (fontsmall->face_h + 3) * 2, textColor, -1);
                Constant::drawTextCentre(screen, fontsmall, msg2.c_str(), screen->w / 2, screen->h / 2 + (fontsmall->face_h + 3) * 3, textColor, -1);
                Constant::drawTextCentre(screen, fontsmall, "Press a key to continue", screen->w / 2, screen->h / 2 + (fontsmall->face_h + 3) * 4, textColor, -1);
                readkey();
            }
        #endif

        string rom = emu.use_extension ? romFile : dir.getFileNameNoExt(romFile);
        commands.emplace_back(encloseWithCharIfSpaces(romdir + rom, "\"")); 
    }

    //string romToLaunch = romdir + rom;
    //if (!dir.fileExists(romToLaunch.c_str())){
    //    clear_to_color(screen, backgroundColor);
    //    string msg = "roms doesn't exist: " + romToLaunch; 
    //    textout_centre_ex(screen, font, msg.c_str(), screen->w / 2, screen->h / 2, textColor, -1);
    //    textout_centre_ex(screen, font, "Press a key to continue", screen->w / 2, screen->h / 2 + (font->height + 3) * 2, textColor, -1);
    //    readkey();
    //    return; // or handle the error as needed
    //}

    if (!emu.options_before_rom){
        commands.emplace_back(emu.global_options);
        //vector<string> v = Constant::splitChar(emu.global_options, ' ');
        //for (auto s : v){
        //    commands.emplace_back(s);
        //}
    }

    saveGameMenuPos(menuData);

    //For some reason, with Alsa, when launching retroarch, the sound must be deactivated. Otherwise, it freezes
    bool resetAudio = false;
    #ifdef UNIX
        resetAudio = true;
    #endif

	LOG_DEBUG("Launching %s", commands.at(0));
	
    /**TODO: IMPLEMENT
	if (cfgLoader->configMain.alsaReset && resetAudio && Constant::getExecMethod() != launch_batch ){
        remove_sound();
    }

    //if we are in fullscreen, switch to windowed, because the launched app maybe not 
    //be showed in the first plane
    bool isFullscreen = !is_windowed_mode();
    #ifdef DOS
        //In MSDOS we don't need to do the fullscreen switch
        isFullscreen = false;
    #endif

    if (isFullscreen)
        swithScreenFullWindow(*this->cfgLoader);

    launcher.launch(commands, cfgLoader->configMain.debug);

    //if we were in fullscreen, switch back to fullscreen
    if (isFullscreen)
        swithScreenFullWindow(*this->cfgLoader);
    
    //Try to reactivate sound, although it's pointless in my tests. I couldn't make it to work with Alsa.
    //Install pipewire instead
    if (cfgLoader->configMain.alsaReset && resetAudio && Constant::getExecMethod() != launch_batch ){
        this->initSound();
    }
	*/
}

/**
 * 
 */
int GameMenu::saveGameMenuPos(ListMenu &menuData){
    FILE* outfile;
    string filepath = Constant::getAppDir() + Constant::getFileSep() + MENUTMP;
    int ret = 0;
    // open file for writing
    outfile = fopen(filepath.c_str(), "wb");
    if (outfile == NULL) {
        cerr << "Error openning file: " << filepath << endl;
        return 1;
    }

    struct ListStatus input1 = { this->emuCfgPos, menuData.iniPos, menuData.endPos, 
        menuData.curPos, menuData.maxLines, menuData.layout, menuData.animateBkg};

    int flag = 0;
    flag = fwrite(&input1, sizeof(struct ListStatus), 1, outfile);

    if (flag) {
        cout << "Contents of the structure written successfully" << endl;
    } else {
        cerr << "Error Writing to File: " << filepath << endl;
        ret = 1;
    }
    fclose(outfile);
    return ret;
}

/**
 * 
 */
int GameMenu::recoverGameMenuPos(ListMenu &menuData, struct ListStatus &read_struct){
    FILE* infile;
    string filepath = Constant::getAppDir() + Constant::getFileSep() + MENUTMP;
    int ret = 0;

    // Open person.dat for reading
    infile = fopen(filepath.c_str(), "rb");
    if (infile == NULL) {
        cerr << "Error openning file: " << filepath << endl;
        return 1;
    }

    if (fread(&read_struct, sizeof(read_struct), 1, infile) > 0){
        cout << "emupos: " << read_struct.emuLoaded << "; inipos: " << read_struct.iniPos
            << "; endpos: " << read_struct.endPos << "; curpos: " << read_struct.curPos 
            << "; maxlines: " << read_struct.maxLines
            << "; layout: " << read_struct.layout
            << "; animateBkg: " << read_struct.animateBkg << endl;
        //Setting the emulator selected        
        this->emuCfgPos = read_struct.emuLoaded;
    } else {
        ret = 1;
    }

    fclose(infile);
    return ret;
}
