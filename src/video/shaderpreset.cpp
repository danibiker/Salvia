/*
 * shaderpreset.cpp - descubrimiento y carga de los shaders de post-proceso
 * desde assets\shaders. Ver shaderpreset.h para la vision general y
 * salvia_shader_api.h para el reparto de responsabilidades con el backend.
 */

#include "shaderpreset.h"

#include <SDL.h>
#include <map>
#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <const/Constant.h>
#include <io/dirutil.h>
#include <io/fileio.h>
#include <io/fileprops.h>
#include <uiobjects/image.h>
#include <utils/langmanager.h>
#include <utils/logger.h>

ShaderRegistry* ShaderRegistry::m_instance = NULL;

/* Directorio de presets, relativo al directorio de la aplicacion. */
static const char* SHADERS_DIR = "assets\\shaders";
/* Extension del preset. OJO al comparar: ver la nota en load(). */
static const char* PRESET_EXT = ".hlslp";

/* Tabla de migracion de los valores 0..12 que guardaban las versiones
 * anteriores en shaderMode. El orden es el del enum videoShaders que habia en
 * menuconst.h, y coincide con el orden ASCII de los ficheros shippeados: por
 * eso los presets llevan prefijo numerico. */
static const char* LEGACY_SHADER_IDS[] = {
	"00-nearest",
	"01-sharp-bilinear",
	"02-bilinear",
	"03-lcd-grid",
	"04-scanlines",
	"05-crt-geom",
	"06-crt-lottes",
	"07-crt-easymode",
	"08-hq2x",
	"09-hq3x",
	"10-hq4x",
	"11-xbr-lv2-fast",
	"12-5xbr-hyllian"
};
static const int LEGACY_SHADER_COUNT =
	(int)(sizeof(LEGACY_SHADER_IDS) / sizeof(LEGACY_SHADER_IDS[0]));


/* ------------------------------------------------------------------ */
/* Utilidades de cadena                                                */
/* ------------------------------------------------------------------ */

static std::string trim(const std::string& s) {
	std::string::size_type first = s.find_first_not_of(" \t\r\n");
	if (first == std::string::npos) return "";
	std::string::size_type last = s.find_last_not_of(" \t\r\n");
	return s.substr(first, last - first + 1);
}

static std::string unquote(const std::string& s) {
	if (s.size() >= 2 && s[0] == '"' && s[s.size() - 1] == '"')
		return s.substr(1, s.size() - 2);
	return s;
}

static std::string toLower(const std::string& s) {
	std::string out = s;
	for (std::string::size_type i = 0; i < out.size(); i++) {
		if (out[i] >= 'A' && out[i] <= 'Z') out[i] = (char)(out[i] - 'A' + 'a');
	}
	return out;
}

static bool endsWithNoCase(const std::string& s, const std::string& suffix) {
	if (s.size() < suffix.size()) return false;
	return toLower(s.substr(s.size() - suffix.size())) == toLower(suffix);
}

static bool parseBool(const std::string& raw, bool defval) {
	std::string v = toLower(trim(unquote(raw)));
	if (v == "true" || v == "1" || v == "yes" || v == "on") return true;
	if (v == "false" || v == "0" || v == "no" || v == "off") return false;
	return defval;
}

static SalviaShaderWrap parseWrap(const std::string& raw) {
	std::string v = toLower(trim(unquote(raw)));
	/* Nomenclatura de RetroArch. Todo lo que no reconocemos cae en CLAMP, que
	 * es el comportamiento seguro para un quad fullscreen con UV en [0,1]. */
	if (v == "repeat") return SALVIA_WRAP_REPEAT;
	if (v == "mirrored_repeat" || v == "mirror") return SALVIA_WRAP_MIRROR;
	return SALVIA_WRAP_CLAMP;
}

/* Une un directorio con una ruta relativa del preset y normaliza los
 * separadores. Imprescindible: el XDK NO convierte '/' en '\', asi que un
 * preset escrito al estilo RetroArch ("shaders/foo.hlsl") no abriria en la
 * consola sin esto. */
static std::string joinPath(const std::string& dir, const std::string& rel) {
	std::string path = dir;
	if (!path.empty()) {
		char last = path[path.size() - 1];
		if (last != '\\' && last != '/') path += "\\";
	}
	path += rel;
	for (std::string::size_type i = 0; i < path.size(); i++) {
		if (path[i] == '/') path[i] = '\\';
	}
	return path;
}

