#include "Verify.h"
#include <windows.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <knownfolders.h>
#include <vector>

#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Shell32.lib")

namespace
{
    bool PathExists(const std::wstring& path)
    {
        return !path.empty() && PathFileExistsW(path.c_str()) == TRUE;
    }

    bool DirectoryExists(const std::wstring& path)
    {
        if (path.empty())
        {
            return false;
        }

        DWORD attributes = GetFileAttributesW(path.c_str());
        return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }

    bool FileExists(const std::wstring& basePath, const std::wstring& relativeFile)
    {
        std::wstring full = basePath;
        if (!full.empty() && full.back() != L'\\')
        {
            full += L'\\';
        }
        full += relativeFile;
        return PathFileExistsW(full.c_str()) == TRUE;
    }

    bool ReadRegStringView(HKEY root, const wchar_t* subkey, const wchar_t* value, REGSAM samView, std::wstring& out)
    {
        HKEY hKey = nullptr;
        if (RegOpenKeyExW(root, subkey, 0, KEY_QUERY_VALUE | samView, &hKey) != ERROR_SUCCESS)
        {
            return false;
        }

        DWORD type = 0;
        DWORD cb = 0;
        const auto closeKey = [&]() { RegCloseKey(hKey); };

        if (RegQueryValueExW(hKey, value, nullptr, &type, nullptr, &cb) != ERROR_SUCCESS ||
            (type != REG_SZ && type != REG_EXPAND_SZ) || cb == 0)
        {
            closeKey();
            return false;
        }

        std::wstring buf(cb / sizeof(wchar_t), L'\0');
        if (RegQueryValueExW(hKey, value, nullptr, nullptr, reinterpret_cast<LPBYTE>(&buf[0]), &cb) != ERROR_SUCCESS)
        {
            closeKey();
            return false;
        }
        closeKey();

        while (!buf.empty() && buf.back() == L'\0')
        {
            buf.pop_back();
        }

        if (buf.empty())
        {
            return false;
        }

        if (type == REG_EXPAND_SZ)
        {
            const DWORD needed = ExpandEnvironmentStringsW(buf.c_str(), nullptr, 0);
            if (needed == 0)
            {
                return false;
            }

            std::wstring expanded(needed, L'\0');
            if (ExpandEnvironmentStringsW(buf.c_str(), &expanded[0], needed) == 0)
            {
                return false;
            }

            while (!expanded.empty() && expanded.back() == L'\0')
            {
                expanded.pop_back();
            }

            out = expanded;
            return !out.empty();
        }

        out = buf;
        return true;
    }

    bool ReadRegStringAnyView(HKEY root, const wchar_t* subkey, const wchar_t* value, std::wstring& out)
    {
        return ReadRegStringView(root, subkey, value, KEY_WOW64_64KEY, out) ||
            ReadRegStringView(root, subkey, value, KEY_WOW64_32KEY, out) ||
            ReadRegStringView(root, subkey, value, 0, out);
    }

    std::wstring GetKnownFolderPathCompat(REFKNOWNFOLDERID id, int csidl)
    {
        std::wstring out;

        PWSTR p = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(id, 0, nullptr, &p)) && p != nullptr)
        {
            out = p;
            CoTaskMemFree(p);
            return out;
        }

        wchar_t path[MAX_PATH] = {};
        if (SUCCEEDED(SHGetFolderPathW(nullptr, csidl, nullptr, SHGFP_TYPE_CURRENT, path)))
        {
            out = path;
        }

        return out;
    }

    std::wstring GetEnvPath(const wchar_t* variable)
    {
        wchar_t value[MAX_PATH] = {};
        const DWORD length = GetEnvironmentVariableW(variable, value, MAX_PATH);
        if (length > 0 && length < MAX_PATH)
        {
            return value;
        }
        return {};
    }

    std::wstring GetProgramFilesX86Path()
    {
        std::wstring path = GetKnownFolderPathCompat(FOLDERID_ProgramFilesX86, CSIDL_PROGRAM_FILESX86);
        if (!path.empty())
        {
            return path;
        }

        path = GetEnvPath(L"ProgramFiles(x86)");
        if (!path.empty())
        {
            return path;
        }

        return GetEnvPath(L"ProgramFiles");
    }

    std::wstring GetProgramFilesPath()
    {
        std::wstring path = GetKnownFolderPathCompat(FOLDERID_ProgramFiles, CSIDL_PROGRAM_FILES);
        if (!path.empty())
        {
            return path;
        }

        return GetEnvPath(L"ProgramFiles");
    }

    bool ValidateRequiredFiles(const std::wstring& basePath, const std::vector<std::wstring>& files)
    {
        if (!DirectoryExists(basePath))
        {
            return false;
        }

        for (const auto& relative : files)
        {
            if (!FileExists(basePath, relative))
            {
                return false;
            }
        }
        return true;
    }

    bool DetectFromCandidates(const std::vector<std::wstring>& candidates, std::wstring& outPath)
    {
        for (const auto& path : candidates)
        {
            if (DirectoryExists(path))
            {
                outPath = path;
                return true;
            }
        }
        return false;
    }
}

