#define _WIN32_WINNT 0x0A00
#include "resource.h"
#pragma GCC diagnostic ignored  "-Wunused-parameter"
#pragma GCC diagnostic ignored  "-Wunused-variable"
#define CLASS_NAME              L"Franklin"
#define SUBCLASS_NAME           L"TodayScheduleMessageWindow";
#define max(a,b)                (((a) > (b)) ? (a) : (b))

LRESULT CALLBACK WndProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK TodayScheduleWndProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam);

int APIENTRY wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int nCmdShow)
{
    WNDCLASSEX wcex = {
        sizeof(wcex),
        CS_HREDRAW | CS_VREDRAW,
        WndProc,
        0,0,
        hInst,
        NULL, LoadCursor(NULL, IDC_ARROW),
        (HBRUSH)(COLOR_WINDOW + 1),
        NULL,
        CLASS_NAME,
        NULL
    };
    RegisterClassEx(&wcex);

    wcex.hInstance = hInst;
    wcex.lpszClassName = SUBCLASS_NAME;
    wcex.lpfnWndProc = (WNDPROC)TodayScheduleWndProc;
    RegisterClassEx(&wcex);

    HWND hWnd = CreateWindowEx(
            WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
            CLASS_NAME,
            CLASS_NAME,
            WS_OVERLAPPEDWINDOW,
            0,0,0,0,
            NULL,
            (HMENU)NULL,
            hInst,
            NULL
            );

    ShowWindow(hWnd, nCmdShow);

    MSG msg;
    while(GetMessage(&msg, NULL, 0,0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

#define TRAY_NOTIFY         (WM_APP + 1)
#define WM_TODAYSCHEDULE	(WM_APP + 2)
LRESULT CALLBACK WndProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam)
{
    static BOOL bBeep, bShowList;
    static HWND MsgWnd[0x20];
    static int Items;

    NOTIFYICONDATA nid;
    HMENU hMenu, hPopupMenu;
    POINT Mouse;

    SYSTEMTIME st;
    WCHAR Temp[0x20];
    DWORD dwStyle, dwExStyle;

    RECT crt, WorkArea;
    APPBARDATA abData;
    int ScreenWidth, ScreenHeight;
    int TaskBarHeight, Width, Height, x, y;
    int MaxLength, TextHeight;

    WCHAR Times[0x20][0x20];
    WCHAR Messages[0x20][0x100];
    WCHAR DisplayMessage[0x20][0x120];
    TEXTMETRIC tm;
    SIZE TextSize;
    HDC hdc;

    switch(iMessage) {
        case WM_CREATE:
            SystemParametersInfo(SPI_GETWORKAREA, 0, &WorkArea, 0);
            bBeep = bShowList = FALSE;
            Items = 0;
            ZeroMemory(&nid, sizeof(nid));
            nid.cbSize = sizeof(NOTIFYICONDATA);
            nid.hWnd = hWnd;
            nid.uID = 0;
            nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
            nid.uCallbackMessage = TRAY_NOTIFY;
            nid.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1));
            wcscpy(nid.szTip, L"예약된 알람이 없습니다.");
            Shell_NotifyIcon(NIM_ADD, &nid);
            return 0;

        case TRAY_NOTIFY:
            switch(lParam){
                case WM_RBUTTONDOWN:
                    hMenu = LoadMenu(GetModuleHandle(NULL), MAKEINTRESOURCE(IDR_MENU1));
                    hPopupMenu = GetSubMenu(hMenu, 0);
                    GetCursorPos(&Mouse);
                    if(bBeep){
                        CheckMenuItem(hMenu, IDM_BEEP, MF_BYCOMMAND | MF_CHECKED);
                    }else{
                        CheckMenuItem(hMenu, IDM_BEEP, MF_BYCOMMAND | MF_UNCHECKED);
                    }
                    SetForegroundWindow(hWnd);
                    TrackPopupMenu(hPopupMenu, TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON, Mouse.x, Mouse.y, 0, hWnd, NULL);
                    SetForegroundWindow(hWnd);
                    DestroyMenu(hPopupMenu);
                    DestroyMenu(hMenu);
                    break;

                case WM_LBUTTONDOWN:
                    SendMessage(hWnd, WM_COMMAND, MAKEWPARAM(IDM_MAKEITEM, BN_CLICKED), (LPARAM)hWnd);
                    break;
            }
            return 0;

        case WM_TIMER:
            switch(wParam){
                case 1:
                    break;
            }
            return 0;

        case WM_COMMAND:
            switch(LOWORD(wParam)){
                case IDM_BEEP:
                    bBeep = !bBeep;
                    break;

                case IDM_SHOWLIST:
                    hdc = GetDC(hWnd);
                    for(int i=0; i<Items; i++){
                        // wsprintf(DisplayMessage[i], L"%s, %s\r\n", Times[i], Messages[i]);
                        GetTextExtentPoint32(hdc, DisplayMessage[i], wcslen(DisplayMessage[i]), &TextSize);
                        MaxLength = max(MaxLength, TextSize.cx);
                    }
                    ReleaseDC(hWnd, hdc);

                    TextHeight = tm.tmHeight;
                    dwStyle	= WS_POPUP | WS_BORDER | WS_VISIBLE | WS_CLIPSIBLINGS;
                    dwExStyle = WS_EX_COMPOSITED;
                    SetRect(&crt, 0,0, MaxLength, TextHeight);
                    AdjustWindowRectEx(&crt, dwStyle, FALSE, dwExStyle);
                    if(dwStyle & WS_VSCROLL){ crt.right += GetSystemMetrics(SM_CXVSCROLL); }
                    if(dwStyle & WS_HSCROLL){ crt.bottom += GetSystemMetrics(SM_CYHSCROLL); }

                    ScreenWidth		= WorkArea.right - WorkArea.left;
                    ScreenHeight	= WorkArea.bottom - WorkArea.top;

                    memset(&abData, 0, sizeof(abData));
                    abData.cbSize = sizeof(abData);
                    if(SHAppBarMessage(ABM_GETTASKBARPOS, &abData) & ABS_AUTOHIDE){
                        GetWindowRect(FindWindow(L"Shell_TrayWnd", NULL), &abData.rc);
                    }

                    TaskBarHeight	= abData.rc.bottom - abData.rc.top;
                    Width			= crt.right - crt.left;
                    Height			= crt.bottom - crt.top;
                    x				= 0;
                    y				= 0;

                    for(j=0; j<0x20; j++){
                        if(MsgWnd[i] && IsWindow(MsgWnd[i])){
                            DestroyWindow(MsgWnd[i]);
                        }else{ break; }
                    }

                    // MsgWnd[i] = CreateWindowEx(dwExStyle, SUBCLASS_NAME, NULL, dwStyle, x, y - ((Height - 18) * k), Width, Height, _hWnd, (HMENU)NULL, GetModuleHandle(NULL), NULL);
                    // SendMessage(MsgWnd[i], WM_TODAYSCHEDULE, (WPARAM)0, (LPARAM)DisplayMessage);
                    break;

                case IDM_MAKEITEM:
                    // TODO: 작업 추가 대화상자
                    break;

                case IDM_EXIT:
                    DestroyWindow(hWnd);
                    break;
            }
            return 0;

        case WM_DESTROY:
            KillTimer(hWnd, 1);
            nid.cbSize = sizeof(NOTIFYICONDATA);
            nid.hWnd = hWnd;
            nid.uID = 0;
            Shell_NotifyIcon(NIM_DELETE, &nid);
            PostQuitMessage(0);
            return 0;
    }

    return (DefWindowProc(hWnd, iMessage, wParam, lParam));
}

