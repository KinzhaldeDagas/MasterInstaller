#pragma once
#include <string>

/// Checks minimal files to verify a Morrowind installation.
bool ValidateMorrowindFiles(const std::wstring& path);

/// Checks minimal files to verify an Oblivion installation.
bool ValidateOblivionFiles(const std::wstring& path);

bool DetectOblivionPath(std::wstring& outPath);
bool DetectMorrowindPath(std::wstring& outPath);