bool ValidateMorrowindFiles(const std::wstring& path)
{
    static const std::vector<std::wstring> kRequired = {
        L"Morrowind.exe",
        L"Data Files\\Morrowind.esm",
        L"Data Files\\Tribunal.esm",
        L"Data Files\\Bloodmoon.esm"
    };
    return ValidateRequiredFiles(path, kRequired);
}

bool ValidateOblivionFiles(const std::wstring& path)
{
    static const std::vector<std::wstring> kRequired = {
        L"Oblivion.exe",
        L"Data\\Oblivion.esm",
        L"Data\\Oblivion - Meshes.bsa"
    };

    if (!ValidateRequiredFiles(path, kRequired))
    {
        return false;
    }

    const bool hasCompressed = FileExists(path, L"Data\\Oblivion - Textures - compressed.bsa");
    const bool hasUncompressed = FileExists(path, L"Data\\Oblivion - Textures.bsa");
    return hasCompressed || hasUncompressed;
}

bool DetectOblivionPath(std::wstring& outPath)
{
    const wchar_t* kBeth32 = L"SOFTWARE\\Bethesda Softworks\\Oblivion";
    const wchar_t* kBeth64 = L"SOFTWARE\\WOW6432Node\\Bethesda Softworks\\Oblivion";

    std::wstring path;
    if (((ReadRegStringAnyView(HKEY_LOCAL_MACHINE, kBeth64, L"Installed Path", path) ||
          ReadRegStringAnyView(HKEY_LOCAL_MACHINE, kBeth32, L"Installed Path", path) ||
          ReadRegStringAnyView(HKEY_CURRENT_USER, kBeth64, L"Installed Path", path) ||
          ReadRegStringAnyView(HKEY_CURRENT_USER, kBeth32, L"Installed Path", path))) &&
        DirectoryExists(path))
    {
        outPath = path;
        return true;
    }

    const std::wstring pf86 = GetProgramFilesX86Path();
    const std::wstring pf = GetProgramFilesPath();

    std::vector<std::wstring> guesses;
    if (!pf86.empty())
    {
        guesses.push_back(pf86 + L"\\Steam\\steamapps\\common\\Oblivion");
        guesses.push_back(pf86 + L"\\GOG Galaxy\\Games\\Oblivion");
    }
    guesses.push_back(L"C:\\GOG Games\\Oblivion");
    if (!pf.empty())
    {
        guesses.push_back(pf + L"\\Steam\\steamapps\\common\\Oblivion");
    }

    return DetectFromCandidates(guesses, outPath);
}

bool DetectMorrowindPath(std::wstring& outPath)
{
    const wchar_t* kMW32 = L"SOFTWARE\\Bethesda Softworks\\Morrowind";
    const wchar_t* kMW64 = L"SOFTWARE\\WOW6432Node\\Bethesda Softworks\\Morrowind";

    std::wstring path;
    if (((ReadRegStringAnyView(HKEY_LOCAL_MACHINE, kMW64, L"Installed Path", path) ||
          ReadRegStringAnyView(HKEY_LOCAL_MACHINE, kMW32, L"Installed Path", path) ||
          ReadRegStringAnyView(HKEY_CURRENT_USER, kMW64, L"Installed Path", path) ||
          ReadRegStringAnyView(HKEY_CURRENT_USER, kMW32, L"Installed Path", path))) &&
        DirectoryExists(path))
    {
        outPath = path;
        return true;
    }

    const std::wstring pf86 = GetProgramFilesX86Path();
    const std::wstring pf = GetProgramFilesPath();

    std::vector<std::wstring> guesses;
    if (!pf86.empty())
    {
        guesses.push_back(pf86 + L"\\Steam\\steamapps\\common\\Morrowind");
        guesses.push_back(pf86 + L"\\GOG Galaxy\\Games\\Morrowind");
    }
    if (!pf.empty() && pf != pf86)
    {
        guesses.push_back(pf + L"\\Steam\\steamapps\\common\\Morrowind");
        guesses.push_back(pf + L"\\GOG Galaxy\\Games\\Morrowind");
    }
    guesses.push_back(L"C:\\GOG Games\\Morrowind");

    return DetectFromCandidates(guesses, outPath);
}
