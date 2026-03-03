#include "Verify.h"
#include <windows.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <knownfolders.h>
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Shell32.lib")

static bool FileExists(const std::wstring& basePath, const std::wstring& relativeFile)
{
    std::wstring full = basePath;
    if (!full.empty() && full.back() != L'\\') full += L'\\';
    full += relativeFile;
    return PathFileExistsW(full.c_str()) == TRUE;
}

static bool ReadRegString(HKEY root, const wchar_t* subkey, const wchar_t* value, std::wstring& out)
{
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(root, subkey, 0, KEY_QUERY_VALUE, &hKey) != ERROR_SUCCESS) return false;
    DWORD type = 0, cb = 0;
    if (RegQueryValueExW(hKey, value, nullptr, &type, nullptr, &cb) != ERROR_SUCCESS || type != REG_SZ) { RegCloseKey(hKey); return false; }
    std::wstring buf(cb / sizeof(wchar_t), L'\0');
    if (RegQueryValueExW(hKey, value, nullptr, nullptr, reinterpret_cast<LPBYTE>(&buf[0]), &cb) == ERROR_SUCCESS) {
        RegCloseKey(hKey);
        while (!buf.empty() && buf.back() == L'\0') buf.pop_back();
        out = buf;
        return !out.empty();
    }
    RegCloseKey(hKey);
    return false;
}

static std::wstring GetKnownFolder(REFKNOWNFOLDERID id)
{
    PWSTR p = nullptr;
    std::wstring out;
    if (SUCCEEDED(SHGetKnownFolderPath(id, 0, nullptr, &p))) {
        out = p;
        CoTaskMemFree(p);
    }
    return out;
}

bool ValidateMorrowindFiles(const std::wstring& path)
{
    if (path.empty() || GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) return false;
    if (!FileExists(path, L"Morrowind.exe")) return false;
    if (!FileExists(path, L"Data Files\\Morrowind.esm")) return false;
    if (!FileExists(path, L"Data Files\\Tribunal.esm")) return false;
    if (!FileExists(path, L"Data Files\\Bloodmoon.esm")) return false;
    return true;
}

bool ValidateOblivionFiles(const std::wstring& path)
{
    if (path.empty() || GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) return false;
    if (!FileExists(path, L"Oblivion.exe")) return false;
    if (!FileExists(path, L"Data\\Oblivion.esm")) return false;
    if (!FileExists(path, L"Data\\Oblivion - Meshes.bsa")) return false;
    bool hasCompressed = FileExists(path, L"Data\\Oblivion - Textures - compressed.bsa");
    bool hasUncompressed = FileExists(path, L"Data\\Oblivion - Textures.bsa");
    if (!hasCompressed && !hasUncompressed) return false;
    return true;
}

bool DetectOblivionPath(std::wstring& outPath)
{
    const wchar_t* kBeth32 = L"SOFTWARE\\Bethesda Softworks\\Oblivion";
    const wchar_t* kBeth64 = L"SOFTWARE\\WOW6432Node\\Bethesda Softworks\\Oblivion";
    std::wstring path;
    if (ReadRegString(HKEY_LOCAL_MACHINE, kBeth64, L"Installed Path", path) ||
        ReadRegString(HKEY_LOCAL_MACHINE, kBeth32, L"Installed Path", path)) {
        if (PathFileExistsW(path.c_str())) { outPath = path; return true; }
    }

    std::wstring pf86 = GetKnownFolder(FOLDERID_ProgramFilesX86);
    if (!pf86.empty()) {
        std::wstring guess = pf86 + L"\\Steam\\steamapps\\common\\Oblivion";
        if (PathFileExistsW(guess.c_str())) { outPath = guess; return true; }
        guess = pf86 + L"\\GOG Galaxy\\Games\\Oblivion";
        if (PathFileExistsW(guess.c_str())) { outPath = guess; return true; }
        guess = L"C:\\GOG Games\\Oblivion";
        if (PathFileExistsW(guess.c_str())) { outPath = guess; return true; }
    }

    std::wstring pf = GetKnownFolder(FOLDERID_ProgramFiles);
    if (!pf.empty()) {
        std::wstring guess = pf + L"\\Steam\\steamapps\\common\\Oblivion";
        if (PathFileExistsW(guess.c_str())) { outPath = guess; return true; }
    }

    return false;
}

bool DetectMorrowindPath(std::wstring& outPath)
{
    const wchar_t* kMW32 = L"SOFTWARE\\Bethesda Softworks\\Morrowind";
    const wchar_t* kMW64 = L"SOFTWARE\\WOW6432Node\\Bethesda Softworks\\Morrowind";
    std::wstring path;
    if (ReadRegString(HKEY_LOCAL_MACHINE, kMW64, L"Installed Path", path) ||
        ReadRegString(HKEY_LOCAL_MACHINE, kMW32, L"Installed Path", path)) {
        if (PathFileExistsW(path.c_str())) { outPath = path; return true; }
    }

    std::wstring pf86 = GetKnownFolder(FOLDERID_ProgramFilesX86);
    if (!pf86.empty()) {
        std::wstring guess = pf86 + L"\\Steam\\steamapps\\common\\Morrowind";
        if (PathFileExistsW(guess.c_str())) { outPath = guess; return true; }
        guess = pf86 + L"\\GOG Galaxy\\Games\\Morrowind";
        if (PathFileExistsW(guess.c_str())) { outPath = guess; return true; }
        guess = L"C:\\GOG Games\\Morrowind";
        if (PathFileExistsW(guess.c_str())) { outPath = guess; return true; }
    }

    return false;
}
