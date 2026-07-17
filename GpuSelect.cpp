#include <utility>
#include "Steam.h"
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")

const std::wstring context_path = L"Software\\Classes\\";
const std::wstring ctx_classes[] =
{
	L"exefile\\shell\\",
	L"lnkfile\\shell\\",
	L"InternetShortcut\\shell\\"
};
using MapMode = std::pair<std::wstring, std::wstring>;
const MapMode gpu_modes[] =
{
	{L"GpuAuto", L"Auto"},
	{L"GpuPowerSaving", L"Integrated"},
	{L"GpuHighPerformance", L"Dedicated"}
};
constexpr size_t n_modes = sizeof(gpu_modes) / sizeof(MapMode);

std::wstring GetExePath()
{
	wchar_t buffer[MAX_PATH];
	GetModuleFileNameW(nullptr, buffer, MAX_PATH);
	return buffer;
}

std::wstring GetInstallPath()
{
	wchar_t buffer[MAX_PATH];

	SHGetFolderPathW(
		nullptr,
		CSIDL_LOCAL_APPDATA,
		nullptr,
		0,
		buffer
	);

	return std::wstring(buffer) +
		L"\\GpuSelect\\GpuSelect.exe";
}


bool IsInstalledCopy()
{
	return _wcsicmp(
		GetExePath().c_str(),
		GetInstallPath().c_str()
	) == 0;
}


void WriteStringValue(
	HKEY root,
	const std::wstring& path,
	const std::wstring& name,
	const std::wstring& value)
{
	HKEY key;

	if (RegCreateKeyExW(
		root,
		path.c_str(),
		0,
		nullptr,
		0,
		KEY_WRITE,
		nullptr,
		&key,
		nullptr) == ERROR_SUCCESS)
	{
		RegSetValueExW(
			key,
			name.empty() ? nullptr : name.c_str(),
			0,
			REG_SZ,
			(BYTE*)value.c_str(),
			DWORD((value.size() + 1) * sizeof(wchar_t))
		);

		RegCloseKey(key);
	}
}

void CreateMenuEntry(
	const std::wstring& keyName,
	const std::wstring& text,
	int gpuMode)
{
	std::wstring exe = GetInstallPath();

	std::wstring command =
		L"\"" + exe +
		L"\" \"%1\" " +
		std::to_wstring(gpuMode);

	for (const auto& cls : ctx_classes)
	{
		WriteStringValue(
			HKEY_CURRENT_USER,
			(context_path + cls +
				keyName).c_str(),
			L"",
			text
		);

		WriteStringValue(
			HKEY_CURRENT_USER,
			(context_path + cls +
				keyName + L"\\command").c_str(),
			L"",
			command
		);
	}
}

void CreateGpuSubmenu()
{
	std::wstring exe = GetInstallPath();

	auto cmd = [&](int mode)
		{
			return L"\"" + exe + L"\" \"%1\" " + std::to_wstring(mode);
		};

	for (const auto& cls : ctx_classes)
	{
		std::wstring base =
			context_path + cls + L"GpuMode";

		WriteStringValue(
			HKEY_CURRENT_USER,
			base,
			L"MUIVerb",
			L"Select GPU");

		WriteStringValue(
			HKEY_CURRENT_USER,
			base,
			L"SubCommands",
			L"");
		for (size_t id = 0; id < n_modes; id++)
		{
			WriteStringValue(
				HKEY_CURRENT_USER,
				base + L"\\shell\\" + gpu_modes[id].first,
				L"",
				gpu_modes[id].second);

			WriteStringValue(
				HKEY_CURRENT_USER,
				base + L"\\shell\\" + gpu_modes[id].first + L"\\command",
				L"",
				cmd(id));
		}

	}
}

void DeleteMenuEntry(const std::wstring& keyName)
{
	for (const auto& cls : ctx_classes)
	{
		RegDeleteTreeW(
			HKEY_CURRENT_USER,
			(context_path + cls + keyName).c_str()
		);
	}
}

void Install()
{
	fs::path destination(GetInstallPath());

	if (fs::exists(destination)) {
		DeleteFileW(destination.c_str());
		DeleteMenuEntry(L"GpuMode");
		for (size_t id = 0; id < n_modes; id++) {
			DeleteMenuEntry(gpu_modes[id].first);
		}
		LOG(L"GPU context menu uninstalled.");
		return;
	}

	fs::create_directories(
		destination.parent_path()
	);

	CopyFileW(
		GetExePath().c_str(),
		destination.c_str(),
		FALSE
	);
	CreateGpuSubmenu();
	/*
	for (size_t id = 0; id < n_modes; id++) {
		CreateMenuEntry(
			gpu_modes[id].first,
			gpu_modes[id].second,
			id
		);
	}
	*/

	LOG(L"GPU context menu installed.");
}

std::wstring ResolveShortcut(
	const std::wstring& path)
{
	IShellLinkW* link = nullptr;
	IPersistFile* file = nullptr;

	std::wstring result = L"";


	if (FAILED(CoCreateInstance(
		CLSID_ShellLink,
		nullptr,
		CLSCTX_INPROC_SERVER,
		IID_IShellLinkW,
		(void**)&link)))
	{
		return result;
	}


	if (SUCCEEDED(link->QueryInterface(
		IID_IPersistFile,
		(void**)&file)))
	{
		if (SUCCEEDED(file->Load(
			path.c_str(),
			STGM_READ)))
		{
			wchar_t target[MAX_PATH];

			if (SUCCEEDED(link->GetPath(
				target,
				MAX_PATH,
				nullptr,
				0)))
			{
				result = target;
			}
		}

		file->Release();
	}


	link->Release();

	return result;
}


void SetGpuPreference(
	std::wstring app,
	int mode)
{
	if (app.size() >= 4 &&
		_wcsicmp(
			app.substr(app.size() - 4).c_str(),
			L".lnk") == 0)
	{
		app = ResolveShortcut(app);
	}
	else if (app.size() >= 4 &&
		_wcsicmp(
			app.substr(app.size() - 4).c_str(),
			L".url") == 0)
	{
		app = ResolveSteamShortcut(app);
	}
	if (app.empty() || !fs::exists(app)) {
		LOG((L"Couldn't locate the app!"));
		return;
	}

	LOG((L"Set GPU of: " + fs::path(app).filename().wstring() + L"\nAs: " + gpu_modes[mode].second).c_str());

	HKEY key;

	if (RegCreateKeyExW(
		HKEY_CURRENT_USER,
		L"Software\\Microsoft\\DirectX\\UserGpuPreferences",
		0,
		nullptr,
		0,
		KEY_WRITE,
		nullptr,
		&key,
		nullptr) != ERROR_SUCCESS)
	{
		return;
	}


	std::wstring value =
		L"GpuPreference=" +
		std::to_wstring(mode) +
		L";";


	RegSetValueExW(
		key,
		app.c_str(),
		0,
		REG_SZ,
		(BYTE*)value.c_str(),
		DWORD((value.size() + 1) *
			sizeof(wchar_t))
	);


	RegCloseKey(key);
}


int WINAPI wWinMain(
	HINSTANCE,
	HINSTANCE,
	PWSTR,
	int)
{
	CoInitialize(nullptr);


	int argc;
	LPWSTR* argv = CommandLineToArgvW(
		GetCommandLineW(),
		&argc
	);


	if (!IsInstalledCopy() && argc == 1)
	{
		Install();
	}
	else if (argc >= 3)
	{
		SetGpuPreference(
			argv[1],
			_wtoi(argv[2])
		);
	}


	LocalFree(argv);

	CoUninitialize();

	return 0;
}