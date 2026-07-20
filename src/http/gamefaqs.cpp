#include <fstream>
#include <sstream>

#include <SDL_image.h>

#include "gamefaqs.h"
#include <http/pugixml.hpp>
#include <utils/logger.h>
#include <const/constant.h>
#include <io/dirutil.h>

const std::string URL_GAMEFAQS = "https://gamefaqs.gamespot.com/search?game=";
const std::string GAMEFAQS_HOME = "https://gamefaqs.gamespot.com/";
const std::string BASE = "https://gamefaqs.gamespot.com";

GameFaqs::GameFaqs(void){
}

GameFaqs::~GameFaqs(void){
}

// ============================================================================
// Generadores de cabeceras
// ============================================================================

std::map<std::string, std::string> GameFaqs::buildFirefoxHeaders(){
	std::map<std::string, std::string> h;
	h["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:152.0) Gecko/20100101 Firefox/152.0";
	h["Accept"] = "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8";
	h["Accept-Language"] = "es-ES,es;q=0.9,en-US;q=0.8,en;q=0.7";
	h["Accept-Encoding"] = "gzip, deflate, br, zstd";
	h["Upgrade-Insecure-Requests"] = "1";
	h["Sec-Fetch-Dest"] = "document";
	h["Sec-Fetch-Mode"] = "navigate";
	h["Sec-Fetch-Site"] = "none";
	h["Sec-Fetch-User"] = "?1";
	return h;
}

// ============================================================================
// Funcion de busqueda de juego
// ============================================================================

#define SEARCH_LIST "D:\\Temp\\searchList.html"
#define SEARCH_GUIDES "D:\\Temp\\searchGuides.html"
#define CONTENT_GUIDE "D:\\Temp\\guide.html"
#define TXT_GUIDE "D:\\Temp\\guide.txt"

#ifndef USE_GUMBO

// Tabla de entidades HTML nombradas mas comunes.
// Valores en UTF-8 (multibyte donde aplica).
struct HtmlEntity { const char* name; const char* value; };

static const HtmlEntity HTML_ENTITIES[] = {
	{"amp",    "&"},     {"lt",     "<"},     {"gt",      ">"},
	{"quot",   "\""},    {"apos",   "'"},
	{"nbsp",   "\xC2\xA0"},
	{"mdash",  "\xE2\x80\x94"}, {"ndash",  "\xE2\x80\x93"},
	{"hellip", "\xE2\x80\xA6"},
	{"copy",   "\xC2\xA9"},     {"reg",    "\xC2\xAE"},
	{"trade",  "\xE2\x81\xA2"},
	{"laquo",  "\xC2\xAB"},     {"raquo",  "\xC2\xBB"},
	{"lsquo",  "\xE2\x80\x98"}, {"rsquo",  "\xE2\x80\x99"},
	{"ldquo",  "\xE2\x80\x9C"}, {"rdquo",  "\xE2\x80\x9D"},
	{"times",  "\xC3\x97"},     {"divide", "\xC3\xB7"},
	{"cent",   "\xC2\xA2"},     {"pound",  "\xC2\xA3"},
	{"yen",    "\xC2\xA5"},     {"euro",   "\xE2\x82\xAC"},
	{"bull",   "\xE2\x80\xA2"}, {"middot", "\xC2\xB7"},
	{"iexcl",  "\xC2\xA1"},     {"iquest", "\xC2\xBF"},
	{NULL, NULL}
};

// Escribe un codepoint Unicode en UTF-8 en 'out'.
static void utf8Append(unsigned long cp, std::string &out){
	if (cp < 0x80) {
		out += (char)cp;
	} else if (cp < 0x800) {
		out += (char)(0xC0 | (cp >> 6));
		out += (char)(0x80 | (cp & 0x3F));
	} else if (cp < 0x10000) {
		out += (char)(0xE0 | (cp >> 12));
		out += (char)(0x80 | ((cp >> 6) & 0x3F));
		out += (char)(0x80 | (cp & 0x3F));
	} else {
		out += (char)(0xF0 | (cp >> 18));
		out += (char)(0x80 | ((cp >> 12) & 0x3F));
		out += (char)(0x80 | ((cp >> 6) & 0x3F));
		out += (char)(0x80 | (cp & 0x3F));
	}
}

// Decodifica la entidad HTML que comienza en s[0] == '&'.
// Escribe el resultado en 'out' y devuelve cuantos caracteres se consumieron
// (incluyendo '&' y ';), o 0 si no es una entidad valida.
static std::size_t decodeHtmlEntity(const char *s, std::size_t len, std::string &out){
	if (len < 2 || s[0] != '&') return 0;

	// Buscar ';' — longitud maxima razonable para una entidad: 32
	std::size_t semi = 0;
	std::size_t maxLen = (len < 32) ? len : 32;
	while (semi < maxLen && s[semi] != ';') ++semi;
	if (semi >= maxLen) return 0;

	if (s[1] == '#') {
		// Entidad numerica: &#123; o &#x7B;
		if (semi < 3) return 0;
		bool hex = (s[2] == 'x' || s[2] == 'X');
		std::size_t start = hex ? 3 : 2;
		if (start >= semi) return 0;

		unsigned long cp = 0;
		for (std::size_t i = start; i < semi; ++i) {
			char c = s[i];
			int d = -1;
			if (c >= '0' && c <= '9')      d = c - '0';
			else if (hex && c >= 'a' && c <= 'f') d = c - 'a' + 10;
			else if (hex && c >= 'A' && c <= 'F') d = c - 'A' + 10;
			else return 0;
			cp = cp * (hex ? 16UL : 10UL) + (unsigned long)d;
		}
		utf8Append(cp, out);
		return semi + 1;
	}

	// Entidad nombrada: &amp; &lt; etc.
	std::size_t nameLen = semi - 1;
	for (const HtmlEntity *e = HTML_ENTITIES; e->name; ++e) {
		std::size_t nlen = 0;
		const char *p = e->name;
		while (*p++) ++nlen;
		if (nlen == nameLen) {
			bool match = true;
			for (std::size_t i = 0; i < nameLen; ++i) {
				if (s[1 + i] != e->name[i]) { match = false; break; }
			}
			if (match) {
				out += e->value;
				return semi + 1;
			}
		}
	}

	return 0; // entidad desconocida, no consumir
}

// Decodifica todas las entidades HTML de una cadena.
static std::string htmlDecode(const std::string &in){
	std::string out;
	out.reserve(in.size());
	for (std::size_t i = 0; i < in.size(); ) {
		if (in[i] == '&') {
			std::size_t consumed = decodeHtmlEntity(in.c_str() + i, in.size() - i, out);
			if (consumed > 0) { i += consumed; continue; }
		}
		out += in[i++];
	}
	return out;
}
#endif /* !USE_GUMBO (htmlDecode) */

int GameFaqs::searchGame(const std::string &gameName){
	//std::ifstream archivoIn(SEARCH_LIST);
	//if (archivoIn.is_open()) {
	//	/**TODO: Borrar esta parte cuando se haya parseado todo correctamente*/
	//	std::string fileContent;
	//	// Leer el bufer completo en el stringstream
	//	std::stringstream buffer;
	//	buffer << archivoIn.rdbuf();
	//	archivoIn.close();
	//	this->games = processGameSearch(buffer.str());
	//} else {
		downloader.setHeaders(buildFirefoxHeaders());
		// Sin cookie hardcodeada: no se llama setCookie(), asi que fetchUrl usa el
		// cookie jar de curl (CURLOPT_COOKIEFILE/COOKIEJAR = cookies.txt). El servidor
		// emite la cookie de sesion en el primer contacto y curl la reutiliza.

		float downloadProgress = 0;

		// Warm-up: visitar la home para que el servidor deje la cookie de sesion en
		// el jar. Sin esto la busqueda devuelve la pagina pero SIN resultados.
		std::string warmup;
		downloader.fetchUrl(GAMEFAQS_HOME, warmup, &downloadProgress);

		// Busqueda real, reutilizando la sesion del jar.
		const std::string fullUrl = URL_GAMEFAQS + downloader.escape(gameName);
		std::string response;
		if (downloader.fetchUrl(fullUrl, response, &downloadProgress)) {
			this->games = processGameSearch(response);
			LOG_DEBUG("searchGame('%s'): %d juegos encontrados", gameName.c_str(), (int)games.size());
			//std::ofstream archivo(SEARCH_LIST);
			//if (archivo.is_open()) {
			//	archivo << response;
			//	archivo.close();
			//}
		}
	//}
	return this->games.size();
}

int GameFaqs::findGuides(int posGame){

	if (posGame < 0 || (std::size_t) posGame >= games.size())
		return 0;

	//std::ifstream archivoIn(SEARCH_GUIDES);
	//if (archivoIn.is_open()) {
	//	/**TODO: Borrar esta parte cuando se haya parseado todo correctamente*/
	//	std::string fileContent;
	//	// Leer el bufer completo en el stringstream
	//	std::stringstream buffer;
	//	buffer << archivoIn.rdbuf();
	//	archivoIn.close();
	//	this->guides = processGuideFetch(buffer.str());
	//} else {
		float downloadProgress = 0;
		// Busqueda real, reutilizando la sesion del jar.
		std::string response;
		if (downloader.fetchUrl(games[posGame].faqUrl, response, &downloadProgress)) {
			this->guides = processGuideFetch(response);
			LOG_DEBUG("findGuides('%s'): %d Guias encontradas", games[posGame].name.c_str(), (int)guides.size());
			//std::ofstream archivo(SEARCH_GUIDES);
			//if (archivo.is_open()) {
			//	archivo << response;
			//	archivo.close();
			//}
		}
	//}
	return this->guides.size();
}


void GameFaqs::getGuideText(int posGuide, GuideContent &guideContent){
	guideContent.type = GUIDE_NONE;

	if (posGuide < 0 || (std::size_t) posGuide >= this->guides.size())
		return;

	//std::ifstream archivoIn(CONTENT_GUIDE);
	//if (archivoIn.is_open()) {
	//	/**TODO: Borrar esta parte cuando se haya parseado todo correctamente*/
	//	std::string fileContent;
	//	// Leer el bufer completo en el stringstream
	//	std::stringstream buffer;
	//	buffer << archivoIn.rdbuf();
	//	archivoIn.close();
	//	guideText = processGuideText(buffer.str());
	//} else {
		float downloadProgress = 0;
		// Busqueda real, reutilizando la sesion del jar.
		std::string response;
		if (downloader.fetchUrl(this->guides[posGuide].url, response, &downloadProgress)) {
			guideContent.text = processGuideText(response);
			
			if (guideContent.text.empty()){
				guideContent.url = processGuideImage(response);
				guideContent.type = GUIDE_IMG;
			} else {
				guideContent.type = GUIDE_TXT;
			}
			LOG_DEBUG("getGuideText('%s')", this->guides[posGuide].name.c_str());
			//std::ofstream archivo(CONTENT_GUIDE);
			//if (archivo.is_open()) {
			//	archivo << response;
			//	archivo.close();
			//}
		}
	//}

	return;
}

void GameFaqs::getImage(const std::string &url, Image &img, SDL_PixelFormat *format){
	std::string response;
	float downloadProgress;
	if (downloader.fetchUrl(url, response, &downloadProgress)) {
		SDL_Surface *converted = img.loadConvertedSurfaceFromMem((const unsigned char *)response.data(), (int)response.size(), format);
		img.adoptSurface(converted, url);
	}
}

std::string GameFaqs::processGuideImage(const std::string &data){
	static const std::string SR_TYPE = "class=\"gf_map";
	static const std::string SR_SRC = "src=\"";
	std::size_t imgIni = data.find(SR_TYPE);

	if (imgIni != string::npos){
		std::size_t srcIni = data.find(SR_SRC, imgIni);
		if (srcIni != string::npos){
			std::size_t srcEnd = data.find("\"", srcIni + SR_SRC.length());
			if (srcEnd != string::npos && srcEnd > srcIni){
				return getHrefContent(data, srcIni, srcEnd, BASE, "src");
			}
		}
	}

	return "";
}

std::string GameFaqs::processGuideText(const std::string &data){
	static const std::string SR_TYPE = "id=\"faqspan";

	std::string text;
	std::size_t pos = 0;
	bool first = true;

	for (std::size_t faqTxtOpen = data.find(SR_TYPE, pos);
		 faqTxtOpen != std::string::npos;
		 faqTxtOpen = data.find(SR_TYPE, pos))
	{
		std::size_t faqTxtInit = data.find('>', faqTxtOpen);
		if (faqTxtInit == std::string::npos) break;

		std::size_t faqTxtEnd = data.find("</pre>", faqTxtInit);
		if (faqTxtEnd == std::string::npos) break;

		// Extraer bloque, limpiar \r y decodificar entidades HTML
		std::string block = data.substr(faqTxtInit + 1, faqTxtEnd - faqTxtInit - 1);
		block.erase(std::remove(block.begin(), block.end(), '\r'), block.end());

		if (!first) text += '\n';
		first = false;
		text += htmlDecode(Constant::Trim(block));

		pos = faqTxtEnd + 6; // strlen("</pre>")
	}

	std::ofstream archivo(TXT_GUIDE);
	if (archivo.is_open()) {
		archivo << text;
		archivo.close();
	}

	return text;
}


namespace {

// Extrae el texto entre '>' (tras posStart) y </tag>. Devuelve npos en tagEndOut si falla.
std::string extractTagText(const std::string &data, std::size_t searchFrom, const std::string &closeTag, std::size_t &tagEndOut) {
    std::size_t gt = data.find('>', searchFrom);
    if (gt == std::string::npos) { tagEndOut = std::string::npos; return ""; }
    std::size_t close = data.find(closeTag, gt);
    if (close == std::string::npos) { tagEndOut = std::string::npos; return ""; }
    tagEndOut = close;
    return htmlDecode(Constant::Trim(data.substr(gt + 1, close - gt - 1)));
}

} // namespace

int GameFaqs::resolveCategoryId(const std::string &category) {
    auto it = std::find(categories.begin(), categories.end(), category);
    if (it != categories.end())
        return (int)(it - categories.begin());

    categories.push_back(category);
    return (int)categories.size() - 1;
}

// Intenta parsear una guia a partir de la posicion del primer <a> tras la categoria.
// Devuelve true si se ha anyadido una guia valida a 'results'.
// 'nextTagPos' se actualiza a la posicion desde la que continuar buscando.
bool GameFaqs::parseGuideEntry(const std::string &data, std::size_t &searchPos, int categId,
                                std::vector<GuidesResult> &results) {
    static const std::string SR_TYPE = "class=\"flair\"";
    static const std::string SR_DATE = "class=\"guide_date\"";
    static const std::string TAG_A_START = "<a";
    static const std::string TAG_A_CLOSE = "</a>";

    GuidesResult guide;
    guide.categ_id = categId;

    // Nombre de la guia (primer <a>)
    std::size_t nameOpen = searchPos;
    std::size_t nameClose = data.find('>', nameOpen);
    if (nameClose == std::string::npos) return false;
    guide.url = getHrefContent(data, nameOpen, nameClose, BASE);

    std::size_t tagEnd;
    guide.name = extractTagText(data, nameOpen, TAG_A_CLOSE, tagEnd);
    if (tagEnd == std::string::npos) return false;

    // Nombre del autor (segundo <a>)
   std::size_t authorOpen = data.find(TAG_A_START, tagEnd);
    if (authorOpen == std::string::npos) return false;
    guide.author = extractTagText(data, authorOpen, TAG_A_CLOSE, tagEnd);
    if (tagEnd == std::string::npos) return false;

    std::size_t next_a = data.find(TAG_A_START, tagEnd);

    // Plataforma / tipo (descartar guias HTML)
    std::size_t typePos = data.find(SR_TYPE, tagEnd);
    if (typePos != std::string::npos && typePos < next_a) {
        std::size_t typeEnd;
        std::string platform = extractTagText(data, typePos, "</span>", typeEnd);
        guide.platform = platform;
        if (platform == "HTML") {
            searchPos = next_a;
            return false; // se descarta esta guia, pero se sigue avanzando
        }
    }

    // Fecha
    std::size_t datePos = data.find(SR_DATE, tagEnd);
    if (datePos != std::string::npos) {
        std::size_t dateEnd;
        guide.year = extractTagText(data, datePos, "</span>", dateEnd);
    }

	if (!guide.url.empty()) results.push_back(guide);
    searchPos = next_a;
    return true;
}

std::vector<GuidesResult> GameFaqs::processGuideFetch(const std::string &data) {
    std::vector<GuidesResult> results;

    static const std::string SR_CATEGORY = "class=\"title\"";
    static const std::string SR_END_CATEGORIES = "class=\"pod\"";

    std::size_t firstCategory = data.find(SR_CATEGORY);
    if (firstCategory == std::string::npos) return results;

    std::size_t endCategoriesPos = data.find(SR_END_CATEGORIES, firstCategory);
    if (endCategoriesPos == std::string::npos) return results;

    std::size_t pos = firstCategory;
    while ((pos = data.find(SR_CATEGORY, pos)) != std::string::npos) {
        pos += SR_CATEGORY.size();

        // Nombre de la categoria
        std::size_t categTagEnd;
        std::string category = extractTagText(data, pos, "</", categTagEnd);
        if (categTagEnd == std::string::npos) break;

        int categId = resolveCategoryId(category);

        std::size_t nextCateg = data.find(SR_CATEGORY, categTagEnd);
        std::size_t entryPos = data.find("<a", categTagEnd);

        // Procesa todas las guias de esta categoria
        while (entryPos != std::string::npos &&
               entryPos < nextCateg &&
               entryPos < endCategoriesPos) {
            parseGuideEntry(data, entryPos, categId, results);
            if (entryPos == std::string::npos) break;
        }

        pos = nextCateg;
    }

    LOG_DEBUG("processGuideFetch: %d resultados encontrados", (int)results.size());
    for (std::size_t i = 0; i < results.size(); ++i)
        LOG_DEBUG("  [%d] '%s' (%s), (%s) -> %s", (int)(i + 1),
            results[i].name.c_str(), results[i].author.c_str(),
            results[i].platform.c_str(), results[i].year.c_str());

    return results;
}

// El HTML de GameFAQs NO es XML valido (pugixml no sirve). Cada resultado tiene
// esta estructura estable:
//   <div class="sr_name">
//       <a class="log_search" ... href="\plat\id-slug">NOMBRE</a>
//   </div>
// Parseamos directamente por strings, que ademas es ligero y portable a Xbox 360.
// Con USE_GUMBO definido se usa en su lugar el parser HTML5 Gumbo (arbol DOM).

#ifdef USE_GUMBO
#include "gumbo.h"

// class="a b c" contiene el token 'cls'?
static bool gumboClassContains(GumboNode* node, const char* cls){
	if (node->type != GUMBO_NODE_ELEMENT) return false;
	GumboAttribute* a = gumbo_get_attribute(&node->v.element.attributes, "class");
	if (!a) return false;
	std::string v = a->value;
	std::string needle = cls;
	size_t p = v.find(needle);
	while (p != std::string::npos) {
		bool lok = (p == 0 || v[p-1] == ' ');
		size_t end = p + needle.size();
		bool rok = (end == v.size() || v[end] == ' ');
		if (lok && rok) return true;
		p = v.find(needle, p + 1);
	}
	return false;
}

// Concatena el texto (ya decodificado por Gumbo) de un subarbol.
static std::string gumboText(GumboNode* node){
	if (node->type == GUMBO_NODE_TEXT) return std::string(node->v.text.text);
	if (node->type != GUMBO_NODE_ELEMENT) return std::string();
	std::string out;
	GumboVector* ch = &node->v.element.children;
	for (unsigned i = 0; i < ch->length; ++i)
		out += gumboText((GumboNode*)ch->data[i]);
	return out;
}

// Primer descendiente (incluido node) con la etiqueta dada.
static GumboNode* gumboFirstTag(GumboNode* node, GumboTag tag){
	if (node->type != GUMBO_NODE_ELEMENT) return NULL;
	if (node->v.element.tag == tag) return node;
	GumboVector* ch = &node->v.element.children;
	for (unsigned i = 0; i < ch->length; ++i) {
		GumboNode* r = gumboFirstTag((GumboNode*)ch->data[i], tag);
		if (r) return r;
	}
	return NULL;
}

// Primer <div> descendiente cuyo class contiene 'cls'.
static GumboNode* gumboFirstDivClass(GumboNode* node, const char* cls){
	if (node->type != GUMBO_NODE_ELEMENT) return NULL;
	if (node->v.element.tag == GUMBO_TAG_DIV && gumboClassContains(node, cls))
		return node;
	GumboVector* ch = &node->v.element.children;
	for (unsigned i = 0; i < ch->length; ++i) {
		GumboNode* r = gumboFirstDivClass((GumboNode*)ch->data[i], cls);
		if (r) return r;
	}
	return NULL;
}

// Recorre el arbol; por cada bloque sr_title extrae nombre/url/info.
static void gumboCollectResults(GumboNode* node, std::vector<GameResult>& out){
	if (node->type != GUMBO_NODE_ELEMENT) return;

	if (node->v.element.tag == GUMBO_TAG_DIV && gumboClassContains(node, "sr_title")) {
		GumboNode* nameDiv = gumboFirstDivClass(node, "sr_name");
		GumboNode* a = nameDiv ? gumboFirstTag(nameDiv, GUMBO_TAG_A) : NULL;
		if (a) {
			GameResult g;
			GumboAttribute* href = gumbo_get_attribute(&a->v.element.attributes, "href");
			if (href) {
				std::string h = href->value;
				for (size_t i = 0; i < h.size(); ++i)
					if (h[i] == '\\') h[i] = '/';
				if (!h.empty())
					g.url = std::string("https://gamefaqs.gamespot.com") + h;
			}
			g.name = Constant::Trim(gumboText(a));
			GumboNode* infoDiv = gumboFirstDivClass(node, "sr_info");
			if (infoDiv)
				g.info = Constant::Trim(gumboText(infoDiv));
			out.push_back(g);
			return; /* no recursar dentro de este bloque ya procesado */
		}
	}

	GumboVector* ch = &node->v.element.children;
	for (unsigned i = 0; i < ch->length; ++i)
		gumboCollectResults((GumboNode*)ch->data[i], out);
}
#endif /* USE_GUMBO */

std::vector<GameResult> GameFaqs::processGameSearch(const std::string &data){
	std::vector<GameResult> results;

#ifdef USE_GUMBO
	// Parser HTML5 Gumbo: construye el DOM y recorre buscando los bloques sr_title.
	// Gumbo decodifica las entidades HTML automaticamente (no hace falta htmlDecode).
	GumboOutput* output = gumbo_parse(data.c_str());
	if (output) {
		gumboCollectResults(output->root, results);
		gumbo_destroy_output(&kGumboDefaultOptions, output);
	}
#else
	static const std::string SR_NAME = "class=\"sr_name\"";
	static const std::string SR_INFO = "class=\"sr_info\"";
	static const std::string SR_PLATFORM = "class=\"meta float_r\"";
	static const std::string SR_FAQ_URL = "data-col=\"2\"";
	static const std::string HREF_TAG = "href";

	size_t pos = 0;
	while ((pos = data.find(SR_NAME, pos)) != std::string::npos) {
		pos += SR_NAME.size();

		// Primer <a ...> tras el div.sr_name
		size_t aOpen = data.find("<a", pos);
		if (aOpen == std::string::npos) break;
		size_t aTagEnd = data.find('>', aOpen);
		if (aTagEnd == std::string::npos) break;

		GameResult g;

		// href del enlace (viene con backslashes: \snes\588436-...) -> URL completa

		g.url = getHrefContent(data, aOpen, aTagEnd, BASE);

		// Texto entre <a ...> y </a> = nombre del juego
		size_t textStart = aTagEnd + 1;
		size_t textEnd = data.find("</a>", textStart);
		if (textEnd == std::string::npos) break;
		g.name = htmlDecode(Constant::Trim(data.substr(textStart, textEnd - textStart)));

		// sr_info (genero, anyo) del mismo bloque: debe estar antes del siguiente sr_name
		size_t nextName = data.find(SR_NAME, textEnd);
		size_t infoPos  = data.find(SR_INFO, textEnd);
		size_t platformPos  = data.find(SR_PLATFORM, textEnd);
		size_t faqPos  = data.find(SR_FAQ_URL, textEnd);


		if (infoPos != std::string::npos &&
			(nextName == std::string::npos || infoPos < nextName)) {
			size_t is = data.find('>', infoPos);
			size_t ie = 0;
			if (is != std::string::npos) {
				ie = data.find("</div>", is);
				if (ie != std::string::npos)
					g.info = htmlDecode(Constant::Trim(data.substr(is + 1, ie - is - 1)));
			}

			is = data.find('>', platformPos);
			if (is != std::string::npos) {
				ie = data.find("</div>", is);
				if (ie != std::string::npos)
					g.platform = htmlDecode(Constant::Trim(data.substr(is + 1, ie - is - 1)));
			}

			is = data.find("href", faqPos);
			ie = data.find(">", faqPos);
			g.faqUrl = getHrefContent(data, is, ie, BASE);
		}

		results.push_back(g);
		pos = textEnd + 4; // strlen("</a>")
	}
#endif /* USE_GUMBO */

	LOG_DEBUG("processGameSearch: %d resultados encontrados", (int)results.size());
	for (size_t i = 0; i < results.size(); ++i)
		LOG_DEBUG("  [%d] '%s' (%s), (%s) -> %s", (int)(i + 1),
			results[i].name.c_str(), results[i].info.c_str(), results[i].platform.c_str(), results[i].url.c_str());

	return results;
}


std::string GameFaqs::getHrefContent(const std::string &data, std::size_t startPos, std::size_t aTagEnd, std::string base, std::string prop){
	std::string url;
	size_t propContentIni = string(prop + "=\"").length();
	// href del enlace (viene con backslashes: \snes\588436-...) -> URL completa
	size_t propPos = data.find(prop + "=\"", startPos);
	if (propPos != std::string::npos && propPos < aTagEnd) {
		size_t hrefStart = propPos + propContentIni; // strlen("href=\"")
		size_t hrefEnd = data.find('"', hrefStart);
		if (hrefEnd != std::string::npos) {
			std::string href = data.substr(hrefStart, hrefEnd - hrefStart);
			for (size_t i = 0; i < href.size(); ++i)
				if (href[i] == '\\') href[i] = '/';
			if (!href.empty())
				url = base + href;
		}
	}
	return url;
}