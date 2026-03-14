#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <string>
#include <functional>
#include "Resource.h"
#include "Verify.h"
#include "Installer.h"

#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Ole32.lib")

static HWND hStaticLegal, hBtnAccept;
static HWND hStaticSelectInstall, hEditInstall, hBtnBrowseInstall, hBtnAutoInstall, hBtnContinueToVerify;
static HWND hStaticOwnership, hStaticMWLabel, hEditMWPath, hBtnBrowseMW;
static HWND hStaticOBLabel, hEditOBPath, hBtnBrowseOB, hBtnContinueToInstall;
static HWND hStaticFinal, hBtnInstallMaster;
static HWND hBtnAutoMW, hBtnAutoOB;
static HWND hStaticInstallProgress, hProgressInstall;
static HWND hTooltip;
static std::wstring tipBuf;
static bool gInstallCompleted = false;

static const wchar_t* TIP_BROWSE_INSTALL = L"Browse to your Oblivion game folder where Morroblivion will be installed.";
static const wchar_t* TIP_AUTO_INSTALL = L"Auto-detect your Oblivion folder and fill the path automatically.";
static const wchar_t* TIP_BROWSE_MW = L"Select your Morrowind install folder (root containing 'Data Files').";
static const wchar_t* TIP_BROWSE_OB = L"Select your Oblivion install folder (root containing Oblivion.exe).";
static const wchar_t* TIP_AUTO_MW = L"Auto-detect your Morrowind folder via registry or Steam.";
static const wchar_t* TIP_AUTO_OB = L"Auto-detect your Oblivion folder via registry or Steam.";

static void AddToolTip(HWND parent, HWND tooltip, HWND target)
{
    TOOLINFOW ti = { sizeof(ti) };
    ti.hwnd = parent;
    ti.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
    ti.uId = (UINT_PTR)target;
    ti.lpszText = LPSTR_TEXTCALLBACKW;
    SendMessageW(tooltip, TTM_ADDTOOL, 0, (LPARAM)&ti);
}

static std::wstring BrowseForFolder(HWND owner, const std::wstring& prompt)
{
    std::wstring result;
    BROWSEINFOW bi = {};
    bi.hwndOwner = owner;
    bi.lpszTitle = prompt.c_str();
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_USENEWUI;
    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
    if (pidl)
    {
        wchar_t path[MAX_PATH];
        if (SHGetPathFromIDListW(pidl, path))
        {
            result = path;
        }
        CoTaskMemFree(pidl);
    }
    return result;
}

static void SetControlRect(HWND control, int x, int y, int width, int height)
{
    SetWindowPos(control, nullptr, x, y, width, height, SWP_NOZORDER);
}

static void LayoutWizardPages(HWND dlg)
{
    RECT client{};
    GetClientRect(dlg, &client);

    const int margin = 10;
    const int fullWidth = (client.right - client.left) - (margin * 2);
    const int labelHeight = 20;
    const int editHeight = 16;
    const int buttonHeight = 16;
    const int buttonWidth = 80;
    const int buttonWidthSmall = 64;
    const int centerButtonX = ((client.right - client.left) - 100) / 2;
    const int nextButtonX = (client.right - client.left) - margin - buttonWidth;

    // Step 1: legal
    SetControlRect(hStaticLegal, margin, margin, fullWidth, 172);
    SetControlRect(hBtnAccept, centerButtonX, 214, 100, buttonHeight);

    // Step 2: install location
    SetControlRect(hStaticSelectInstall, margin, margin, fullWidth, labelHeight);
    SetControlRect(hEditInstall, margin, 36, 240, editHeight);
    SetControlRect(hBtnBrowseInstall, 256, 36, buttonWidthSmall, buttonHeight);
    SetControlRect(hBtnAutoInstall, 324, 36, buttonWidthSmall, buttonHeight);
    SetControlRect(hBtnContinueToVerify, nextButtonX, 214, buttonWidth, buttonHeight);

    // Step 3: ownership verification
    SetControlRect(hStaticOwnership, margin, margin, fullWidth, labelHeight);
    SetControlRect(hStaticMWLabel, margin, 36, 120, 14);
    SetControlRect(hEditMWPath, margin, 52, 240, editHeight);
    SetControlRect(hBtnBrowseMW, 256, 52, buttonWidthSmall, buttonHeight);
    SetControlRect(hBtnAutoMW, 324, 52, buttonWidthSmall, buttonHeight);
    SetControlRect(hStaticOBLabel, margin, 88, 120, 14);
    SetControlRect(hEditOBPath, margin, 104, 240, editHeight);
    SetControlRect(hBtnBrowseOB, 256, 104, buttonWidthSmall, buttonHeight);
    SetControlRect(hBtnAutoOB, 324, 104, buttonWidthSmall, buttonHeight);
    SetControlRect(hBtnContinueToInstall, nextButtonX, 214, buttonWidth, buttonHeight);
}


