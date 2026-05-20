#include <string>
#include <vector>

#include <libretro.h>
#include <SDL.h>

#include <const/constant.h>

#define KEYCAP_W 70
#define KEYCAP_H 40
#define TOTAL_BLINK_FRAMES 15


struct t_cap_key{
	int w;
	int h;
	uint16_t retro_key;
	uint16_t retro_mod;
	uint32_t character;

	int keySpaces;
	std::string keyLabel;
	int blinkingPeriod;

	t_cap_key(){
		w = KEYCAP_W;
		h = KEYCAP_H; 
		keySpaces = 1;
		keyLabel = "X";
		retro_key = 0;
		retro_mod = 0;
		character = 0;
		blinkingPeriod = 0;
	}

	t_cap_key(t_cap_key* capToCopy){
		w = capToCopy->w;
		h = capToCopy->h;
		retro_key = capToCopy->retro_key;
		retro_mod = capToCopy->retro_mod;
		character = capToCopy->character;
		keySpaces = capToCopy->keySpaces;
		keyLabel = capToCopy->keyLabel;
		blinkingPeriod = capToCopy->blinkingPeriod;
	}
};

struct t_pressed_key{
	int row, col;
	retro_mod mod;
	t_pressed_key(){
		row = 0;
		col = 0;
		mod = RETROKMOD_NONE;
	}
};

struct t_keyboard {
    int rows;
    int cols;
    const int spaceX;
    const int spaceY;
    int iniX;
    int iniY;
	SDL_Color textColor;
	SDL_Color textSelectedColor;
	int dirtyKeyb;
	int keyW;
	int keyH;
	// Variables de control de seleccion (-1 significa que no hay nada seleccionado)
    int selectedRow;
    int selectedCol;
	int savedSelectedCol;
	int savedSelectedRow;
	int keyboardW;
	int keyboardH;

    std::vector<std::vector<t_cap_key> > caps;
    std::vector<std::vector<std::string> > layout;
    std::vector<std::vector<int> > layoutWidth;  // Almacena el ancho de la celda
    std::vector<std::vector<int> > layoutHeight; // Almacena la altura de la celda

	std::vector<std::vector<retro_key> > retroKeys;
	std::vector<std::vector<retro_mod> > retroMods;
	std::vector<std::vector<uint32_t> > retroCharacters;
	std::vector<t_pressed_key> pressedMods;

	SDL_Surface* keyboardSurface;	

    t_keyboard(): spaceX(3), spaceY(6), iniX(0), iniY(0), selectedRow(0), selectedCol(0)
    {
		textColor = white;
		textSelectedColor = black;
		dirtyKeyb = true;
		keyW = KEYCAP_W;
		keyH = KEYCAP_H;
		keyboardSurface = nullptr;
		keyboardW = 0;
		keyboardH = 0;
    }
	
	~t_keyboard(){
		if (keyboardSurface != nullptr){
			SDL_FreeSurface(keyboardSurface);
		}
	}

	void resize(int r, int c){
		rows = r;
		cols = c;
		savedSelectedCol = 0;
		savedSelectedRow = 0;
		selectedRow = 0;
		selectedCol = 0;
		
		caps.clear();
        layout.clear();
        layoutWidth.clear();
        layoutHeight.clear();
		retroKeys.clear();
		retroMods.clear();
		retroCharacters.clear();

		caps.resize(rows, std::vector<t_cap_key>(cols));
        layout.resize(rows, std::vector<std::string>(cols));
        layoutWidth.resize(rows, std::vector<int>(cols));
        layoutHeight.resize(rows, std::vector<int>(cols));
		retroKeys.resize(rows, std::vector<retro_key>(cols));
		retroMods.resize(rows, std::vector<retro_mod>(cols));
		retroCharacters.resize(rows, std::vector<uint32_t>(cols));
		
		if (keyboardSurface != nullptr){
			SDL_FreeSurface(keyboardSurface);
			keyboardSurface = nullptr;
		}
	}

