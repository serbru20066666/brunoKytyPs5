#include "ui.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_dialog.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#include <stb_image.h>

#include <algorithm>
#include <cmath>
#include <regex>

namespace Port {
namespace {

// The PS4 ports' look: one bright blue for what is selected or is the main action, rounded slate cards on a dark slate.
constexpr ImVec4 kAcento {0.10f, 0.55f, 1.00f, 1.00f};
constexpr ImVec4 kAcentoEncima {0.27f, 0.65f, 1.00f, 1.00f};
constexpr ImVec4 kFondo {0.090f, 0.098f, 0.122f, 1.00f};
constexpr ImVec4 kTarjeta {0.133f, 0.145f, 0.180f, 1.00f};
constexpr ImVec4 kChip {0.180f, 0.196f, 0.240f, 1.00f};
constexpr ImVec4 kTenue {0.58f, 0.61f, 0.68f, 1.00f};
constexpr ImVec4 kAviso {1.00f, 0.72f, 0.30f, 1.00f};
constexpr ImVec4 kBien {0.45f, 0.85f, 0.55f, 1.00f};

void EstiloPush(float s) {
	const auto color = [](ImGuiCol id, const ImVec4& v) { ImGui::PushStyleColor(id, v); };
	color(ImGuiCol_WindowBg, kFondo);
	color(ImGuiCol_ChildBg, kTarjeta);
	color(ImGuiCol_PopupBg, ImVec4(0.150f, 0.163f, 0.200f, 1.0f));
	color(ImGuiCol_Text, ImVec4(0.95f, 0.95f, 0.96f, 1.0f));
	color(ImGuiCol_TextDisabled, kTenue);
	color(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
	color(ImGuiCol_Button, kChip);
	color(ImGuiCol_ButtonHovered, ImVec4(0.225f, 0.245f, 0.300f, 1.0f));
	color(ImGuiCol_ButtonActive, ImVec4(0.270f, 0.295f, 0.360f, 1.0f));
	color(ImGuiCol_FrameBg, kChip);
	color(ImGuiCol_FrameBgHovered, ImVec4(0.215f, 0.235f, 0.285f, 1.0f));
	color(ImGuiCol_FrameBgActive, ImVec4(0.250f, 0.272f, 0.330f, 1.0f));
	color(ImGuiCol_Header, ImVec4(0.215f, 0.235f, 0.285f, 1.0f));
	color(ImGuiCol_HeaderHovered, ImVec4(0.250f, 0.272f, 0.330f, 1.0f));
	color(ImGuiCol_HeaderActive, ImVec4(0.290f, 0.315f, 0.380f, 1.0f));
	color(ImGuiCol_PlotHistogram, kAcento);
	color(ImGuiCol_CheckMark, kAcento);
	color(ImGuiCol_NavCursor, kAcento);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 24.0f * s);
	ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 18.0f * s);
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 100.0f * s); // everything with a frame is a pill
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f * s, 12.0f * s));
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(18.0f * s, 9.0f * s));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(28.0f * s, 24.0f * s));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
}

void EstiloPop() {
	ImGui::PopStyleVar(10);
	ImGui::PopStyleColor(18);
}

// The main action of a screen: a pill filled with the accent colour.
bool BotonAcento(const char* texto, ImVec2 tam = {}) {
	ImGui::PushStyleColor(ImGuiCol_Button, kAcento);
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kAcentoEncima);
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, kAcentoEncima);
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
	const bool pulsado = ImGui::Button(texto, tam);
	ImGui::PopStyleColor(4);
	return pulsado;
}

// A PS5 style switch.
bool Interruptor(const char* id, bool* valor, float s) {
	const float alto = ImGui::GetFrameHeight() * 0.82f;
	const float ancho = alto * 1.9f;
	const ImVec2 at = ImGui::GetCursorScreenPos();
	const bool pulsado = ImGui::InvisibleButton(id, {ancho, ImGui::GetFrameHeight()});
	if (pulsado) *valor = !*valor;
	const float y = at.y + (ImGui::GetFrameHeight() - alto) * 0.5f;
	auto* dibujo = ImGui::GetWindowDrawList();
	const ImU32 pista = ImGui::GetColorU32(*valor ? kAcento : (ImGui::IsItemHovered() ? ImVec4(0.26f, 0.28f, 0.34f, 1) : kChip));
	dibujo->AddRectFilled({at.x, y}, {at.x + ancho, y + alto}, pista, alto);
	const float r = alto * 0.5f - 3.0f * s;
	const float cx = *valor ? at.x + ancho - alto * 0.5f : at.x + alto * 0.5f;
	dibujo->AddCircleFilled({cx, y + alto * 0.5f}, r, IM_COL32_WHITE);
	return pulsado;
}

