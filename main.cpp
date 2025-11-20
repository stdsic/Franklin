#define _WIN32_WINNT 0x0A00
#include "resource.h"
#include <shobjidl.h>
#include <commctrl.h>
#include <math.h>
#include <gdiplus.h>
using namespace Gdiplus;

#pragma GCC diagnostic ignored  "-Wunused-parameter"
#pragma GCC diagnostic ignored  "-Wunused-variable"
#define CLASS_NAME              L"Franklin"
#define SUBCLASS_NAME           L"TodayScheduleMessageWindow"
#define RGN_CLASS_NAME          L"VisualRgnWndClass"
#define max(a,b)                (((a) > (b)) ? (a) : (b))
#define min(a,b)                (((a) < (b)) ? (a) : (b))
#define GET_X_LPARAM(lParam)    (int)(short)((lParam) & 0xFFFF)
#define GET_Y_LPARAM(lParam)    (int)(short)((lParam) >> 16 & 0xFFFF)

LRESULT CALLBACK WndProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK TodayScheduleWndProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK DlgProc(HWND hDlg, UINT iMessage, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK DeleteDlgProc(HWND hDlg, UINT iMessage, WPARAM wParam, LPARAM lParam);

int APIENTRY wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int nCmdShow)
{
    WNDCLASSEX wcex = {
        sizeof(wcex),
        CS_HREDRAW | CS_VREDRAW,
        WndProc,
        0,0,
        hInst,
        NULL, LoadCursor(NULL, IDC_ARROW),
        NULL,
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
            WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
            CLASS_NAME,
            CLASS_NAME,
            WS_POPUP | WS_VISIBLE,
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

typedef enum tag_Part { AM, PM } Part;

typedef struct tag_inParam{
    int Hour, Minute;
    BOOL bFlag;
    COLORREF color;
    Part VisualPart;
    WCHAR Message[0x100];
}InputParam;

typedef struct tag_outParam{
    int nItems, nDelete;
    InputParam *ptr;
}OutputParam;

typedef struct tag_Tick {
    float x, y;
}Tick;

typedef struct tag_bwAttributes{
    COLORREF	rgb;
    BYTE		Opacity;
    DWORD		Flags;
}bwAttributes;

void DrawPiece(HDC hdc, POINT Origin, int iRadius, float AngleStartDeg, float AngleEndDeg, InputParam param);
void DrawBkPaper(HDC hdc, POINT Origin, int iRadius, HBITMAP hBitmap);
void DrawOutLine(HDC hdc, POINT Origin, int iRadius);
void DrawTick(HDC hdc, POINT Origin, int iRadius, BOOL IsDark);
void DrawHand(HDC hdc, POINT Origin, int iRadius, BOOL IsDark);
COLORREF GetAverageColor(HDC hdc, int x, int y, int rad);
BOOL IsColorDark(COLORREF color);
HBITMAP LoadFile();

ULONG GetMagicNumber(ULONG Divisor);
ULONG MyMod(ULONG Dividend, ULONG Magic, ULONG Divisor);
void SetAttribute(HWND hWnd, bwAttributes Attr);
void GetAttribute(HWND hWnd, bwAttributes *Attr);
void InsertionSort(InputParam *DataSet, int Length);

LRESULT CALLBACK WndProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam)
{
    static const int MaxSize = 0x20;
    static BOOL bAlertMsg, bAlertBeep, bBeepSnd;
    static HWND MsgWnd[MaxSize];
    static HWND AlertWnd[MaxSize];
    static int Items, BeepCnt;
    static WCHAR Times[MaxSize][0x20];
    static WCHAR Messages[MaxSize][0x100];
    static WCHAR DisplayMessage[MaxSize][0x120];
    static HICON hAlarmClock, hAlarmOn; //, hBallonTitle;

    NOTIFYICONDATA nid;
    HMENU hMenu, hPopupMenu;
    POINT Mouse;

    SYSTEMTIME st;
    WCHAR Temp[0x20];
    DWORD dwStyle, dwExStyle;

    RECT srt, crt, wrt, WorkArea;
    APPBARDATA abData;
    int ScreenWidth, ScreenHeight;
    int TaskBarHeight, Width, Height, x, y;
    int MaxLength, TextHeight;

    TEXTMETRIC tm;
    SIZE TextSize;

    // 본래 DialogBoxParam으로 전달할 때는 static일 필요가 없다.
    // 단, 최대 개수를 32개로 정해뒀으므로 static 키워드를 붙이고 배열로 관리하기로 한다.
    // 이러면 파싱 함수가 필요없고 멤버 변수인 Hour, Min을 이용하여 비교 하면된다.
    static InputParam param[MaxSize];
    OutputParam outParam;
    INT_PTR dlgret;

    MONITORINFO mi;
    HMONITOR hMonitor;
    int cnt;

    HRESULT hr;
    INITCOMMONCONTROLSEX icex;

    HRGN hWndRgn;
    int sx,sy, cx,cy, BorderSize, Position;
    static BOOL bVisual;

    PAINTSTRUCT ps;
    HDC hdc, hMemDC;
    HGDIOBJ hOld;
    BITMAP bmp;

    static HBITMAP hBitmap;
    static RECT rcOrigin;
    static COLORREF CircleColor, AmColor, PmColor;
    static int ERadius;

    int iWidth, iHeight, iRadius;
    POINT Origin;
    HBRUSH hBrush, hOldBrush;
    HPEN hPen, hOldPen;
    static BOOL bTop;

    float AngleStart, AngleEnd;
    float HourAngle, MinuteAngle;
    int ItemHour, ItemMinute, ItemVisualPart;

    WORD lwParam;
    int Next;

    static Part CurrentVisualPart;

    int prevMode;
    COLORREF prevColor;
    static SIZE PartTextSize;

    static ULONG minMagic, hourMagic;
    ULONG uHourMod, uMinuteMod;

    static COLORREF clMask, clWhite, clBlack;
    static bwAttributes Attr;
    static HBITMAP hTempBitmap;
    HDC hTempDC;
    HGDIOBJ hTempOld;

    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    static HBITMAP hBkBitmap;
    HBITMAP hNewBitmap;
    COLORREF BkColor;
    BOOL IsDark;

    static BOOL bDeleteDlg;

    switch(iMessage) {
        case WM_CREATE:
            bDeleteDlg = FALSE;
            GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

            clWhite = RGB(255, 255, 255);
            clBlack = RGB(0, 0, 0);

            clMask = RGB(128, 128, 128);
            Attr = {clMask, 255, LWA_ALPHA | LWA_COLORKEY};
            SetAttribute(hWnd, Attr);

            minMagic = GetMagicNumber(60);
            hourMagic = GetMagicNumber(12);

            GetLocalTime(&st);
            CurrentVisualPart = (st.wHour < 12) ? AM : PM;

            AmColor = RGB(255, 165, 0);
            PmColor = RGB(135, 206, 235);
            CircleColor = (CurrentVisualPart == AM) ? AmColor : PmColor;

            hdc = GetDC(hWnd);
            GetTextExtentPoint32(hdc, L"AM", 2, &PartTextSize);
            ReleaseDC(hWnd, hdc);

            srand((unsigned int)GetTickCount());

            bTop = bVisual = FALSE;
            ERadius = max(PartTextSize.cx, PartTextSize.cy) + 8 >> 1;

            // 알림 센터에서 인식하는 모듈 ID설정
            hr = SetCurrentProcessExplicitAppUserModelID(L"Franklin Alert");
            bBeepSnd = bAlertMsg = bAlertBeep = TRUE;
            dlgret = Items = 0;
            memset(&icex, 0, sizeof(icex));
            icex.dwSize = sizeof(icex);
            icex.dwICC = ICC_LISTVIEW_CLASSES;
            InitCommonControlsEx(&icex);
            hAlarmClock = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1));
            hAlarmOn = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON2));

            memset(&param, 0, sizeof(param));
            ZeroMemory(&nid, sizeof(NOTIFYICONDATA));
            nid.cbSize = sizeof(NOTIFYICONDATA);
            nid.hWnd = hWnd;
            nid.uID = 1201;
            nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
            wcscpy(nid.szTip, L"예약된 알람이 없습니다.");
            nid.uCallbackMessage = TRAY_NOTIFY;
            nid.hIcon = hAlarmClock;
            Shell_NotifyIcon(NIM_ADD, &nid);

            RegisterHotKey(hWnd, 0x0000, MOD_CONTROL | MOD_SHIFT, L'E');
            RegisterHotKey(hWnd, 0x0001, MOD_CONTROL | MOD_SHIFT, L'V');
            RegisterHotKey(hWnd, 0x0002, MOD_CONTROL | MOD_SHIFT, L'D');
            RegisterHotKey(hWnd, 0x0003, MOD_CONTROL | MOD_SHIFT, L'X');
            RegisterHotKey(hWnd, 0x0004, MOD_CONTROL | MOD_SHIFT, L'A');
            SetTimer(hWnd, 1, 100, NULL);
            return 0;

        case WM_PAINT:
            hdc = BeginPaint(hWnd, &ps);
            if(bVisual){
                hMemDC = CreateCompatibleDC(hdc);
                hTempDC = CreateCompatibleDC(hdc);

                GetClientRect(hWnd, &crt);
                if(hBitmap == NULL){
                    hBitmap = CreateCompatibleBitmap(hdc, crt.right, crt.bottom);
                    hTempBitmap = CreateCompatibleBitmap(hdc, crt.right, crt.bottom);
                }
                hOld = SelectObject(hMemDC, hBitmap);
                hTempOld = SelectObject(hTempDC, hTempBitmap);

                iWidth = crt.right - crt.left;
                iHeight = crt.bottom - crt.top;
                iRadius = min(iWidth, iHeight) >> 1;

                Origin = {iWidth >> 1, iHeight >> 1};

                hBrush = CreateSolidBrush(clMask);
                FillRect(hMemDC, &crt, hBrush);
                FillRect(hTempDC, &crt, hBrush);
                DeleteObject(hBrush);
                {// Draw Pie
                    int Next = 0;
                    for(int i=0; i<Items; i++){
                        Next = (i+1) % Items;
                        if(Items > 1 && param[i].Hour == param[Next].Hour && param[i].Minute == param[Next].Minute){ continue; }

                        ItemVisualPart = param[i].VisualPart;
                        if(ItemVisualPart != CurrentVisualPart){ continue; }

                        ItemHour = param[i].Hour;
                        ItemMinute = param[i].Minute;

                        uHourMod = MyMod(ItemHour, hourMagic, 12);
                        HourAngle = (FLOAT)uHourMod * 30.f;

                        uMinuteMod = MyMod(ItemMinute, minMagic, 60);
                        MinuteAngle = ((uMinuteMod > 0) ? 0.5f * (FLOAT)(uMinuteMod) : 0.f);

                        AngleStart = HourAngle + MinuteAngle;

                        ItemHour = param[Next].Hour;
                        ItemMinute = param[Next].Minute;

                        uHourMod = MyMod(ItemHour, hourMagic, 12);
                        HourAngle = (FLOAT)uHourMod * 30.f;

                        uMinuteMod = MyMod(ItemMinute, minMagic, 60);
                        MinuteAngle = ((uMinuteMod > 0) ? 0.5f * (FLOAT)(uMinuteMod) : 0.f);

                        AngleEnd = HourAngle + MinuteAngle;
                        
                        DrawPiece(hTempDC, Origin, iRadius, AngleStart, ((Next == 0) ? 360.f : AngleEnd), param[i]);
                    }
                }

                {// Draw OutLine
                    DrawOutLine(hTempDC, Origin, iRadius);
                }

                if(hBkBitmap){// Draw Background Bitmap
                    DrawBkPaper(hTempDC, Origin, iRadius, hBkBitmap);
                }

                {// Check BkColor;
                    BkColor = GetAverageColor(hTempDC, Origin.x, Origin.y, iRadius * 0.2f);
                    IsDark = IsColorDark(BkColor);
                }

                {// Draw Tick
                    DrawTick(hTempDC, Origin, iRadius, IsDark);
                }

                {// Draw Hand
                    DrawHand(hTempDC, Origin, iRadius, IsDark);
                }

                {// Draw Origin Circle
                    if(IsDark){
                        CircleColor = clWhite;
                    }else{
                        CircleColor = clBlack;
                    }

                    hBrush = CreateSolidBrush(CircleColor);
                    hOldBrush = (HBRUSH)SelectObject(hTempDC, hBrush);
                    hPen = CreatePen(PS_SOLID, 1, RGB(0,0,0));
                    hOldPen = (HPEN)SelectObject(hTempDC, hPen);

                    prevMode = SetBkMode(hTempDC, TRANSPARENT);
                    prevColor = SetTextColor(hTempDC, IsDark ? clBlack : clWhite);

                    Ellipse(hTempDC, Origin.x - ERadius, Origin.y - ERadius, Origin.x + ERadius, Origin.y + ERadius);
                    SetRect(&rcOrigin, Origin.x - ERadius, Origin.y - ERadius, Origin.x + ERadius, Origin.y + ERadius);

                    TextOut(hTempDC, Origin.x - PartTextSize.cx * 0.5, Origin.y - PartTextSize.cy * 0.5, (CurrentVisualPart == AM) ? L"AM" : L"PM", 2);

                    GetObject(hTempBitmap, sizeof(BITMAP), &bmp);
                    TransparentBlt(hMemDC, 0,0, bmp.bmWidth, bmp.bmHeight, hTempDC, 0,0, bmp.bmWidth, bmp.bmHeight, Attr.rgb);

                    SetBkMode(hTempDC, prevMode);
                    SetTextColor(hTempDC, prevColor);

                    SelectObject(hTempDC, hOldBrush);
                    SelectObject(hTempDC, hOldPen);
                    DeleteObject(hPen);
                }

                GetObject(hBitmap, sizeof(BITMAP), &bmp);
                BitBlt(hdc, 0,0, bmp.bmWidth, bmp.bmHeight, hMemDC, 0,0, SRCCOPY);

                SelectObject(hTempDC, hTempOld);
                SelectObject(hMemDC, hOld);
                DeleteDC(hTempDC);
                DeleteDC(hMemDC);
            }
            EndPaint(hWnd, &ps);
            return 0;

        case WM_HOTKEY:
            switch(wParam){
                case 0:
                    SendMessage(hWnd, WM_COMMAND, MAKEWPARAM(IDM_MAKEITEM, 0), (LPARAM)hWnd);
                    break;

                case 1:
                    if(bVisual){
                        if(FindWindow(NULL, CLASS_NAME)){
                            ShowWindow(hWnd, SW_RESTORE);
                            SetForegroundWindow(hWnd);
                        }
                    }else{
                        SendMessage(hWnd, WM_COMMAND, MAKEWPARAM(IDM_VISUAL, 0), (LPARAM)hWnd);
                    }
                    break;

                case 2:
                    SendMessage(hWnd, WM_COMMAND, MAKEWPARAM(IDM_DELETEITEM, 0), (LPARAM)hWnd);
                    break;

                case 3:
                    SendMessage(hWnd, WM_COMMAND, MAKEWPARAM(IDM_CLEARPAST, 0), (LPARAM)hWnd);
                    break;

                case 4:
                    SendMessage(hWnd, WM_COMMAND, MAKEWPARAM(IDM_TOPMOST, 0), (LPARAM)hWnd);
                    break;
            }
            return 0;

        case WM_LBUTTONDOWN:
            CurrentVisualPart = (Part)((CurrentVisualPart + 1) % 2);
            CircleColor = (CurrentVisualPart == AM) ? AmColor : PmColor;
            InvalidateRect(hWnd, NULL, FALSE);
            return 0;

        case WM_RBUTTONDOWN:
            /*
            // 배경을 바꾸겠다는 의지 표명으로 보고 기존 배경 데이터는 보존하지 않기로 결정
            hNewBitmap = LoadFile();
            if(hNewBitmap != NULL){
            }
             */
            if(hBkBitmap != NULL){
                DeleteObject(hBkBitmap);
                hBkBitmap = NULL;
            }
            hBkBitmap = LoadFile(); 
            InvalidateRect(hWnd, NULL, FALSE);
            return 0;

        case WM_NCHITTEST:
            if(bVisual){
                Mouse = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                GetWindowRect(hWnd, &wrt);

                if(Mouse.x >= wrt.left && Mouse.y >= wrt.top && Mouse.x <= wrt.right && Mouse.y <= wrt.bottom){
                    ScreenToClient(hWnd, &Mouse);
                    if(PtInRect(&rcOrigin, Mouse)){
                        return HTCLIENT;
                    }
                    return HTCAPTION;
                }
            }
            break;

        case WM_SIZE:
            if(wParam != SIZE_MINIMIZED){
                if(bVisual){
                    if(hTempBitmap){
                        DeleteObject(hTempBitmap);
                        hTempBitmap = NULL;
                    }

                    if(hBitmap){
                        DeleteObject(hBitmap);
                        hBitmap = NULL;
                    }
                }
            }
            return 0;

        case WM_KEYDOWN:
            switch(wParam){
                case VK_ESCAPE:
                    if(bVisual){
                        KillTimer(hWnd, 3);
                        bVisual = FALSE;
                        dwExStyle = GetWindowLongPtr(hWnd, GWL_EXSTYLE);
                        dwExStyle |= WS_EX_NOACTIVATE;
                        SetWindowLongPtr(hWnd, GWL_EXSTYLE, dwExStyle);
                        SetWindowPos(hWnd, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_HIDEWINDOW);
                    }
                    break;
            }
            return 0;

        case WM_COMMAND:
            lwParam = LOWORD(wParam);
            switch(lwParam){
                case IDM_HELP:
                    MessageBox(hWnd, L"위 프로그램은 윈도우 환경에서 실행 가능한 일정 관리 도구입니다.\r\n\r\n우측 하단의 파란색 종 모양 아이콘을 좌클릭하면 빠른 대화상자가 생성되며 일정 시작 시간을 입력하실 수 있습니다.\r\n\r\n아이콘 우클릭시 다양한 메뉴 항목을 확인하실 수 있으며 Visual 항목을 선택하면 하루 일정을 시각화하여 보여줍니다.\r\n\r\n시각화된 일정표는 정오를 기준으로 오전과 오후 일정을 나누어 따로 표현합니다.\r\n시간대(AM/PM)는 일정표 중앙에 위치한 원형 모양을 좌클릭하여 변경할 수 있습니다.\r\n또한, 원형 모양을 우클릭하면 시계의 배경 이미지를 설정할 수 있는 대화상자가 생성됩니다.\r\n\r\nv1.1.0부터 조합키를 지원하여 메뉴 항목을 빠르게 이용하실 수 있습니다.\r\n\r\n<조합키 목록>\r\n일정 추가: \t\tCtrl + Shift + E\r\n시각화: \t\t\tCtrl + Shift + V\r\n시각화 종료: \t\tESC\r\n일정 삭제: \t\tCtrl + Shift + D\r\n지난 일정 자동 삭제: \tCtrl + Shift + X\r\n항상 위에 표시: \tCtrl + Shift + A", L"v1.1.0 Franklin", MB_OK | MB_ICONINFORMATION);
                    break;

                case IDM_VISUAL:
                    bVisual = TRUE;
                    dwExStyle = GetWindowLongPtr(hWnd, GWL_EXSTYLE);
                    dwExStyle &= ~WS_EX_NOACTIVATE;
                    SetWindowLongPtr(hWnd, GWL_EXSTYLE, dwExStyle);

                    sx = GetSystemMetrics(SM_CXSCREEN);
                    sy = GetSystemMetrics(SM_CYSCREEN);
                    BorderSize = GetSystemMetrics(SM_CXEDGE);

                    cx = cy = 400;
                    SetWindowPos(hWnd, NULL, sx - cx, 0, cx, cy, SWP_NOZORDER | SWP_SHOWWINDOW | SWP_FRAMECHANGED);

                    hWndRgn = CreateEllipticRgn(BorderSize, BorderSize, cx-BorderSize, cy-BorderSize);
                    SetWindowRgn(hWnd, hWndRgn, FALSE);
                    SetTimer(hWnd, 3, 1000, NULL);
                    break;

                case IDM_TOPMOST:
                    bTop = !bTop;
                    SetWindowPos(hWnd, bTop ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
                    break;

                case IDM_BEEPOFF:
                    KillTimer(hWnd, 2);
                    break;

                case IDM_ALERTBEEP:
                    bAlertBeep = !bAlertBeep;
                    bBeepSnd = (bAlertBeep == TRUE) ? TRUE : FALSE;
                    break;

                case IDM_ALERTMSG:
                    bAlertMsg = !bAlertMsg;
                    break;

                case IDM_SHOWLIST:
                    if(Items > 0){
                        hdc = GetDC(hWnd);
                        memset(DisplayMessage, 0, sizeof(DisplayMessage));
                        for(int i=0; i<Items; i++){
                            wsprintf(DisplayMessage[i], L"%s, %s\r\n", Times[i], Messages[i]);
                            GetTextExtentPoint32(hdc, DisplayMessage[i], wcslen(DisplayMessage[i]), &TextSize);
                            MaxLength = max(MaxLength, TextSize.cx);
                        }
                        GetTextMetrics(hdc, &tm);
                        TextHeight = tm.tmHeight;
                        ReleaseDC(hWnd, hdc);

                        dwStyle	= WS_POPUP | WS_BORDER | WS_VISIBLE | WS_CLIPSIBLINGS;
                        dwExStyle = WS_EX_COMPOSITED | WS_EX_TOPMOST;
                        SetRect(&crt, 0,0, MaxLength, TextHeight);
                        AdjustWindowRectEx(&crt, dwStyle, FALSE, dwExStyle);

                        SystemParametersInfo(SPI_GETWORKAREA, 0, &WorkArea, 0);
                        ScreenWidth		= WorkArea.right - WorkArea.left;
                        ScreenHeight	= WorkArea.bottom - WorkArea.top;

                        memset(&abData, 0, sizeof(abData));
                        abData.cbSize = sizeof(abData);
                        if(SHAppBarMessage(ABM_GETTASKBARPOS, &abData) & ABS_AUTOHIDE){
                            GetWindowRect(FindWindow(L"Shell_TrayWnd", NULL), &abData.rc);
                        }

                        for(int i=0; i<MaxSize; i++){
                            if(MsgWnd[i] && IsWindow(MsgWnd[i])){
                                DestroyWindow(MsgWnd[i]);
                            }else{ break; }
                        }

                        TaskBarHeight	= abData.rc.bottom - abData.rc.top;
                        Width			= crt.right - crt.left + 4;
                        Height			= crt.bottom - crt.top + TextHeight + 18;
                        x				= ScreenWidth - Width;
                        y				= ScreenHeight - (TaskBarHeight + Height);

                        for(int i=0; i<Items; i++){
                            MsgWnd[i] = CreateWindowEx(dwExStyle, SUBCLASS_NAME, NULL, dwStyle, x, y - ((Height - 18) * i), Width, Height, hWnd, (HMENU)NULL, GetModuleHandle(NULL), NULL);
                            SendMessage(MsgWnd[i], WM_TODAYSCHEDULE, (WPARAM)0, (LPARAM)DisplayMessage[Items - (1 + i)]);
                        }
                    }
                    break;

                case IDM_MAKEITEM:
                    if(Items >= MaxSize){ 
                        if(IDYES == MessageBox(hWnd, L"알람을 더이상 등록할 수 없습니다(Max: 32)\r\n이전에 등록된 알람이 해제되면 다시 등록할 수 있습니다.\r\n정리 프로세스를 실행하시겠습니까?", L"정보", MB_ICONINFORMATION | MB_YESNO)){
                            PostMessage(hWnd, IDM_CLEARPAST, 0, 0);
                        }
                    }else{
                        // 부모 윈도우가 ACTIVATE 상태가 되지 않으므로 포커스 옮기려면 임의로 활성화 필요
                        SetForegroundWindow(hWnd);
                        dlgret = DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_ITEMSELECTOR1), hWnd, (DLGPROC)DlgProc, (LPARAM)&param[Items]);
                        if(dlgret == IDOK){
                            Items++;

                            ZeroMemory(&nid, sizeof(nid));
                            nid.cbSize = sizeof(NOTIFYICONDATA);
                            nid.hWnd = hWnd;
                            nid.uID = 1201;
                            nid.uFlags = NIF_TIP | NIF_ICON;
                            wsprintf(Temp, L"%d개의 알람이 있습니다.", Items);
                            wcscpy(nid.szTip, Temp);
                            nid.hIcon = hAlarmOn;
                            Shell_NotifyIcon(NIM_MODIFY, &nid);

                            InsertionSort(param, Items);
                        }
                    }
                    break;

                case IDM_DELETEITEM:
                    if(bDeleteDlg == FALSE){
                        bDeleteDlg = TRUE;
                        memset(&outParam, 0, sizeof(outParam));
                        outParam.nItems = Items;
                        outParam.ptr = &param[0];

                        dlgret = DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_ITEMSELECTOR2), hWnd, (DLGPROC)DeleteDlgProc, (LPARAM)&outParam);

                        cnt = outParam.nDelete;
                        Items -= cnt;

                        if(cnt > 0){
                            memset(DisplayMessage, 0, sizeof(DisplayMessage));
                            memset(Times, 0, sizeof(Times));
                            memset(Messages, 0, sizeof(Messages));

                            for(int i=0; i<Items; i++){
                                wsprintf(Times[i], L"%02d : %02d", outParam.ptr[i].Hour, outParam.ptr[i].Minute);
                                if(outParam.ptr[i].Message == NULL){
                                    wsprintf(Messages[i], L"");
                                }else{
                                    wsprintf(Messages[i], L"%s", outParam.ptr[i].Message);
                                }
                                wsprintf(DisplayMessage[i], L"%s, %s\r\n", Times[i], Messages[i]);
                            }

                            ZeroMemory(&nid, sizeof(nid));
                            nid.cbSize = sizeof(NOTIFYICONDATA);
                            nid.uFlags = NIF_TIP | NIF_ICON;
                            nid.hWnd = hWnd;
                            nid.uID = 1201;

                            wsprintf(Temp, L"%d개의 알람을 정리하였습니다.", cnt);
                            nid.uFlags |= NIF_INFO;
                            nid.dwInfoFlags = NIIF_INFO;
                            wcscpy(nid.szInfo, Temp);
                            wcscpy(nid.szInfoTitle, L"정보");

                            if(Items > 0){
                                wsprintf(Temp, L"%d개의 알람이 있습니다.", Items);
                                nid.hIcon = hAlarmOn;
                            }else{
                                wsprintf(Temp, L"예약된 알람이 없습니다.");
                                nid.hIcon = hAlarmClock;
                            }

                            wcscpy(nid.szTip, Temp);
                            Shell_NotifyIcon(NIM_MODIFY, &nid);
                        }
                        bDeleteDlg = FALSE;
                    }
                    break;

                case IDM_CLEARPAST:
                    if(Items > 0){
                        cnt = 0;
                        GetLocalTime(&st);
                        for(int i=Items - 1; i>=0; i--){
                            if(param[i].bFlag == TRUE || param[i].Hour < st.wHour || (param[i].Hour == st.wHour && param[i].Minute < st.wMinute)){
                                memmove(param + i, param + i + 1, sizeof(InputParam) * (Items - i - 1));
                                memmove(Times + i, Times + i + 1, sizeof(Times[i]));
                                memmove(Messages+ i, Messages + i + 1, sizeof(Messages[i]));

                                Items--;
                                cnt++;
                            }
                        }
                        memset(param + Items, 0, sizeof(InputParam) * (MaxSize - Items));
                        memset(Times + Items, 0, sizeof(Times[0]) * (MaxSize - Items));
                        memset(Messages+ Items, 0, sizeof(Messages[0]) * (MaxSize - Items));

                        ZeroMemory(&nid, sizeof(nid));
                        nid.cbSize = sizeof(NOTIFYICONDATA);
                        nid.hWnd = hWnd;
                        nid.uID = 1201;
                        nid.uFlags = NIF_TIP | NIF_INFO | NIF_ICON;
                        nid.dwInfoFlags = NIIF_INFO;
                        if(cnt > 0){
                            wsprintf(Temp, L"%d개의 알람을 정리하였습니다.", cnt);
                        }else{
                            wsprintf(Temp, L"정리할 알람이 없습니다.");
                        }
                        wcscpy(nid.szInfo, Temp);
                        wcscpy(nid.szInfoTitle, L"정보");
                        if(Items > 0){
                            wsprintf(Temp, L"%d개의 알람이 있습니다.", Items);
                            nid.hIcon = hAlarmOn;
                        }else{
                            wsprintf(Temp, L"예약된 알람이 없습니다.");
                            nid.hIcon = hAlarmClock;
                        }
                        wcscpy(nid.szTip, Temp);
                        Shell_NotifyIcon(NIM_MODIFY, &nid);
                    }
                    break;

                case IDM_EXIT:
                    DestroyWindow(hWnd);
                    break;
            }

            if(bVisual && (lwParam == IDM_MAKEITEM || lwParam == IDM_CLEARPAST || lwParam == IDM_DELETEITEM)){
                InvalidateRect(hWnd, NULL, FALSE);
            }
            return 0;

        case TRAY_NOTIFY:
            switch(LOWORD(lParam)){
                case WM_RBUTTONDOWN:
                    hMenu = LoadMenu(GetModuleHandle(NULL), MAKEINTRESOURCE(IDR_MENU1));
                    hPopupMenu = GetSubMenu(hMenu, 0);
                    GetCursorPos(&Mouse);
                    if(bAlertBeep){
                        CheckMenuItem(hMenu, IDM_ALERTBEEP, MF_BYCOMMAND | MF_CHECKED);
                    }else{
                        CheckMenuItem(hMenu, IDM_ALERTBEEP, MF_BYCOMMAND | MF_UNCHECKED);
                    }
                    if(bAlertMsg){
                        CheckMenuItem(hMenu, IDM_ALERTMSG, MF_BYCOMMAND | MF_CHECKED);
                    }else{
                        CheckMenuItem(hMenu, IDM_ALERTMSG, MF_BYCOMMAND | MF_UNCHECKED);
                    }
                    if(bVisual){
                        EnableMenuItem(hMenu, IDM_TOPMOST, MF_BYCOMMAND | MF_ENABLED);
                    }else{
                        EnableMenuItem(hMenu, IDM_TOPMOST, MF_BYCOMMAND | MF_GRAYED);
                    }
                    if(bTop){
                        CheckMenuItem(hMenu, IDM_TOPMOST, MF_BYCOMMAND | MF_CHECKED);
                    }else{
                        CheckMenuItem(hMenu, IDM_TOPMOST, MF_BYCOMMAND | MF_UNCHECKED);
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
                    GetLocalTime(&st);
                    for(int i=0; i<Items; i++){
                        if(st.wHour == param[i].Hour && st.wMinute == param[i].Minute && param[i].bFlag == FALSE){
                            param[i].bFlag = TRUE;
                            if(bAlertMsg){
                                dwStyle	= WS_POPUP | WS_BORDER | WS_VISIBLE | WS_CLIPSIBLINGS;
                                dwExStyle = WS_EX_COMPOSITED | WS_EX_TOPMOST;

                                SetRect(&crt, 0,0, 300, 200);
                                AdjustWindowRectEx(&crt, dwStyle, FALSE, dwExStyle);

                                SystemParametersInfo(SPI_GETWORKAREA, 0, &WorkArea, 0);

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
                                x				= ScreenWidth - Width >> 1;
                                y				= ScreenHeight - (TaskBarHeight + Height) >> 1;

                                memset(Times[i], 0, sizeof(Times[i]));
                                memset(Messages[i], 0, sizeof(Messages[i]));
                                memset(DisplayMessage[i], 0, sizeof(DisplayMessage[i]));

                                wsprintf(Times[i], L"%02d : %02d", param[i].Hour, param[i].Minute);
                                if(param[i].Message == NULL){
                                    wsprintf(Messages[i], L"");
                                }else{
                                    wsprintf(Messages[i], L"%s", param[i].Message);
                                }
                                wsprintf(DisplayMessage[i], L"%s, %s", Times[i], Messages[i]);

                                if(IsWindow(AlertWnd[i])){ DestroyWindow(AlertWnd[i]); AlertWnd[i] = NULL; }
                                AlertWnd[i] = CreateWindowEx(dwExStyle, SUBCLASS_NAME, NULL, dwStyle, x, y, Width, Height, hWnd, (HMENU)NULL, GetModuleHandle(NULL), NULL);
                                SendMessage(AlertWnd[i], WM_TODAYSCHEDULE, (WPARAM)0, (LPARAM)DisplayMessage[i]);
                            }

                            if(bAlertBeep){
                                BeepCnt = 0;
                                SetTimer(hWnd, 2, 5000, NULL);
                                SendMessage(hWnd, WM_TIMER, 2, 0);
                            }
                        }
                    }
                    break;

                case 2:
                    if(BeepCnt < 12){
                        MessageBeep(0);
                        BeepCnt++;
                    }else{
                        KillTimer(hWnd, 2);
                    }
                    break;

                case 3:
                    if(bVisual){
                        // 큐에 메세지가 없으면 WM_PAINT 메세지가 처리된다는 점을 이용한다.
                        // 분침과 초침은 지원하지 않을 생각이므로 이 정도면 충분하다.
                        InvalidateRect(hWnd, NULL, FALSE);
                    }else{
                        KillTimer(hWnd, 3);
                    }
                    break;
            }
            return 0;

        case WM_WINDOWPOSCHANGING:
            {
                MONITORINFOEX miex;
                memset(&miex, 0, sizeof(miex));
                miex.cbSize = sizeof(miex);

                HMONITOR hCurrentMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
                GetMonitorInfo(hCurrentMonitor, &miex);

                LPWINDOWPOS lpwp = (LPWINDOWPOS)lParam;
                int SideSnap = 10;

                if (abs(lpwp->x - miex.rcMonitor.left) < SideSnap) {
                    lpwp->x = miex.rcMonitor.left;
                } else if (abs(lpwp->x + lpwp->cx - miex.rcMonitor.right) < SideSnap) {
                    lpwp->x = miex.rcMonitor.right - lpwp->cx;
                } 
                if (abs(lpwp->y - miex.rcMonitor.top) < SideSnap) {
                    lpwp->y = miex.rcMonitor.top;
                } else if (abs(lpwp->y + lpwp->cy - miex.rcMonitor.bottom) < SideSnap) {
                    lpwp->y = miex.rcMonitor.bottom - lpwp->cy;
                }
            }
            return 0;

        case WM_DESTROY:
			UnregisterHotKey(hWnd, 0x0000);
			UnregisterHotKey(hWnd, 0x0001);
			UnregisterHotKey(hWnd, 0x0002);
			UnregisterHotKey(hWnd, 0x0003);
			UnregisterHotKey(hWnd, 0x0004);
            GdiplusShutdown(gdiplusToken);
            if(hTempBitmap){DeleteObject(hTempBitmap);}
            if(hBkBitmap){DeleteObject(hBkBitmap);}
            if(hBitmap){DeleteObject(hBitmap);}
            KillTimer(hWnd, 1);
            KillTimer(hWnd, 2);
            KillTimer(hWnd, 3);
            nid.cbSize = sizeof(NOTIFYICONDATA);
            nid.hWnd = hWnd;
            nid.uID = 1201;
            Shell_NotifyIcon(NIM_DELETE, &nid);
            DestroyIcon(hAlarmClock);
            DestroyIcon(hAlarmOn);
            PostQuitMessage(0);
            return 0;
    }

    return (DefWindowProc(hWnd, iMessage, wParam, lParam));
}

