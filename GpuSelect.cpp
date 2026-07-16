#include "Steam.h"
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")

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

const std::wstring classes[] =
{
	L"exefile\\shell\\",
	L"lnkfile\\shell\\",
	L"InternetShortcut\\shell\\"
};

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

	for (const auto& cls : classes)
	{
		WriteStringValue(
			HKEY_CURRENT_USER,
			(L"Software\\Classes\\" + cls +
				keyName).c_str(),
			L"",
			text
		);

		WriteStringValue(
			HKEY_CURRENT_USER,
			(L"Software\\Classes\\" + cls +
				keyName + L"\\command").c_str(),
			L"",
			command
		);
	}
}

void DeleteMenuEntry(const std::wstring& keyName)
{
	for (const auto& cls : classes)
	{
		RegDeleteTreeW(
			HKEY_CURRENT_USER,
			(L"Software\\Classes\\" + cls + keyName).c_str()
		);
	}
}

void Install()
{
	fs::path destination(GetInstallPath());

	if (fs::exists(destination)) {
		DeleteFileW(destination.c_str());

		DeleteMenuEntry(L"GpuAuto");
		DeleteMenuEntry(L"GpuPowerSaving");
		DeleteMenuEntry(L"GpuHighPerformance");

		MessageBoxW(
			nullptr,
			L"GPU context menu uninstalled.",
			L"GpuSelect",
			MB_OK
		);
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

	CreateMenuEntry(
		L"GpuAuto",
		L"GPU: Auto",
		0
	);
	CreateMenuEntry(
		L"GpuPowerSaving",
		L"GPU: Power Saving",
		1
	);
	CreateMenuEntry(
		L"GpuHighPerformance",
		L"GPU: High Performance",
		2
	);

	MessageBoxW(
		nullptr,
		L"GPU context menu installed.",
		L"GpuSelect",
		MB_OK
	);
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
	if (app.size() >= 4 &&
		_wcsicmp(
			app.substr(app.size() - 4).c_str(),
			L".url") == 0)
	{
		app = ResolveSteamShortcut(app);
	}

	MessageBoxW(
		nullptr,
		(L"Resolved as:" + app).c_str(),
		L"GpuSelect",
		MB_OK
	);
	if (app.empty() || !fs::exists(app)) { return; }

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