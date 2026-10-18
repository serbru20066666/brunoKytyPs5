#include "sistema.h"

#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <fstream>
#include <sstream>

namespace Port::Sistema {

std::filesystem::path CarpetaPrograma() {
	std::wstring ruta(MAX_PATH, L'\0');
	for (;;) {
		const auto n = GetModuleFileNameW(nullptr, ruta.data(), static_cast<DWORD>(ruta.size()));
		if (n < ruta.size()) {
			ruta.resize(n);
			break;
		}
		ruta.resize(ruta.size() * 2);
	}
	return std::filesystem::path(ruta).parent_path();
}

std::wstring Ancho(const std::string& utf8) {
	if (utf8.empty()) return {};
	const int n = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
	std::wstring texto(n, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), texto.data(), n);
	return texto;
}

std::string Utf8(const std::wstring& texto) {
	if (texto.empty()) return {};
	const int n = WideCharToMultiByte(CP_UTF8, 0, texto.data(), static_cast<int>(texto.size()), nullptr, 0, nullptr, nullptr);
	std::string utf8(n, '\0');
	WideCharToMultiByte(CP_UTF8, 0, texto.data(), static_cast<int>(texto.size()), utf8.data(), n, nullptr, nullptr);
	return utf8;
}

std::string LeerTexto(const std::filesystem::path& archivo) {
	std::ifstream entrada(archivo, std::ios::binary);
	if (!entrada) return {};
	std::ostringstream texto;
	texto << entrada.rdbuf();
	auto contenido = texto.str();
	if (contenido.size() >= 3 && static_cast<unsigned char>(contenido[0]) == 0xef &&
	    static_cast<unsigned char>(contenido[1]) == 0xbb && static_cast<unsigned char>(contenido[2]) == 0xbf)
		contenido.erase(0, 3);
	return contenido;
}

Proceso::~Proceso() {
	if (m_proceso != nullptr) CloseHandle(m_proceso);
	if (m_trabajo != nullptr) CloseHandle(m_trabajo); // (kill-on-close: what is still running ends here)
}

bool Proceso::Iniciar(const std::filesystem::path& programa, const std::wstring& argumentos, const std::filesystem::path& carpeta,
                      const std::vector<std::pair<std::string, std::string>>& entorno, const std::filesystem::path& registro,
                      uint64_t nucleos, bool prioridad_alta) {
	// The environment: the launcher's own, with the additions (a name set again replaces it).
	std::vector<std::wstring> variables;
	if (auto* bloque = GetEnvironmentStringsW()) {
		for (const wchar_t* at = bloque; *at != L'\0'; at += wcslen(at) + 1) variables.emplace_back(at);
		FreeEnvironmentStringsW(bloque);
	}
	for (const auto& [nombre, valor]: entorno) {
		const auto prefijo = Ancho(nombre) + L"=";
		std::erase_if(variables, [&](const std::wstring& v) { return _wcsnicmp(v.c_str(), prefijo.c_str(), prefijo.size()) == 0; });
		if (!valor.empty()) variables.push_back(prefijo + Ancho(valor));
	}
	std::sort(variables.begin(), variables.end(), [](const std::wstring& a, const std::wstring& b) { return _wcsicmp(a.c_str(), b.c_str()) < 0; });
	std::wstring bloque;
	for (const auto& v: variables) bloque += v + L'\0';
	bloque += L'\0';

	SECURITY_ATTRIBUTES heredable {sizeof(heredable), nullptr, TRUE};
	HANDLE salida = INVALID_HANDLE_VALUE;
	if (!registro.empty()) {
		std::error_code error;
		std::filesystem::create_directories(registro.parent_path(), error);
		salida = CreateFileW(registro.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &heredable, CREATE_ALWAYS,
		                     FILE_ATTRIBUTE_NORMAL, nullptr);
	} else {
		salida = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &heredable, OPEN_EXISTING, 0, nullptr);
	}
	STARTUPINFOW inicio {};
	inicio.cb          = sizeof(inicio);
	inicio.dwFlags     = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	inicio.wShowWindow = SW_SHOWNORMAL;
	inicio.hStdInput   = nullptr;
	inicio.hStdOutput  = salida;
	inicio.hStdError   = salida;
	PROCESS_INFORMATION info {};
	std::wstring linea = L"\"" + programa.wstring() + L"\" " + argumentos;
	const BOOL ok = CreateProcessW(programa.c_str(), linea.data(), nullptr, nullptr, TRUE,
	                               CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT | CREATE_SUSPENDED, bloque.data(),
	                               carpeta.c_str(), &inicio, &info);
	if (salida != INVALID_HANDLE_VALUE) CloseHandle(salida);
	if (!ok) return false;

	m_trabajo = CreateJobObjectW(nullptr, nullptr);
	if (m_trabajo != nullptr) {
		JOBOBJECT_EXTENDED_LIMIT_INFORMATION limites {};
		limites.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
		SetInformationJobObject(m_trabajo, JobObjectExtendedLimitInformation, &limites, sizeof(limites));
		AssignProcessToJobObject(m_trabajo, info.hProcess);
	}
	if (nucleos != 0) SetProcessAffinityMask(info.hProcess, static_cast<DWORD_PTR>(nucleos));
	if (prioridad_alta) SetPriorityClass(info.hProcess, HIGH_PRIORITY_CLASS);
	ResumeThread(info.hThread);
	CloseHandle(info.hThread);
	m_proceso = info.hProcess;
	return true;
}

