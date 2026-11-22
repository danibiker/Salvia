#pragma once

#include "fonts.h"
#include "utf8util.h"
#include <font/Arimo_Regular.ttf.h>
#include <utils/logger.h>
#include <io/cfgloader.h>
#include <stdio.h>

Fileio Fonts::fileio;
TTF_Font* Fonts::vFonts[2];
TTF_Font* Fonts::vFallbackFonts[2];
std::map<TTF_Font*, TTF_Font*> Fonts::fallbackOf;
std::string Fonts::s_fallbackPath;
std::vector<char> Fonts::s_fallbackData;
int Fonts::s_fontSize = 0;
SDL_mutex *Fonts::mutex = NULL;

/* Trozo de texto que se dibuja entero con una misma fuente. */
struct TextRun {
	TTF_Font* font;
	std::string text;
};

enum RenderMode { RENDER_BLENDED, RENDER_SOLID, RENDER_SHADED };

Fonts::Fonts(){
	mutex = SDL_CreateMutex();
	if (TTF_Init() == -1) {
		LOG_ERROR("Error TTF_Init: %s\n", TTF_GetError());
	}
}

Fonts::~Fonts(){
	LOG_DEBUG("Deleting Fonts...");
	destroy();
}

/**
    *
    */
void Fonts::destroy(){
    for (unsigned int i=0; i < 2; i++){
        if (vFonts[i] != NULL){
            TTF_CloseFont(vFonts[i]);
            vFonts[i] = NULL;
        }
        if (vFallbackFonts[i] != NULL){
            TTF_CloseFont(vFallbackFonts[i]);
            vFallbackFonts[i] = NULL;
        }
    }
	fallbackOf.clear();
	TTF_Quit();
	fileio.clearFile();
	std::vector<char>().swap(s_fallbackData);   // libera la memoria
	if (mutex) {
		SDL_DestroyMutex(mutex);
		mutex = NULL;
	}
}

/**
 * Fichero de la fuente CJK que se usa para el idioma del menu. Cada idioma
 * tiene la suya porque el mismo ideograma se dibuja distinto en japones, en
 * coreano y en chino simplificado o tradicional. Con cualquier otro idioma se
 * usa la japonesa, que es la mas habitual en los titulos de los juegos.
 */
std::string Fonts::cjkFontFile(const std::string& lang){
	if (lang == "kr") return "NotoSansKR-Regular.ttf";
	if (lang == "tw") return "NotoSansTC-Regular.ttf";
	if (lang == "zh") return "NotoSansSC-Regular.ttf";
	return "NotoSansJP-Regular.ttf";
}

/**
 * Idiomas del menu escritos con caracteres CJK: en ellos casi todo el texto
 * se dibuja con la fuente de reserva.
 */
bool Fonts::isCjkLang(const std::string& lang){
	return lang == "ja" || lang == "kr" || lang == "tw" || lang == "zh";
}

/**
 * Lee un fichero entero a memoria. Devuelve false si no se pudo.
 */
static bool loadFileToMemory(const std::string& path, std::vector<char>& out){
	out.clear();
	SDL_RWops* rw = SDL_RWFromFile(path.c_str(), "rb");
	if (!rw)
		return false;
	const int size = SDL_RWseek(rw, 0, SEEK_END);
	bool ok = false;
	if (size > 0 && SDL_RWseek(rw, 0, SEEK_SET) == 0){
		out.resize(size);
		ok = SDL_RWread(rw, &out[0], 1, size) == size;
	}
	SDL_RWclose(rw);
	if (!ok)
		std::vector<char>().swap(out);
	return ok;
}

/**
 * Abre la fuente de reserva. Si se ha cargado en memoria (menu en chino,
 * japones o coreano) todas las fuentes comparten ese buffer, como Arimo; si
 * no, FreeType lee del disco solo los glifos que necesita.
 */
TTF_Font* Fonts::openFallbackFont(int ptsize){
	if (s_fallbackPath.empty())
		return NULL;
	SDL_RWops* rw = !s_fallbackData.empty()
		? SDL_RWFromMem(&s_fallbackData[0], (int)s_fallbackData.size())
		: SDL_RWFromFile(s_fallbackPath.c_str(), "rb");
	if (!rw)
		return NULL;
	// 2do arg = 1 -> TTF cierra el RWops al cerrar la fuente
	TTF_Font* font = TTF_OpenFontRW(rw, 1, ptsize);
	if (!font) {
		LOG_ERROR("Error al cargar la fuente de reserva %s: %s\n", s_fallbackPath.c_str(), TTF_GetError());
	}
	return font;
}

