#include "juego.h"

#include "sistema.h"

#include <windows.h>

#include <algorithm>
#include <fstream>
#include <regex>
#include <sstream>

namespace Port {
namespace {

// key=value lines under [sections] (section names are not needed: keys are unique), # and ; comments.
std::map<std::string, std::string> LeerIni(const std::filesystem::path& archivo) {
	std::map<std::string, std::string> valores;
	std::istringstream texto(Sistema::LeerTexto(archivo));
	std::string linea;
	const auto recortar = [](std::string s) {
		const auto a = s.find_first_not_of(" \t\r\n");
		const auto b = s.find_last_not_of(" \t\r\n");
		return a == std::string::npos ? std::string {} : s.substr(a, b - a + 1);
	};
	while (std::getline(texto, linea)) {
		linea = recortar(linea);
		if (linea.empty() || linea[0] == '#' || linea[0] == ';' || linea[0] == '[') continue;
		const auto igual = linea.find('=');
		if (igual == std::string::npos) continue;
		valores[recortar(linea.substr(0, igual))] = recortar(linea.substr(igual + 1));
	}
	return valores;
}

std::vector<std::string> Partir(const std::string& texto, char separador) {
	std::vector<std::string> partes;
	std::string parte;
	std::istringstream entrada(texto);
	while (std::getline(entrada, parte, separador))
		if (!parte.empty()) partes.push_back(parte);
	return partes;
}

Opcion Interruptor(std::string clave, std::string texto, std::string ayuda, bool defecto, bool recomendado,
                   std::string requiere = {}) {
	Opcion o;
	o.clave       = std::move(clave);
	o.texto       = std::move(texto);
	o.ayuda       = std::move(ayuda);
	o.interruptor = true;
	o.defecto     = defecto ? "1" : "0";
	o.recomendado = recomendado ? "1" : "0";
	o.requiere    = std::move(requiere);
	return o;
}

Opcion Lista(std::string clave, std::string texto, std::string ayuda, std::vector<Valor> valores, std::string defecto,
             std::string recomendado) {
	Opcion o;
	o.clave       = std::move(clave);
	o.texto       = std::move(texto);
	o.ayuda       = std::move(ayuda);
	o.valores     = std::move(valores);
	o.defecto     = std::move(defecto);
	o.recomendado = std::move(recomendado);
	return o;
}

const std::vector<Valor> kResoluciones {
    {"Tu pantalla", "auto"}, {"1920 x 1080", "1920x1080"}, {"2560 x 1440", "2560x1440"}, {"3840 x 2160", "3840x2160"}};

// The console languages with Latin letters, as the PS5 numbers them.
std::vector<Valor> Idiomas() {
	return {{"Español (España)", "3"},   {"Español (Latinoamérica)", "20"}, {"English (US)", "1"},  {"English (UK)", "18"},
	        {"Français", "2"},           {"Deutsch", "4"},                  {"Italiano", "5"},      {"Português (Brasil)", "17"},
	        {"Português (Portugal)", "7"}, {"Nederlands", "6"},             {"Polski", "16"}};
}

// Each port's main settings: what its tests found matters, with what they found best on an RX 6800 XT.
std::vector<Opcion> OpcionesDe(const std::string& perfil) {
	std::vector<Opcion> o;
	if (perfil == "demons_souls") {
		o.push_back(Lista("resolucion", "Resolución de salida", "El tamaño de la imagen final (la interfaz del juego es 4K).",
		                  kResoluciones, "auto", "auto"));
		o.push_back(Interruptor("pantalla_completa", "Pantalla completa", "F11 o Alt+Intro la cambian en el juego.", true, true));
		o.push_back(Lista("render", "Resolución 3D",
		                  "La altura a la que el juego dibuja el 3D antes de escalarlo. Más baja: más rápida donde manda la "
		                  "tarjeta gráfica, menos nítida.",
		                  {{"1440 (como PS5)", "1440"}, {"1296", "1296"}, {"1152", "1152"}, {"1080", "1080"}, {"900", "900"}},
		                  "1440", "1440"));
		o.push_back(Lista("calidad", "Calidad gráfica",
		                  "Baja y Mínima acortan sombras y simplifican lo lejano: ayudan donde manda el procesador.",
		                  {{"Estándar (como PS5)", "standard"}, {"Baja", "low"}, {"Mínima", "lowest"}}, "standard", "standard"));
		o.push_back(Interruptor("generacion", "Generación de cuadros (FSR 3)",
		                        "Dobla los cuadros que se muestran con AMD FSR 3. Más fluido; la respuesta a los mandos no mejora.",
		                        false, true, "amd_fidelityfx_vk.dll"));
		o.push_back(Interruptor("fps120", "Hasta 120 fps",
		                        "Deja que el juego pase de 60 donde el equipo puede. Los vídeos y menús siguen a 60.", false, false));
		o.push_back(Interruptor("vsync", "Sincronización vertical", "Sin cortes en la imagen; puede añadir algo de retardo.", false,
		                        false));
		o.push_back(Interruptor("mostrar_fps", "Mostrar FPS", "Los cuadros por segundo arriba a la derecha (F8 lo cambia).", false,
		                        true));
		o.push_back(Lista("idioma", "Idioma del juego", "", Idiomas(), "3", "3"));
	} else if (perfil == "fc27") {
		o.push_back(Lista("resolucion", "Resolución de salida", "El tamaño de la imagen final.", kResoluciones, "2560x1440",
		                  "2560x1440"));
		o.push_back(Interruptor("pantalla_completa", "Pantalla completa", "", true, true));
		o.push_back(Lista("interna", "Resolución del juego",
		                  "La salida que el juego cree tener. 2160p dibuja hasta 2656x1496 (jugadores nítidos) y necesita 12 GB "
		                  "de memoria de vídeo; Automática la elige según tu tarjeta.",
		                  {{"Automática", "3"}, {"1080p", "1"}, {"2160p", "2"}}, "1", "1"));
		o.push_back(Interruptor("generacion", "Generación de cuadros", "Cuadros intermedios generados: más fluido.", true, true));
		o.push_back(Interruptor("fsr", "Escalado FSR", "Escala la imagen a la resolución de salida con AMD FSR.", true, true));
		o.push_back(Interruptor("vsync", "Sincronización vertical", "", false, false));
		o.push_back(Interruptor("nucleos", "Solo núcleos de rendimiento",
		                        "El juego en los núcleos rápidos del procesador y con prioridad alta.", true, true));
		o.push_back(Interruptor("mostrar_fps", "Mostrar FPS",
		                        "Los cuadros por segundo arriba a la derecha; con generación de cuadros, los reales y los mostrados.",
		                        false, true));
		o.push_back(Lista("idioma", "Idioma del juego", "", Idiomas(), "20", "20"));
	} else if (perfil == "astro_playroom") {
		o.push_back(Lista("resolucion", "Resolución de salida", "El tamaño de la imagen final.", kResoluciones, "auto", "auto"));
		o.push_back(Interruptor("pantalla_completa", "Pantalla completa", "", true, true));
		o.push_back(Lista("parche", "Resolución interna",
		                  "La resolución a la que dibuja el juego (parches del juego, sin iluminación global por trazado de "
		                  "rayos). 1440p mantiene los fps más estables que 4K.",
		                  {{"1440p", "PPSA01325_1440p.json"}, {"1080p", "PPSA01325_1080p.json"}, {"4K", "PPSA01325_4K.json"},
		                   {"Nativa", "PPSA01325.json"}},
		                  "PPSA01325_1440p.json", "PPSA01325_1440p.json"));
		o.push_back(Interruptor("fsr", "Escalado FSR", "Escala la imagen interna a toda la pantalla con AMD FSR.", true, true));
		o.push_back(Interruptor("generacion", "Generación de cuadros", "Cuadros intermedios generados: más fluido.", false, false));
		o.push_back(Interruptor("vsync", "Sincronización vertical", "", false, false));
		o.push_back(Interruptor("nucleos", "Solo núcleos de rendimiento",
		                        "El juego en los núcleos rápidos del procesador y con prioridad alta.", true, true));
		o.push_back(Interruptor("mostrar_fps", "Mostrar FPS",
		                        "Los cuadros por segundo arriba a la derecha; con generación de cuadros, los reales y los mostrados.",
		                        false, true));
		o.push_back(Lista("idioma", "Idioma del juego", "", Idiomas(), "20", "20"));
	}
	return o;
}

std::wstring Comillas(const std::filesystem::path& ruta) {
	return L"\"" + ruta.wstring() + L"\"";
}

std::pair<int, int> Tamano(const std::string& valor, int ancho, int alto) {
	if (valor == "auto") return {ancho, alto};
	int w = 0, h = 0;
	if (std::sscanf(valor.c_str(), "%dx%d", &w, &h) == 2 && w > 0 && h > 0) return {w, h};
	return {1920, 1080};
}

std::filesystem::path PowerShell() {
	wchar_t sistema[MAX_PATH] {};
	GetSystemDirectoryW(sistema, MAX_PATH);
	return std::filesystem::path(sistema) / L"WindowsPowerShell" / L"v1.0" / L"powershell.exe";
}

} // namespace

bool Juego::Cargar(const std::filesystem::path& carpeta) {
	m_carpeta = carpeta;
	const auto ini = LeerIni(carpeta / "juego.ini");
	if (ini.empty()) return false;
	const auto valor = [&](const char* clave, std::string defecto = {}) {
		const auto it = ini.find(clave);
		return it == ini.end() ? defecto : it->second;
	};
	m_titulo     = valor("titulo", "Juego");
	m_plataforma = valor("plataforma", m_plataforma);
	m_perfil     = valor("perfil");
	m_series     = Partir(valor("series"), ',');
	m_buscar     = Partir(valor("buscar"), ';');
	for (const auto& par: Partir(valor("entorno"), ';'))
		if (const auto igual = par.find('='); igual != std::string::npos && igual > 0)
			m_entorno.push_back({par.substr(0, igual), par.substr(igual + 1)});
	m_motor      = m_perfil == "demons_souls" ? Motor::DemonsSouls : Motor::Kyty;
	m_motor_carpeta = Sistema::Ancho(valor("motor", "motor"));
	std::error_code error;
	// The engine's folder stays out of sight (a zip does not keep the attribute).
	if (const auto atributos = GetFileAttributesW(CarpetaMotor().c_str());
	    atributos != INVALID_FILE_ATTRIBUTES && (atributos & FILE_ATTRIBUTE_HIDDEN) == 0)
		SetFileAttributesW(CarpetaMotor().c_str(), atributos | FILE_ATTRIBUTE_HIDDEN);
	for (auto& o: OpcionesDe(m_perfil))
		if (o.requiere.empty() || std::filesystem::exists(CarpetaMotor() / o.requiere, error)) m_opciones.push_back(std::move(o));

	const auto guardados = LeerIni(carpeta / "ajustes.ini");
	for (const auto& o: m_opciones) {
		const auto it = guardados.find(o.clave);
		m_valores[o.clave] = it != guardados.end() ? it->second : o.recomendado;
	}
	if (const auto it = guardados.find("carpeta_juego"); it != guardados.end() && !it->second.empty())
		m_carpeta_juego = std::filesystem::path(Sistema::Ancho(it->second));
	LeerInfo();
	if (!m_info.valido) BuscarCarpetaJuego();
	// A package opened for the first time keeps what it found.
	if (!std::filesystem::exists(carpeta / "ajustes.ini", error)) Guardar();
	return true;
}

std::string Juego::Valor(const std::string& clave) const {
	const auto it = m_valores.find(clave);
	return it == m_valores.end() ? std::string {} : it->second;
}

void Juego::Fijar(const std::string& clave, const std::string& valor) {
	if (Valor(clave) == valor) return;
	m_valores[clave] = valor;
	m_sucio          = true;
}

void Juego::UsarRecomendados() {
	for (const auto& o: m_opciones) Fijar(o.clave, o.recomendado);
}

bool Juego::SonLosRecomendados() const {
	return std::all_of(m_opciones.begin(), m_opciones.end(), [&](const Opcion& o) { return Valor(o.clave) == o.recomendado; });
}

void Juego::Guardar() {
	std::ofstream salida(m_carpeta / "ajustes.ini", std::ios::binary | std::ios::trunc);
	salida << "# Ajustes de " << m_titulo << " (los guarda el programa del juego).\n";
	salida << "carpeta_juego=" << Sistema::Utf8(m_carpeta_juego.wstring()) << "\n";
	for (const auto& o: m_opciones) salida << o.clave << "=" << Valor(o.clave) << "\n";
	m_sucio = false;
}

void Juego::FijarCarpetaJuego(const std::filesystem::path& carpeta) {
	m_carpeta_juego = carpeta;
	LeerInfo();
	Guardar();
}

void Juego::LeerInfo() {
	m_info = {};
	std::error_code error;
	if (m_carpeta_juego.empty() || !std::filesystem::exists(m_carpeta_juego / "eboot.bin", error)) return;
	m_info.valido = true;
	const auto param = Sistema::LeerTexto(m_carpeta_juego / "sce_sys" / "param.json");
	std::smatch encontrado;
	if (std::regex_search(param, encontrado, std::regex(R"re("titleId"\s*:\s*"([^"]+)")re"))) m_info.serie = encontrado[1];
	if (std::regex_search(param, encontrado, std::regex(R"re("contentVersion"\s*:\s*"([^"]+)")re"))) m_info.version = encontrado[1];
	m_info.serie_conocida = m_series.empty() || std::find(m_series.begin(), m_series.end(), m_info.serie) != m_series.end();
}

void Juego::BuscarCarpetaJuego() {
	std::error_code error;
	for (const auto& lugar: m_buscar) {
		auto ruta = std::filesystem::path(Sistema::Ancho(lugar));
		if (ruta.is_relative()) ruta = m_carpeta / ruta;
		if (std::filesystem::exists(ruta / "eboot.bin", error)) {
			m_carpeta_juego = std::filesystem::weakly_canonical(ruta, error);
			LeerInfo();
			return;
		}
	}
}

Juego::Orden Juego::OrdenJugar(int pantalla_ancho, int pantalla_alto) const {
	Orden orden;
	const auto [ancho, alto] = Tamano(Valor("resolucion"), pantalla_ancho, pantalla_alto);
	const auto si            = [&](const char* clave) { return Valor(clave) == "1"; };
	std::wstring a;
	if (m_motor == Motor::DemonsSouls) {
		// Its run-windows.ps1: the performance switches of run-windows.json, the CPUs for the render threads, the shader
		// prefetch's inputs; -Follow keeps it until the game ends, with the game's exit code.
		orden.programa = PowerShell();
		a = L"-NoProfile -ExecutionPolicy Bypass -File " + Comillas(CarpetaMotor() / L"run-windows.ps1") + L" -Follow";
		a += L" -Game " + Comillas(m_carpeta_juego);
		a += L" -Width " + std::to_wstring(ancho) + L" -Height " + std::to_wstring(alto);
		if (si("pantalla_completa")) a += L" -Fullscreen";
		a += L" -Language " + Sistema::Ancho(Valor("idioma"));
		if (si("fps120")) a += L" -Fps120";
		if (si("vsync")) a += L" -PresentMode Fifo";
		if (const auto render = Valor("render"); !render.empty() && render != "1440") a += L" -RenderHeight " + Sistema::Ancho(render);
		if (const auto calidad = Valor("calidad"); calidad == "low" || calidad == "lowest")
			a += L" -Quality " + Sistema::Ancho(calidad) + L" -TextureReduce " + (calidad == "lowest" ? L"2" : L"1");
		if (si("generacion")) a += L" -Fsr3FrameGen";
		if (si("mostrar_fps")) a += L" -Set KYTY_FPS_HUD=1";
		orden.argumentos = a;
		orden.entorno    = m_entorno;
		return orden;
	}
	// The engine itself: what the port's tests settled on, then the settings.
	orden.programa = CarpetaMotor() / L"kyty_emulator.exe";
	orden.registro = CarpetaMotor() / L"logs" / L"ultimo_arranque.txt";
	orden.entorno.push_back({"DISABLE_VK_LAYER_reshade_1", "1"});
	a = L"--game " + Comillas(m_carpeta_juego);
	a += L" --screen-width " + std::to_wstring(ancho) + L" --screen-height " + std::to_wstring(alto);
	if (si("pantalla_completa")) a += L" --fullscreen";
	a += L" --present-mode " + std::wstring(si("vsync") ? L"Fifo" : L"Immediate");
	a += L" --console-language " + Sistema::Ancho(Valor("idioma"));
	a += L" --redzone --amd-cpu --shader-validation false --shader-optimization-type Performance --shader-precompile true"
	     L" --readback-linear-images true --printf-direction Silent";
	a += L" --frame-generation " + std::wstring(si("generacion") ? L"true" : L"false");
	a += L" --fsr-upscaling " + std::wstring(si("fsr") ? L"true" : L"false");
	if (m_perfil == "fc27") {
		// Told 1080p it draws for 1080p; told 2160p, up to 2656x1496. Its DCC clears on the GPU: the CPU path drained
		// the GPU ~4 times a frame (22 -> 30 fps in a 2160p match), and its menus draw right since 0.5.7's dispatch
		// barriers.
		a += L" --reported-resolution " + Sistema::Ancho(Valor("interna"));
		a += L" --relaxed-readback true";
	} else if (m_perfil == "astro_playroom") {
		// Its threads read back what the GPU writes every frame; relaxed readbacks and pipelines compiled aside.
		a += L" --relaxed-readback true --async-pipelines true --gpu-frames-ahead 2 --label-flush-interval-us 5000";
		if (const auto parche = Valor("parche"); !parche.empty())
			a += L" --game-patch " + Comillas(CarpetaMotor() / L"_Patches" / Sistema::Ancho(parche));
	}
	orden.entorno.insert(orden.entorno.end(), m_entorno.begin(), m_entorno.end());
	if (si("mostrar_fps")) orden.entorno.push_back({"KYTY_FPS_HUD", "1"});
	// FC 27: a frame ready between two vblanks flips then (frames took 17 or 33 ms, never in between: 37 fps steady
	// in a 2160p match with it).
	if (m_perfil == "fc27") orden.entorno.push_back({"KYTY_FLIP_IMMEDIATE", "1"});
	orden.argumentos               = a;
	orden.solo_nucleos_rendimiento = si("nucleos");
	return orden;
}

Juego::Orden Juego::OrdenPrecompilar() const {
	Orden orden;
	orden.programa   = PowerShell();
	orden.argumentos = L"-NoProfile -ExecutionPolicy Bypass -File " + Comillas(CarpetaMotor() / L"precompile-windows.ps1") +
	                   L" -Game " + Comillas(m_carpeta_juego);
	orden.registro   = CarpetaMotor() / L"logs" / L"precompilar.txt";
	return orden;
}

bool Juego::ShadersPrecompilados() const {
	if (!m_info.valido) return false;
	std::error_code error;
	const auto base = CarpetaMotor() / L"_PipelineCache" / L"static" / Sistema::Ancho(m_info.serie + "_" + m_info.version);
	for (const auto* fin: {L".binaries", L".blobs", L".bin"}) {
		auto ruta = base;
		ruta += fin;
		if (std::filesystem::exists(ruta, error)) return true;
	}
	return false;
}

} // namespace Port