static void LayoutFinalPageControls(HWND dlg)
{
    RECT client{};
    GetClientRect(dlg, &client);

    const int margin = 10;
    const int fullWidth = (client.right - client.left) - (margin * 2);
    const int progressWidth = (client.right - client.left) - 24;
    const int progressX = ((client.right - client.left) - progressWidth) / 2;
    const int finalTextHeight = 140;
    const int statusY = 154;
    const int progressY = 172;
    const int buttonWidth = 100;
    const int buttonHeight = 16;
    const int buttonX = ((client.right - client.left) - buttonWidth) / 2;
    const int buttonY = 210;

    SetControlRect(hStaticFinal, margin, 10, fullWidth, finalTextHeight);
    SetControlRect(hStaticInstallProgress, progressX, statusY, progressWidth, 14);
    SetControlRect(hProgressInstall, progressX, progressY, progressWidth, 14);
    SetControlRect(hBtnInstallMaster, buttonX, buttonY, buttonWidth, buttonHeight);
}

static void ShowPage(HWND dlg, int page)
{
    ShowWindow(hStaticLegal, page == 0 ? SW_SHOW : SW_HIDE);
    ShowWindow(hBtnAccept, page == 0 ? SW_SHOW : SW_HIDE);

    ShowWindow(hStaticSelectInstall, page == 1 ? SW_SHOW : SW_HIDE);
    ShowWindow(hEditInstall, page == 1 ? SW_SHOW : SW_HIDE);
    ShowWindow(hBtnBrowseInstall, page == 1 ? SW_SHOW : SW_HIDE);
    ShowWindow(hBtnAutoInstall, page == 1 ? SW_SHOW : SW_HIDE);
    ShowWindow(hBtnContinueToVerify, page == 1 ? SW_SHOW : SW_HIDE);

    ShowWindow(hStaticOwnership, page == 2 ? SW_SHOW : SW_HIDE);
    ShowWindow(hStaticMWLabel, page == 2 ? SW_SHOW : SW_HIDE);
    ShowWindow(hEditMWPath, page == 2 ? SW_SHOW : SW_HIDE);
    ShowWindow(hBtnBrowseMW, page == 2 ? SW_SHOW : SW_HIDE);
    ShowWindow(hStaticOBLabel, page == 2 ? SW_SHOW : SW_HIDE);
    ShowWindow(hEditOBPath, page == 2 ? SW_SHOW : SW_HIDE);
    ShowWindow(hBtnBrowseOB, page == 2 ? SW_SHOW : SW_HIDE);
    ShowWindow(hBtnAutoMW, page == 2 ? SW_SHOW : SW_HIDE);
    ShowWindow(hBtnAutoOB, page == 2 ? SW_SHOW : SW_HIDE);
    ShowWindow(hBtnContinueToInstall, page == 2 ? SW_SHOW : SW_HIDE);

    ShowWindow(hStaticFinal, page == 3 ? SW_SHOW : SW_HIDE);
    ShowWindow(hBtnInstallMaster, page == 3 ? SW_SHOW : SW_HIDE);
    ShowWindow(hStaticInstallProgress, page == 3 ? SW_SHOW : SW_HIDE);
    ShowWindow(hProgressInstall, page == 3 ? SW_SHOW : SW_HIDE);

    EnableWindow(hStaticLegal, page == 0);
    EnableWindow(hBtnAccept, page == 0);

    EnableWindow(hStaticSelectInstall, page == 1);
    EnableWindow(hEditInstall, page == 1);
    EnableWindow(hBtnBrowseInstall, page == 1);
    EnableWindow(hBtnAutoInstall, page == 1);
    EnableWindow(hBtnContinueToVerify, page == 1);

    EnableWindow(hStaticOwnership, page == 2);
    EnableWindow(hStaticMWLabel, page == 2);
    EnableWindow(hEditMWPath, page == 2);
    EnableWindow(hBtnBrowseMW, page == 2);
    EnableWindow(hStaticOBLabel, page == 2);
    EnableWindow(hEditOBPath, page == 2);
    EnableWindow(hBtnBrowseOB, page == 2);
    EnableWindow(hBtnAutoMW, page == 2);
    EnableWindow(hBtnAutoOB, page == 2);
    EnableWindow(hBtnContinueToInstall, page == 2);

    EnableWindow(hStaticFinal, page == 3);
    EnableWindow(hBtnInstallMaster, page == 3);
    EnableWindow(hStaticInstallProgress, page == 3);
    EnableWindow(hProgressInstall, page == 3);
}

