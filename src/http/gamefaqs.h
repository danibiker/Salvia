#pragma once

#include <string>
#include <map>
#include <vector>

#include <http/httputil.h>
#include <uiobjects/image.h>

// Un resultado de busqueda de GameFAQs
struct GameResult {
	std::string name;   // nombre del juego
	std::string url;    // URL completa a la ficha
	std::string faqUrl;    // URL completa a la ficha
	std::string info;   // genero y anyo
	std::string platform;   // plataforma 
};

struct GuidesResult{
	std::string name;   // nombre de la guia
	std::string url;    // URL completa a la guia
	std::string author; // Autor de la guia
	std::string year;   // Anyo de la guia
	std::string platform; //Plataforma de la guia
	int categ_id;		// reference to the categories class private vector
	std::size_t guidePos;

	GuidesResult() : categ_id(-1), guidePos((size_t)-1) {}
};

enum GUIDE_TYPE{GUIDE_TXT, GUIDE_IMG, GUIDE_NONE};

struct GuideContent{
	std::string text;
	std::string url;
	GUIDE_TYPE type;
};

class GameFaqs
{
public:
	GameFaqs(void);
	~GameFaqs(void);

	int searchGame(const std::string &gameName);
	int findGuides(int posGame);
	void getGuideText(int posGuide, GuideContent &guideContent);
	void getImage(const std::string &url, Image &img, SDL_PixelFormat *format);

	const std::vector<GameResult> *getGames(){
		return &games;
	}

	const std::vector<GuidesResult> *getGuides(){
		return &guides;
	}

	const std::vector<std::string> *getCategories(){
		return &categories;
	}

	// Bateria de tests TLS/headers para GameFAQS
	// Cada test intenta una configuracion distinta de cabeceras y opciones TLS
	void runAllTests();

private:
	std::vector<GameResult>   processGameSearch(const std::string &data);
	std::vector<GuidesResult> processGuideFetch(const std::string &data);
	std::string processGuideText(const std::string &data);
	std::string processGuideImage(const std::string &data);

	CurlClient downloader;
	std::string getHrefContent(const std::string &data, std::size_t startPos, std::size_t aTagEnd, std::string base, std::string prop = "href");

	std::vector<GameResult>    games;
	std::vector<GuidesResult>  guides;
	std::vector<std::string>   categories;

	// Helpers
	std::map<std::string, std::string> buildFirefoxHeaders();
	std::map<std::string, std::string> buildMinimalHeaders();
	bool runSingleTest(const std::string &testName, const std::string &desc,
					   const std::map<std::string, std::string> &headers,
					   const std::string &cookie,
					   long sslVersion, const std::string &cipherList,
					   bool sessionIdCache, long httpVersion);

	int resolveCategoryId(const std::string &category);
	bool parseGuideEntry(const std::string &data, size_t &searchPos, int categId,
                                std::vector<GuidesResult> &results);
};

