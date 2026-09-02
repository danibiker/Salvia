#pragma once

/* ShaderRegistry: descubrimiento y carga de los shaders de post-proceso desde
 * <appDir>\assets\shaders\*.hlslp.
 *
 * Formato: preset estilo RetroArch (ini de `clave = valor`) apuntando a un
 * cuerpo HLSL ps_3_0. Se usan las extensiones .hlslp/.hlsl -y no .slangp/
 * .slang- porque el cuerpo NO es GLSL: un .slang de RetroArch es GLSL de
 * Vulkan y no se puede compilar con D3DXCompileShader. La sintaxis del preset
 * si es la de RetroArch, incluido el mecanismo `textures =` para las LUT.
 *
 * Esta clase hace todo el trabajo que necesita C++ (enumerar el directorio,
 * parsear el ini, leer ficheros, decodificar el PNG de la LUT) y publica el
 * resultado al backend de video activo a traves de la API C neutra de
 * salvia_shader_api.h. Ver ese fichero para el reparto de responsabilidades.
 *
 * ORDEN DE ARRANQUE:
 *   1. load()     tras fijar Constant::appDir y ANTES de construir CfgLoader
 *                 (la config resuelve nombre de preset -> indice).
 *   2. publish()  ANTES de SDL_SetVideoMode / WinD3D9_Init, porque
 *                 initShaders() se llama desde dentro de ellos.
 *   3. freeTransientBuffers()  despues, para soltar los pixeles de las LUT.
 */

#include <string>
#include <vector>

#include "salvia_shader_api.h"

struct SDL_Surface;

/* LUT declarada por un preset, aun SIN decodificar. El PNG no se puede leer
 * durante load(), que corre antes de SDL_Init: la decodificacion se difiere a
 * publish(), que se llama desde engine.cpp con la SDL ya arrancada. */
struct ShaderLutSpec {
	std::string path;    /* ruta absoluta ya resuelta y normalizada */
	int         filter;  /* SalviaShaderFilter */
	int         wrap;    /* SalviaShaderWrap */
	int         sampler; /* registro s<N> destino */
};

/* Metadatos de un preset que solo interesan del lado de la aplicacion
 * (el backend de video no los necesita). */
struct ShaderPresetInfo {
	std::string id;         /* nombre de fichero sin extension: "05-crt-geom" */
	std::string label;      /* salvia_label: nombre visible por defecto */
	std::string labelKey;   /* salvia_label_key: clave i18n opcional */
	std::string presetPath; /* ruta completa del .hlslp, para el log */
	int         declaredPasses; /* el `shaders = N` original, aunque solo usemos 1 */
	std::vector<ShaderLutSpec> luts;
};

class ShaderRegistry {
public:
	static ShaderRegistry* instance();

	/* Descubre y parsea assets\shaders. Idempotente: la segunda llamada no
	 * hace nada. Devuelve false si no encontro ningun preset valido (en ese
	 * caso deja sintetizados Nearest y Bilinear, asi que la lista NUNCA queda
	 * vacia y el menu siempre tiene algo que mostrar). */
	bool load();

	/* Empuja la tabla al backend de video. Llamar antes de SetVideoMode. */
	bool publish();

	/* Libera los SDL_Surface de las LUT una vez el backend las ha subido a la
	 * GPU. Las texturas resultantes sobreviven al device reset en las dos
	 * plataformas, asi que no hay que conservar los pixeles en RAM. */
	void freeTransientBuffers();

	int         count() const;
	std::string idAt(int index) const;            /* "" si fuera de rango */
	int         indexOf(const std::string& id) const;  /* -1 si no existe */

	/* Resuelve un valor leido de la configuracion a indice. Busca PRIMERO un
	 * preset con ese nombre exacto y solo si no existe intenta la migracion de
	 * los indices numericos antiguos; asi un preset que se llame literalmente
	 * "7" gana sobre la tabla legacy en vez de acabar en "07-crt-easymode".
	 * Devuelve -1 si no resuelve a nada. */
	int         indexOfStored(const std::string& raw) const;

	/* Nombre para el menu: salvia_label_key traducida, si no salvia_label,
	 * si no el id embellecido. Requiere LanguageManager ya cargado. */
	std::string displayName(int index) const;

	/* Migracion de los valores numericos 0..12 que guardaban las versiones
	 * anteriores en shaderMode. Si raw no es un numero se devuelve tal cual. */
	static std::string migrateLegacyId(const std::string& raw);

	/* Id del preset por defecto cuando la config no resuelve. */
	static const char* defaultId();

private:
	ShaderRegistry();
	~ShaderRegistry();

	bool parsePreset(const std::string& path, const std::string& id);
	/* Decodifica el PNG de una LUT. Requiere SDL ya inicializada, asi que solo
	 * se llama desde publish(), nunca desde load(). */
	bool loadLut(const ShaderLutSpec& spec, SalviaShaderLut* out);
	void decodeLuts();
	void addSyntheticFallbacks();
	void bindSourcePointers();

	static ShaderRegistry* m_instance;

	std::vector<ShaderPresetInfo>   m_info;
	std::vector<SalviaShaderPreset> m_table;
	/* Dueño de los cuerpos HLSL. OJO: un vector<string> reubica su contenido
	 * al crecer, invalidando cualquier .c_str() tomado antes. Por eso durante
	 * el parseo solo se guarda el INDICE en m_sourceIndex, y los punteros se
	 * enganchan al final, cuando m_sources ya no va a crecer mas
	 * (bindSourcePointers). */
	std::vector<std::string>        m_sources;
	std::vector<int>                m_sourceIndex; /* [preset*MAX_PASSES + pass], -1 = passthrough */
	/* Dueño de los pixeles de las LUT. Punteros crudos a proposito: son
	 * estables aunque el vector se reubique. */
	std::vector<unsigned char*>     m_lutPixels;
	bool m_loaded;
	bool m_published;
};