static bool CreateDirectoryRecursive(const std::wstring& path)
{
    if (path.empty()) return false;
    if (PathFileExistsW(path.c_str()) == TRUE) return true;
    size_t pos = path.find_last_of(L"\\/");
    if (pos != std::wstring::npos)
    {
        std::wstring parent = path.substr(0, pos);
        if (!CreateDirectoryRecursive(parent)) return false;
    }
    return CreateDirectoryW(path.c_str(), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS;
}

static bool CountFilesRecursively(const std::wstring& rootDir, int& totalFiles)
{
    WIN32_FIND_DATAW findData;
    HANDLE hFind = INVALID_HANDLE_VALUE;

    std::wstring searchPattern = rootDir + L"\\*";
    hFind = FindFirstFileW(searchPattern.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) return false;

    do
    {
        std::wstring name = findData.cFileName;
        if (name == L"." || name == L"..") continue;

        std::wstring childPath = rootDir + L"\\" + name;

        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            if (!CountFilesRecursively(childPath, totalFiles)) { FindClose(hFind); return false; }
        }
        else
        {
            ++totalFiles;
        }
    } while (FindNextFileW(hFind, &findData) != 0);

    FindClose(hFind);
    return true;
}

static bool CopyDirectoryRecursivelyWithProgress(const std::wstring& sourceDir, const std::wstring& targetDir, const std::function<bool(const std::wstring&, const std::wstring&)>& onFile)
{
    WIN32_FIND_DATAW findData;
    HANDLE hFind = INVALID_HANDLE_VALUE;

    std::wstring searchPattern = sourceDir + L"\\*";
    hFind = FindFirstFileW(searchPattern.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) return false;

    do
    {
        std::wstring name = findData.cFileName;
        if (name == L"." || name == L"..") continue;

        std::wstring sourcePath = sourceDir + L"\\" + name;
        std::wstring targetPath = targetDir + L"\\" + name;

        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            if (!CreateDirectoryRecursive(targetPath)) { FindClose(hFind); return false; }
            if (!CopyDirectoryRecursivelyWithProgress(sourcePath, targetPath, onFile)) { FindClose(hFind); return false; }
        }
        else
        {
            if (onFile && !onFile(sourcePath, targetPath)) { FindClose(hFind); return false; }
            if (!CopyFileW(sourcePath.c_str(), targetPath.c_str(), FALSE)) { FindClose(hFind); return false; }
        }
    } while (FindNextFileW(hFind, &findData) != 0);

    FindClose(hFind);
    return true;
}

