#pragma once

#include <string>

class GameFaqs
{
public:
	GameFaqs(void);
	~GameFaqs(void);

	void searchGame(const std::string &gameName);
private:
	void processGameSearch(const std::string &data);

};