LRESULT CALLBACK TodayScheduleWndProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam){
    POINT	pt;
    WCHAR  *ptr, *Data;
    static BOOL		bInit, bDown;
    static RECT		crt;
    static int		ButtonWidth, ButtonHeight;

    int x,y,Left,Top, Length;
    UINT nHit;
    RECT brt;

    switch(iMessage){
        case WM_CREATE:
            ptr = Data = NULL;
            ButtonWidth = ButtonHeight = 16;
            bInit = bDown = FALSE;
            return 0;

        case WM_NCHITTEST:
            nHit		= DefWindowProc(hWnd, WM_NCHITTEST, wParam, lParam);
            pt.x		= LOWORD(lParam);
            pt.y		= HIWORD(lParam);
            ScreenToClient(hWnd, &pt);

            GetClientRect(hWnd, &crt);
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

            GetClientRect(hWnd, &crt);
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

            GetClientRect(hWnd, &crt);
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
                if(Data){ free(Data); }
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
            KillTimer(GetParent(hWnd), 2);
            Data = (WCHAR*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
            if(Data){ free(Data); }
            bInit = bDown = FALSE;
            return 0;
    }

    return (DefWindowProc(hWnd, iMessage, wParam, lParam));
}

INT_PTR CALLBACK DlgProc(HWND hDlg, UINT iMessage, WPARAM wParam, LPARAM lParam){
    static InputParam *ret;

    switch(iMessage){
        case WM_INITDIALOG:
            ret = (InputParam*)lParam;
            memset(ret, 0, sizeof(InputParam));
            SendMessage(GetDlgItem(hDlg, IDC_ITEMEDIT1), EM_LIMITTEXT, (WPARAM)2, 0);
            SendMessage(GetDlgItem(hDlg, IDC_ITEMEDIT2), EM_LIMITTEXT, (WPARAM)2, 0);
            SendMessage(GetDlgItem(hDlg, IDC_ITEMEDIT3), EM_LIMITTEXT, (WPARAM)0xFF, 0);
            SendMessage(GetDlgItem(hDlg, IDC_SPIN1), UDM_SETRANGE, 0, MAKELPARAM(0, 23));
            SendMessage(GetDlgItem(hDlg, IDC_SPIN2), UDM_SETRANGE, 0, MAKELPARAM(0, 23));
            return TRUE;

        case WM_COMMAND:
            switch(LOWORD(wParam)){
                case IDOK:
                    if(ret != NULL){
                        ret->Hour = GetDlgItemInt(hDlg, IDC_ITEMEDIT1, NULL, FALSE);
                        ret->Minute = GetDlgItemInt(hDlg, IDC_ITEMEDIT2, NULL, FALSE);
                        ret->color = RGB(rand() % 106 + 150, rand() % 106 + 150, rand() % 106 + 150);
                        ret->VisualPart = (((ret->Hour) / 12) & 1) ? PM : AM;
                        GetDlgItemText(hDlg, IDC_ITEMEDIT3, ret->Message, 0x100);
                    }else{
                        MessageBox(hDlg, L"전달된 매개변수를 읽지 못했습니다.\r\n다시 시도해주세요.", L"에러", MB_ICONERROR | MB_OK);
                    }
                    EndDialog(hDlg, LOWORD(wParam));
                    return TRUE;

                case IDCANCEL:
                    EndDialog(hDlg, LOWORD(wParam));
                    return TRUE;
            }
            return FALSE;
    }
    return FALSE;
}

INT_PTR CALLBACK DeleteDlgProc(HWND hDlg, UINT iMessage, WPARAM wParam, LPARAM lParam){
    static const int MaxSize = 0x20;
    static int Items;

    static OutputParam *ret;

    WCHAR Temp[0x20];
    static WCHAR Time[MaxSize][0x100];
    static WCHAR Times[MaxSize][0x20];
    static WCHAR Messages[MaxSize][0x100];
    static WCHAR DisplayMessage[MaxSize][0x120];

    static HWND hListView;
    LVCOLUMN lvc;
    LVITEM lvi;
    RECT srt;

    int cnt;

    switch(iMessage){
        case WM_INITDIALOG:
            ret = (OutputParam*)lParam;
            if(ret == NULL){ EndDialog(hDlg, -1); return FALSE; }

            Items = ret->nItems;

            SetRect(&srt, 10, 5, 290, 135);
            MapDialogRect(hDlg, &srt);
            hListView = CreateWindowEx(0, WC_LISTVIEW, NULL, WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS, srt.left, srt.top, srt.right - srt.left, srt.bottom - srt.top, hDlg, (HMENU)(INT_PTR)IDC_LISTVIEW, GetModuleHandle(NULL), NULL);
            ListView_SetExtendedListViewStyle(hListView, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

            memset(&lvc, 0, sizeof(lvc));
            lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
            lvc.iSubItem = 0;
            lvc.pszText = (LPWSTR)L"항목";
            lvc.cx = 600;
            ListView_InsertColumn(hListView, 0, &lvc);

            InsertionSort(ret->ptr, Items);

            for(int i=0; i<Items; i++){
                memset(&lvi, 0, sizeof(lvi));
                lvi.mask = LVIF_TEXT;
                lvi.iItem = i;

                wsprintf(Times[i], L"%02d : %02d", ret->ptr[i].Hour, ret->ptr[i].Minute);
                if(ret->ptr[i].Message == NULL){
                    wsprintf(Messages[i], L"");
                }else{
                    wsprintf(Messages[i], L"%s", ret->ptr[i].Message);
                }

                wsprintf(DisplayMessage[i], L"%s, %s", Times[i], Messages[i]);
                lvi.pszText = DisplayMessage[i];
                ListView_InsertItem(hListView, &lvi);
            }
            return TRUE;

        case WM_COMMAND:
            switch(LOWORD(wParam)){
                case IDOK:
                    cnt = 0;
                    for (int i=Items - 1; i>=0; i--) {
                        BOOL checked = ListView_GetCheckState(hListView, i);
                        if(checked == 1){
                            memmove(&ret->ptr[i], &ret->ptr[i + 1], sizeof(InputParam) * (Items - i - 1));
                            Items--;
                            cnt++;
                        }
                    }
                    memset(ret->ptr + Items, 0, sizeof(InputParam) * (MaxSize - Items));
                    ret->nDelete = cnt;
                    EndDialog(hDlg, LOWORD(wParam));
                    return TRUE;

                case IDCANCEL:
                    EndDialog(hDlg, LOWORD(wParam));
                    return TRUE;
            }
            return FALSE;
    }

    return FALSE;
}

void DrawPiece(HDC hdc, POINT Origin, int iRadius, float AngleStartDeg, float AngleEndDeg, InputParam param){
    float PI = atan(1.f) * 4.f;
    float Quarter = 90.f, Half = 180.f, ThreeQuarter = 270.f, Circle = 360.f;

    int Left   = Origin.x - iRadius;
    int Top    = Origin.y - iRadius;
    int Right  = Origin.x + iRadius;
    int Bottom = Origin.y + iRadius;

    // 0도 : 12시
    // 회전: 시계 방향
    float AngleStartRad = fmod((Circle - AngleStartDeg) + Quarter, Circle) * PI / Half;
    float AngleEndRad = fmod((Circle - AngleEndDeg) + Quarter, Circle) * PI / Half;

    POINT OuterStart = {
        (int)(Origin.x + iRadius * cos(AngleStartRad)),
        (int)(Origin.y - iRadius * sin(AngleStartRad))
    };

    POINT OuterEnd = {
        (int)(Origin.x + iRadius * cos(AngleEndRad)),
        (int)(Origin.y - iRadius * sin(AngleEndRad))
    };

    HBRUSH hBrush = CreateSolidBrush(param.color);
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBrush);

    // 순서 주의
    Pie(hdc, Left, Top, Right, Bottom, OuterEnd.x, OuterEnd.y, OuterStart.x, OuterStart.y);

    BeginPath(hdc);
    Pie(hdc, Left, Top, Right, Bottom, OuterEnd.x, OuterEnd.y, OuterStart.x, OuterStart.y);
    EndPath(hdc);

    SelectClipPath(hdc, RGN_COPY);

    if(param.Message != NULL){
        SIZE TextSize;
        POINT pt1, pt2, pt;
        GetTextExtentPoint32(hdc, param.Message, wcslen(param.Message), &TextSize);

        float inner = iRadius * 0.6f;
        float r = (iRadius + inner) * 0.5f;

        if(AngleEndDeg == Circle){
            AngleEndRad = fmod((Circle + AngleStartDeg) * 0.5f + Quarter, Circle) * PI / Half;
            pt.x = Origin.x - r * cos(AngleEndRad);
            pt.y = Origin.y - r * sin(AngleEndRad);
        }else{
            AngleEndRad = fmod((AngleStartDeg + AngleEndDeg) * 0.5f + Quarter, Circle) * PI / Half;
            pt.x = Origin.x - r * cos(AngleEndRad);
            pt.y = Origin.y - r * sin(AngleEndRad);
        }

        int prevBkMode = SetBkMode(hdc, TRANSPARENT);
        /*
        int prevAlign = SetTextAlign(hdc, TA_CENTER | TA_BASELINE);
        TextOut(hdc, pt.x, pt.y, param.Message, wcslen(param.Message));
        SetTextAlign(hdc, prevAlign);
        */
        TextOut(hdc, pt.x - (TextSize.cx / 2), pt.y - (TextSize.cy / 2), param.Message, wcslen(param.Message));

        SetBkMode(hdc, prevBkMode);
    }

    SelectClipRgn(hdc, NULL);

    SelectObject(hdc, hOldBrush);
    DeleteObject(hBrush);
}

void DrawBkPaper(HDC hdc, POINT Origin, int iRadius, HBITMAP hBitmap) {
    if(hBitmap == NULL){ return; }

    HDC hMemDC = CreateCompatibleDC(hdc);
    HGDIOBJ hOld = SelectObject(hMemDC, hBitmap);

    BITMAP bmp;
    GetObject(hBitmap, sizeof(BITMAP), &bmp);

    int x, y, width, height;
    x = Origin.x - iRadius * 0.6f;
    y = Origin.y - iRadius * 0.6f;
    width = Origin.x + iRadius * 0.6f - x;
    height = Origin.y + iRadius * 0.6f - y;

    HRGN hRgn = CreateEllipticRgn(x, y, x + width, y + height);
    SelectClipRgn(hdc, hRgn);

    SetStretchBltMode(hdc,HALFTONE);
    StretchBlt(hdc, x,y,width,height, hMemDC, 0, 0, bmp.bmWidth, bmp.bmHeight, SRCCOPY);

    SelectClipRgn(hdc, NULL);
    DeleteObject(hRgn);
    SelectObject(hMemDC, hOld);
    DeleteDC(hMemDC);
}

void DrawOutLine(HDC hdc, POINT Origin, int iRadius){
    float fRadian, cosf, sinf, PI = atan(1.f) * 4.f, tau = PI * 2.f;
    float Quarter = 90.f, Half = 180.f, ThreeQuarter = 270.f, Circle = 360.f, x, y;
    POINT Hand;

    HPEN hPen = CreatePen(PS_SOLID, 5, RGB(255, 255, 255));
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);

    HBRUSH hBrush = (HBRUSH)GetStockObject(NULL_BRUSH);
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBrush);
    Ellipse(hdc, Origin.x - iRadius * 0.98f, Origin.y - iRadius * 0.98f, Origin.x + iRadius * 0.98f, Origin.y + iRadius * 0.98f);
    SelectObject(hdc, hOldBrush);

    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);

    hPen = CreatePen(PS_SOLID, 3, RGB(0,0,0));
    hOldPen = (HPEN)SelectObject(hdc, hPen);

    hBrush = GetSysColorBrush(COLOR_WINDOW);
    hOldBrush = (HBRUSH)SelectObject(hdc, hBrush);
    Ellipse(hdc, Origin.x - iRadius * 0.6f, Origin.y - iRadius * 0.6f, Origin.x + iRadius * 0.6f, Origin.y + iRadius * 0.6f);
    SelectObject(hdc, hOldBrush);
    DeleteObject(hBrush);

    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);
}

