#pragma once
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>

#include <filesystem>
#include <string>
#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <cwctype>

#define LOG(MSG) MessageBoxW(\
nullptr,\
(MSG),\
L"GpuSelect",\
MB_OK\
);

namespace fs = std::filesystem;


// Specifically searches for the first word on line in json format
inline bool MatchFirstWord(std::wstring_view line, std::wstring_view word) {
	size_t lsize = line.size();
	size_t msize = word.size();
	size_t i = 0, j = 0;
	while (i < lsize && line[i++] != L'"');
	if (lsize - i < msize) return false;
	while (j < msize) {
		if (i >= lsize || line[i + j] != word[j]) return false;
		j++;
	}
	return line[i + j] == L'"';
}

inline std::wstring_view ReturnSecondWord(std::wstring_view line) {
	size_t lsize = line.size();
	size_t i = 0, quotes = 0;
	while (i < lsize && quotes < 3)
		quotes += line[i++] == L'"';
	if (i >= lsize) return L"";
	return line.substr(i, lsize - i - 1);
}

inline std::wstring GetSteamInstallPath()
{
	HKEY key;
	if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", 0, KEY_READ, &key) != ERROR_SUCCESS)
		return L"";

	wchar_t buffer[MAX_PATH]{};
	DWORD size = sizeof(buffer);
	DWORD type = 0;
	LONG res = RegQueryValueExW(key, L"SteamPath", nullptr, &type, (LPBYTE)buffer, &size);
	RegCloseKey(key);

	if (res != ERROR_SUCCESS) return L"";
	return std::wstring(buffer);
}

inline std::wstring ExtractAppId(const std::wstring& steamUrl)
{
	size_t i = 0;
	size_t size = steamUrl.size();
	while (i < size && !std::iswdigit(steamUrl[i])) i++;
	auto res = steamUrl.substr(i);
	while (std::iswspace(res.back())) res.pop_back();
	return res;
}

inline std::wstring ExtractUrlFromShortcutFile(const std::wstring& path)
{
	std::wifstream in(path);
	if (!in) return path;

	std::wstring line;
	while (std::getline(in, line))
	{
		if (line.rfind(L"URL=", 0) == 0)
			return line.substr(4);
	}
	return path;
}

inline std::wstring FindLibraryPathForApp(
	const std::wstring& vdfPath,
	const std::wstring& appId)
{
	std::wifstream in(vdfPath);
	if (!in) return L"";

	std::wstring line;
	std::wstring currentPath = {};
	bool scan_apps = false;

	while (std::getline(in, line))
	{
		if (scan_apps) {
			if (currentPath.empty()) return L"";
			if (MatchFirstWord(line, appId)) return currentPath;
			if (line.find(L'}') != std::wstring::npos) {
				scan_apps = false;
			}
		}
		else if (MatchFirstWord(line, L"apps")) {
			scan_apps = true;
		}
		else if (MatchFirstWord(line, L"path")) {
			currentPath = ReturnSecondWord(line);
		}

	}
	return L"";
}

// Grep appmanifest_<id>.acf for the "installdir" value.
inline std::wstring FindInstallDir(const std::wstring& manifestPath)
{
	std::wifstream in(manifestPath);
	if (!in) return L"";
	std::wstring line;
	while (std::getline(in, line)) {
		if (MatchFirstWord(line, L"installdir")) {
			return std::wstring(ReturnSecondWord(line));
		}
	}
	return L"";

}

inline std::wstring UnescapeBackslashes(const std::wstring& in)
{
	std::wstring out;
	out.reserve(in.size());
	for (size_t i = 0; i < in.size(); ++i)
	{
		if (in[i] == L'\\' && i + 1 < in.size() && in[i + 1] == L'\\')
		{
			out += L'\\';
			++i;
		}
		else
		{
			out += in[i];
		}
	}
	return out;
}

inline bool IsLikelyJunkExe(const std::wstring& filename)
{
	static const std::vector<std::wstring> junkNames = {
		L"unins", L"vcredist", L"dxsetup", L"dotnetfx",
		L"crashpad", L"crashhandler", L"crash_reporter",
		L"redist", L"setup", L"install", L"activation",
		L"battleye", L"easyanticheat", L"eossdk"
	};

	std::wstring lower;
	lower.reserve(filename.size());
	for (wchar_t c : filename) lower += std::towlower(c);

	for (const auto& junk : junkNames)
		if (lower.find(junk) != std::wstring::npos)
			return true;

	return false;
}

inline std::wstring FindExecutableTopLevel(const std::wstring& installFolder, const std::wstring& installDirName)
{
	if (!fs::exists(installFolder)) return L"";

	std::wstring bestMatch;
	std::wstring largestExe;
	uintmax_t largestSize = 0;

	std::error_code ec;
	for (auto& entry : fs::directory_iterator(installFolder, fs::directory_options::skip_permission_denied, ec))
	{
		if (!entry.is_regular_file(ec)) continue;

		const auto& p = entry.path();
		if (p.extension() != L".exe") continue;

		std::wstring stem = p.stem().wstring();
		std::wstring filename = p.filename().wstring();

		if (IsLikelyJunkExe(filename)) continue;

		if (bestMatch.empty() && _wcsicmp(stem.c_str(), installDirName.c_str()) == 0)
			bestMatch = p.wstring();

		uintmax_t size = fs::file_size(p, ec);
		if (!ec && size > largestSize)
		{
			largestSize = size;
			largestExe = p.wstring();
		}
	}

	return !bestMatch.empty() ? bestMatch : largestExe;
}

inline std::wstring ResolveSteamShortcut(const std::wstring& path)
{
	std::wstring result = L"";

	auto appId = ExtractAppId(ExtractUrlFromShortcutFile(path));
	//LOG(appId.c_str());
	if (appId.empty()) return result;

	auto steamRoot = GetSteamInstallPath();
	if (steamRoot.empty()) return result;

	auto libPath = FindLibraryPathForApp(steamRoot + L"\\steamapps\\libraryfolders.vdf", appId);
	if (libPath.empty()) return result;
	libPath = UnescapeBackslashes(libPath);

	std::wstring manifestPath = libPath + L"\\steamapps\\appmanifest_" + appId + L".acf";
	auto installDir = FindInstallDir(manifestPath);
	if (installDir.empty()) return result;
	installDir = UnescapeBackslashes(installDir);

	std::wstring gameFolder = libPath + L"\\steamapps\\common\\" + installDir;
	std::wstring exe = FindExecutableTopLevel(gameFolder, installDir);
	if (!exe.empty()) return exe;
	const wchar_t* folders[] = {
	L"bin",
	L"bin64",
	L"bin\\x64",
	L"bin\\win_x64",
	L"bin\\Win64",
	L"bin\\Release",
	L"Binaries\\Win64",
	L"Binaries\\Win32",
	L"Game",
	L"Game\\Bin",
	L"Game\\Binaries\\Win64",
	L"x64",
	L"x86",
	L"Win64",
	L"Win32",
	};
	for (const auto& folder : folders) {
		std::wstring exe = FindExecutableTopLevel(gameFolder + L"\\" + folder, installDir);
		if (!exe.empty()) return exe;
	}

	return result;
}