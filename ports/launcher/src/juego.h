// The game a package launches: what juego.ini says it is, its main settings (each port's own), the folder of its
// files and how the engine in the package's hidden folder is started for it.
#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace Port {

// One of a setting's values, as shown and as stored.
struct Valor {
	std::string texto;
	std::string valor;
};

// A main setting: a switch (values "1"/"0") or a list of values. `recomendado` is what "Usar ajustes recomendados"
// sets; `requiere` a file of the engine folder without which the setting is not offered.
struct Opcion {
	std::string         clave;
	std::string         texto;
	std::string         ayuda;
	bool                interruptor = false;
	std::vector<Valor>  valores;
	std::string         defecto;
	std::string         recomendado;
	std::string         requiere;
};

// How a port starts its engine.
enum class Motor {
	DemonsSouls, // the Demon's Souls edition, through its run-windows.ps1 (its ~60 performance switches, shader prefetch)
	Kyty,        // the engine started directly with its command line (EA SPORTS FC 27, ASTRO's PLAYROOM)
};

// What juego.ini and the folder of the game's files say.
struct InfoJuego {
	bool        valido = false;    // a folder with eboot.bin
	std::string serie;             // sce_sys/param.json titleId
	std::string version;           // contentVersion
	bool        serie_conocida = false;
};

class Juego {
public:
	// Reads juego.ini next to the program (`carpeta`), then the saved settings (ajustes.ini).
	bool Cargar(const std::filesystem::path& carpeta);

	const std::string& Titulo() const { return m_titulo; }
	const std::string& Plataforma() const { return m_plataforma; }
	const std::filesystem::path& Carpeta() const { return m_carpeta; }
	std::filesystem::path CarpetaMotor() const { return m_carpeta / m_motor_carpeta; }
	Motor TipoMotor() const { return m_motor; }
	bool PuedePrecompilar() const { return m_motor == Motor::DemonsSouls; }

	// The settings offered (those whose engine file is there).
	const std::vector<Opcion>& Opciones() const { return m_opciones; }
	std::string Valor(const std::string& clave) const;
	void Fijar(const std::string& clave, const std::string& valor);
	void UsarRecomendados();
	bool SonLosRecomendados() const;
	bool CambiosSinGuardar() const { return m_sucio; }
	void Guardar();

	// The folder of the game's files: the saved one, else the first of juego.ini's places that has the game.
	const std::filesystem::path& CarpetaJuego() const { return m_carpeta_juego; }
	void FijarCarpetaJuego(const std::filesystem::path& carpeta);
	const InfoJuego& Info() const { return m_info; }

	// The engine's command: the program, its arguments, the environment it adds. `pantalla` is the display's size.
	struct Orden {
		std::filesystem::path                     programa;
		std::wstring                              argumentos;
		std::vector<std::pair<std::string, std::string>> entorno;
		bool                                      solo_nucleos_rendimiento = false;
		std::filesystem::path                     registro; // where its output goes (empty: the engine's own)
	};
	Orden OrdenJugar(int pantalla_ancho, int pantalla_alto) const;
	Orden OrdenPrecompilar() const;
	// The precompiled shader store of this version of the game is there (Demon's Souls).
	bool ShadersPrecompilados() const;

private:
	void LeerInfo();
	void BuscarCarpetaJuego();

	std::filesystem::path              m_carpeta;
	std::string                        m_titulo, m_plataforma = "PlayStation 5", m_perfil;
	std::vector<std::string>           m_series, m_buscar;
	// juego.ini's entorno=NAME=VALUE;...: variables for the engine that only this package sets.
	std::vector<std::pair<std::string, std::string>> m_entorno;
	std::filesystem::path              m_motor_carpeta = "motor";
	Motor                              m_motor = Motor::Kyty;
	std::vector<Opcion>                m_opciones;
	std::map<std::string, std::string> m_valores;
	bool                               m_sucio = false;
	std::filesystem::path              m_carpeta_juego;
	InfoJuego                          m_info;
};

} // namespace Port