/* Un id acaba escrito en el .cfg como `shaderMode = <id>` sin comillas y se
 * relee partiendo por '='. Rechazamos lo que rompa ese round-trip. */
static bool isValidId(const std::string& id) {
	if (id.empty()) return false;
	if ((int)id.size() >= SALVIA_SHADER_ID_MAX) return false;
	if (id.find_first_of("=#\\/\r\n") != std::string::npos) return false;
	return true;
}


/* ------------------------------------------------------------------ */
/* Parseo del ini                                                      */
/* ------------------------------------------------------------------ */

/* Mapa clave -> valor muy simple, sobre el modelo de
 * LanguageManager::loadLanguage. Se parte por el PRIMER '=' (no por todos,
 * como haria Constant::splitChar) porque una ruta podria contener '='. */
class PresetKeys {
public:
	void parse(const std::string& text) {
		std::string::size_type pos = 0;
		while (pos <= text.size()) {
			std::string::size_type eol = text.find('\n', pos);
			std::string line = (eol == std::string::npos)
				? text.substr(pos) : text.substr(pos, eol - pos);
			addLine(line);
			if (eol == std::string::npos) break;
			pos = eol + 1;
		}
	}

	bool has(const std::string& key) const {
		return m_map.find(toLower(key)) != m_map.end();
	}

	std::string get(const std::string& key, const std::string& defval = "") const {
		std::map<std::string, std::string>::const_iterator it = m_map.find(toLower(key));
		if (it == m_map.end()) return defval;
		return it->second;
	}

	int getInt(const std::string& key, int defval) const {
		if (!has(key)) return defval;
		std::string v = trim(unquote(get(key)));
		if (v.empty()) return defval;
		return atoi(v.c_str());
	}

private:
	void addLine(const std::string& rawLine) {
		std::string line = trim(rawLine);
		if (line.empty() || line[0] == '#' || line[0] == ';' || line[0] == '[') return;
		std::string::size_type eq = line.find('=');
		if (eq == std::string::npos) return;
		std::string key = toLower(trim(line.substr(0, eq)));
		std::string value = trim(unquote(trim(line.substr(eq + 1))));
		if (!key.empty()) m_map[key] = value;
	}

	std::map<std::string, std::string> m_map;
};

/* Compone "clave" + indice de pasada, p.ej. filter_linear0. */
static std::string passKey(const char* base, int pass) {
	char buf[64];
	sprintf(buf, "%s%d", base, pass);
	return std::string(buf);
}

/* Parte la lista de `textures` por ';', ',' o espacios (RetroArch usa ';',
 * pero se ven presets con las otras dos). */
static void splitTextureList(const std::string& raw, std::vector<std::string>& out) {
	std::string current;
	for (std::string::size_type i = 0; i <= raw.size(); i++) {
		char c = (i < raw.size()) ? raw[i] : ';';
		if (c == ';' || c == ',' || c == ' ' || c == '\t') {
			std::string name = trim(current);
			if (!name.empty()) out.push_back(name);
			current.clear();
		} else {
			current += c;
		}
	}
}


/* ------------------------------------------------------------------ */
/* ShaderRegistry                                                      */
/* ------------------------------------------------------------------ */

ShaderRegistry::ShaderRegistry() : m_loaded(false), m_published(false) {}

ShaderRegistry::~ShaderRegistry() {
	freeTransientBuffers();
}

ShaderRegistry* ShaderRegistry::instance() {
	if (m_instance == NULL) m_instance = new ShaderRegistry();
	return m_instance;
}

const char* ShaderRegistry::defaultId() {
	return "01-sharp-bilinear";
}

int ShaderRegistry::count() const {
	return (int)m_table.size();
}

std::string ShaderRegistry::idAt(int index) const {
	if (index < 0 || index >= (int)m_info.size()) return "";
	return m_info[index].id;
}

int ShaderRegistry::indexOf(const std::string& id) const {
	if (id.empty()) return -1;
	for (std::size_t i = 0; i < m_info.size(); i++) {
		if (m_info[i].id == id) return (int)i;
	}
	return -1;
}