void DrawTick(HDC hdc, POINT Origin, int iRadius, BOOL IsDark){
    float fRadian, cosf, sinf, PI = atan(1.f) * 4.f, tau = PI * 2.f;
    float Quarter = 90.f, Half = 180.f, ThreeQuarter = 270.f, Circle = 360.f, x, y;
    POINT Hand;

    HPEN hPen = CreatePen(PS_SOLID, 3, IsDark ? RGB(255, 255, 255) : RGB(0,0,0));
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);

    for(int i=0; i<12; i++){
        fRadian = fmod((float)(30.f * i) + ThreeQuarter, Circle) * PI / Half;
        cosf = cos(fRadian);
        sinf = sin(fRadian);

        Tick pt1 = {Origin.x + iRadius * 0.58f * cosf, Origin.y + iRadius * 0.58f * sinf};
        Tick pt2 = {Origin.x + iRadius * 0.5f * cosf, Origin.y + iRadius * 0.5f * sinf};

        MoveToEx(hdc, pt1.x, pt1.y, NULL);
        LineTo(hdc, pt2.x, pt2.y);
    }

    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);
}

void DrawHand(HDC hdc, POINT Origin, int iRadius, BOOL IsDark){
    float fRadian, cosf, sinf, PI = atan(1.f) * 4.f, tau = PI * 2.f;
    float Quarter = 90.f, Half = 180.f, ThreeQuarter = 270.f, Circle = 360.f, x, y;

    SYSTEMTIME lt;
    GetLocalTime(&lt);

    fRadian = fmod((float)(30.f * (lt.wHour % 12)) + 0.4f * lt.wMinute + ThreeQuarter, Circle) * PI / Half;

    POINT PointTip = {
        (int)(Origin.x + iRadius * 0.4f * cos(fRadian)),
        (int)(Origin.y + iRadius * 0.4f * sin(fRadian))
    };

    POINT Left = {
        (int)(Origin.x - iRadius * 0.04f * cos(fRadian)),
        (int)(Origin.y + iRadius * 0.04f * sin(fRadian))
    };

    POINT Right = {
        (int)(Origin.x + iRadius * 0.04f * cos(fRadian)),
        (int)(Origin.y - iRadius * 0.04f * sin(fRadian))
    };

    POINT Hand[4] = { PointTip, Left, Origin, Right };

    // hour
    HPEN hPen = CreatePen(PS_SOLID, 3, IsDark ? RGB(255, 255, 255) : RGB(0,0,0));
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);

    HBRUSH hBrush = CreateSolidBrush(IsDark ? RGB(255, 255, 255) : RGB(0,0,0));
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBrush);

    Polygon(hdc, Hand, 4);

    SelectObject(hdc, hOldBrush);
    DeleteObject(hBrush);

    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);
}

