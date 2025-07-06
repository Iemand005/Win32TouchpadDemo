// TouchPadTest.cpp : Defines the entry point for the application.
//

#include <windows.h>
#include <objidl.h>
#include <wingdi.h>
#include "framework.h"
#include "TouchPadTest.h"
#include <sstream>
#include <iomanip>

#include <hidsdi.h>  // For HIDP_CAPS, HidP_GetCaps
#pragma comment(lib, "hid.lib")

#pragma comment(lib, "gdiplus.lib")

struct POSITION_PARTS {
    BYTE low;
    BYTE high;
};

struct TOUCH_POSITION {
    BYTE index;
    POSITION_PARTS x, y;
};

struct TOUCH_SIZE {
    BYTE size;
    BYTE dimensions;
};

struct TOUCHPAD_EVENT {
    BYTE unk1;
    BYTE fingers;
    WORD time;

    TOUCH_POSITION positions[5];
    TOUCH_SIZE sizes[5];
};

struct TOUCH {
    RECT rect;
    BYTE size;
};

TOUCH touches[5];
BYTE touchCount = 0;
POINT touchpadSize;
POINT windowSize;

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
ULONG_PTR gdiplusToken;
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Place code here.

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_TOUCHPADTEST, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_TOUCHPADTEST));

    MSG msg;

    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_TOUCHPADTEST));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_TOUCHPADTEST);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Store instance handle in our global variable

   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

//const int IDC_MY_LABEL = 100;
#define IDC_MY_LABEL 1001

void ProcessTouchpadInput(BYTE* rawData, DWORD size)
{
    // Basic touchpad report parsing (adjust for your specific device)
    if (size >= 6) {
        WORD x = (rawData[1] << 8) | rawData[0];
        WORD y = (rawData[3] << 8) | rawData[2];
        BYTE contactCount = rawData[4] & 0x07;
        BYTE buttonState = rawData[5];

        wchar_t debug[256];
        swprintf(debug, 256, L"Touchpad: %d contacts at (%d,%d) Buttons: 0x%02X\n",
            contactCount, x, y, buttonState);
        OutputDebugString(debug);
    }
}

bool GetTouchpadDimensions(HANDLE hDevice, int& width, int& height)
{
    // 1. Get preparsed data
    PHIDP_PREPARSED_DATA pPreparsed = NULL;
    UINT preparsedSize = 0;

    if (GetRawInputDeviceInfo(hDevice, RIDI_PREPARSEDDATA, NULL, &preparsedSize) != 0)
        return false;

    pPreparsed = (PHIDP_PREPARSED_DATA)malloc(preparsedSize);
    if (!pPreparsed) return false;

    if (GetRawInputDeviceInfo(hDevice, RIDI_PREPARSEDDATA, pPreparsed, &preparsedSize) == 0)
    {
        free(pPreparsed);
        return false;
    }

    // 2. Get device capabilities
    HIDP_CAPS caps;
    if (HidP_GetCaps(pPreparsed, &caps) != HIDP_STATUS_SUCCESS)
    {
        free(pPreparsed);
        return false;
    }

    // 3. Get value capabilities for X/Y axes
    PHIDP_VALUE_CAPS pValueCaps = (PHIDP_VALUE_CAPS)malloc(caps.NumberInputValueCaps * sizeof(HIDP_VALUE_CAPS));
    if (!pValueCaps)
    {
        free(pPreparsed);
        return false;
    }

    USHORT valueCapsLength = caps.NumberInputValueCaps;
    if (HidP_GetValueCaps(HidP_Input, pValueCaps, &valueCapsLength, pPreparsed) != HIDP_STATUS_SUCCESS)
    {
        free(pValueCaps);
        free(pPreparsed);
        return false;
    }

    // 4. Find X and Y axes
    LONG xMin = 0, xMax = 0;
    LONG yMin = 0, yMax = 0;

    for (USHORT i = 0; i < valueCapsLength; i++)
    {
        if (pValueCaps[i].UsagePage == 0x01)  // Generic Desktop Page
        {
            if (pValueCaps[i].Range.UsageMin == 0x30) // X axis
            {
                xMin = pValueCaps[i].LogicalMin;
                xMax = pValueCaps[i].LogicalMax;
            }
            else if (pValueCaps[i].Range.UsageMin == 0x31) // Y axis
            {
                yMin = pValueCaps[i].LogicalMin;
                yMax = pValueCaps[i].LogicalMax;
            }
        }
    }

    width = xMax - xMin;
    height = yMax - yMin;

    free(pValueCaps);
    free(pPreparsed);
    return true;
}