LRESULT CALLBACK TodayScheduleWndProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam){
    POINT	pt;
    WCHAR  *ptr, *Data;
    static BOOL		bInit, bDown;
    static HBITMAP	hBitmap;
    static RECT		crt;
    static int		ButtonWidth, ButtonHeight;

    int x,y,Left,Top, Length;
    UINT nHit;
    RECT brt;

    switch(iMessage){
        case WM_CREATE:
            ptr = Data = NULL;
            ButtonWidth = ButtonHeight = 16;
            bInit = FALSE;
            return 0;

        case WM_NCHITTEST:
            nHit		= DefWindowProc(hWnd, WM_NCHITTEST, wParam, lParam);
            pt.x		= LOWORD(lParam);
            pt.y		= HIWORD(lParam);
            ScreenToClient(hWnd, &pt);

            x	= crt.right - (crt.left + ButtonWidth + 2);
            y	= crt.top + 2;

            SetRect(&brt, x,y, x + ButtonWidth, y + ButtonHeight);

            if(nHit == HTCLIENT){
                if(!PtInRect(&brt, pt)){
                    nHit = HTCAPTION;
                }
            }
            return nHit;

        case WM_LBUTTONDOWN:
            pt.x = LOWORD(lParam);
            pt.y = HIWORD(lParam);

            x	= (crt.right - crt.left) - (ButtonWidth + 2);
            y	= crt.top + 2;

            if((pt.x > x) && (pt.x < (x + ButtonWidth)) && (pt.y > y) && (pt.y < (y + ButtonHeight))){
                bDown = TRUE;
            }else{
                bDown = FALSE;
            }
            return 0;

        case WM_LBUTTONUP:
            pt.x = LOWORD(lParam);
            pt.y = HIWORD(lParam);

            x	= (crt.right - crt.left) - (ButtonWidth + 2);
            y	= crt.top + 2;

            if(bDown){
                if((pt.x > x) && (pt.x < (x + ButtonWidth)) && (pt.y > y) && (pt.y < (y + ButtonHeight))){
                    DestroyWindow(hWnd);
                }
            }
            return 0;

        case WM_TODAYSCHEDULE:
            {
                // LPARAM으로 문자열 포인터 전달
                ptr		= (WCHAR*)lParam;
                Length	= wcslen(ptr);
                Data	= (WCHAR*)malloc(sizeof(WCHAR) * (Length + 1));
                wsprintf(Data, L"%s", ptr);
                SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)Data);

                bInit = TRUE;
                InvalidateRect(hWnd, NULL, TRUE);
            }
            return 0;

        case WM_PAINT:
            {
                PAINTSTRUCT ps;

                HDC hdc = BeginPaint(hWnd, &ps);

                if(bInit == TRUE){
                    SetBkMode(hdc, TRANSPARENT);

                    GetClientRect(hWnd, &crt);
                    FillRect(hdc, &crt, GetSysColorBrush(COLOR_BTNFACE));

                    x	= (crt.right - crt.left) - (ButtonWidth + 2);
                    y	= crt.top + 2;

                    Rectangle(hdc, x, y, x + ButtonWidth, y + ButtonHeight);
                    MoveToEx(hdc, x, y, NULL);
                    LineTo(hdc, x + ButtonWidth, y + ButtonHeight);

                    MoveToEx(hdc, x + ButtonWidth, y, NULL);
                    LineTo(hdc, x, y + ButtonHeight);

                    Data	= (WCHAR*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
                    if(Data){
                        TEXTMETRIC tm;
                        GetTextMetrics(hdc, &tm);

                        SIZE TextSize;
                        GetTextExtentPoint32(hdc, Data, wcslen(Data), &TextSize);

                        Left	= ((crt.right - crt.left) * 0.5) - (TextSize.cx * 0.5);
                        Top		= ((crt.bottom - crt.top) * 0.5) - (tm.tmHeight * 0.5) + ButtonHeight;

                        TextOut(hdc, Left, Top, Data, wcslen(Data));
                    }
                }

                EndPaint(hWnd, &ps);
            }
            return 0;

        case WM_DESTROY:
            Data = (WCHAR*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
            if(Data){ free(Data); }
            return 0;
    }

    return (DefWindowProc(hWnd, iMessage, wParam, lParam));
}