	void checkUpRows(){
		
		if (savedSelectedRow != 0){
			selectedRow = savedSelectedRow;
		}
		savedSelectedRow = 0;
		savedSelectedCol = 0;

		int tmpRow = selectedRow;
		while(layoutHeight[tmpRow][selectedCol] == 0){
				
			if (tmpRow == 0){
				tmpRow = rows - 1;
			} else {
				tmpRow--;
			}

			if (tmpRow == selectedRow)
				break;
				
			if (layoutHeight[tmpRow][selectedCol] > 1){
				savedSelectedRow = selectedRow;
				selectedRow = tmpRow;
			}
		}
	}
	
	void nextCol(){
		do{
			selectedCol = (selectedCol + 1) % cols;
			checkUpRows();
		} while(layout[selectedRow][selectedCol].empty());
	}

	void prevCol(){
		do{
			if (selectedCol == 0){
				selectedCol = cols - 1;
			} else {
				selectedCol--;
			}
			checkUpRows();
		} while(layout[selectedRow][selectedCol].empty());
	}
	
	void checkLeftCols(){
		int tmpCol = selectedCol;
			
		if (savedSelectedCol != 0){
			selectedCol = savedSelectedCol;
		}
			
		savedSelectedCol = 0;
		savedSelectedRow = 0;

		while(layoutWidth[selectedRow][tmpCol] == 0){
			if (tmpCol == 0){
				tmpCol = cols - 1;
			} else {
				tmpCol--;
			}
				
			//Si hemos dado toda la vuelta, salimos
			if (tmpCol == selectedCol)
				break;
			//Si encontramos la tecla alargada, la seleccionamos
			if (layoutWidth[selectedRow][tmpCol] > 1){
				savedSelectedCol = selectedCol;
				selectedCol = tmpCol;
			}
		}
	}
	
	void nextRow(){
		do{
			selectedRow = (selectedRow + 1) % rows;
			checkLeftCols();
		} while(layout[selectedRow][selectedCol].empty());
	}

	void prevRow(){
		do{
			if (selectedRow == 0){
				selectedRow = rows - 1;
			} else {
				selectedRow--;
			}
			checkLeftCols();
		} while(layout[selectedRow][selectedCol].empty());
	}

	t_cap_key getSelectedKey(){
		t_cap_key ret = t_cap_key(caps[selectedRow][selectedCol]);

		if (caps[selectedRow][selectedCol].retro_mod != RETROKMOD_NONE){
			// Guardamos la tecla mod pulsada
			t_pressed_key pressed;
			pressed.row = selectedRow;
			pressed.col = selectedCol;
			pressed.mod = (retro_mod) caps[selectedRow][selectedCol].retro_mod;
			pressedMods.emplace_back(pressed);
		} else {
			// Si es una tecla normal, comprobamos si tiene algun modificador anterior pulsado
			ret.retro_mod = 0;
			for (unsigned int i=0; i < pressedMods.size(); i++){
				//Asignamos los modificadores
				ret.retro_mod |= pressedMods[i].mod;
			}
			//Limpiamos los modificadores si habia alguno
			pressedMods.clear();
			caps[selectedRow][selectedCol].blinkingPeriod = TOTAL_BLINK_FRAMES;
		}
		return ret;
	}

	void setKeyboardLayout(std::string keybType, int w, int h){
		if (keybType == "spectrum"){
			initSpectrum(w, h);
		} else if (keybType == "msx"){
			initMSX(w, h);
		} else {
			initMSX(w, h);
		}
	}