int ShaderRegistry::indexOfStored(const std::string& raw) const {
	std::string v = trim(raw);
	if (v.empty()) return -1;
	/* El nombre real manda: LEGACY_SHADER_IDS es solo una tabla historica para
	 * los .cfg de versiones anteriores, y no debe secuestrar a un preset que se
	 * llame igual que un indice antiguo. */
	int idx = indexOf(v);
	if (idx >= 0) return idx;
	return indexOf(migrateLegacyId(v));
}

std::string ShaderRegistry::migrateLegacyId(const std::string& raw) {
	std::string v = trim(raw);
	if (v.empty()) return v;
	/* Solo se migra si es un entero puro: cualquier otra cosa ya es un id. */
	for (std::string::size_type i = 0; i < v.size(); i++) {
		if (v[i] < '0' || v[i] > '9') return v;
	}
	int idx = atoi(v.c_str());
	if (idx < 0 || idx >= LEGACY_SHADER_COUNT) return v;
	return std::string(LEGACY_SHADER_IDS[idx]);
}

std::string ShaderRegistry::displayName(int index) const {
	if (index < 0 || index >= (int)m_info.size()) return "";
	const ShaderPresetInfo& info = m_info[index];

	if (!info.labelKey.empty()) {
		std::string text = LanguageManager::instance()->get(info.labelKey);
		/* get() devuelve "[clave]" cuando no la encuentra. */
		if (text != "[" + info.labelKey + "]") return text;
	}
	if (!info.label.empty()) return info.label;

	/* Ultimo recurso: embellecer el id ("05-crt-geom" -> "crt geom"). */
	std::string pretty = info.id;
	std::string::size_type dash = pretty.find('-');
	if (dash != std::string::npos && dash > 0 && dash <= 2) {
		bool allDigits = true;
		for (std::string::size_type i = 0; i < dash; i++) {
			if (pretty[i] < '0' || pretty[i] > '9') { allDigits = false; break; }
		}
		if (allDigits) pretty = pretty.substr(dash + 1);
	}
	for (std::string::size_type i = 0; i < pretty.size(); i++) {
		if (pretty[i] == '-' || pretty[i] == '_') pretty[i] = ' ';
	}
	return pretty;
}


/* ------------------------------------------------------------------ */
/* Carga                                                               */
/* ------------------------------------------------------------------ */

bool ShaderRegistry::load() {
	if (m_loaded) return !m_table.empty();
	m_loaded = true;

	std::string dir = dirutil::getPathPrefix(SHADERS_DIR);

	/* No se comprueba antes si el directorio existe: dirutil::dirExists usa
	 * stat(), y en Xbox el comportamiento con rutas "game:\..." no es de fiar.
	 * listFiles usa FindFirstFile y ya devuelve 0 si el directorio no esta, que
	 * es la senal que necesitamos. */

	/* NO se puede filtrar por extension aqui: dirutil::foundFilter hace
	 * filtroExt.find(extension), no una comparacion exacta, asi que pedir
	 * ".hlslp" tambien devolveria los ".hlsl" (".hlslp".find(".hlsl") == 0).
	 * Listamos todo y comprobamos el sufijo a mano. */
	dirutil dirHelper;
	std::vector<std::unique_ptr<FileProps> > files;
	dirHelper.listFiles(dir.c_str(), files, "", "", false /*includeDirs*/,
	                    true /*order*/, false /*properties*/);

	int accepted = 0;
	for (std::size_t i = 0; i < files.size(); i++) {
		const std::string& name = files[i]->filename;
		if (!endsWithNoCase(name, PRESET_EXT)) continue;

		if (accepted >= SALVIA_SHADER_MAX_PRESETS) {
			LOG_ERROR("Mas de %d presets en %s; se ignora %s\n",
			          SALVIA_SHADER_MAX_PRESETS, dir.c_str(), name.c_str());
			break;
		}

		std::string id = name.substr(0, name.size() - strlen(PRESET_EXT));
		if (!isValidId(id)) {
			LOG_ERROR("Nombre de preset no valido, se ignora: %s\n", name.c_str());
			continue;
		}
		if (indexOf(id) >= 0) {
			LOG_ERROR("Preset duplicado, se ignora: %s\n", name.c_str());
			continue;
		}
		if (parsePreset(joinPath(dir, name), id)) accepted++;
	}

	if (m_table.empty()) {
		LOG_ERROR("Ningun preset valido en %s; solo habra los filtros integrados\n",
		          dir.c_str());
		addSyntheticFallbacks();
	}

	bindSourcePointers();

	LOG_INFO("Shaders: %d presets cargados de %s\n", (int)m_table.size(), dir.c_str());
	for (std::size_t i = 0; i < m_info.size(); i++) {
		const SalviaShaderPass& p = m_table[i].passes[0];
		LOG_INFO("  [%2d] %-24s %s %s%s%s\n",
		         (int)i, m_info[i].id.c_str(),
		         (p.source != NULL) ? "shader " : "sampler",
		         (p.filter == SALVIA_FILTER_LINEAR) ? "linear" : "nearest",
		         (p.psFlags & SALVIA_PS_FULL_PRECISION) ? " full-prec" : "",
		         (!m_info[i].luts.empty()) ? " +lut" : "");
	}
	return !m_table.empty();
}