SDL_Texture* CargarTextura(SDL_Renderer* renderer, const std::filesystem::path& archivo, float& ancho, float& alto) {
	const auto datos = Sistema::LeerTexto(archivo);
	if (datos.empty()) return nullptr;
	int w = 0, h = 0, canales = 0;
	auto* pixeles = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(datos.data()), static_cast<int>(datos.size()), &w, &h, &canales, 4);
	if (pixeles == nullptr) return nullptr;
	SDL_Texture* textura = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, w, h);
	if (textura != nullptr) {
		SDL_UpdateTexture(textura, nullptr, pixeles, w * 4);
		SDL_SetTextureBlendMode(textura, SDL_BLENDMODE_BLEND);
		SDL_SetTextureScaleMode(textura, SDL_SCALEMODE_LINEAR);
	}
	stbi_image_free(pixeles);
	ancho = static_cast<float>(w);
	alto  = static_cast<float>(h);
	return textura;
}

std::filesystem::path PrimeroQueExista(const std::filesystem::path& carpeta, std::initializer_list<const wchar_t*> nombres) {
	std::error_code error;
	for (const auto* n: nombres)
		if (std::filesystem::exists(carpeta / n, error)) return carpeta / n;
	return {};
}

void SDLCALL AlElegirCarpeta(void* datos, const char* const* lista, int) {
	if (lista != nullptr && lista[0] != nullptr)
		static_cast<Pantalla*>(datos)->CarpetaElegida(std::filesystem::path(Sistema::Ancho(lista[0])));
}

} // namespace

Pantalla::Pantalla(Juego& juego, SDL_Window* ventana, SDL_Renderer* renderer) : m_juego(juego), m_ventana(ventana), m_renderer(renderer) {
	const auto carpeta = m_juego.Carpeta();
	if (const auto f = PrimeroQueExista(carpeta, {L"fondo.jpg", L"fondo.png"}); !f.empty())
		m_fondo = CargarTextura(renderer, f, m_fondo_ancho, m_fondo_alto);
	float w = 0, h = 0;
	if (const auto f = PrimeroQueExista(carpeta, {L"icono.png"}); !f.empty()) {
		m_icono      = CargarTextura(renderer, f, w, h);
		m_icono_lado = w;
		// The window's icon too.
		const auto datos = Sistema::LeerTexto(f);
		int iw = 0, ih = 0, c = 0;
		if (auto* px = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(datos.data()), static_cast<int>(datos.size()), &iw, &ih, &c, 4)) {
			if (SDL_Surface* superficie = SDL_CreateSurfaceFrom(iw, ih, SDL_PIXELFORMAT_ABGR8888, px, iw * 4)) {
				SDL_SetWindowIcon(ventana, superficie);
				SDL_DestroySurface(superficie);
			}
			stbi_image_free(px);
		}
	}
}

Pantalla::~Pantalla() {
	if (m_fondo != nullptr) SDL_DestroyTexture(m_fondo);
	if (m_icono != nullptr) SDL_DestroyTexture(m_icono);
}

void Pantalla::CarpetaElegida(const std::filesystem::path& carpeta) {
	std::scoped_lock lock(m_carpeta_mutex);
	m_carpeta_elegida = carpeta;
}