/**
    *
    */
void Fonts::initFonts(int fontSize, const std::string& fallbackPath, bool fallbackInMemory){
	s_fontSize = fontSize;   // necesario para reabrir copias independientes
	s_fallbackPath = fallbackPath;
	vFonts[FONTBIG] = NULL;
	vFonts[FONTSMALL] = NULL;
	vFallbackFonts[FONTBIG] = NULL;
	vFallbackFonts[FONTSMALL] = NULL;
	fallbackOf.clear();

	fileio.loadFromMem(Arimo_Regular_ttf, Arimo_Regular_ttf_size);
	SDL_RWops *RWOps = SDL_RWFromMem(fileio.getFile(), (int)fileio.getFileSize());
	if (RWOps != NULL){
		vFonts[FONTBIG] = TTF_OpenFontRW(RWOps, 1, fontSize);
		if (vFonts[FONTBIG] == NULL) {
			LOG_ERROR("Error al cargar fuente grande: %s\n", TTF_GetError());
		}
	}

	SDL_RWops *RWOps2 = SDL_RWFromMem(fileio.getFile(), (int)fileio.getFileSize());
	if (RWOps2 != NULL){
		vFonts[FONTSMALL] = TTF_OpenFontRW(RWOps2, 1, fontSize - 9);
		if (vFonts[FONTSMALL] == NULL) {
			LOG_ERROR("Error al cargar fuente pequenya: %s\n", TTF_GetError());
		}
	}

	// Fuentes de reserva para los caracteres que Arimo no tiene (chino,
	// japones, coreano). Si el fichero no esta, se sigue sin ellas.
	if (!s_fallbackPath.empty()){
		// Con el menu en chino, japones o coreano se usa constantemente: se
		// carga entera en memoria para no leer del disco en cada glifo.
		std::vector<char>().swap(s_fallbackData);
		if (fallbackInMemory){
			if (loadFileToMemory(s_fallbackPath, s_fallbackData)){
				LOG_DEBUG("Fuente de reserva en memoria: %s (%u bytes)\n", s_fallbackPath.c_str(), (unsigned int)s_fallbackData.size());
			} else {
				LOG_ERROR("No se pudo cargar en memoria %s, se leera del disco\n", s_fallbackPath.c_str());
			}
		}
		vFallbackFonts[FONTBIG] = openFallbackFont(fontSize);
		vFallbackFonts[FONTSMALL] = openFallbackFont(fontSize - 9);
		if (vFonts[FONTBIG] && vFallbackFonts[FONTBIG])
			fallbackOf[vFonts[FONTBIG]] = vFallbackFonts[FONTBIG];
		if (vFonts[FONTSMALL] && vFallbackFonts[FONTSMALL])
			fallbackOf[vFonts[FONTSMALL]] = vFallbackFonts[FONTSMALL];
		if (vFallbackFonts[FONTBIG] == NULL) {
			LOG_ERROR("Sin fuente de reserva CJK: %s\n", s_fallbackPath.c_str());
		}
	}
}

TTF_Font *Fonts::getFont(int fontId){
    if (fontId <= FONTSMALL && fontId >= 0)
        return vFonts[fontId];
    else
        return NULL;
}

/**
 * Fuente de reserva asociada a una fuente principal (o NULL si no tiene).
 * Llamar con el mutex cogido.
 */
static TTF_Font* findFallback(const std::map<TTF_Font*, TTF_Font*>& fallbackOf, TTF_Font* font){
	std::map<TTF_Font*, TTF_Font*>::const_iterator it = fallbackOf.find(font);
	return it != fallbackOf.end() ? it->second : NULL;
}

/**
 * Parte el texto en trozos segun la fuente que tiene cada caracter: la
 * principal si tiene el glifo y, si no, la de reserva cuando esta lo tiene.
 * Si la de reserva tampoco lo tiene se deja en la principal (dibuja la caja
 * de caracter desconocido como hasta ahora).
 * Devuelve false si todo el texto va con la fuente principal.
 */