bool Proceso::Activo() const {
	return m_proceso != nullptr && WaitForSingleObject(m_proceso, 0) == WAIT_TIMEOUT;
}

std::optional<uint32_t> Proceso::Codigo() const {
	DWORD codigo = 0;
	if (m_proceso == nullptr || Activo() || !GetExitCodeProcess(m_proceso, &codigo)) return std::nullopt;
	return codigo;
}

void Proceso::Terminar() {
	if (m_trabajo != nullptr) TerminateJobObject(m_trabajo, 1);
	else if (m_proceso != nullptr) TerminateProcess(m_proceso, 1);
}

void Proceso::Soltar() {
	if (m_trabajo == nullptr) return;
	JOBOBJECT_EXTENDED_LIMIT_INFORMATION limites {};
	SetInformationJobObject(m_trabajo, JobObjectExtendedLimitInformation, &limites, sizeof(limites));
}

uint64_t NucleosRendimiento() {
	DWORD tam = 0;
	GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &tam);
	std::vector<uint8_t> datos(tam);
	if (tam == 0 || !GetLogicalProcessorInformationEx(RelationProcessorCore,
	                                                 reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(datos.data()), &tam))
		return 0;
	struct Nucleo {
		uint8_t  clase;
		uint64_t mascara;
	};
	std::vector<Nucleo> nucleos;
	for (size_t at = 0; at < tam;) {
		const auto* info = reinterpret_cast<const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(datos.data() + at);
		if (info->Relationship == RelationProcessorCore && info->Processor.GroupCount >= 1 && info->Processor.GroupMask[0].Group == 0)
			nucleos.push_back({info->Processor.EfficiencyClass, static_cast<uint64_t>(info->Processor.GroupMask[0].Mask)});
		at += info->Size;
	}
	uint8_t mas_rapida = 0;
	for (const auto& n: nucleos) mas_rapida = std::max(mas_rapida, n.clase);
	uint64_t mascara = 0, todos = 0;
	for (const auto& n: nucleos) {
		todos |= n.mascara;
		if (n.clase == mas_rapida) mascara |= n.mascara;
	}
	return mascara == todos ? 0 : mascara;
}

std::pair<int, int> ResolucionPantalla() {
	DEVMODEW modo {};
	modo.dmSize = sizeof(modo);
	if (EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &modo)) return {static_cast<int>(modo.dmPelsWidth), static_cast<int>(modo.dmPelsHeight)};
	return {1920, 1080};
}

void AbrirEnExplorador(const std::filesystem::path& carpeta) {
	ShellExecuteW(nullptr, L"open", carpeta.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void PodarRegistros(const std::filesystem::path& carpeta, const std::wstring& fin, size_t conservar) {
	std::error_code error;
	std::vector<std::filesystem::directory_entry> registros;
	for (const auto& entrada: std::filesystem::directory_iterator(carpeta, error)) {
		const auto nombre = entrada.path().filename().wstring();
		if (entrada.is_regular_file(error) && nombre.size() >= fin.size() &&
		    nombre.compare(nombre.size() - fin.size(), fin.size(), fin) == 0)
			registros.push_back(entrada);
	}
	if (registros.size() <= conservar) return;
	std::sort(registros.begin(), registros.end(),
	          [](const auto& a, const auto& b) { return a.last_write_time() > b.last_write_time(); });
	for (size_t i = conservar; i < registros.size(); i++) std::filesystem::remove(registros[i].path(), error);
}

} // namespace Port::Sistema