float Pantalla::Dibujar(float s) {
	{
		std::scoped_lock lock(m_carpeta_mutex);
		if (m_carpeta_elegida) {
			m_juego.FijarCarpetaJuego(*m_carpeta_elegida);
			m_carpeta_elegida.reset();
			m_aviso.clear();
		}
	}
	if (m_precompilar) LeerProgreso();

	EstiloPush(s);
	const ImGuiViewport* vista = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(vista->WorkPos);
	ImGui::SetNextWindowSize(vista->WorkSize);
	ImGui::Begin("Juego", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings);
	const float ancho = ImGui::GetContentRegionAvail().x;

	// Top bar: the game's folder.
	const char* etiqueta = "Carpeta del juego";
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(etiqueta).x -
	                     ImGui::GetStyle().FramePadding.x * 2.0f);
	if (ImGui::Button(etiqueta)) {
		const auto actual = Sistema::Utf8(m_juego.CarpetaJuego().wstring());
		SDL_ShowOpenFolderDialog(AlElegirCarpeta, this, m_ventana, actual.empty() ? nullptr : actual.c_str(), false);
	}
	ImGui::Dummy({0, 6.0f * s});

	Banner(s, ancho);

	// The two panels.
	const float hueco = 22.0f * s;
	const float panel = (ancho - hueco) * 0.5f;
	const ImVec2 arriba = ImGui::GetCursorPos();
	const ImGuiChildFlags flags = ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_NavFlattened | ImGuiChildFlags_AutoResizeY;
	ImGui::BeginChild("Ajustes", ImVec2(panel, 0), flags);
	PanelAjustes(s);
	ImGui::EndChild();
	const float alto_ajustes = ImGui::GetItemRectSize().y;
	ImGui::SetCursorPos({arriba.x + panel + hueco, arriba.y});
	ImGui::BeginChild("EsteJuego", ImVec2(panel, 0), flags);
	PanelJuego(s);
	ImGui::EndChild();
	const float alto_juego = ImGui::GetItemRectSize().y;
	ImGui::SetCursorPos({arriba.x, arriba.y + std::max(alto_ajustes, alto_juego) + 14.0f * s});

	// Footer.
	const float derecha = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
	ImGui::TextDisabled("Porteado a PC");
	const char* version = "Programa del juego " LAUNCHER_VERSION;
	ImGui::SameLine(derecha - ImGui::CalcTextSize(version).x);
	ImGui::TextDisabled("%s", version);
	const float alto = ImGui::GetCursorPosY() + ImGui::GetStyle().WindowPadding.y;
	ImGui::End();
	EstiloPop();
	return alto;
}

