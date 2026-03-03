#pragma once

#include <string>
#include <windows.h>

/// Attempts to extract the RCDATA with resource‐ID `resId`
/// and write its raw bytes to `outputPath`.
/// Returns true on success, false on failure.
bool ExtractEmbeddedResource(UINT resId, const std::wstring& outputPath);
