#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <string>
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
static HWND hTooltip;
static std::wstring tipBuf;

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

static bool CopyDirectoryRecursively(const std::wstring& sourceDir, const std::wstring& targetDir)
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
            if (!CopyDirectoryRecursively(sourcePath, targetPath)) { FindClose(hFind); return false; }
        }
        else
        {
            if (!CopyFileW(sourcePath.c_str(), targetPath.c_str(), FALSE)) { FindClose(hFind); return false; }
        }
    } while (FindNextFileW(hFind, &findData) != 0);

    FindClose(hFind);
    return true;
}

static void NormalizeBackslashes(std::wstring& s)
{
    for (auto& ch : s) if (ch == L'/') ch = L'\\';
    size_t n = s.size();
    while (n && (s[n - 1] == L'\\' || s[n - 1] == L'/')) { s.pop_back(); n = s.size(); }
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

        hBtnAutoMW = GetDlgItem(dlg, IDC_BTN_AUTO_MW);
        hBtnAutoOB = GetDlgItem(dlg, IDC_BTN_AUTO_OB);

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
            wchar_t installPathW[MAX_PATH] = {};
            GetWindowTextW(hEditInstall, installPathW, MAX_PATH);
            if (!*installPathW)
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
            wchar_t mwPathW[MAX_PATH] = {}, obPathW[MAX_PATH] = {};
            GetWindowTextW(hEditMWPath, mwPathW, MAX_PATH);
            GetWindowTextW(hEditOBPath, obPathW, MAX_PATH);

            bool mwOK = ValidateMorrowindFiles(mwPathW);
            bool obOK = ValidateOblivionFiles(obPathW);

            if (!mwOK || !obOK)
            {
                std::wstring msg = L"Validation failed for:\r\n";
                if (!mwOK) msg += L"  • Morrowind folder\r\n";
                if (!obOK) msg += L"  • Oblivion folder\r\n";
                MessageBoxW(dlg, msg.c_str(), L"Validation Error", MB_ICONERROR);
                break;
            }

            wchar_t installPathW[MAX_PATH] = {};
            GetWindowTextW(hEditInstall, installPathW, MAX_PATH);
            std::wstring installPath = installPathW;

            std::wstring dataFolder = installPath;
            if (dataFolder.size() < 4 || _wcsicmp(dataFolder.substr(dataFolder.size() - 4).c_str(), L"Data") != 0)
            {
                dataFolder += L"\\Data";
            }

            std::wstring finalText =
                L"All Morroblivion files will be installed to:\r\n\r\n" +
                dataFolder + L"\r\n\r\n" +
                L"Click 'Install' to begin copying. When installation finishes, click 'Finish' to exit this installer.";

            SetWindowTextW(hStaticFinal, finalText.c_str());

            ShowPage(dlg, 3);
            return TRUE;
        }

        case IDC_BTN_INSTALL_MASTER:
        {
            wchar_t installPathW[MAX_PATH] = {};
            GetWindowTextW(hEditInstall, installPathW, MAX_PATH);
            std::wstring installPath = installPathW;
            if (installPath.empty())
            {
                MessageBoxW(dlg, L"No install folder specified.", L"Error", MB_ICONERROR);
                break;
            }

            std::wstring dataDir = installPath;
            if (dataDir.size() < 4 || _wcsicmp(dataDir.substr(dataDir.size() - 4).c_str(), L"Data") != 0)
            {
                dataDir += L"\\Data";
            }

            if (!CreateDirectoryRecursive(installPath) || !CreateDirectoryRecursive(dataDir))
            {
                MessageBoxW(dlg, L"Failed to create the target folder(s).", L"Error", MB_ICONERROR);
                break;
            }

            wchar_t exePath[MAX_PATH] = {};
            GetModuleFileNameW(nullptr, exePath, MAX_PATH);
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

            bool ok1 = ExtractEmbeddedResource(IDR_MORROWIND_OB_ESM, dataDir + L"\\Morrowind_ob.esm");
            bool ok2 = ExtractEmbeddedResource(IDR_MORROWIND_OB_ESP, dataDir + L"\\Morrowind_ob.esp");
            bool okCore = CopyDirectoryRecursively(sourceCoreDir, dataDir);

            EnableWindow(hBtnInstallMaster, TRUE);
            SetCursor(oldCur);

            if (ok1 && ok2 && okCore)
            {
                MessageBoxW(dlg, L"The Elder Scrolls Renewal Project: Morroblivion installed successfully!", L"Success", MB_ICONINFORMATION);
                EndDialog(dlg, IDOK);
            }
            else
            {
                std::wstring errMsg = L"Installation failed:\r\n";
                if (!ok1) errMsg += L"  • Failed to extract Morrowind_ob.esm\r\n";
                if (!ok2) errMsg += L"  • Failed to extract Morrowind_ob.esp\r\n";
                if (!okCore) errMsg += L"  • Failed to copy Resource\\00 Core contents\r\n";
                errMsg += L"\r\nTarget: " + dataDir;
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
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    DialogBoxParamW(hInstance, MAKEINTRESOURCEW(IDD_MAIN_DIALOG), nullptr, MainDlgProc, 0);
    CoUninitialize();
    return 0;
}