void Pantalla::Banner(float s, float ancho) {
	auto* dibujo = ImGui::GetWindowDrawList();
	const ImVec2 min = ImGui::GetCursorScreenPos();
	const float redondeo = 26.0f * s;
	// The banner goes behind its contents and grows to hold them: they are drawn first.
	ImDrawListSplitter capas;
	capas.Split(dibujo, 2);
	capas.SetCurrentChannel(dibujo, 1);

	const float izquierda = min.x + 44.0f * s;
	ImGui::SetCursorScreenPos({izquierda, min.y + 36.0f * s});
	ImGui::BeginGroup();
	{
		// The platform, as a small pill.
		const char* insignia = m_juego.Plataforma().c_str();
		ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 0.8f);
		const ImVec2 texto = ImGui::CalcTextSize(insignia);
		const ImVec2 at = ImGui::GetCursorScreenPos();
		const ImVec2 pad {10.0f * s, 4.0f * s};
		dibujo->AddRectFilled(at, {at.x + texto.x + pad.x * 2, at.y + texto.y + pad.y * 2}, ImGui::GetColorU32(kAcento), 100.0f);
		ImGui::SetCursorScreenPos({at.x + pad.x, at.y + pad.y});
		ImGui::TextUnformatted(insignia);
		ImGui::SetCursorScreenPos({at.x, at.y + texto.y + pad.y * 2});
		ImGui::PopFont();
	}
	ImGui::Dummy({0, 10.0f * s});
	ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 2.1f);
	ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ancho * 0.52f);
	ImGui::TextUnformatted(m_juego.Titulo().c_str());
	ImGui::PopTextWrapPos();
	ImGui::PopFont();
	const auto& info = m_juego.Info();
	if (info.valido) {
		ImGui::TextDisabled("%s   versión %s", info.serie.c_str(), info.version.c_str());
		if (!info.serie_conocida) ImGui::TextColored(kAviso, "Esta versión del juego no está probada con este port.");
	} else {
		ImGui::TextColored(kAviso, "Elige la carpeta del juego (la que tiene eboot.bin).");
	}
	ImGui::Dummy({0, 14.0f * s});
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(42.0f * s, 15.0f * s));
	ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 1.15f);
	ImGui::BeginDisabled(!info.valido);
	if (BotonAcento("Jugar")) Jugar();
	ImGui::EndDisabled();
	ImGui::PopFont();
	ImGui::PopStyleVar();
	if (m_juego.PuedePrecompilar() && info.valido) {
		ImGui::SameLine(0, 14.0f * s);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6.0f * s);
		if (m_precompilar) {
			if (ImGui::Button("Detener precompilación")) {
				m_precompilar->Terminar();
				m_precompilar.reset();
				m_progreso = {};
			}
		} else if (ImGui::Button(m_juego.ShadersPrecompilados() ? "Actualizar shaders" : "Precompilar shaders")) {
			Precompilar();
		}
	}
	if (!m_aviso.empty()) {
		ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ancho * 0.55f);
		ImGui::TextColored(kAviso, "%s", m_aviso.c_str());
		ImGui::PopTextWrapPos();
	}
	ImGui::EndGroup();
	const ImVec2 max {min.x + ancho, std::max(min.y + 250.0f * s, ImGui::GetItemRectMax().y + 40.0f * s)};

	capas.SetCurrentChannel(dibujo, 0);
	dibujo->AddRectFilled(min, max, ImGui::GetColorU32(kTarjeta), redondeo);
	if (m_fondo != nullptr && m_fondo_ancho > 0 && m_fondo_alto > 0) {
		// The picture fills the banner, anchored to the right; the left darkens toward the text.
		const ImVec2 tam {max.x - min.x, max.y - min.y};
		const float escala = std::max(tam.x / m_fondo_ancho, tam.y / m_fondo_alto);
		const float u = tam.x / (m_fondo_ancho * escala), v = tam.y / (m_fondo_alto * escala);
		const ImVec2 uv0 {1.0f - u, (1.0f - v) * 0.5f};
		dibujo->AddImageRounded(ImTextureRef(reinterpret_cast<ImTextureID>(m_fondo)), min, max, uv0, {1.0f, uv0.y + v},
		                        IM_COL32_WHITE, redondeo);
		const ImU32 oscuro = ImGui::GetColorU32(ImVec4(kTarjeta.x, kTarjeta.y, kTarjeta.z, 0.93f));
		const ImU32 claro  = ImGui::GetColorU32(ImVec4(kTarjeta.x, kTarjeta.y, kTarjeta.z, 0.0f));
		const float solido = min.x + redondeo * 2.0f;
		dibujo->AddRectFilled(min, {solido, max.y}, oscuro, redondeo, ImDrawFlags_RoundCornersLeft);
		dibujo->AddRectFilledMultiColor({solido, min.y}, {min.x + tam.x * 0.68f, max.y}, oscuro, claro, claro, oscuro);
	}
	capas.Merge(dibujo);
	ImGui::SetCursorScreenPos({min.x, max.y + 22.0f * s});
}

bool Pantalla::OpcionInterruptor(const Opcion& o, float s) {
	bool valor = m_juego.Valor(o.clave) == "1";
	const float derecha = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(o.texto.c_str());
	if (!o.ayuda.empty()) ImGui::SetItemTooltip("%s", o.ayuda.c_str());
	const float alto = ImGui::GetFrameHeight() * 0.82f;
	ImGui::SameLine(derecha - alto * 1.9f);
	ImGui::PushID(o.clave.c_str());
	const bool cambio = Interruptor("##v", &valor, s);
	if (!o.ayuda.empty()) ImGui::SetItemTooltip("%s", o.ayuda.c_str());
	ImGui::PopID();
	if (cambio) m_juego.Fijar(o.clave, valor ? "1" : "0");
	return cambio;
}

bool Pantalla::OpcionLista(const Opcion& o, float s) {
	const auto actual = m_juego.Valor(o.clave);
	std::string texto = actual;
	for (const auto& v: o.valores)
		if (v.valor == actual) texto = v.texto;
	const float derecha = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(o.texto.c_str());
	if (!o.ayuda.empty()) ImGui::SetItemTooltip("%s", o.ayuda.c_str());
	const float ancho = 330.0f * s;
	ImGui::SameLine(derecha - ancho);
	ImGui::SetNextItemWidth(ancho);
	bool cambio = false;
	ImGui::PushID(o.clave.c_str());
	if (ImGui::BeginCombo("##v", texto.c_str())) {
		for (const auto& v: o.valores) {
			if (ImGui::Selectable(v.texto.c_str(), v.valor == actual)) {
				m_juego.Fijar(o.clave, v.valor);
				cambio = true;
			}
		}
		ImGui::EndCombo();
	}
	if (!o.ayuda.empty()) ImGui::SetItemTooltip("%s", o.ayuda.c_str());
	ImGui::PopID();
	return cambio;
}