    void initSpectrum(int screenW, int screenH) {
		resize(4, 13);

        const char* localLayout[4][13] = {
            {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "", "", "DEL"},
            {"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "", "", ""},
            {"A", "S", "D", "F", "G", "H", "J", "K", "L", "ENTER", "", "\xE2\x86\x91", ""},
            {"CAPS", "Z", "X", "C", "V", "B", "N", "M", "SYM", "SPACE", "\xE2\x86\x90", "\xE2\x86\x93", "\xE2\x86\x92"}
        };

        const int localLayoutW[4][13] = {
            {1,1,1,1,1,1,1,1,1,1,1,1,1},
            {1,1,1,1,1,1,1,1,1,1,1,1,1},
            {1,1,1,1,1,1,1,1,1,1,1,1,1},
            {1,1,1,1,1,1,1,1,1,1,1,1,1}
        };

        // Todas las teclas del Spectrum ocupan exactamente 1 fila de alto
        const int localLayoutH[4][13] = {
            {1,1,1,1,1,1,1,1,1,1,1,1,1},
            {1,1,1,1,1,1,1,1,1,1,1,1,1},
            {1,1,1,1,1,1,1,1,1,1,1,1,1},
            {1,1,1,1,1,1,1,1,1,1,1,1,1}
        };

		const retro_key retroKeyLayout[4][13] = {
			{RETROK_1, RETROK_2, RETROK_3, RETROK_4, RETROK_5, RETROK_6, RETROK_7, RETROK_8, RETROK_9, RETROK_0, RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_BACKSPACE},
			{RETROK_q, RETROK_w, RETROK_e, RETROK_r, RETROK_t, RETROK_y, RETROK_u, RETROK_i, RETROK_o, RETROK_p, RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN},
			{RETROK_a, RETROK_s, RETROK_d, RETROK_f, RETROK_g, RETROK_h, RETROK_j, RETROK_k, RETROK_l, RETROK_RETURN, RETROK_UNKNOWN, RETROK_UP, RETROK_UNKNOWN},
			{RETROK_LSHIFT, RETROK_z, RETROK_x, RETROK_c, RETROK_v, RETROK_b, RETROK_n, RETROK_m, RETROK_RSHIFT, RETROK_SPACE, RETROK_LEFT, RETROK_DOWN, RETROK_RIGHT}
		};
		
		const retro_mod retroModLayout[4][13] = {
			{RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE},
			{RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE},
			{RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE},
			{RETROKMOD_CAPSLOCK, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_CTRL, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE},
		};
		
        // Enviamos las tres matrices de configuraciï¿½n
        setLayout((const char**)localLayout, &localLayoutW[0][0], &localLayoutH[0][0], &retroKeyLayout[0][0], &retroModLayout[0][0], screenW, screenH);
    }

    void initMSX(int screenW, int screenH){
        resize(6, 15);

        const char* localLayout[6][15] = {
            {"F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "", "", "HOME", "INS", "SUPR"},
            {"ESC", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "=", "\\", "BS"},
            {"TAB", "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "[", "]", "ENTER", ""}, 
            {"CTRL", "A", "S", "D", "F", "G", "H", "J", "K", "L", ";", "´", "`", "", ""},      
            {"SHIFT", "Z", "X", "C", "V", "B", "N", "M", ",", ".", "/", "'", "SHIFT", "\xE2\x86\x91", ""},
            {"", "", "CAPS", "GRAPH", "SPACE", "", "", "", "", "SELECT", "CODE", "STOP", "\xE2\x86\x90", "\xE2\x86\x93", "\xE2\x86\x92"}
        };

		const retro_key retroKeyLayout[6][15] = {
			{RETROK_F1, RETROK_F2, RETROK_F3, RETROK_F4, RETROK_F5, RETROK_F6, RETROK_F7, RETROK_F8, RETROK_F9, RETROK_F10, RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_HOME, RETROK_INSERT, RETROK_DELETE},
			{RETROK_ESCAPE, RETROK_1, RETROK_2, RETROK_3, RETROK_4, RETROK_5, RETROK_6, RETROK_7, RETROK_8, RETROK_9, RETROK_0, RETROK_MINUS, RETROK_EQUALS, RETROK_BACKSLASH, RETROK_BACKSPACE},
			{RETROK_TAB, RETROK_q, RETROK_w, RETROK_e, RETROK_r, RETROK_t, RETROK_y, RETROK_u, RETROK_i, RETROK_o, RETROK_p, RETROK_LEFTBRACKET, RETROK_RIGHTBRACKET, RETROK_RETURN, RETROK_UNKNOWN},
			{RETROK_LCTRL, RETROK_a, RETROK_s, RETROK_d, RETROK_f, RETROK_g, RETROK_h, RETROK_j, RETROK_k, RETROK_l, RETROK_SEMICOLON, RETROK_QUOTE, RETROK_BACKQUOTE , RETROK_UNKNOWN, RETROK_UNKNOWN},
			{RETROK_LSHIFT, RETROK_z, RETROK_x, RETROK_c, RETROK_v, RETROK_b, RETROK_n, RETROK_m, RETROK_COMMA, RETROK_COLON, RETROK_SLASH, RETROK_SEMICOLON, RETROK_RSHIFT, RETROK_UP, RETROK_UNKNOWN},
			{RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_CAPSLOCK, RETROK_LALT, RETROK_SPACE, RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_UNKNOWN, RETROK_TAB, RETROK_LCTRL, RETROK_BREAK, RETROK_LEFT, RETROK_DOWN, RETROK_RIGHT}
		};
		
		const retro_mod retroModLayout[6][15] = {
			{RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE},
			{RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE},
			{RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE},
			{RETROKMOD_CTRL, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE},
			{RETROKMOD_SHIFT, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_SHIFT, RETROKMOD_NONE, RETROKMOD_NONE},
			{RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE, RETROKMOD_NONE}
		};

        const int localLayoutW[6][15] = {
            {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
            {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
            {1,1,1,1,1,1,1,1,1,1,1,1,1,2,0}, 
            {1,1,1,1,1,1,1,1,1,1,1,1,1,0,0}, 
            {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
            {1,1,1,1,5,0,0,0,0,1,1,1,1,1,1}
        };

        const int localLayoutH[6][15] = {
            {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
            {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
            {1,1,1,1,1,1,1,1,1,1,1,1,1,2,1}, 
            {1,1,1,1,1,1,1,1,1,1,1,1,1,0,1}, 
            {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
            {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
        };

        setLayout((const char**)localLayout, &localLayoutW[0][0], &localLayoutH[0][0], &retroKeyLayout[0][0], &retroModLayout[0][0], screenW, screenH);
    }

    void setLayout(const char** srcLayout, const int* srcLayoutW, const int* srcLayoutH, const retro_key* keyLayout, const retro_mod* modLayout, 
					int screenW, int screenH) {
		
		keyboardW = cols * keyW + (cols - 1) * spaceX;
		keyboardH = rows * keyH + rows * spaceY;
		iniX = (screenW - keyboardW) / 2;
		iniY = screenH - keyboardH;

		// 1. Volcar la distribucion calculando anchos y altos basados en t_cap_key
		for (int row = 0; row < rows; row++) {
			for (int col = 0; col < cols; col++) {

				this->layout[row][col] = srcLayout[(row * cols) + col];
                this->layoutWidth[row][col] = srcLayoutW[(row * cols) + col];
                this->layoutHeight[row][col] = srcLayoutH[(row * cols) + col];
				this->retroKeys[row][col] = keyLayout[(row * cols) + col];
				this->retroMods[row][col] = modLayout[(row * cols) + col];

				caps[row][col].keyLabel = layout[row][col];
				caps[row][col].keySpaces = layoutWidth[row][col];
				caps[row][col].retro_key = retroKeys[row][col];
				caps[row][col].retro_mod = retroMods[row][col];

				if (this->layout[row][col].length() == 1){
					caps[row][col].character = this->layout[row][col][0];
				} else {
					caps[row][col].character = 0;
				}

				int defaultW = caps[row][col].w; // Ancho inicial por defecto
				int defaultH = caps[row][col].h; // Alto inicial por defecto (definido en t_cap_key)

				// Calcular ancho dinamico
				if (caps[row][col].keySpaces > 0) {
					caps[row][col].w = (caps[row][col].keySpaces * defaultW) + ((caps[row][col].keySpaces - 1) * spaceX);
				} else {
					caps[row][col].w = 0; 
				}

				// Calcular alto dinamico
				int currentKeyH = layoutHeight[row][col];
				if (currentKeyH > 0) {
					caps[row][col].h = (currentKeyH * defaultH) + ((currentKeyH - 1) * spaceY);
				} else {
					caps[row][col].h = 0; // Celda absorbida verticalmente
				}
			}
		}
    }
};