bool IsTouchpadDevice(HANDLE hDevice)
{
    // Get preparsed data
    UINT preparsedSize = 0;
    if (GetRawInputDeviceInfo(hDevice, RIDI_PREPARSEDDATA, NULL, &preparsedSize) != 0)
        return false;

    PHIDP_PREPARSED_DATA pPreparsed = (PHIDP_PREPARSED_DATA)malloc(preparsedSize);
    if (!pPreparsed) return false;

    if (GetRawInputDeviceInfo(hDevice, RIDI_PREPARSEDDATA, pPreparsed, &preparsedSize) == 0)
    {
        free(pPreparsed);
        return false;
    }

    // Get device capabilities
    HIDP_CAPS caps;
    if (HidP_GetCaps(pPreparsed, &caps) != HIDP_STATUS_SUCCESS)
    {
        free(pPreparsed);
        return false;
    }

    // Check usage page and usage
    bool isTouchpad = (caps.UsagePage == 0x0D && caps.Usage == 0x05);

    free(pPreparsed);
    return isTouchpad;
}


//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static HBITMAP hbmBuffer = NULL;
    static HDC hdcBuffer = NULL;
    static POINT pt = { 0, 0 };
    static HWND hLabel;

    switch (message)
    {
    case WM_SIZE:
    {
        RECT clientRect;
        GetClientRect(hWnd, &clientRect);
        int windowWidth = clientRect.right - clientRect.left;
        int windowHeight = clientRect.bottom - clientRect.top;
        windowSize.x = windowWidth;
        windowSize.y = windowHeight;

        DeleteObject(hbmBuffer);
        hbmBuffer = NULL;
    }
    break;
    case WM_CREATE:
        {
            HDC hdc = GetDC(hWnd);
            hdcBuffer = CreateCompatibleDC(hdc);
            ReleaseDC(hWnd, hdc);

            hLabel = CreateWindow(
                L"STATIC",                     // Predefined class for static controls
                L"Initial Text",               // Text to display
                WS_CHILD | WS_VISIBLE,         // Styles
                10, 10,                        // Position (x, y)
                0,0,
                //1400, 200,                       // Width, height
                hWnd,                          // Parent window
                (HMENU)IDC_MY_LABEL,           // Control ID (define this as a constant)
                hInst,                     // Application instance
                NULL);

            RegisterTouchWindow(hWnd, TWF_FINETOUCH | TWF_WANTPALM);
            BOOL success = EnableMouseInPointer(TRUE);

            // For Windows 10 (1809+) touchpad gestures
            SetProp(hWnd, L"MicrosoftTabletPenServiceProperty", (HANDLE)0x00000001);

            RAWINPUTDEVICE rid[1];

            // Register for touchpad input
            rid[0].usUsagePage = 0x0D;  // Digitizer
            rid[0].usUsage = 0x05;      // Touchpad
            rid[0].dwFlags = RIDEV_INPUTSINK;
            rid[0].hwndTarget = hWnd;

            if (!RegisterRawInputDevices(rid, 1, sizeof(RAWINPUTDEVICE)))
            {
                // Error handling
                OutputDebugString(L"Failed to register for raw input\n");
            }

            UINT deviceCount = 0;

            if (GetRawInputDeviceList(NULL, &deviceCount, sizeof(RAWINPUTDEVICELIST)) != -1) {

                PRAWINPUTDEVICELIST inputDeviceList = (PRAWINPUTDEVICELIST)malloc(sizeof(RAWINPUTDEVICELIST) * deviceCount);
                if (GetRawInputDeviceList(inputDeviceList, &deviceCount, sizeof(RAWINPUTDEVICELIST))) {

                    PRAWINPUTDEVICE rawInputDevices = (PRAWINPUTDEVICE)malloc(sizeof(RAWINPUTDEVICE) * deviceCount);

                    for (UINT i = 0; i < deviceCount; i++)
                    {
                        RAWINPUTDEVICELIST inputDevice = inputDeviceList[i];
                        HANDLE hDevice = inputDevice.hDevice;
                        DWORD deviceType = inputDevice.dwType;


                        UINT deviceInfoSize;
                        GetRawInputDeviceInfo(hDevice, RIDI_DEVICEINFO, NULL, &deviceInfoSize);

                        RID_DEVICE_INFO deviceInfo;
                        deviceInfo.cbSize = sizeof(RID_DEVICE_INFO);

                        GetRawInputDeviceInfo(hDevice, RIDI_DEVICEINFO, &deviceInfo, &deviceInfoSize);

                        if (deviceInfo.hid.usUsagePage == 0x0D && deviceInfo.hid.usUsage == 0x05) // Touchpad
                        {
                            // Touchpad detected - now get its dimensions
                             OutputDebugString(L"AKAKAK");

                             int width, height;
                             GetTouchpadDimensions(hDevice, width, height);

                             touchpadSize.x = width;
                             touchpadSize.y = height;
                        }

                        switch (inputDevice.dwType)
                        {
                        case RIM_TYPEMOUSE:    OutputDebugString(L"Mouse\n"); break;
                        case RIM_TYPEKEYBOARD: OutputDebugString(L"Keyboard\n"); break;
                        case RIM_TYPEHID:
                            OutputDebugString(L"HID\n");
                            break;
                        default:               OutputDebugString(L"Unknown (%d)\n"); break;
                        }

                        RAWINPUTDEVICE rawInputDevice;
                        rawInputDevice.usUsagePage = deviceInfo.hid.usUsagePage;
                        rawInputDevice.usUsage = deviceInfo.hid.usUsage;
                        rawInputDevice.dwFlags = RIDEV_INPUTSINK;
                        rawInputDevice.hwndTarget = hWnd;

                        if (!RegisterRawInputDevices(&rawInputDevice, 1, sizeof(RAWINPUTDEVICE))) {
                            OutputDebugString(L"Failed to register raw input devices\n");
                        }
                    }
                }
                else free(inputDeviceList);
            }
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            RECT rc;
            GetClientRect(hWnd, &rc);

            if (!hbmBuffer)
                hbmBuffer = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);

            HBITMAP hbmOld = (HBITMAP)SelectObject(hdcBuffer, hbmBuffer);

            HBRUSH hbrBkgnd = CreateSolidBrush(GetSysColor(COLOR_WINDOW));
            FillRect(hdcBuffer, &rc, hbrBkgnd);
            DeleteObject(hbrBkgnd);

            for (int i = 0; i < touchCount; i++)
            {
                

                TOUCH touch = touches[i];

                double percentX = (double)touch.rect.left / (double)touchpadSize.x;
                double percentY = (double)touch.rect.top / (double)touchpadSize.y;

                UINT x = percentX * windowSize.x;
                UINT y = percentY * windowSize.y;

                RECT touchRect;
                touchRect.left = x;
                touchRect.top = y;
                touchRect.right = x + touch.rect.right * 10;
                touchRect.bottom = y + touch.rect.bottom * 10;

                int colourPressure = touch.size * 2;
                if (colourPressure < 100) colourPressure = colourPressure * 2;
                else colourPressure += 100;
                if (colourPressure > 255) colourPressure = 255;

                HBRUSH hbr = CreateSolidBrush(RGB(colourPressure, -colourPressure + 255, touch.size));
                FillRect(hdcBuffer, &touchRect, hbr);
                DeleteObject(hbr);
            }

            BitBlt(hdc, 0, 0, rc.right, rc.bottom, hdcBuffer, 0, 0, SRCCOPY);

            SelectObject(hdcBuffer, hbmOld);

            EndPaint(hWnd, &ps);
        }
        break;
    case WM_ERASEBKGND:
        return 1;
    case WM_INPUT:
    {
        OutputDebugString(L"Don't touch me bro.");

        HRAWINPUT hRawInput = (HRAWINPUT)lParam;
        UINT size = 0;

        GetRawInputData(hRawInput, RID_INPUT, NULL, &size, sizeof(RAWINPUTHEADER)) == 0;

        BYTE* buffer = new BYTE[size];
        if (GetRawInputData(hRawInput, RID_INPUT, buffer, &size, sizeof(RAWINPUTHEADER)) == size)
        {
            RAWINPUT* raw = (RAWINPUT*)buffer;

            if (raw->header.dwType == RIM_TYPEHID)
            {
                std::wstringstream ss;
                if (IsTouchpadDevice(raw->header.hDevice))
                {
                    ss << L"HID Data (" << raw->data.hid.dwSizeHid << L" bytes): ";

                    for (DWORD i = 0; i < raw->data.hid.dwSizeHid; i++)
                        ss << std::hex << std::setw(2) << std::setfill(L'0') << static_cast<int>(raw->data.hid.bRawData[i]) << L" ";
               
                    TOUCHPAD_EVENT touch = *(TOUCHPAD_EVENT*)(raw->data.hid.bRawData);

                    touchCount = touch.fingers >> 4;
                    ss << std::dec << "So many fingers: " << touchCount << "\n";

                    for (size_t i = 0; i < touchCount; i++)
                    {
                        TOUCH_POSITION position = touch.positions[i];
                        UINT x = (((WORD)position.x.high) << 8) + position.x.low;
                        UINT y = (((WORD)position.y.high) << 8) + position.y.low;


                        TOUCH_SIZE size = touch.sizes[i];
                        int width = size.dimensions >> 4;
                        int height = size.dimensions & 0b00001111;
                        ss << "Finger " << i << " is at X: " << x << " Y: " << y << " with size: " << size.size << " width: " << width << " height: " << height << "\n";

                        touches[i].rect.left = x;
                        touches[i].rect.top = y;
                        touches[i].rect.right = width;
                        touches[i].rect.bottom = height;
                        touches[i].size = size.size;
                    }
                }
                else
                {
                    OutputDebugString(L"Ignoring non-touchpad HID input\n");
                }


                // Update label
                SetWindowText(hLabel, ss.str().c_str());
                InvalidateRect(hWnd, NULL, TRUE);
            }
        }
        delete[] buffer;

        break;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