static void UpdateInstallProgress(HWND dlg, const std::wstring& fileFrom, const std::wstring& fileTo, int current, int total)
{
    const int safeTotal = total > 0 ? total : 1;
    const int pct = (current * 100) / safeTotal;
    std::wstring line;
    if (fileFrom.empty())
    {
        line = L"Preparing installation...";
    }
    else if (fileFrom == L"Installation complete")
    {
        line = L"Installation complete.";
    }
    else
    {
        line = L"Installing: " + fileFrom + L" (" + std::to_wstring(pct) + L"%)";
    }

    (void)fileTo;
    SetWindowTextW(hStaticInstallProgress, line.c_str());
    SendMessageW(hProgressInstall, PBM_SETRANGE32, 0, safeTotal);
    SendMessageW(hProgressInstall, PBM_SETPOS, current, 0);

    InvalidateRect(hStaticInstallProgress, nullptr, TRUE);
    InvalidateRect(hProgressInstall, nullptr, TRUE);
    UpdateWindow(hStaticInstallProgress);
    UpdateWindow(hProgressInstall);
    UpdateWindow(dlg);

    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

static void NormalizeBackslashes(std::wstring& s)
{
    for (auto& ch : s) if (ch == L'/') ch = L'\\';
    size_t n = s.size();
    while (n && (s[n - 1] == L'\\' || s[n - 1] == L'/')) { s.pop_back(); n = s.size(); }
}

static std::wstring EnsureDataSubdir(std::wstring path)
{
    if (path.size() < 4 || _wcsicmp(path.substr(path.size() - 4).c_str(), L"Data") != 0)
    {
        path += L"\\Data";
    }
    return path;
}

static std::wstring GetControlText(HWND control)
{
    if (!control)
    {
        return {};
    }

    const int length = GetWindowTextLengthW(control);
    if (length <= 0)
    {
        return {};
    }

    std::wstring text(static_cast<size_t>(length) + 1, L'\0');
    GetWindowTextW(control, text.data(), length + 1);
    text.resize(static_cast<size_t>(length));
    return text;
}

INT_PTR CALLBACK MainDlgProc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_INITDIALOG:
    {
        RECT rc;
        GetWindowRect(dlg, &rc);
        int dlgW = rc.right - rc.left, dlgH = rc.bottom - rc.top;
        int screenW = GetSystemMetrics(SM_CXSCREEN), screenH = GetSystemMetrics(SM_CYSCREEN);
        SetWindowPos(dlg, HWND_TOP,
            (screenW - dlgW) / 2,
            (screenH - dlgH) / 2,
            0, 0, SWP_NOZORDER | SWP_NOSIZE);

        hStaticLegal = GetDlgItem(dlg, IDC_STATIC_LEGAL_TEXT);
        hBtnAccept = GetDlgItem(dlg, IDC_BTN_ACCEPT);

        hStaticSelectInstall = GetDlgItem(dlg, IDC_STATIC_SELECT_INSTALL);
        hEditInstall = GetDlgItem(dlg, IDC_EDIT_INSTALL_PATH);
        hBtnBrowseInstall = GetDlgItem(dlg, IDC_BTN_BROWSE_INSTALL);
        hBtnAutoInstall = GetDlgItem(dlg, IDC_BTN_AUTO_INSTALL);
        hBtnContinueToVerify = GetDlgItem(dlg, IDC_BTN_CONTINUE_TO_VERIFY);

        hStaticOwnership = GetDlgItem(dlg, IDC_STATIC_OWNERSHIP_TEXT);
        hStaticMWLabel = GetDlgItem(dlg, IDC_STATIC_MW_LABEL);
        hEditMWPath = GetDlgItem(dlg, IDC_EDIT_MW_PATH);
        hBtnBrowseMW = GetDlgItem(dlg, IDC_BTN_BROWSE_MW);
        hStaticOBLabel = GetDlgItem(dlg, IDC_STATIC_OB_LABEL);
        hEditOBPath = GetDlgItem(dlg, IDC_EDIT_OB_PATH);
        hBtnBrowseOB = GetDlgItem(dlg, IDC_BTN_BROWSE_OB);
        hBtnContinueToInstall = GetDlgItem(dlg, IDC_BTN_CONTINUE_TO_INSTALL);

        hStaticFinal = GetDlgItem(dlg, IDC_STATIC_FINAL_TEXT);
        hBtnInstallMaster = GetDlgItem(dlg, IDC_BTN_INSTALL_MASTER);

        hStaticInstallProgress = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | SS_CENTER,
            10, 172, 380, 14, dlg, nullptr, GetModuleHandle(nullptr), nullptr);
        hProgressInstall = CreateWindowExW(0, PROGRESS_CLASSW, nullptr, WS_CHILD,
            10, 188, 380, 12, dlg, nullptr, GetModuleHandle(nullptr), nullptr);
        SendMessageW(hProgressInstall, PBM_SETRANGE32, 0, 1);
        SendMessageW(hProgressInstall, PBM_SETPOS, 0, 0);

        hBtnAutoMW = GetDlgItem(dlg, IDC_BTN_AUTO_MW);
        hBtnAutoOB = GetDlgItem(dlg, IDC_BTN_AUTO_OB);

        LayoutWizardPages(dlg);

        std::wstring legalText =
            L"TERMS AND CONDITIONS\r\n"
            L"\r\n"
            L"1. Acceptance of Our Terms\r\n"
            L"\r\n"
            L"By visiting morroblivion.com or using any of the services or information, you agree to be bound by these Terms and Conditions. If you do not wish to be bound by these Terms, you must not use or access morroblivion.com.\r\n"
            L"\r\n"
            L"2. Provision of Services\r\n"
            L"\r\n"
            L"morroblivion.com may modify or discontinue any of its services at any time without notice. morroblivion.com may provide services through affiliated entities.\r\n"
            L"\r\n"
            L"3. Proprietary Rights\r\n"
            L"\r\n"
            L"All content on morroblivion.com is proprietary or licensed. You may view and make a single copy for offline, personal use. This content must not be sold, reproduced, or distributed without written permission.\r\n"
            L"\r\n"
            L"4. Submitted Content\r\n"
            L"\r\n"
            L"When you submit content, you grant morroblivion.com a worldwide, royalty-free license to use, display, modify, and distribute it. You warrant that you have authority to grant this license.\r\n"
            L"\r\n"
            L"5. Termination of Agreement\r\n"
            L"\r\n"
            L"These Terms remain in effect until terminated by either party at any time without notice. Provisions intended to survive termination remain in effect.\r\n"
            L"\r\n"
            L"6. Disclaimer of Warranties\r\n"
            L"\r\n"
            L"morroblivion.com provides services “As Is” and “As Available.” No warranties are made regarding uninterrupted or error-free access.\r\n"
            L"\r\n"
            L"7. Limitation of Liability\r\n"
            L"\r\n"
            L"morroblivion.com is not liable for any direct, indirect, or incidental damages arising from your use of the services.\r\n"
            L"\r\n"
            L"8. External Content\r\n"
            L"\r\n"
            L"morroblivion.com may include links to third-party content. morroblivion.com is not responsible for linked content or products.\r\n"
            L"\r\n"
            L"9. Jurisdiction\r\n"
            L"\r\n"
            L"You agree to submit to the exclusive jurisdiction of courts chosen by morroblivion.com for disputes arising out of these Terms.\r\n"
            L"\r\n"
            L"10. Entire Agreement\r\n"
            L"\r\n"
            L"These Terms constitute the entire agreement between you and morroblivion.com.\r\n"
            L"\r\n"
            L"11. Changes to the Terms\r\n"
            L"\r\n"
            L"morroblivion.com may modify these Terms at any time without notice. Continued use signifies acceptance of the new Terms.\r\n";
        SetWindowTextW(hStaticLegal, legalText.c_str());

        hTooltip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr, WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX | TTS_BALLOON, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, dlg, nullptr, GetModuleHandle(nullptr), nullptr);
        SendMessageW(hTooltip, TTM_SETMAXTIPWIDTH, 0, 350);
        AddToolTip(dlg, hTooltip, hBtnBrowseInstall);
        AddToolTip(dlg, hTooltip, hBtnAutoInstall);
        AddToolTip(dlg, hTooltip, hBtnBrowseMW);
        AddToolTip(dlg, hTooltip, hBtnBrowseOB);
        AddToolTip(dlg, hTooltip, hBtnAutoMW);
        AddToolTip(dlg, hTooltip, hBtnAutoOB);

        ShowPage(dlg, 0);
        SetFocus(hBtnAccept);
        return FALSE;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDC_BTN_ACCEPT:
            ShowPage(dlg, 1);
            return TRUE;

        case IDC_BTN_BROWSE_INSTALL:
        {
            std::wstring folder = BrowseForFolder(dlg, L"Select your Oblivion game folder (recommended)");
            if (!folder.empty())
            {
                SetWindowTextW(hEditInstall, folder.c_str());
            }
            return TRUE;
        }

        case IDC_BTN_AUTO_INSTALL:
        {
            std::wstring obPath;
            if (DetectOblivionPath(obPath))
            {
                NormalizeBackslashes(obPath);
                SetWindowTextW(hEditInstall, obPath.c_str());
            }
            else
            {
                MessageBoxW(dlg, L"Couldn't automatically locate Oblivion.\n\nPlease click Browse and select your Oblivion game folder.", L"AUTO Detect", MB_ICONINFORMATION);
            }
            return TRUE;
        }

        case IDC_BTN_CONTINUE_TO_VERIFY:
        {
            const std::wstring installPath = GetControlText(hEditInstall);
            if (installPath.empty())
            {
                MessageBoxW(dlg, L"Please choose a folder.", L"Error", MB_ICONWARNING);
                break;
            }
            ShowPage(dlg, 2);
            return TRUE;
        }

        case IDC_BTN_BROWSE_MW:
        {
            std::wstring folder = BrowseForFolder(dlg, L"Select your Morrowind folder");
            if (!folder.empty())
            {
                SetWindowTextW(hEditMWPath, folder.c_str());
            }
            return TRUE;
        }

        case IDC_BTN_BROWSE_OB:
        {
            std::wstring folder = BrowseForFolder(dlg, L"Select your Oblivion folder");
            if (!folder.empty())
            {
                SetWindowTextW(hEditOBPath, folder.c_str());
            }
            return TRUE;
        }

        case IDC_BTN_AUTO_MW:
        {
            std::wstring mwPath;
            if (DetectMorrowindPath(mwPath))
            {
                NormalizeBackslashes(mwPath);
                SetWindowTextW(hEditMWPath, mwPath.c_str());
            }
            else
            {
                MessageBoxW(dlg, L"Couldn't automatically locate Morrowind.\n\nPlease click Browse and select your Morrowind folder.", L"AUTO Detect", MB_ICONINFORMATION);
            }
            return TRUE;
        }

        case IDC_BTN_AUTO_OB:
        {
            std::wstring obPath;
            if (DetectOblivionPath(obPath))
            {
                NormalizeBackslashes(obPath);
                SetWindowTextW(hEditOBPath, obPath.c_str());
            }
            else
            {
                MessageBoxW(dlg, L"Couldn't automatically locate Oblivion.\n\nPlease click Browse and select your Oblivion folder.", L"AUTO Detect", MB_ICONINFORMATION);
            }
            return TRUE;
        }

        case IDC_BTN_CONTINUE_TO_INSTALL:
        {
            const std::wstring mwPath = GetControlText(hEditMWPath);
            const std::wstring obPath = GetControlText(hEditOBPath);

            bool mwOK = ValidateMorrowindFiles(mwPath);
            bool obOK = ValidateOblivionFiles(obPath);

            if (!mwOK || !obOK)
            {
                std::wstring msg = L"Validation failed for:\r\n";
                if (!mwOK) msg += L"  • Morrowind folder\r\n";
                if (!obOK) msg += L"  • Oblivion folder\r\n";
                MessageBoxW(dlg, msg.c_str(), L"Validation Error", MB_ICONERROR);
                break;
            }

            const std::wstring installPath = GetControlText(hEditInstall);
            std::wstring dataFolder = EnsureDataSubdir(installPath);

            std::wstring finalText =
                L"Morroblivion is ready to install.\r\n\r\nInstall location:\r\n" +
                dataFolder + L"\r\n\r\n" +
                L"Click 'Install' to begin. When setup is complete, click 'Close' to exit.";

            SetWindowTextW(hStaticFinal, finalText.c_str());
            gInstallCompleted = false;
            SetWindowTextW(hBtnInstallMaster, L"Install");

            ShowPage(dlg, 3);
            return TRUE;
        }

        case IDC_BTN_INSTALL_MASTER:
        {
            if (gInstallCompleted)
            {
                EndDialog(dlg, IDOK);
                return TRUE;
            }

            std::wstring installPath = GetControlText(hEditInstall);
            if (installPath.empty())
            {
                MessageBoxW(dlg, L"No install folder specified.", L"Error", MB_ICONERROR);
                break;
            }

            NormalizeBackslashes(installPath);
            std::wstring dataDir = EnsureDataSubdir(installPath);

            if (!CreateDirectoryRecursive(installPath) || !CreateDirectoryRecursive(dataDir))
            {
                MessageBoxW(dlg, L"Failed to create the target folder(s).", L"Error", MB_ICONERROR);
                break;
            }

            wchar_t exePath[MAX_PATH] = {};
            const DWORD exeLen = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
            if (exeLen == 0 || exeLen >= MAX_PATH)
            {
                MessageBoxW(dlg, L"Failed to resolve installer executable path.", L"Error", MB_ICONERROR);
                break;
            }

            std::wstring exeDir = exePath;
            size_t slash = exeDir.find_last_of(L"\\/");
            if (slash != std::wstring::npos) exeDir.resize(slash);

            std::wstring sourceCoreDir = exeDir + L"\\Resource\\00 Core";

            DWORD srcAttr = GetFileAttributesW(sourceCoreDir.c_str());
            if (srcAttr == INVALID_FILE_ATTRIBUTES || !(srcAttr & FILE_ATTRIBUTE_DIRECTORY))
            {
                std::wstring err = L"Required folder not found:\r\n" + sourceCoreDir + L"\r\n\r\nPlease make sure the installer’s Resource folder is next to the EXE.";
                MessageBoxW(dlg, err.c_str(), L"Missing Files", MB_ICONERROR);
                break;
            }

            HCURSOR oldCur = SetCursor(LoadCursor(nullptr, IDC_WAIT));
            EnableWindow(hBtnInstallMaster, FALSE);

            int totalFiles = 2;
            if (!CountFilesRecursively(sourceCoreDir, totalFiles))
            {
                EnableWindow(hBtnInstallMaster, TRUE);
                SetCursor(oldCur);
                MessageBoxW(dlg, L"Failed to enumerate files in Resource\\00 Core.", L"Installation Error", MB_ICONERROR);
                break;
            }

            int copiedFiles = 0;
            UpdateInstallProgress(dlg, L"", L"", copiedFiles, totalFiles);

            const std::wstring esmPath = dataDir + L"\\Morrowind_ob.esm";
            const std::wstring espPath = dataDir + L"\\Morrowind_ob.esp";

            UpdateInstallProgress(dlg, L"Embedded: Morrowind_ob.esm", esmPath, copiedFiles, totalFiles);
            bool ok1 = ExtractEmbeddedResource(IDR_MORROWIND_OB_ESM, esmPath);
            if (ok1)
            {
                ++copiedFiles;
                UpdateInstallProgress(dlg, L"Embedded: Morrowind_ob.esm", esmPath, copiedFiles, totalFiles);
            }

            UpdateInstallProgress(dlg, L"Embedded: Morrowind_ob.esp", espPath, copiedFiles, totalFiles);
            bool ok2 = ExtractEmbeddedResource(IDR_MORROWIND_OB_ESP, espPath);
            if (ok2)
            {
                ++copiedFiles;
                UpdateInstallProgress(dlg, L"Embedded: Morrowind_ob.esp", espPath, copiedFiles, totalFiles);
            }

            bool okCore = CopyDirectoryRecursivelyWithProgress(sourceCoreDir, dataDir,
                [&](const std::wstring& from, const std::wstring& to)
                {
                    UpdateInstallProgress(dlg, from, to, copiedFiles, totalFiles);
                    ++copiedFiles;
                    UpdateInstallProgress(dlg, from, to, copiedFiles, totalFiles);
                    return true;
                });

            if (ok1 && ok2 && okCore)
            {
                UpdateInstallProgress(dlg, L"Installation complete", dataDir, totalFiles, totalFiles);
            }

            EnableWindow(hBtnInstallMaster, TRUE);
            SetCursor(oldCur);

            if (ok1 && ok2 && okCore)
            {
                gInstallCompleted = true;
                SetWindowTextW(hBtnInstallMaster, L"Close");
                SetWindowTextW(hStaticInstallProgress, L"Installation complete.");

                std::wstring successText =
                    L"Installation complete.\r\n\r\nInstalled to:\r\n" +
                    dataDir + L"\r\n\r\n" +
                    L"Click 'Close' to finish.";
                SetWindowTextW(hStaticFinal, successText.c_str());
            }
            else
            {
                std::wstring errMsg = L"Installation failed:\r\n";
                if (!ok1) errMsg += L"  • Failed to extract Morrowind_ob.esm\r\n";
                if (!ok2) errMsg += L"  • Failed to extract Morrowind_ob.esp\r\n";
                if (!okCore) errMsg += L"  • Failed to copy Resource\\00 Core contents\r\n";
                errMsg += L"\r\nTarget: " + dataDir;
                SetWindowTextW(hBtnInstallMaster, L"Install");
                MessageBoxW(dlg, errMsg.c_str(), L"Installation Error", MB_ICONERROR);
            }
            return TRUE;
        }
        }
        break;
    }

    case WM_NOTIFY:
    {
        LPNMHDR nm = (LPNMHDR)lParam;
        if (nm->code == TTN_GETDISPINFOW && nm->hwndFrom == hTooltip)
        {
            NMTTDISPINFOW* disp = (NMTTDISPINFOW*)lParam;
            HWND from = (HWND)disp->hdr.idFrom;
            if (from == hBtnBrowseInstall)      tipBuf = TIP_BROWSE_INSTALL;
            else if (from == hBtnAutoInstall)   tipBuf = TIP_AUTO_INSTALL;
            else if (from == hBtnBrowseMW)      tipBuf = TIP_BROWSE_MW;
            else if (from == hBtnBrowseOB)      tipBuf = TIP_BROWSE_OB;
            else if (from == hBtnAutoMW)        tipBuf = TIP_AUTO_MW;
            else if (from == hBtnAutoOB)        tipBuf = TIP_AUTO_OB;
            else                                tipBuf.clear();
            disp->lpszText = tipBuf.empty() ? const_cast<wchar_t*>(L"") : const_cast<wchar_t*>(tipBuf.c_str());
            return TRUE;
        }
        break;
    }

    case WM_CLOSE:
        EndDialog(dlg, 0);
        return TRUE;
    }
    return FALSE;
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int)
{
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS };
    InitCommonControlsEx(&icc);
    const HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    DialogBoxParamW(hInstance, MAKEINTRESOURCEW(IDD_MAIN_DIALOG), nullptr, MainDlgProc, 0);
    if (SUCCEEDED(hr))
    {
        CoUninitialize();
    }
    return 0;
}
