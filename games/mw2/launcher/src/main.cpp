#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <string>
#include <vector>
#include "diagnostics.h"

namespace {
constexpr COLORREF Background = RGB(8,11,17), Sidebar = RGB(12,16,23), Card = RGB(17,24,34);
constexpr COLORREF White = RGB(235,241,250), Muted = RGB(148,163,184), Orange = RGB(255,119,53), Cyan = RGB(67,218,216);
enum { Path=100, Browse, Check, Export, Original, FourPlayer, Website, Source };
HWND window{}, pathEdit{}, reportEdit{};
HFONT controlFont{};
HBRUSH cardBrush{}, backgroundBrush{};
mw2::Diagnostics diagnostics;
UINT dpi = 96;
bool checking = false;
int px(int value) { return MulDiv(value, static_cast<int>(dpi), 96); }
std::wstring textOf(HWND hwnd) {
    int length = GetWindowTextLengthW(hwnd);
    std::wstring out(static_cast<std::size_t>(length) + 1, L'\0');
    GetWindowTextW(hwnd, out.data(), length + 1);
    out.resize(length); return out;
}
void fill(HDC dc, RECT r, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color); FillRect(dc, &r, brush); DeleteObject(brush);
}
void text(HDC dc, const wchar_t* value, int x, int y, int width, int height, int size, COLORREF color, int weight=FW_NORMAL, UINT flags=DT_LEFT|DT_TOP|DT_NOPREFIX) {
    HFONT font = CreateFontW(-px(size),0,0,0,weight,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    auto previous = SelectObject(dc, font); SetTextColor(dc, color); SetBkMode(dc, TRANSPARENT);
    RECT rect{px(x),px(y),px(x+width),px(y+height)};
    DrawTextW(dc,value,-1,&rect,flags); SelectObject(dc,previous); DeleteObject(font);
}
HWND button(int id, const wchar_t* value) {
    return CreateWindowExW(0,L"BUTTON",value,WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,0,0,0,0,window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),nullptr,nullptr);
}
void position(int id, int x, int y, int width, int height) {
    MoveWindow(GetDlgItem(window,id),px(x),px(y),px(width),px(height),TRUE);
}
void layout() {
    RECT client{}; GetClientRect(window,&client);
    int width = MulDiv(client.right,96,static_cast<int>(dpi));
    int content = width - 300;
    position(Path,260,273,content-118,38); position(Browse,260+content-106,273,106,38);
    position(Check,260,326,212,38); position(Export,484,326,156,38);
    position(200,260,413,content,166);
    position(FourPlayer,260,603,260,44); position(Original,534,603,content-274,44);
    position(Website,26,540,166,36); position(Source,26,586,166,36);
    if (controlFont) DeleteObject(controlFont);
    controlFont=CreateFontW(-px(14),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    SendMessageW(pathEdit,WM_SETFONT,reinterpret_cast<WPARAM>(controlFont),TRUE);
    SendMessageW(reportEdit,WM_SETFONT,reinterpret_cast<WPARAM>(controlFont),TRUE);
    InvalidateRect(window,nullptr,TRUE);
}
void refresh(bool components) {
    checking=true;
    EnableWindow(GetDlgItem(window,Check),FALSE);
    SetWindowTextW(reportEdit,L"Reading executable and checking compatibility...");
    diagnostics=mw2::inspect(textOf(pathEdit),components);
    SetWindowTextW(reportEdit,diagnostics.report.c_str());
    EnableWindow(GetDlgItem(window,Original),diagnostics.found);
    EnableWindow(GetDlgItem(window,Export),TRUE);
    EnableWindow(GetDlgItem(window,Check),TRUE);
    if (diagnostics.found) mw2::saveFolder(textOf(pathEdit));
    checking=false;
    InvalidateRect(window,nullptr,TRUE);
}
void browse() {
    IFileOpenDialog* dialog=nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&dialog)))) return;
    DWORD options=0; dialog->GetOptions(&options);
    dialog->SetOptions(options|FOS_PICKFOLDERS|FOS_FORCEFILESYSTEM|FOS_PATHMUSTEXIST);
    dialog->SetTitle(L"Select Call of Duty Modern Warfare 2 (2009) - folder containing iw4sp.exe");
    if (SUCCEEDED(dialog->Show(window))) {
        IShellItem* item=nullptr;
        if (SUCCEEDED(dialog->GetResult(&item))) {
            PWSTR path=nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH,&path))) {
                SetWindowTextW(pathEdit,path); CoTaskMemFree(path); refresh(false);
            }
            item->Release();
        }
    }
    dialog->Release();
}
void exportReport() {
    wchar_t path[32768]=L"MW2-SpecOps4-diagnostics.txt";
    OPENFILENAMEW ofn{sizeof(ofn)}; ofn.hwndOwner=window;
    ofn.lpstrFilter=L"Text report (*.txt)\0*.txt\0"; ofn.lpstrFile=path;
    ofn.nMaxFile=32768; ofn.lpstrDefExt=L"txt"; ofn.Flags=OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST;
    if (GetSaveFileNameW(&ofn)) {
        try { mw2::saveReport(path,diagnostics.report); }
        catch (...) { MessageBoxW(window,L"The report could not be saved. Choose a writable location.",L"Save report",MB_OK|MB_ICONERROR); }
    }
}
void open(const wchar_t* uri) {
    auto result=ShellExecuteW(window,L"open",uri,nullptr,nullptr,SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(result)<=32) MessageBoxW(window,L"Windows could not open this link. Check that Steam or a browser is installed.",L"Open link",MB_OK|MB_ICONERROR);
}
void paint(HDC dc) {
    RECT bounds{}; GetClientRect(window,&bounds);
    fill(dc,bounds,Background);
    fill(dc,{0,0,px(220),bounds.bottom},Sidebar);
    fill(dc,{px(219),0,px(220),bounds.bottom},RGB(32,41,55));
    int width=MulDiv(bounds.right,96,static_cast<int>(dpi))-300;
    text(dc,L"OPS",26,32,70,34,26,White,FW_BOLD);
    text(dc,L"HUB",86,32,90,34,26,Orange,FW_BOLD);
    text(dc,L"COMMUNITY CO-OP",26,75,174,22,10,Muted,FW_SEMIBOLD);
    fill(dc,{px(26),px(123),px(194),px(124)},RGB(40,49,64));
    text(dc,L"YOUR PROJECT",26,151,174,22,10,Muted,FW_SEMIBOLD);
    fill(dc,{px(16),px(184),px(204),px(238)},Card);
    fill(dc,{px(16),px(184),px(19),px(238)},Orange);
    text(dc,L"MW2 / SPEC OPS",32,198,165,24,14,White,FW_SEMIBOLD);
    text(dc,L"TARGET SQUAD",26,289,170,20,10,Muted,FW_SEMIBOLD);
    for (int i=0;i<4;++i) {
        int x=26+i*43;
        fill(dc,{px(x),px(321),px(x+34),px(355)},Card);
        std::wstring number=L"0"+std::to_wstring(i+1);
        text(dc,number.c_str(),x,328,34,22,14,i<2?White:Muted,FW_SEMIBOLD,DT_CENTER|DT_NOPREFIX);
    }
    text(dc,L"Four-player goal.\nNot playable yet.",26,375,170,54,13,Muted,FW_NORMAL,DT_WORDBREAK|DT_NOPREFIX);
    text(dc,L"WINDOWS x64",26,665,174,18,10,Muted,FW_SEMIBOLD);
    text(dc,L"v0.1.0-dev.1",26,685,174,20,11,Muted);
    text(dc,L"MODERN WARFARE 2  /  2009",260,35,width,22,11,Cyan,FW_SEMIBOLD);
    text(dc,L"SPEC OPS / 4",260,70,width,53,42,White,FW_BOLD);
    text(dc,L"Your squad starts here. The four-player mod is still taking shape.",260,128,width,24,15,Muted);
    fill(dc,{px(260),px(172),px(260+width),px(222)},Card);
    fill(dc,{px(260),px(172),px(263),px(222)},Orange);
    text(dc,L"IN DEVELOPMENT",276,181,168,22,12,Orange,FW_SEMIBOLD);
    text(dc,L"Launcher preview only. Original Spec Ops remains two-player.",448,183,width-204,30,12,Muted,FW_NORMAL,DT_WORDBREAK|DT_NOPREFIX);
    text(dc,L"GAME INSTALLATION",260,245,width,21,11,Muted,FW_SEMIBOLD);
    text(dc,L"COMPATIBILITY & COMPONENT REPORT",260,385,width,22,11,Cyan,FW_SEMIBOLD);
    text(dc,L"Read-only checks. No game files changed.",260,658,width,22,12,Muted);
    const wchar_t* summary=diagnostics.componentPassed?L"CHECKS PASSED / OFFLINE ONLY":diagnostics.supported?L"SUPPORTED STEAM x64 BUILD":L"SELECT OR CHECK YOUR GAME FOLDER";
    text(dc,summary,260,686,width,20,10,diagnostics.supported?Cyan:Muted,FW_SEMIBOLD);
}
LRESULT CALLBACK procedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch(message) {
    case WM_CREATE: {
        window=hwnd; dpi=GetDpiForWindow(hwnd);
        BOOL dark=TRUE; DwmSetWindowAttribute(hwnd,20,&dark,sizeof(dark));
        cardBrush=CreateSolidBrush(Card); backgroundBrush=CreateSolidBrush(Background);
        pathEdit=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_VISIBLE|WS_CHILD|WS_TABSTOP|ES_AUTOHSCROLL,0,0,0,0,hwnd,reinterpret_cast<HMENU>(Path),nullptr,nullptr);
        SendMessageW(pathEdit,EM_SETLIMITTEXT,32760,0);
        reportEdit=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_VISIBLE|WS_CHILD|WS_TABSTOP|WS_VSCROLL|ES_MULTILINE|ES_AUTOVSCROLL|ES_READONLY,0,0,0,0,hwnd,reinterpret_cast<HMENU>(200),nullptr,nullptr);
        button(Browse,L"Browse..."); button(Check,L"Run component checks"); button(Export,L"Save report...");
        button(FourPlayer,L"4-player mode - coming later"); EnableWindow(GetDlgItem(hwnd,FourPlayer),FALSE);
        button(Original,L"Launch MW2 via Steam"); button(Website,L"Open OPS HUB"); button(Source,L"Source on GitHub");
        auto paths=mw2::discover();
        if (!paths.empty()) SetWindowTextW(pathEdit,paths.front().c_str());
        layout(); refresh(false); return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wParam)==Path && HIWORD(wParam)==EN_CHANGE && !checking && reportEdit) {
            diagnostics={}; diagnostics.report=L"Folder changed. Run component checks to verify this installation.";
            SetWindowTextW(reportEdit,diagnostics.report.c_str());
            EnableWindow(GetDlgItem(hwnd,Original),FALSE); InvalidateRect(hwnd,nullptr,TRUE); return 0;
        }
        switch(LOWORD(wParam)) {
        case Browse: browse(); break;
        case Check: refresh(true); break;
        case Export: exportReport(); break;
        case Original: open(L"steam://rungameid/10180"); break;
        case Website: open(mw2::Site); break;
        case Source: open(mw2::Source); break;
        }
        return 0;
    case WM_SIZE: if (pathEdit) layout(); return 0;
    case WM_GETMINMAXINFO: {
        auto info=reinterpret_cast<MINMAXINFO*>(lParam);
        info->ptMinTrackSize={px(1040),px(780)}; return 0;
    }
    case WM_DPICHANGED: {
        dpi=HIWORD(wParam); auto r=reinterpret_cast<RECT*>(lParam);
        SetWindowPos(hwnd,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE); layout(); return 0;
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC: {
        HDC dc=reinterpret_cast<HDC>(wParam);
        SetTextColor(dc,White); SetBkColor(dc,Card); return reinterpret_cast<LRESULT>(cardBrush);
    }
    case WM_DRAWITEM: {
        const auto* item=reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
        bool disabled=(item->itemState&ODS_DISABLED)!=0, primary=item->CtlID==Check;
        COLORREF color=disabled?RGB(25,31,41):primary?Orange:RGB(28,38,52);
        if (item->itemState&ODS_SELECTED) color=primary?RGB(216,91,27):RGB(39,54,72);
        fill(item->hDC,item->rcItem,color);
        std::wstring label=textOf(item->hwndItem);
        auto previous=SelectObject(item->hDC,controlFont);
        SetBkMode(item->hDC,TRANSPARENT); SetTextColor(item->hDC,disabled?Muted:primary?Background:White);
        RECT r=item->rcItem; DrawTextW(item->hDC,label.c_str(),-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
        if(item->itemState&ODS_FOCUS) { InflateRect(&r,-px(4),-px(4)); DrawFocusRect(item->hDC,&r); }
        SelectObject(item->hDC,previous); return TRUE;
    }
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps{}; HDC dc=BeginPaint(hwnd,&ps); RECT r{}; GetClientRect(hwnd,&r);
        HDC memory=CreateCompatibleDC(dc); HBITMAP bitmap=CreateCompatibleBitmap(dc,r.right,r.bottom);
        auto previous=SelectObject(memory,bitmap); paint(memory); BitBlt(dc,0,0,r.right,r.bottom,memory,0,0,SRCCOPY);
        SelectObject(memory,previous); DeleteObject(bitmap); DeleteDC(memory); EndPaint(hwnd,&ps); return 0;
    }
    case WM_DESTROY:
        DeleteObject(controlFont); DeleteObject(cardBrush); DeleteObject(backgroundBrush); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd,message,wParam,lParam);
}
}

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int show) {
    int argc=0; wchar_t** argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    // Headless verification shares exactly the diagnostics path used by the UI.
    if (argc==5 && std::wstring(argv[1])==L"--self-test" && std::wstring(argv[3])==L"--report") {
        int code=2;
        try { auto result=mw2::inspect(argv[2],true); mw2::saveReport(argv[4],result.report); code=result.componentPassed?0:1; } catch (...) { code=2; }
        LocalFree(argv); return code;
    }
    LocalFree(argv);
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    const auto init=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    WNDCLASSW wc{}; wc.hInstance=instance; wc.lpfnWndProc=procedure; wc.lpszClassName=L"BreezOpsMW2Launcher";
    wc.hCursor=LoadCursorW(nullptr,IDC_ARROW); wc.hIcon=LoadIconW(nullptr,IDI_APPLICATION);
    RegisterClassW(&wc);
    dpi=GetDpiForSystem();
    HWND hwnd=CreateWindowExW(0,wc.lpszClassName,L"OPS HUB | MW2 Spec Ops 4 - Development Launcher",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,px(1100),px(790),nullptr,nullptr,instance,nullptr);
    if (!hwnd) { if(SUCCEEDED(init)) CoUninitialize(); return 2; }
    ShowWindow(hwnd,show); UpdateWindow(hwnd);
    MSG message{};
    while(GetMessageW(&message,nullptr,0,0)>0) {
        if(!IsDialogMessageW(hwnd,&message)) { TranslateMessage(&message); DispatchMessageW(&message); }
    }
    if(SUCCEEDED(init)) CoUninitialize();
    return static_cast<int>(message.wParam);
}