static bool splitRuns(TTF_Font* font, TTF_Font* fallback, const std::string& text, std::vector<TextRun>& runs){
	runs.clear();
	if (fallback == NULL)
		return false;

	// Camino rapido: ASCII puro, Arimo lo tiene todo
	bool ascii = true;
	for (std::size_t i = 0; i < text.size(); i++){
		if ((unsigned char)text[i] >= 0x80) { ascii = false; break; }
	}
	if (ascii)
		return false;

	bool usesFallback = false;
	std::size_t pos = 0;
	while (pos < text.size()){
		std::size_t len;
		const unsigned int cp = utf8::decode(text, pos, len);
		TTF_Font* chosen = font;
		// SDL_ttf 2.0.x solo maneja caracteres de 16 bits
		if (cp >= 0x80 && cp <= 0xFFFF
			&& !TTF_GlyphIsProvided(font, (Uint16)cp)
			&& TTF_GlyphIsProvided(fallback, (Uint16)cp)) {
			chosen = fallback;
			usesFallback = true;
		}

		if (runs.empty() || runs.back().font != chosen){
			TextRun run;
			run.font = chosen;
			runs.push_back(run);
		}
		runs.back().text.append(text, pos, len);
		pos += len;
	}
	return usesFallback;
}

/**
 * Tamanyo de un texto que puede mezclar fuentes. La altura es siempre la de
 * la fuente principal para que las lineas midan lo mismo que sin caracteres
 * CJK. Llamar con el mutex cogido.
 */
static void sizeText(const std::map<TTF_Font*, TTF_Font*>& fallbackOf, TTF_Font* font, const std::string& text, int* w, int* h){
	std::vector<TextRun> runs;
	if (!splitRuns(font, findFallback(fallbackOf, font), text, runs)){
		TTF_SizeUTF8(font, text.c_str(), w, h);
		return;
	}

	int totalW = 0;
	for (std::size_t i = 0; i < runs.size(); i++){
		int runW = 0;
		TTF_SizeUTF8(runs[i].font, runs[i].text.c_str(), &runW, NULL);
		totalW += runW;
	}
	if (w) *w = totalW;
	if (h) *h = TTF_FontHeight(font);
}

static SDL_Surface* renderSingle(TTF_Font* font, const char* text, RenderMode mode, const SDL_Color& fg, const SDL_Color& bg){
	switch (mode){
		case RENDER_SOLID:  return TTF_RenderUTF8_Solid(font, text, fg);
		case RENDER_SHADED: return TTF_RenderUTF8_Shaded(font, text, fg, bg);
		default:            return TTF_RenderUTF8_Blended(font, text, fg);
	}
}

/**
 * Dibuja un texto que puede mezclar la fuente principal y la de reserva.
 * Cada trozo se dibuja con su fuente y se copia en una sola superficie del
 * mismo formato que devuelve SDL_ttf, con la altura y la linea base de la
 * fuente principal (los ideogramas caben dentro de ese alto aunque la caja
 * de la fuente CJK sea mas grande). Llamar con el mutex cogido.
 */