bool ShaderRegistry::parsePreset(const std::string& path, const std::string& id) {
	Fileio io;
	std::string text = io.cargarFichero(path);
	if (text.empty()) {
		LOG_ERROR("No se puede leer el preset %s\n", path.c_str());
		return false;
	}

	PresetKeys keys;
	keys.parse(text);

	int declared = keys.getInt("shaders", 1);
	if (declared < 1) {
		LOG_ERROR("%s declara shaders=%d; se ignora el preset\n", path.c_str(), declared);
		return false;
	}
	if (declared > 1) {
		/* Multi-pasada aun no implementada: el pipeline dibuja una sola vez
		 * al backbuffer. Cargamos la pasada 0 y avisamos. */
		LOG_INFO("%s declara %d pasadas; solo se ejecuta la 0\n", path.c_str(), declared);
	}

	std::string baseDir = path.substr(0, path.find_last_of("\\/"));

	SalviaShaderPreset preset;
	memset(&preset, 0, sizeof(preset));
	strncpy(preset.id, id.c_str(), SALVIA_SHADER_ID_MAX - 1);
	preset.id[SALVIA_SHADER_ID_MAX - 1] = '\0';
	preset.passCount = 1;
	preset.activePass = 0;

	SalviaShaderPass& pass = preset.passes[0];
	pass.source = NULL;   /* se engancha en bindSourcePointers */
	pass.filter = parseBool(keys.get(passKey("filter_linear", 0)), false)
	              ? SALVIA_FILTER_LINEAR : SALVIA_FILTER_NEAREST;
	pass.wrap = keys.has(passKey("wrap_mode", 0))
	            ? parseWrap(keys.get(passKey("wrap_mode", 0))) : SALVIA_WRAP_CLAMP;
	pass.psFlags = (toLower(keys.get(passKey("salvia_precision", 0))) == "full")
	               ? SALVIA_PS_FULL_PRECISION : 0;
	pass.lutCount = 0;

	/* Cuerpo HLSL. Sin shader0 el preset solo describe estado de sampler y
	 * usa el passthrough integrado: asi se representan Nearest y Bilinear. */
	int sourceIdx = -1;
	std::string shaderFile = keys.get(passKey("shader", 0));
	if (!shaderFile.empty()) {
		std::string hlslPath = joinPath(baseDir, shaderFile);
		Fileio srcIo;
		std::string body = srcIo.cargarFichero(hlslPath);
		if (body.empty()) {
			LOG_ERROR("%s: no se puede leer %s; se ignora el preset\n",
			          path.c_str(), hlslPath.c_str());
			return false;
		}
		m_sources.push_back(body);
		sourceIdx = (int)m_sources.size() - 1;
	}

	/* LUTs, con el mecanismo `textures =` de RetroArch. Aqui solo se anotan:
	 * el PNG se decodifica en publish(), porque load() corre antes de
	 * SDL_Init y no conviene invocar a IMG_Load todavia. */
	std::vector<ShaderLutSpec> lutSpecs;
	std::vector<std::string> textureNames;
	splitTextureList(keys.get("textures"), textureNames);
	for (std::size_t t = 0; t < textureNames.size(); t++) {
		if ((int)lutSpecs.size() >= SALVIA_SHADER_MAX_LUTS) {
			LOG_ERROR("%s: mas de %d LUTs; se ignora %s\n",
			          path.c_str(), SALVIA_SHADER_MAX_LUTS, textureNames[t].c_str());
			break;
		}
		const std::string& name = textureNames[t];
		std::string file = keys.get(name);
		if (file.empty()) {
			LOG_ERROR("%s: textures declara '%s' pero no hay clave '%s'\n",
			          path.c_str(), name.c_str(), name.c_str());
			continue;
		}
		ShaderLutSpec spec;
		spec.path = joinPath(baseDir, file);
		spec.filter = parseBool(keys.get(name + "_linear"), false)
		              ? SALVIA_FILTER_LINEAR : SALVIA_FILTER_NEAREST;
		spec.wrap = keys.has(name + "_wrap_mode")
		            ? parseWrap(keys.get(name + "_wrap_mode")) : SALVIA_WRAP_CLAMP;
		/* Por defecto la primera LUT va a s1, la segunda a s2... */
		spec.sampler = keys.getInt("salvia_sampler_" + name, (int)lutSpecs.size() + 1);
		if (spec.sampler < 1 || spec.sampler > SALVIA_SHADER_MAX_LUTS)
			spec.sampler = (int)lutSpecs.size() + 1;
		lutSpecs.push_back(spec);
	}

	ShaderPresetInfo info;
	info.id = id;
	info.label = keys.get("salvia_label");
	info.labelKey = keys.get("salvia_label_key");
	info.presetPath = path;
	info.declaredPasses = declared;
	info.luts = lutSpecs;

	m_table.push_back(preset);
	m_info.push_back(info);
	for (int p = 0; p < SALVIA_SHADER_MAX_PASSES; p++)
		m_sourceIndex.push_back(p == 0 ? sourceIdx : -1);
	return true;
}

