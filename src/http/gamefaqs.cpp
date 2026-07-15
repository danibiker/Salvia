#include "gamefaqs.h"
#include <http/httputil.h>
#include <http/pugixml.hpp>
#include <utils/logger.h>
#include <const/constant.h>

const std::string URL_GAMEFAQS = "https://gamefaqs.gamespot.com/search?game=";

GameFaqs::GameFaqs(void)
{
}


GameFaqs::~GameFaqs(void)
{
}

//docker run --rm lwthiker/curl-impersonate curl_ff109 https://gamefaqs.gamespot.com/snes/588436-the-legend-of-zelda-a-link-to-the-past/faqs/9491
void GameFaqs::searchGame(const std::string &gameName){
	CurlClient downloader;
	const std::string fullUrl = URL_GAMEFAQS + downloader.escape(gameName);
	//const std::string fullUrl = "https://gamefaqs.gamespot.com/snes/588436-the-legend-of-zelda-a-link-to-the-past/faqs/9491";
	float downloadProgress = 0;
	std::string response;

	downloader.setCookie("OptanonConsent=groups=C0001%3A1%2CC0002%3A1%2CC0003%3A1%2CC0004%3A0%2CC0005%3A0; wikia_beacon_id=laKznnp0m9; _b2=iwAO-oeQLc.1776543844636; wikia_session_id=Y2izGng2HJ; Geo=%7B%22country%22%3A%22ES%22%2C%22region%22%3A%22VC%22%2C%22continent%22%3A%22EU%22%7D; gf_dvi=ZjY5ZTNlODYzMDAwMTgxMDI2N2JmMmM3MmNhYzE5MGQ1OTgyYjg4NWM2ZjIyZTJlNGI3NjExMzRjODExNmE1NzVmNGY%3D; gf_geo_id=RVMvVkMvQQ%3D%3D; fv20260716=1; gf_jbi=588436-faqs-9491%2F409940-faqs-81159%2F10-boards-77554392%2F7-boards-72062484%2F%21002cf400; gf_faq_bookmark_auto=9491%7C0%7C0%7C1%7C; cf_clearance=CzMJIqDCMVWiN_5IWVd7XtgBY6UtA2IFoMkgIVTqFUY-1784125677-1.2.1.1-4P4v0LEdoGPcn7YyotjL1XfxQns8Q81pJVvAoq9Xpn_KQBXMS.FTPppkgptcd4L_6wk42ktfC6NrnQdnEi4FenzV5AoKaMkey.bZZtT4aQwcRNRyTlGcFmmgy3V4fbBq5ax8p6AJhboPb_3qXQCqZvYLziSMHxELhoRp3CAl6lqOzVBfDmcjJ4UQ28DjyeG7qSxlF5sQaLuj_97r412Dp8SYRXhb29SKOqCBk5aqlS_FbZeVteQq_V3HRuQh1Umqlu7tthweZXAqYrBrftBUqUOl9dj4sPhYnS_ctwGLsjXJ.gi03Qj88tAZhi3mKetz88un1HdJxjAbZ_I08stXGQ; __cf_bm=n9Af2ShvTdhdlHvadxWvQxvSxu0Xwis.9mx4GfqbKco-1784125677.31794-1.0.1.1-GASfmW2.Odvs4hQ.85sOOGOcPLz5vJ2QmXZoYbwddlSOBfX95mmNJqTFhJ3nhFwdH0x4xvnN3Csn10WQ19SGc3RYvhjA45GJedUEK2AEimo_EdaKQRH9.MI7DTr3aDrd; sessionId=f2cbe382-324e-400d-9568-a2820221b717; OptanonAlertBoxClosed=Wed, 15 Jul 2026 14:28:17 GMT; OptanonConsent=groups=C0001%3A1%2CC0002%3A1%2CC0003%3A1%2CC0004%3A0%2CC0005%3A0&hosts=&datestamp=Wed+Jul+15+2026+16%3A28%3A17+GMT%2B0200+(hora+de+verano+de+Europa+central)&version=202510.1.0; pvNumber=2; pvNumberGlobal=2; pvNumberDaily=19");

	if (downloader.fetchUrl(fullUrl, response, &downloadProgress)) {
		processGameSearch(response);

		std::ofstream archivo("D:\\develop\\Github\\xbox360\\project\\Salvia\\salida.txt");
		if (archivo.is_open()) {
			archivo << response;
			archivo.close();
		}
	}
}

void GameFaqs::processGameSearch(const std::string &data){
	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_string(data.c_str());

	// Navigate to <table class="servers">
	pugi::xml_node html = doc.document_element();
	if (!html) return;
	pugi::xml_node body = html.child("body");
	if (!body) return;

	pugi::xml_node search_results = body.find_child_by_attribute("div", "class", "search_results_title");
	if (!search_results) {
		LOG_DEBUG("search_results empty");
		return;
	}

	for (pugi::xml_node result = search_results.child("div"); result; result = result.next_sibling("div")) {
		pugi::xml_node resultTitle = result.find_child_by_attribute("div", "class", "sr_name");
		if (!resultTitle) continue;
		std::string name = Constant::Trim(resultTitle.child("a").child_value());
		LOG_DEBUG("Found title name: ", name.c_str());
	}

}
