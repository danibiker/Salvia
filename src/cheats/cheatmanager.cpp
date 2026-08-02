#include <cheats/cheatmanager.h>

#include <map>
#include <cstdlib>

#include <io/filelist.h>
#include <libretro/libretro.h>

// ---------------------------------------------------------------------------
// Helpers locales
// ---------------------------------------------------------------------------
static std::string trimStr(const std::string& s) {
	const char* ws = " \t\r\n";
	std::size_t a = s.find_first_not_of(ws);
	if (a == std::string::npos) return "";
	std::size_t b = s.find_last_not_of(ws);
	return s.substr(a, b - a + 1);
}

// Quita comillas dobles envolventes si las hay.
static std::string unquote(const std::string& s) {
	std::string t = trimStr(s);
	if (t.size() >= 2 && t[0] == '"' && t[t.size() - 1] == '"')
		return t.substr(1, t.size() - 2);
	return t;
}

// Descompone una clave "cheat<idx>_<field>" en su indice y su campo.
// Devuelve false si no encaja (p.ej. la clave "cheats" no tiene digitos).
static bool parseCheatKey(const std::string& key, int& idx, std::string& field) {
	const std::string prefix = "cheat";
	if (key.size() <= prefix.size()) return false;
	if (key.compare(0, prefix.size(), prefix) != 0) return false;

	std::size_t start = prefix.size();
	std::size_t p = start;
	while (p < key.size() && key[p] >= '0' && key[p] <= '9') ++p;

	if (p == start) return false;                 // no hay digitos ("cheats", ...)
	if (p >= key.size() || key[p] != '_') return false;

	idx   = std::atoi(key.substr(start, p - start).c_str());
	field = key.substr(p + 1);
	return true;
}

// ---------------------------------------------------------------------------
// CheatManager
// ---------------------------------------------------------------------------
int CheatManager::loadFromFile(const std::string& path) {
	cheats.clear();
	currentPath = path;

	std::vector<std::string> lines;
	if (path.empty() || !FileList::cargarVector(path, lines))
		return 0;

	// Acumulamos por indice; solo nos interesan desc y code. El estado "enable" del
	// fichero se ignora a proposito: todos los cheats arrancan desactivados.
	std::map<int, Cheat> byIndex;
	for (std::size_t i = 0; i < lines.size(); ++i) {
		std::string line = trimStr(lines[i]);
		if (line.empty() || line[0] == '#' || line[0] == ';' || line[0] == '[') continue;

		std::size_t eq = line.find('=');
		if (eq == std::string::npos) continue;

		std::string key = trimStr(line.substr(0, eq));
		std::string val = line.substr(eq + 1);

		int idx;
		std::string field;
		if (!parseCheatKey(key, idx, field)) continue;   // ignora "cheats = N", basura, etc.

		if (field == "desc")      byIndex[idx].desc = unquote(val);
		else if (field == "code") byIndex[idx].code = unquote(val);
	}

	// Aplanamos ordenado por indice, descartando los cheats sin codigo: los cheats
	// "solo memoria" del formato nuevo de RetroArch (cheatN_handler=2 con address/value
	// y sin cheatN_code) no se pueden aplicar sin motor de poke.
	for (std::map<int, Cheat>::iterator it = byIndex.begin(); it != byIndex.end(); ++it) {
		if (it->second.code.empty()) continue;
		if (it->second.desc.empty()) it->second.desc = it->second.code;
		cheats.push_back(it->second);
	}

	return (int)cheats.size();
}

void CheatManager::applyToCore() {
	// Misma semantica que RetroArch: reset y luego indice compactado solo de los
	// cheats habilitados.
	retro_cheat_reset();
	unsigned idx = 0;
	for (std::size_t i = 0; i < cheats.size(); ++i) {
		if (cheats[i].enabled && !cheats[i].code.empty()) {
			retro_cheat_set(idx, true, cheats[i].code.c_str());
			++idx;
		}
	}
}

void CheatManager::clear() {
	// Se llama en closeGame con el core aun cargado, asi que el reset es seguro.
	retro_cheat_reset();
	cheats.clear();
	currentPath.clear();
	sourceName.clear();
}