bool ShaderRegistry::loadLut(const ShaderLutSpec& spec, SalviaShaderLut* out) {
	const std::string& path = spec.path;

	/* Con format = NULL devuelve el surface crudo de IMG_Load, SIN pasar por
	 * SDL_ConvertSurface. Es deliberado: en SDL 1.2, convertir un surface que
	 * tenga SDL_SRCALPHA hace un blit CON MEZCLA ALFA, y una LUT no es una
	 * imagen -son datos empaquetados en RGBA, con alfas cercanos a 0- asi que
	 * la conversion los destruiria. Aqui despaquetamos a mano. */
	SDL_Surface* raw = Image::loadConvertedSurface(path, NULL);
	if (raw == NULL) return false;

	int w = raw->w;
	int h = raw->h;
	if (w <= 0 || h <= 0) { SDL_FreeSurface(raw); return false; }

	/* Contrato de salvia_shader_api.h: words 0xAARRGGBB en endianness nativa.
	 * Escribir Uint32 lo cumple en las dos plataformas sin byte-swap. */
	unsigned char* buffer = new unsigned char[(std::size_t)w * h * 4];
	if (SDL_MUSTLOCK(raw)) SDL_LockSurface(raw);

	const int bpp = raw->format->BytesPerPixel;
	for (int y = 0; y < h; y++) {
		Uint8* srcRow = (Uint8*)raw->pixels + y * raw->pitch;
		Uint32* dstRow = (Uint32*)(buffer + (std::size_t)y * w * 4);
		for (int x = 0; x < w; x++) {
			Uint8* p = srcRow + x * bpp;
			Uint32 pixel = 0;
			switch (bpp) {
				case 1: pixel = *p; break;
				case 2: pixel = *(Uint16*)p; break;
				case 3:
					/* Los 3 bytes en el orden del surface. */
					if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
						pixel = (p[0] << 16) | (p[1] << 8) | p[2];
					else
						pixel = p[0] | (p[1] << 8) | (p[2] << 16);
					break;
				default: pixel = *(Uint32*)p; break;
			}
			Uint8 r, g, b, a;
			SDL_GetRGBA(pixel, raw->format, &r, &g, &b, &a);
			dstRow[x] = ((Uint32)a << 24) | ((Uint32)r << 16)
			          | ((Uint32)g << 8) | (Uint32)b;
		}
	}

	if (SDL_MUSTLOCK(raw)) SDL_UnlockSurface(raw);
	SDL_FreeSurface(raw);

	m_lutPixels.push_back(buffer);
	out->pixels = buffer;
	out->width = w;
	out->height = h;
	out->pitch = w * 4;
	out->filter = (SalviaShaderFilter)spec.filter;
	out->wrap = (SalviaShaderWrap)spec.wrap;
	out->sampler = spec.sampler;
	return true;
}