COLORREF GetAverageColor(HDC hdc, int x, int y, int rad){
    int	 r  = 0,
         g	= 0,
         b	= 0;

    int cnt = 0,
        SampleX[] = {x - rad * 2, x - rad, x + rad, x + rad * 2},
        SampleY[] = {y - rad * 2, y - rad, y + rad, y + rad * 2};

    COLORREF color;
    for (int i=0; i<4; i++){
        for (int j=0; j<4; j++){
            color = GetPixel(hdc, SampleX[i], SampleY[j]);
            r += GetRValue(color);
            g += GetGValue(color);
            b += GetBValue(color);
            ++cnt;
        }
    }

    r /= cnt;
    g /= cnt;
    b /= cnt;

    return RGB(r, g, b);
}

// 0.5 미만 == 어두운 계열
BOOL IsColorDark(COLORREF color){
    int  r = GetRValue(color),
         g = GetGValue(color),
         b = GetBValue(color);

    // 가중 평균
    double brightness = (r * 0.299f + g * 0.587f + b * 0.114f) / 255.f;

    return brightness < 0.56f;
}

HBITMAP LoadFile(){
    void *buf = NULL;
    WCHAR lpstrFile[MAX_PATH] = L"";
    WCHAR FileName[MAX_PATH];
    WCHAR InitDir[MAX_PATH];
    WCHAR *path[MAX_PATH];
    WCHAR *pt = NULL;
    OPENFILENAME ofn;

    memset(&ofn, 0, sizeof(OPENFILENAME));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.lpstrFile = lpstrFile;
    ofn.lpstrFilter = L"이미지 파일\0*.png;*.jpg;*.jpeg;*.bmp\0모든 파일\0*.*\0";
    ofn.lpstrTitle= L"파일 선택 대화상자";
    ofn.lpstrDefExt = L"png";
    ofn.nMaxFile = MAX_PATH;
    ofn.nMaxFileTitle = MAX_PATH;
    ofn.hwndOwner = NULL;

    GetWindowsDirectory(InitDir, MAX_PATH);
    ofn.lpstrInitialDir = InitDir;

    if(GetOpenFileName(&ofn) != 0)
    {
        BOOL bExtension = FALSE;
        WCHAR *ext = lpstrFile + ofn.nFileExtension;

        bExtension = (wcscmp(ext, L"bmp") == 0 || wcscmp(ext, L"png") == 0 || wcscmp(ext, L"jpeg") == 0 || wcscmp(ext, L"jpg") == 0);

        if(bExtension){
            HBITMAP hBitmap = NULL;
            Bitmap *gdiBitmap = new Bitmap(lpstrFile);
            gdiBitmap->GetHBITMAP(Color(0,0,0), &hBitmap);
            delete gdiBitmap;
            return hBitmap;
        }
    }

    return NULL;
}



