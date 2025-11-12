#define _WIN32_WINNT 0x0A00
#include "resource.h"
#include <shobjidl.h>
#include <commctrl.h>
#pragma GCC diagnostic ignored  "-Wunused-parameter"
#pragma GCC diagnostic ignored  "-Wunused-variable"
#define CLASS_NAME              L"Franklin"
#define SUBCLASS_NAME           L"TodayScheduleMessageWindow"
#define max(a,b)                (((a) > (b)) ? (a) : (b))

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

typedef struct tag_inParam{
    int Hour, Minute;
    BOOL bFlag;
    WCHAR Message[0x100];
}InputParam;

typedef struct tag_outParam{
    int nItems, nDelete;
    InputParam *ptr;
}OutputParam;

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
    HDC hdc;

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

    switch(iMessage) {
        case WM_CREATE:
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
            SetTimer(hWnd, 1, 500, NULL);
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

                                if(IsWindow(AlertWnd[i])){ DestroyWindow(AlertWnd[i]); AlertWnd[i] = NULL; }
                                AlertWnd[i] = CreateWindowEx(dwExStyle, SUBCLASS_NAME, NULL, dwStyle, x, y, Width, Height, hWnd, (HMENU)NULL, GetModuleHandle(NULL), NULL);
                                memset(DisplayMessage[i], 0, sizeof(DisplayMessage[i]));
                                wsprintf(DisplayMessage[i], L"%s, %s\r\n", Times[i], Messages[i]);
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
            }
            return 0;

        case WM_COMMAND:
            switch(LOWORD(wParam)){
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
                            wsprintf(Times[Items], L"%02d : %02d", param[Items].Hour, param[Items].Minute);
                            if(param[Items].Message == NULL){
                                wsprintf(Messages[Items], L"");
                            }else{
                                wsprintf(Messages[Items], L"%s", param[Items].Message);
                            }
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
                        }
                    }
                    break;

                case IDM_DELETEITEM:
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
                    break;

                case IDM_CLEARPAST:
                    if(Items > 0){
                        cnt = 0;
                        GetLocalTime(&st);
                        for(int i=Items - 1; i>=0; i--){
                            if(param[i].bFlag == TRUE || param[i].Hour <= st.wHour && param[i].Minute < st.wMinute){
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
            return 0;

        case WM_DESTROY:
            KillTimer(hWnd, 1);
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
