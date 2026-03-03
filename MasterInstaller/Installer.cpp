#include "Installer.h"
#include <fstream>

bool ExtractEmbeddedResource(UINT resId, const std::wstring& outputPath)
{
    // 1) Find the resource in our .exe (RCDATA type)
    HRSRC hResInfo = FindResourceW(nullptr, MAKEINTRESOURCEW(resId), RT_RCDATA);
    if (!hResInfo)
        return false;

    // 2) Load the resource into memory
    HGLOBAL hRes = LoadResource(nullptr, hResInfo);
    if (!hRes)
        return false;

    // 3) Get a pointer to its data and its size
    void* pData = LockResource(hRes);
    DWORD dataSize = SizeofResource(nullptr, hResInfo);
    if (!pData || dataSize == 0)
        return false;

    // 4) Open the output file for writing (binary)
    std::ofstream ofs(outputPath, std::ios::binary | std::ios::out);
    if (!ofs)
        return false;

    // 5) Write the raw data
    ofs.write(reinterpret_cast<const char*>(pData), dataSize);
    ofs.close();

    return true;
}