static SDL_Surface* renderText(const std::map<TTF_Font*, TTF_Font*>& fallbackOf, TTF_Font* font, const char* text, RenderMode mode, const SDL_Color& fg, const SDL_Color& bg){
	std::vector<TextRun> runs;
	if (!splitRuns(font, findFallback(fallbackOf, font), text, runs))
		return renderSingle(font, text, mode, fg, bg);

	const int baseline = TTF_FontAscent(font);
	const int height = TTF_FontHeight(font);

	std::vector<SDL_Surface*> surfaces(runs.size(), (SDL_Surface*)NULL);
	std::vector<int> widths(runs.size(), 0);
	SDL_Surface* model = NULL;
	int totalW = 0;
	for (std::size_t i = 0; i < runs.size(); i++){
		surfaces[i] = renderSingle(runs[i].font, runs[i].text.c_str(), mode, fg, bg);
		if (surfaces[i]){
			widths[i] = surfaces[i]->w;
			if (!model) model = surfaces[i];
		} else {
			// Trozo sin anchura (p. ej. solo espacios); se respeta su avance
			TTF_SizeUTF8(runs[i].font, runs[i].text.c_str(), &widths[i], NULL);
		}
		totalW += widths[i];
	}

	SDL_Surface* result = NULL;
	if (model && totalW > 0 && height > 0){
		SDL_PixelFormat* fmt = model->format;
		result = SDL_CreateRGBSurface(SDL_SWSURFACE, totalW, height, fmt->BitsPerPixel,
			fmt->Rmask, fmt->Gmask, fmt->Bmask, fmt->Amask);
	}

	if (result){
		// La transparencia de los trozos se guarda antes de quitarsela para copiarlos
		const bool useColorKey = (model->flags & SDL_SRCCOLORKEY) != 0;
		const Uint32 colorKey = model->format->colorkey;

		// Mismos colores que los trozos (Solid y Shaded son de 8 bits)
		if (model->format->palette && result->format->palette){
			SDL_SetColors(result, model->format->palette->colors, 0, model->format->palette->ncolors);
		}
		// Indice 0: transparente en Solid, fondo en Shaded, alfa 0 en Blended
		SDL_FillRect(result, NULL, 0);

		int x = 0;
		for (std::size_t i = 0; i < runs.size(); i++){
			SDL_Surface* srf = surfaces[i];
			if (srf){
				// Copia literal de los pixeles (incluido el alfa), sin mezclar
				SDL_SetAlpha(srf, 0, SDL_ALPHA_OPAQUE);
				SDL_SetColorKey(srf, 0, 0);
				SDL_Rect dest;
				dest.x = (Sint16)x;
				dest.y = (Sint16)(baseline - TTF_FontAscent(runs[i].font));
				dest.w = 0;
				dest.h = 0;
				SDL_BlitSurface(srf, NULL, result, &dest);
			}
			x += widths[i];
		}

		if (useColorKey){
			SDL_SetColorKey(result, SDL_SRCCOLORKEY, colorKey);
		}
	}

	for (std::size_t i = 0; i < surfaces.size(); i++){
		if (surfaces[i]) SDL_FreeSurface(surfaces[i]);
	}
	return result;
}

/**
 * Indice (en bytes) hasta donde cabe el texto en maxW pixeles. Siempre cae
 * en el limite de un caracter para no partir los caracteres UTF-8.
 */
std::size_t Fonts::idxToCutTTF(std::string text, int maxW, int fontId){
    if (text.empty())
        return 0;

	int textW = getSize(fontId, text);

    if (textW < maxW){
        return text.length();
    }

    std::size_t last = 0;
    std::size_t i = utf8::next(text, 0);
    while (i < text.length()){
		textW = getSize(fontId, text.substr(0, i));
        if (textW >= maxW){
            return last;
        }
        last = i;
        i = utf8::next(text, i);
    }
    return last;
}

int Fonts::getSize(int fontId, std::string text){
	ScopedFontLock lock(mutex);
	int textW = 0;
	TTF_Font *font = getFont(fontId);
	if (font == NULL || text.empty())
		return 0;
	sizeText(fallbackOf, font, text, &textW, NULL);
	return textW;
}

int Fonts::getSize(TTF_Font* font, const std::string& text){
	ScopedFontLock lock(mutex);
	if (font == NULL || text.empty())
		return 0;
	int textW = 0;
	sizeText(fallbackOf, font, text, &textW, NULL);
	return textW;
}

void Fonts::getSize(TTF_Font* font, const std::string& text, int &w, int &h){
	ScopedFontLock lock(mutex);
	w = 0;
	h = 0;
	if (font == NULL || text.empty())
		return;
	sizeText(fallbackOf, font, text, &w, &h);
}

int Fonts::getLineSkip(int fontId){
	ScopedFontLock lock(mutex);
	TTF_Font *font = getFont(fontId);
	if (font == NULL)
		return 0;

	return TTF_FontLineSkip(font);
}

TTF_Font* Fonts::createIndependentFont(int fontId){
	ScopedFontLock lock(mutex);
	if (fontId < 0 || fontId > FONTSMALL)
		return NULL;
	if (s_fontSize <= 0 || fileio.getFile() == NULL)
		return NULL;

	const int sz = (fontId == FONTBIG) ? s_fontSize : (s_fontSize - 9);
	SDL_RWops* rw = SDL_RWFromMem(fileio.getFile(), (int)fileio.getFileSize());
	if (!rw) return NULL;
	// 2do arg = 1 -> TTF cierra el RWops al cerrar la fuente
	TTF_Font* font = TTF_OpenFontRW(rw, 1, sz);
	if (!font) {
		LOG_ERROR("Fonts::createIndependentFont(%d): %s", fontId, TTF_GetError());
		return NULL;
	}

	// La copia lleva tambien su propia fuente de reserva
	if (vFallbackFonts[fontId] != NULL){
		TTF_Font* fallback = openFallbackFont(sz);
		if (fallback) fallbackOf[font] = fallback;
	}
	return font;
}