#define FIXED_SHIFT 32
#define FIXED_POINT (1ULL << FIXED_SHIFT)
ULONG GetMagicNumber(ULONG Divisor){
    // (2^32 + (Divisor - 1)) / Divisor;
    return ((ULONGLONG)FIXED_POINT + (Divisor - 1)) / Divisor;
}

ULONG MyMod(ULONG Dividend, ULONG Magic, ULONG Divisor){
    if(Divisor == 0){ return 0; }
    ULONG Quotient = ((ULONGLONG)Dividend * Magic) >> FIXED_SHIFT;
    ULONG Remainder = Dividend - (Quotient * Divisor);

    if(Remainder >= Divisor){ Remainder -= Divisor; }
    if(Remainder < 0){ Remainder += Divisor; }

    return Remainder;
}

void SetAttribute(HWND hWnd, bwAttributes Attr){
    SetLayeredWindowAttributes(hWnd, Attr.rgb, Attr.Opacity, Attr.Flags);
}

void GetAttribute(HWND hWnd, bwAttributes *Attr){
    GetLayeredWindowAttributes(hWnd, &Attr->rgb, &Attr->Opacity, &Attr->Flags);
}

void InsertionSort(InputParam *DataSet, int Length){
    InputParam temp;
    int AbsValueOne = 0, AbsValueTwo = 0;
    int i,j;

    for(i = 1; i < Length; i++){
        memcpy(&temp, &DataSet[i], sizeof(InputParam));
        AbsValueOne = DataSet[i].Hour * 3600 + DataSet[i].Minute * 60;
        for(j = i; j > 0; j--){
            AbsValueTwo = DataSet[j-1].Hour * 3600 + DataSet[j-1].Minute * 60;
            if(AbsValueTwo <= AbsValueOne){ break; }

            memcpy(&DataSet[j], &DataSet[j-1], sizeof(InputParam));
        }
        memcpy(&DataSet[j], &temp, sizeof(InputParam));
    }
}
