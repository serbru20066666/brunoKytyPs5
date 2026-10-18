// What the launcher asks of Windows: its own folder, text conversions, starting the engine (hidden, with its
// environment, its output to a file, inside a job that ends with the launcher), the performance cores.
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Port::Sistema {

std::filesystem::path CarpetaPrograma();
std::wstring          Ancho(const std::string& utf8);
std::string           Utf8(const std::wstring& texto);
std::string           LeerTexto(const std::filesystem::path& archivo);

// A started process. Kept in a job object: closing the launcher (or Terminar) ends it and what it started.
class Proceso {
public:
	Proceso() = default;
	Proceso(const Proceso&)            = delete;
	Proceso& operator=(const Proceso&) = delete;
	~Proceso();

	// `registro`: stdout and stderr to that file (none: nowhere). `nucleos`: the affinity mask (0: all).
	bool Iniciar(const std::filesystem::path& programa, const std::wstring& argumentos, const std::filesystem::path& carpeta,
	             const std::vector<std::pair<std::string, std::string>>& entorno, const std::filesystem::path& registro,
	             uint64_t nucleos, bool prioridad_alta);
	bool Activo() const;
	// The exit code, once it ended.
	std::optional<uint32_t> Codigo() const;
	void Terminar();
	// The process outlives the launcher (the game, once started: the launcher closes when it ends anyway).
	void Soltar();

private:
	void* m_proceso = nullptr;
	void* m_trabajo = nullptr;
};

// The logical processors of the cores that run fastest (the P-cores of an Intel hybrid CPU), as an affinity mask;
// 0 when every core is the same.
uint64_t NucleosRendimiento();
// The largest display mode of the display the point is on.
std::pair<int, int> ResolucionPantalla();
void AbrirEnExplorador(const std::filesystem::path& carpeta);
// Keeps the newest `conservar` files of the folder whose name ends in `fin` (the engine's run logs).
void PodarRegistros(const std::filesystem::path& carpeta, const std::wstring& fin, size_t conservar);

} // namespace Port::Sistema