void Pantalla::PanelAjustes(float s) {
	ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 1.15f);
	ImGui::TextUnformatted("Ajustes principales");
	ImGui::PopFont();
	ImGui::Dummy({0, 2.0f * s});
	for (const auto& o: m_juego.Opciones()) {
		if (o.interruptor) OpcionInterruptor(o, s);
		else OpcionLista(o, s);
	}
	ImGui::Dummy({0, 6.0f * s});
	// Changes are saved with Aplicar, or when the game starts.
	if (m_juego.CambiosSinGuardar()) {
		if (BotonAcento("Aplicar")) m_juego.Guardar();
		ImGui::SameLine(0, 12.0f * s);
		ImGui::AlignTextToFramePadding();
		ImGui::TextDisabled("Cambios sin aplicar");
	} else {
		ImGui::BeginDisabled();
		ImGui::Button("Aplicar");
		ImGui::EndDisabled();
		ImGui::SameLine(0, 12.0f * s);
		ImGui::AlignTextToFramePadding();
		ImGui::TextDisabled("Ajustes aplicados");
	}
	ImGui::BeginDisabled(m_juego.SonLosRecomendados());
	if (ImGui::Button("Usar ajustes recomendados")) m_juego.UsarRecomendados();
	ImGui::EndDisabled();
}

void Pantalla::PanelJuego(float s) {
	ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 1.15f);
	ImGui::TextUnformatted("Este juego");
	ImGui::PopFont();
	ImGui::Dummy({0, 2.0f * s});
	const auto& info = m_juego.Info();
	ImGui::PushTextWrapPos(0.0f);
	ImGui::TextDisabled("Carpeta del juego");
	ImGui::TextUnformatted(info.valido ? Sistema::Utf8(m_juego.CarpetaJuego().wstring()).c_str() : "Sin elegir");
	if (info.valido) {
		ImGui::TextDisabled("Versión");
		ImGui::Text("%s %s", info.serie.c_str(), info.version.c_str());
		ImGui::SameLine();
		if (info.serie_conocida) ImGui::TextColored(kBien, "compatible");
		else ImGui::TextColored(kAviso, "no probada");
	}
	if (m_juego.PuedePrecompilar() && info.valido) {
		ImGui::Dummy({0, 4.0f * s});
		ImGui::TextDisabled("Shaders");
		if (m_precompilar) {
			ImGui::TextUnformatted(m_progreso.estado.c_str());
			if (m_progreso.fraccion >= 0.0f) {
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 3.0f * s));
				ImGui::ProgressBar(m_progreso.fraccion, {-1.0f, 10.0f * s}, "");
				ImGui::PopStyleVar();
			}
			if (m_progreso.listo_para_jugar) ImGui::TextColored(kBien, "Ya se puede jugar: lo que usa el juego está listo y el resto sigue.");
		} else if (m_progreso.terminado) {
			ImGui::TextColored(kBien, "Precompilados: sin tirones por shaders.");
		} else if (m_juego.ShadersPrecompilados()) {
			ImGui::TextColored(kBien, "Precompilados: sin tirones por shaders.");
		} else {
			ImGui::TextColored(kAviso, "Sin precompilar: el juego dará tirones la primera vez que vea cada cosa.");
			ImGui::TextDisabled("Precompílalos una vez (unos 40 minutos; tras actualizar el driver de la tarjeta, otra vez).");
		}
	}
	ImGui::PopTextWrapPos();
	ImGui::Dummy({0, 4.0f * s});
	if (ImGui::Button("Abrir registros")) {
		std::error_code error;
		std::filesystem::create_directories(m_juego.CarpetaMotor() / L"logs", error);
		Sistema::AbrirEnExplorador(m_juego.CarpetaMotor() / L"logs");
	}
}