/* Decodifica todas las LUT anotadas durante load(). Se llama desde publish(),
 * con la SDL ya arrancada. Una LUT que falle deja al shader dibujando raro,
 * pero no cuelga ni deja la pantalla negra: es preferible a tirar el preset. */
void ShaderRegistry::decodeLuts() {
	for (std::size_t i = 0; i < m_table.size() && i < m_info.size(); i++) {
		SalviaShaderPass& pass = m_table[i].passes[m_table[i].activePass];
		pass.lutCount = 0;
		for (std::size_t l = 0; l < m_info[i].luts.size(); l++) {
			if (pass.lutCount >= SALVIA_SHADER_MAX_LUTS) break;
			SalviaShaderLut lut;
			memset(&lut, 0, sizeof(lut));
			if (loadLut(m_info[i].luts[l], &lut)) {
				pass.luts[pass.lutCount++] = lut;
			} else {
				LOG_ERROR("%s: no se pudo cargar la LUT %s\n",
				          m_info[i].id.c_str(), m_info[i].luts[l].path.c_str());
			}
		}
	}
}

/* Dos entradas minimas para que la lista nunca quede vacia si assets\shaders
 * falta o esta ilegible. Ambas usan el passthrough integrado del backend, asi
 * que no necesitan ningun fichero. */
void ShaderRegistry::addSyntheticFallbacks() {
	const char* ids[2] = { "00-nearest", "02-bilinear" };
	const char* labels[2] = { "Nearest", "Bilinear" };
	const SalviaShaderFilter filters[2] = { SALVIA_FILTER_NEAREST, SALVIA_FILTER_LINEAR };

	for (int i = 0; i < 2; i++) {
		SalviaShaderPreset preset;
		memset(&preset, 0, sizeof(preset));
		strncpy(preset.id, ids[i], SALVIA_SHADER_ID_MAX - 1);
		preset.passCount = 1;
		preset.activePass = 0;
		preset.passes[0].source = NULL;
		preset.passes[0].filter = filters[i];
		preset.passes[0].wrap = SALVIA_WRAP_CLAMP;

		ShaderPresetInfo info;
		info.id = ids[i];
		info.label = labels[i];
		info.labelKey = (i == 0) ? "menu.video.shader0" : "menu.video.shader2";
		info.declaredPasses = 1;

		m_table.push_back(preset);
		m_info.push_back(info);
		for (int p = 0; p < SALVIA_SHADER_MAX_PASSES; p++) m_sourceIndex.push_back(-1);
	}
}

/* Engancha los punteros a los cuerpos HLSL. Se hace AQUI, y no durante el
 * parseo, porque m_sources ya no va a crecer: cualquier .c_str() tomado antes
 * habria quedado colgando en cuanto el vector se reubicase. */
void ShaderRegistry::bindSourcePointers() {
	for (std::size_t i = 0; i < m_table.size(); i++) {
		for (int p = 0; p < SALVIA_SHADER_MAX_PASSES; p++) {
			std::size_t flat = i * SALVIA_SHADER_MAX_PASSES + p;
			int idx = (flat < m_sourceIndex.size()) ? m_sourceIndex[flat] : -1;
			m_table[i].passes[p].source =
				(idx >= 0 && idx < (int)m_sources.size()) ? m_sources[idx].c_str() : NULL;
		}
	}
}

bool ShaderRegistry::publish() {
	if (m_table.empty()) return false;
	if (m_published) return true;
	decodeLuts();
	m_published = (SalviaShader_SetTable(&m_table[0], (int)m_table.size()) != 0);
	if (!m_published) LOG_ERROR("El backend de video rechazo la tabla de shaders\n");
	return m_published;
}

void ShaderRegistry::freeTransientBuffers() {
	if (m_lutPixels.empty()) return;
	/* Solo cuando el backend confirma que ya las subio a la GPU: a partir de
	 * ahi las texturas sobreviven por su cuenta al device lost/reset. */
	if (m_published && !SalviaShader_LutsUploaded()) return;

	for (std::size_t i = 0; i < m_lutPixels.size(); i++) delete[] m_lutPixels[i];
	m_lutPixels.clear();

	for (std::size_t i = 0; i < m_table.size(); i++) {
		for (int p = 0; p < SALVIA_SHADER_MAX_PASSES; p++) {
			for (int l = 0; l < m_table[i].passes[p].lutCount; l++)
				m_table[i].passes[p].luts[l].pixels = NULL;
		}
	}
}