void Fonts::closeIndependentFont(TTF_Font* font){
	if (font == NULL)
		return;
	ScopedFontLock lock(mutex);
	std::map<TTF_Font*, TTF_Font*>::iterator it = fallbackOf.find(font);
	if (it != fallbackOf.end()){
		TTF_CloseFont(it->second);
		fallbackOf.erase(it);
	}
	TTF_CloseFont(font);
}

std::string Fonts::recortarAlTamanyo(std::string text, int maxWidth){
	std::string newText = text;
	TTF_Font* font = Fonts::getFont(Fonts::FONTBIG);
	if (!font)
		return newText;

	int textPixelSize = getSize(font, text);

	if (textPixelSize > maxWidth) {
		int totalChars = text.length();

		// 1. Precalculamos el ancho promedio por caracter
		float avgCharWidth = (float)textPixelSize / (float)totalChars;

		// 2. Estimamos cuantos caracteres sobran para que quepa (incluyendo el "...")
		int dotsWidth = getSize(font, "...");

		int targetWidth = maxWidth - dotsWidth;
		int charsThatFit = (int)(targetWidth / avgCharWidth);
		if (charsThatFit < 0) charsThatFit = 0;

		// 3. Aplicamos el recorte inicial basado en la estimacion
		// Queremos la mitad de los que caben al principio y la otra mitad al final.
		// Los cortes se ajustan al limite de un caracter UTF-8 (los CJK ocupan 3 bytes).
		std::size_t leftPart = utf8::floorBoundary(text, charsThatFit / 2);
		std::size_t rightPart = utf8::ceilBoundary(text, totalChars - (charsThatFit / 2));
		if (rightPart < leftPart) rightPart = leftPart;

		newText = text.substr(0, leftPart) + "..." + text.substr(rightPart);
		textPixelSize = getSize(font, newText);

		// 4. Ajuste fino (por si la estimacion fue optimista debido a caracteres anchos como 'W')
		// Este bucle se ejecutara como mucho 1 o 2 veces, ahorrando mucha CPU
		while (textPixelSize > maxWidth && (leftPart > 0 || rightPart < text.length())) {
			if (leftPart > 0) leftPart = utf8::prev(text, leftPart);
			if (rightPart < text.length()) rightPart = utf8::next(text, rightPart);

			newText = text.substr(0, leftPart) + "..." + text.substr(rightPart);
			textPixelSize = getSize(font, newText);
		}
	}

	return newText;
}

void Fonts::getBadgeSize(int &w, int &h, int &badgePad, int &line_height){
	const int face_h_small = getLineSkip(Fonts::FONTSMALL);
	badgePad = 2;
	line_height = face_h_small + 4;
	w = line_height * 3 - badgePad * 2;
	h = line_height * 3 - badgePad * 2;
}

SDL_Rect Fonts::drawText(SDL_Surface* surface, TTF_Font* font, const char *s, int x, int y, SDL_Color color, int bg){
	SDL_Rect dest = { x, y, 0, 0 };
	if (font && s != NULL && s[0] != '\0') {
		ScopedFontLock lock(mutex);
		SDL_Surface* textSurf = NULL;
		if (!CfgLoader::configMain[cfg::smoothFonts].valueBool)
			textSurf = Fonts::renderUtf8Solid(font, s, color);
		else
			textSurf = Fonts::renderUtf8Blended(font, s, color);

		if (textSurf) {
			dest.w = textSurf->w;
			dest.h = textSurf->h;
			SDL_BlitSurface(textSurf, NULL, surface, &dest);
			SDL_FreeSurface(textSurf);
		} else {
			LOG_ERROR("Error al crear la surface para: %s\n", s);
		}
	}
	//else {
	//	LOG_ERROR("Error al comprobar los parametros de entrada: %s\n", s);
	//}
	return dest;
}

