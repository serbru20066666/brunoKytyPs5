// brunoPortLauncher: the program of a PC port of a PS5 game. Named after its game in each package, it shows the game's
// page (its picture, main settings, Play) and starts the engine in the package's hidden folder with them.
#include "fuente_poppins.h"
#include "juego.h"
#include "sistema.h"
#include "ui.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <string_view>

int main(int argc, char** argv) {
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	Port::Juego juego;
	if (!juego.Cargar(Port::Sistema::CarpetaPrograma())) {
		MessageBoxW(nullptr, L"Falta juego.ini junto al programa: el paquete está incompleto.", L"Port", MB_ICONERROR);
		return 1;
	}
	SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "windows");
	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) return 1;

	// Everything is sized for a 4K display (as the PS4 ports); smaller displays get a smaller page.
	const auto [pantalla_w, pantalla_h] = Port::Sistema::ResolucionPantalla();
	const float s = std::clamp(static_cast<float>(pantalla_h) / 2160.0f, 0.62f, 1.0f);
	const int ancho = static_cast<int>(std::lround(1440.0f * s));
	SDL_Window* ventana = SDL_CreateWindow(juego.Titulo().c_str(), ancho, static_cast<int>(900.0f * s), SDL_WINDOW_HIDDEN);
	if (ventana == nullptr) return 1;
	SDL_Renderer* renderer = SDL_CreateRenderer(ventana, nullptr);
	SDL_SetRenderVSync(renderer, 1);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.IniFilename = nullptr;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad | ImGuiConfigFlags_NavEnableKeyboard;
	ImFontConfig fuente;
	fuente.FontDataOwnedByAtlas = false;
	io.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char*>(kPoppins), static_cast<int>(kPoppinsSize), 0.0f, &fuente);
	ImGui::GetStyle().FontSizeBase = 30.0f * s;
	ImGui_ImplSDL3_InitForSDLRenderer(ventana, renderer);
	ImGui_ImplSDLRenderer3_Init(renderer);

	Port::Pantalla pantalla(juego, ventana, renderer);
	bool mostrada = false;
	// --jugar: straight into the game with the saved settings (the page shows only if it cannot start).
	for (int i = 1; i < argc; i++)
		if (std::string_view(argv[i]) == "--jugar" && juego.Info().valido) {
			pantalla.JugarAhora();
			mostrada = pantalla.Jugando();
		}
	int  alto_actual = 0;
	while (!pantalla.Cerrar()) {
		SDL_Event evento;
		const bool jugando = pantalla.Jugando();
		// While the game runs the window is hidden: only its end is watched.
		if (jugando ? SDL_WaitEventTimeout(&evento, 500) : SDL_PollEvent(&evento)) {
			do {
				ImGui_ImplSDL3_ProcessEvent(&evento);
				if (evento.type == SDL_EVENT_QUIT || evento.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
					if (!jugando) goto fin;
				}
			} while (SDL_PollEvent(&evento));
		}
		if (jugando) {
			if (pantalla.Vigilar()) break;
			continue;
		}
		ImGui_ImplSDLRenderer3_NewFrame();
		ImGui_ImplSDL3_NewFrame();
		ImGui::NewFrame();
		const float alto = pantalla.Dibujar(s);
		ImGui::Render();
		SDL_SetRenderDrawColor(renderer, 23, 25, 31, 255);
		SDL_RenderClear(renderer);
		ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
		SDL_RenderPresent(renderer);

		// The window takes the height the page needs, within the screen.
		const int deseado = std::min(static_cast<int>(std::ceil(alto)), pantalla_h - static_cast<int>(120 * s));
		if (std::abs(deseado - alto_actual) > 2) {
			SDL_SetWindowSize(ventana, ancho, deseado);
			if (!mostrada) SDL_SetWindowPosition(ventana, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
			alto_actual = deseado;
		}
		if (!mostrada && !pantalla.Jugando()) {
			SDL_ShowWindow(ventana);
			mostrada = true;
		}
	}
fin:
	ImGui_ImplSDLRenderer3_Shutdown();
	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(ventana);
	SDL_Quit();
	return 0;
}
