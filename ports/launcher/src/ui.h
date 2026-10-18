// The launcher's page: the game's banner (its picture, its name, Play), its main settings and how it is set up, in the
// dark blue look of the PS4 ports.
#pragma once

#include "juego.h"
#include "sistema.h"

#include <imgui.h>

#include <chrono>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

struct SDL_Renderer;
struct SDL_Texture;
struct SDL_Window;

namespace Port {

class Pantalla {
public:
	Pantalla(Juego& juego, SDL_Window* ventana, SDL_Renderer* renderer);
	~Pantalla();

	// One frame of the page; `escala` sizes everything. Returns the window height the page needs.
	float Dibujar(float escala);
	// The game was started and runs: the window hides until it ends.
	bool Jugando() const { return m_juego_proceso != nullptr; }
	// Called while the game runs: true when the launcher should close (the game ended well, nothing else running).
	bool Vigilar();
	bool Cerrar() const { return m_cerrar; }
	// Starts the game with the saved settings (--jugar: a shortcut that skips the page).
	void JugarAhora() { Jugar(); }
	// A folder chosen in the folder dialog (any thread).
	void CarpetaElegida(const std::filesystem::path& carpeta);

private:
	void Banner(float escala, float ancho);
	void PanelAjustes(float escala);
	void PanelJuego(float escala);
	bool OpcionInterruptor(const Opcion& o, float escala);
	bool OpcionLista(const Opcion& o, float escala);
	void Jugar();
	void Precompilar();
	void LeerProgreso();

	Juego&        m_juego;
	SDL_Window*   m_ventana  = nullptr;
	SDL_Renderer* m_renderer = nullptr;
	SDL_Texture*  m_fondo    = nullptr;
	SDL_Texture*  m_icono    = nullptr;
	float         m_fondo_ancho = 0.0f, m_fondo_alto = 0.0f, m_icono_lado = 0.0f;

	std::unique_ptr<Sistema::Proceso> m_juego_proceso;
	std::unique_ptr<Sistema::Proceso> m_precompilar;
	struct Progreso {
		std::string estado;            // what it is doing, in words
		float       fraccion = -1.0f;  // -1: no number yet
		bool        listo_para_jugar = false;
		bool        terminado = false;
	} m_progreso;
	std::chrono::steady_clock::time_point m_ultima_lectura {};
	std::string m_aviso;  // a message under the banner (an error)
	bool        m_cerrar = false;

	std::mutex                           m_carpeta_mutex;
	std::optional<std::filesystem::path> m_carpeta_elegida;
};

} // namespace Port