SDL_Rect Fonts::drawTextTransparent(SDL_Surface* surface, TTF_Font* font, const char *s, int x, int y, SDL_Color color, int bg, JFY_TYPE& justifyHelper){
	SDL_Rect dest = { x, y, 0, 0 };
	if (font && s != NULL && s[0] != '\0') {
		ScopedFontLock lock(mutex);
		SDL_Surface* textSurf = NULL;
		if (!CfgLoader::configMain[cfg::smoothFonts].valueBool)
			textSurf = Fonts::renderUtf8Solid(font, s, color);
		else
			textSurf = Fonts::renderUtf8Blended(font, s, color);
		
		if (textSurf) {
			dest.w = textSurf->w;
			dest.h = textSurf->h;
			dest.x += justifyHelper.getJustification(textSurf->w);
			SDL_BlitSurface(textSurf, NULL, surface, &dest);
			SDL_FreeSurface(textSurf);
		} else {
			LOG_ERROR("Error al crear la surface para: %s\n", s);
		}
	}
	//else {
	//	LOG_ERROR("Error al comprobar los parametros de entrada: %s\n", s);
	//}
	return dest;
}

void Fonts::drawTextCent(SDL_Surface* surface, TTF_Font* font, const char* dato, int x, int y, bool centx, bool centy, SDL_Color color, int bg){
	if (!font || !surface) return;

	int textW = 0, textH = 0;
	if (centx || centy) {
		Fonts::getSize(font, std::string(dato), textW, textH);
	}

	if (centx) x = (surface->w - textW) / 2 + x;
	if (centy) y = (surface->h - textH) / 2 + y;

	drawText(surface, font, dato, x, y, color, bg);
}

void Fonts::drawTextCentTransparent(SDL_Surface* surface, TTF_Font* font, const char* dato, int x, int y, bool centx, bool centy, SDL_Color color, int bg) {
	if (!font || !surface) return;

	int textW = 0, textH = 0;
	if (centx || centy) {
		Fonts::getSize(font, std::string(dato), textW, textH);
	}

	if (centx) x = (surface->w - textW) / 2 + x;
	if (centy) y = (surface->h - textH) / 2 + y;

	drawTextTransparent(surface, font, dato, x, y, color, bg);
}

// Funcion auxiliar estatica o privada para verificar si tiene texto real legible
bool tieneTextoLegible(const char* str) {
    if (!str) return false;
    while (*str) {
        // Si encontramos cualquier caracter que no sea espacio, tabulador o salto de linea
        if (*str != ' ' && *str != '\t' && *str != '\n' && *str != '\r') {
            return true; // Contiene texto real renderizable
        }
        str++;
    }
    return false; // Solo contiene espacios en blanco o esta totalmente vacio
}

// Saltamos el BOM UTF-8 (EF BB BF) inicial, SDL_ttf no lo entiende y devolveraa
// "Text has zero width" (el BOM es un caracter de anchura cero sin glifo).
static const char* skipUtf8Bom(const char* str){
    if (str &&
        (unsigned char)str[0] == 0xEF &&
        (unsigned char)str[1] == 0xBB &&
        (unsigned char)str[2] == 0xBF) {
        return str + 3;
    }
    return str;
}

SDL_Surface *Fonts::renderUtf8Blended(TTF_Font* font, const char* dato, const SDL_Color& color){
	const char* text = skipUtf8Bom(dato);
	if (font && tieneTextoLegible(text)) {
		ScopedFontLock lock(mutex);
		return renderText(fallbackOf, font, text, RENDER_BLENDED, color, color);
	}
	return NULL;
}

SDL_Surface *Fonts::renderUtf8Solid(TTF_Font* font, const char* dato, const SDL_Color& color){
	const char* text = skipUtf8Bom(dato);
	if (font && tieneTextoLegible(text)) {
		ScopedFontLock lock(mutex);
		return renderText(fallbackOf, font, text, RENDER_SOLID, color, color);
	}
	return NULL;
}

SDL_Surface *Fonts::renderUtf8Shaded(TTF_Font* font, const char* dato, const SDL_Color& bg, const SDL_Color& fg){
	const char* text = skipUtf8Bom(dato);
	if (font && tieneTextoLegible(text)) {
		ScopedFontLock lock(mutex);
		// Se mantiene el orden de argumentos de la llamada original
		return renderText(fallbackOf, font, text, RENDER_SHADED, bg, fg);
	}
	return NULL;
}