void Pantalla::Jugar() {
	if (m_juego.CambiosSinGuardar()) m_juego.Guardar();
	m_aviso.clear();
	const auto logs = m_juego.CarpetaMotor() / L"logs";
	Sistema::PodarRegistros(logs, L".out.log", 5);
	Sistema::PodarRegistros(logs, L".err.log", 5);
	const auto [w, h] = Sistema::ResolucionPantalla();
	const auto orden  = m_juego.OrdenJugar(w, h);
	auto proceso = std::make_unique<Sistema::Proceso>();
	const uint64_t nucleos = orden.solo_nucleos_rendimiento ? Sistema::NucleosRendimiento() : 0;
	if (!proceso->Iniciar(orden.programa, orden.argumentos, m_juego.CarpetaMotor(), orden.entorno, orden.registro, nucleos,
	                      orden.solo_nucleos_rendimiento)) {
		m_aviso = "No se pudo iniciar el juego: falta la carpeta motor del paquete o está incompleta.";
		return;
	}
	m_juego_proceso = std::move(proceso);
	SDL_HideWindow(m_ventana);
}

bool Pantalla::Vigilar() {
	if (!m_juego_proceso || m_juego_proceso->Activo()) return false;
	const auto codigo = m_juego_proceso->Codigo().value_or(0);
	m_juego_proceso.reset();
	if (codigo != 0) {
		char texto[200];
		std::snprintf(texto, sizeof(texto), "El juego se cerró con un error (código 0x%08X). Los detalles están en Abrir registros.", codigo);
		m_aviso = texto;
		SDL_ShowWindow(m_ventana);
		SDL_RaiseWindow(m_ventana);
		return false;
	}
	if (m_precompilar) {
		SDL_ShowWindow(m_ventana);
		return false;
	}
	m_cerrar = true;
	return true;
}

void Pantalla::Precompilar() {
	const auto orden = m_juego.OrdenPrecompilar();
	auto proceso = std::make_unique<Sistema::Proceso>();
	if (!proceso->Iniciar(orden.programa, orden.argumentos, m_juego.CarpetaMotor(), orden.entorno, orden.registro, 0, false)) {
		m_aviso = "No se pudo iniciar la precompilación.";
		return;
	}
	m_precompilar = std::move(proceso);
	m_progreso    = {};
	m_progreso.estado = "Preparando la lista de shaders del juego...";
}

void Pantalla::LeerProgreso() {
	const auto ahora = std::chrono::steady_clock::now();
	if (ahora - m_ultima_lectura < std::chrono::milliseconds(700)) return;
	m_ultima_lectura = ahora;
	const auto texto = Sistema::LeerTexto(m_juego.OrdenPrecompilar().registro);
	static const std::regex shards(R"(shards: (\d+) done, (\d+) running, (\d+) waiting)");
	std::smatch m;
	std::string::const_iterator desde = texto.begin();
	std::optional<std::smatch> ultimo;
	while (std::regex_search(desde, texto.end(), m, shards)) {
		ultimo = m;
		desde  = m.suffix().first;
	}
	if (ultimo) {
		const double hechos = std::stod((*ultimo)[1]), activos = std::stod((*ultimo)[2]), faltan = std::stod((*ultimo)[3]);
		const double total  = hechos + activos + faltan;
		m_progreso.fraccion = total > 0 ? static_cast<float>(hechos / total) : 0.0f;
		char linea[96];
		std::snprintf(linea, sizeof(linea), "Precompilando: %.0f%%", m_progreso.fraccion * 100.0);
		m_progreso.estado = linea;
	} else if (texto.find("inputs") != std::string::npos || texto.find("seeds") != std::string::npos) {
		m_progreso.estado = "Preparando la lista de shaders del juego...";
	}
	if (texto.find("ready to play") != std::string::npos) m_progreso.listo_para_jugar = true;
	if (!m_precompilar->Activo()) {
		const bool bien = m_precompilar->Codigo().value_or(1) == 0 && texto.find("done in") != std::string::npos;
		m_precompilar.reset();
		m_progreso           = {};
		m_progreso.terminado = bien;
		if (!bien) m_aviso = "La precompilación se detuvo antes de acabar; al volver a empezar sigue donde quedó.";
		if (!m_juego_proceso && (SDL_GetWindowFlags(m_ventana) & SDL_WINDOW_HIDDEN) != 0) SDL_ShowWindow(m_ventana);
	}
}

} // namespace Port
