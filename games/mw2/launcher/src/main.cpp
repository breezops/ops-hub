#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h> // Windows SDK headers below require this first.
#include <objidl.h> // GDI+ requires IUnknown/IStream with WIN32_LEAN_AND_MEAN.
#include "diagnostics.h"
#include "lobby.h"
#include "missions.h"
#include "mission_images.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <commdlg.h>
#include <cstring>
#include <dwmapi.h>
#include <gdiplus.h>
#include <memory>
#include <roapi.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <string>
#include <thread>
#include <vector>
#include <windowsx.h>

namespace {
constexpr COLORREF Bg = RGB(14, 17, 13), Panel = RGB(23, 28, 21),
                   Field = RGB(19, 24, 18), Header = RGB(55, 65, 43);
constexpr COLORREF Ink = RGB(222, 229, 200), Muted = RGB(145, 157, 126),
                   Green = RGB(128, 173, 88), Bright = RGB(157, 211, 102),
                   Line = RGB(68, 80, 55), Amber = RGB(203, 179, 99);
enum {
  Name = 100,
  Code,
  ModeHost,
  ModeJoin,
  LobbyButton,
  MissionBox,
  Diff0,
  Diff1,
  Diff2,
  Diff3,
  Launch,
  TabPlay,
  TabSettings,
  TabInfo,
  Minimize,
  Close,
  GamePath,
  Browse,
  Check,
  Update,
  Export,
  Original,
  Website,
  Source,
  Report,
  Player0,
  Player1,
  Player2,
  Player3
};
enum class Tab { Play, Settings, Info };
struct Job {
  mw2::LobbyRequest request;
  mw2::LobbyReply result;
  std::atomic<bool> finished = false;
};
HWND window{}, nameEdit{}, codeEdit{}, missionCombo{}, pathEdit{}, reportEdit{};
UINT dpi = 96;
HFONT font{}, codeFont{}, rowFont{};
HBRUSH fieldBrush{}, occupiedBrush{}, waitingBrush{};
ULONG_PTR graphicsToken{};
std::unique_ptr<Gdiplus::Bitmap> artwork;
IStream *artStream{};
std::unique_ptr<Gdiplus::Bitmap> brandArtwork;
IStream *brandStream{};
std::vector<mw2::MissionRaster> missionPixels;
std::vector<std::unique_ptr<Gdiplus::Bitmap>> missionPreviews;
std::size_t missionImageCount=0;
Tab tab = Tab::Play;
bool hostMode = true, online = false, initializing = true, closing = false,
     settingsDirty = false;
int missionBeforeDropdown = 0;
int selectedMission = 0, selectedDifficulty = 3, lastRtt = 0;
ULONGLONG lastRequest = 0, lastSuccess = 0, closeAt = 0;
std::wstring token, testBase,
    status = L"Connecting to OPS HUB lobby service...";
mw2::Lobby lobby;
mw2::Diagnostics diagnostics;
std::shared_ptr<Job> job;
constexpr const wchar_t *DiffIds[] = {L"recruit", L"regular", L"hardened",
                                      L"veteran"};
constexpr const wchar_t *DiffNames[] = {L"Recruit", L"Regular", L"Hardened",
                                        L"Veteran"};
int px(int v) { return MulDiv(v, static_cast<int>(dpi), 96); }
int width() {
  RECT r{};
  GetClientRect(window, &r);
  return MulDiv(r.right, 96, static_cast<int>(dpi));
}
int height() {
  RECT r{};
  GetClientRect(window, &r);
  return MulDiv(r.bottom, 96, static_cast<int>(dpi));
}
std::wstring value(HWND hwnd) {
  int n = GetWindowTextLengthW(hwnd);
  std::wstring s(static_cast<std::size_t>(n) + 1, 0);
  GetWindowTextW(hwnd, s.data(), n + 1);
  s.resize(n);
  return s;
}
Gdiplus::Bitmap* missionBitmap(std::size_t index){
  if(index>=std::size(mw2::MissionCatalog))return nullptr;
  if(mw2::MissionCatalog[index].concept)return artwork.get();
  return index<missionPreviews.size()?missionPreviews[index].get():nullptr;
}
void drawImageFit(HDC dc,Gdiplus::Bitmap* image,const RECT& bounds){
  if(!image||!image->GetWidth()||!image->GetHeight())return;
  const auto w=static_cast<float>(bounds.right-bounds.left),h=static_cast<float>(bounds.bottom-bounds.top);
  const auto originalW=static_cast<float>(image->GetWidth()),originalH=static_cast<float>(image->GetHeight());
  const auto scale=std::min(w/originalW,h/originalH);
  const auto imageW=originalW*scale,imageH=originalH*scale;
  Gdiplus::Graphics graphics(dc);graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
  graphics.DrawImage(image,Gdiplus::RectF(static_cast<float>(bounds.left)+(w-imageW)/2,static_cast<float>(bounds.top)+(h-imageH)/2,imageW,imageH));
}
void reloadMissionImages(){
  missionPreviews.clear();missionPixels.clear();missionImageCount=0;
  std::vector<std::string> assets;for(const auto& mission:mw2::MissionCatalog)assets.emplace_back(mission.imageAsset);
  missionPixels=mw2::readMissionImages(std::filesystem::path(value(pathEdit)),assets);
  missionPreviews.resize(missionPixels.size());
  for(std::size_t i=0;i<missionPixels.size();i++){
    auto& image=missionPixels[i];if(!image.available())continue;
    auto bitmap=std::make_unique<Gdiplus::Bitmap>(image.width,image.height,image.width*4,PixelFormat32bppARGB,image.bgra.data());
    if(bitmap->GetLastStatus()==Gdiplus::Ok){missionPreviews[i]=std::move(bitmap);++missionImageCount;}
  }
  InvalidateRect(missionCombo,nullptr,TRUE);
}
void fill(HDC dc, RECT rect, COLORREF color) {
  HBRUSH b = CreateSolidBrush(color);
  FillRect(dc, &rect, b);
  DeleteObject(b);
}
void box(HDC dc, int x, int y, int w, int h, COLORREF color) {
  fill(dc, {px(x), px(y), px(x + w), px(y + h)}, color);
}
void line(HDC dc, int x, int y, int x2, int y2, COLORREF color) {
  HPEN p = CreatePen(PS_SOLID, 1, color);
  auto old = SelectObject(dc, p);
  MoveToEx(dc, px(x), px(y), nullptr);
  LineTo(dc, px(x2), px(y2));
  SelectObject(dc, old);
  DeleteObject(p);
}
void text(HDC dc, const std::wstring &s, int x, int y, int w, int h, int size,
          COLORREF color, int weight = FW_NORMAL,
          UINT flags = DT_LEFT | DT_TOP | DT_NOPREFIX,
          const wchar_t *face = L"Bahnschrift SemiCondensed") {
  HFONT f = CreateFontW(-px(size), 0, 0, 0, weight, FALSE, FALSE, FALSE,
                        DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, face);
  auto old = SelectObject(dc, f);
  SetBkMode(dc, TRANSPARENT);
  SetTextColor(dc, color);
  RECT r{px(x), px(y), px(x + w), px(y + h)};
  DrawTextW(dc, s.c_str(), -1, &r, flags);
  SelectObject(dc, old);
  DeleteObject(f);
}
void outline(HDC dc, int x, int y, int w, int h, COLORREF color) {
  line(dc, x, y, x + w, y, color);
  line(dc, x + w, y, x + w, y + h, color);
  line(dc, x + w, y + h, x, y + h, color);
  line(dc, x, y + h, x, y, color);
}
void star(HDC dc, int cx, int cy, int radius, COLORREF color,
          bool solid = true) {
  POINT points[10];
  for (int i = 0; i < 10; i++) {
    double a = -1.57079632679 + i * 0.62831853072;
    double r = i % 2 ? radius * .43 : radius;
    points[i] = {px(cx + static_cast<int>(std::cos(a) * r)),
                 px(cy + static_cast<int>(std::sin(a) * r))};
  }
  HPEN pen = CreatePen(PS_SOLID, 1, color);
  HBRUSH brush = solid ? CreateSolidBrush(color)
                       : static_cast<HBRUSH>(GetStockObject(HOLLOW_BRUSH));
  auto p = SelectObject(dc, pen), b = SelectObject(dc, brush);
  Polygon(dc, points, 10);
  SelectObject(dc, p);
  SelectObject(dc, b);
  DeleteObject(pen);
  if (solid)
    DeleteObject(brush);
}
void badge(HDC dc) {
  star(dc, 52, 48, 31, Muted, false);
  star(dc, 52, 48, 23, Muted);
  star(dc, 52, 48, 18, Panel);
  for (auto p : std::vector<POINT>{{34, 21}, {70, 21}, {34, 75}, {70, 75}})
    star(dc, p.x, p.y, 4, Ink);
  text(dc, L"4", 40, 31, 24, 35, 29, Ink, FW_BOLD, DT_CENTER | DT_NOPREFIX);
}
void panel(HDC dc, const wchar_t *name, int x, int y, int w, int h) {
  box(dc, x, y, w, h, Panel);
  outline(dc, x, y, w, h, Line);
  box(dc, x + 1, y + 1, w - 2, 30, Header);
  text(dc, name, x + 12, y + 6, w - 24, 24, 19, Ink, FW_SEMIBOLD);
}
HWND button(int id, const wchar_t *label) {
  return CreateWindowExW(
      0, L"BUTTON", label, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 0,
      0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
      nullptr, nullptr);
}
void position(int id, int x, int y, int w, int h) {
  MoveWindow(GetDlgItem(window, id), px(x), px(y), px(w), px(h), TRUE);
}
void controls() {
  bool busy = !!job;
  for (int id : {Name, Code, ModeHost, ModeJoin, LobbyButton, MissionBox, Diff0,
                 Diff1, Diff2, Diff3, Launch})
    ShowWindow(GetDlgItem(window, id), tab == Tab::Play ? SW_SHOW : SW_HIDE);
  for (int i = 0; i < 4; i++) {
    auto control = GetDlgItem(window, Player0 + i);
    ShowWindow(control, tab == Tab::Play ? SW_SHOW : SW_HIDE);
    const auto &p = lobby.players[i];
    std::wstring label = L"PLAYER " + std::to_wstring(i + 1) + L": ";
    label += p.occupied ? p.name + (i == lobby.ownSlot ? L" (You)" : L"") +
                              (i == 0 ? L" / HOST" : L"")
                        : L"Waiting for player";
    if (value(control) != label)
      SetWindowTextW(control, label.c_str());
    InvalidateRect(control, nullptr, TRUE);
  }
  for (int id : {GamePath, Browse, Check, Update, Export, Report})
    ShowWindow(GetDlgItem(window, id),
               tab == Tab::Settings ? SW_SHOW : SW_HIDE);
  for (int id : {Original, Website, Source})
    ShowWindow(GetDlgItem(window, id), tab == Tab::Play ? SW_HIDE : SW_SHOW);
  EnableWindow(nameEdit, !lobby.member() && !busy);
  EnableWindow(codeEdit, !busy);
  SendMessageW(codeEdit, EM_SETREADONLY, lobby.member() || hostMode, 0);
  EnableWindow(GetDlgItem(window, ModeHost), !lobby.member() && !busy);
  EnableWindow(GetDlgItem(window, ModeJoin), !lobby.member() && !busy);
  EnableWindow(GetDlgItem(window, LobbyButton), !busy && !closing);
  SetWindowTextW(GetDlgItem(window, LobbyButton), lobby.member() ? L"LEAVE"
                                                  : hostMode     ? L"CREATE"
                                                                 : L"JOIN");
  bool editingBlocked = job &&
                        job->request.action != mw2::LobbyAction::Heartbeat &&
                        job->request.action != mw2::LobbyAction::Health;
  bool editable = !editingBlocked && (lobby.member() ? lobby.host() : hostMode);
  EnableWindow(missionCombo, editable);
  for (int i = 0; i < 4; i++)
    EnableWindow(GetDlgItem(window, Diff0 + i), editable);
  EnableWindow(GetDlgItem(window, Launch), FALSE);
  EnableWindow(GetDlgItem(window, Update), !lobby.member());
  EnableWindow(GetDlgItem(window, Original), diagnostics.found);
  // Owner-drawn children are excluded from the parent's repaint region.
  // Refresh their selected styles explicitly when the tab or settings change.
  for (int id : {ModeHost, ModeJoin, Diff0, Diff1, Diff2, Diff3, TabPlay,
                 TabSettings, TabInfo, LobbyButton})
    InvalidateRect(GetDlgItem(window, id), nullptr, TRUE);
  InvalidateRect(window, nullptr, FALSE);
}
void layout() {
  const int w = width();
  position(TabPlay, w - 326, 50, 88, 30);
  position(TabSettings, w - 232, 50, 108, 30);
  position(TabInfo, w - 118, 50, 94, 30);
  position(Minimize, w - 75, 8, 28, 24);
  position(Close, w - 40, 8, 28, 24);
  position(ModeHost, 40, 145, 250, 28);
  position(ModeJoin, 298, 145, 260, 28);
  position(Name, 118, 185, 149, 29);
  position(Code, 329, 185, 125, 29);
  position(LobbyButton, 464, 185, 94, 29);
  position(MissionBox, 125, 407, 433, 330);
  SendMessageW(missionCombo,CB_SETITEMHEIGHT,static_cast<WPARAM>(-1),px(30));
  SendMessageW(missionCombo,CB_SETITEMHEIGHT,0,px(42));
  for (int i = 0; i < 4; i++)
    position(Diff0 + i, 125 + i * 108, 453, i == 3 ? 109 : 105, 32);
  position(Launch, 24, 516, 550, 61);
  for (int i = 0; i < 4; i++)
    position(Player0 + i, 98, 235 + i * 42, 423, 26);
  position(GamePath, 48, 163, w - 218, 36);
  position(Browse, w - 154, 163, 106, 36);
  position(Check, 48, 216, 217, 36);
  position(Export, 277, 216, 160, 36);
  position(Update, 449, 216, 180, 36);
  position(Report, 48, 280, w - 96, 211);
  position(Original, 48, 522, 265, 38);
  position(Website, 327, 522, 210, 38);
  position(Source, 551, 522, 210, 38);
  if (font)
    DeleteObject(font);
  if (codeFont)
    DeleteObject(codeFont);
  if (rowFont)
    DeleteObject(rowFont);
  font = CreateFontW(-px(15), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                     DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
  codeFont =
      CreateFontW(-px(19), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                  DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Consolas");
  rowFont = CreateFontW(-px(19), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                        DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0,
                        L"Bahnschrift SemiCondensed");
  for (int i = 0; i < 4; i++)
    SendMessageW(GetDlgItem(window, Player0 + i), WM_SETFONT,
                 reinterpret_cast<WPARAM>(rowFont), TRUE);
  for (HWND control : {nameEdit, pathEdit, reportEdit, missionCombo})
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
  SendMessageW(codeEdit, WM_SETFONT, reinterpret_cast<WPARAM>(codeFont), TRUE);
  controls();
}
void inspect(bool component) {
  diagnostics = mw2::inspect(value(pathEdit), component);
  reloadMissionImages();
  diagnostics.report+=L"\r\nOriginal mission images found in local game files: "+std::to_wstring(missionImageCount)+L" / 23\r\n";
  SetWindowTextW(reportEdit, diagnostics.report.c_str());
  if (diagnostics.found)
    mw2::saveFolder(value(pathEdit));
  controls();
}
void open(const wchar_t *uri) {
  if (reinterpret_cast<INT_PTR>(ShellExecuteW(window, L"open", uri, nullptr,
                                              nullptr, SW_SHOWNORMAL)) <= 32)
    MessageBoxW(window,
                L"Windows could not open the link. Check that Steam or a "
                L"browser is installed.",
                L"OPS HUB", MB_OK | MB_ICONERROR);
}
void browse() {
  IFileOpenDialog *dialog = nullptr;
  if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr,
                              CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog))))
    return;
  DWORD options = 0;
  dialog->GetOptions(&options);
  dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM |
                     FOS_PATHMUSTEXIST);
  dialog->SetTitle(L"Choose the MW2 (2009) folder containing iw4sp.exe");
  if (SUCCEEDED(dialog->Show(window))) {
    IShellItem *item = nullptr;
    if (SUCCEEDED(dialog->GetResult(&item))) {
      PWSTR path = nullptr;
      if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
        SetWindowTextW(pathEdit, path);
        CoTaskMemFree(path);
        inspect(false);
      }
      item->Release();
    }
  }
  dialog->Release();
}
void exportReport() {
  wchar_t path[32768] = L"MW2-SpecOps4-diagnostics.txt";
  OPENFILENAMEW ofn{sizeof(ofn)};
  ofn.hwndOwner = window;
  ofn.lpstrFilter = L"Text report (*.txt)\0*.txt\0";
  ofn.lpstrFile = path;
  ofn.nMaxFile = 32768;
  ofn.lpstrDefExt = L"txt";
  ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
  if (GetSaveFileNameW(&ofn)) {
    try {
      mw2::saveReport(path, diagnostics.report);
    } catch (...) {
      MessageBoxW(window, L"Choose a writable location for the report.",
                  L"Save report", MB_OK | MB_ICONERROR);
    }
  }
}
void start(mw2::LobbyAction action) {
  if (job)
    return;
  try {
    auto pending = std::make_shared<Job>();
    pending->request.action = action;
    pending->request.name = value(nameEdit);
    pending->request.code = lobby.member() ? lobby.code : value(codeEdit);
    pending->request.token = token;
    pending->request.mission = mw2::MissionCatalog[selectedMission].id;
    pending->request.difficulty = DiffIds[selectedDifficulty];
    pending->request.previousRtt = lastRtt;
    if (action == mw2::LobbyAction::Create || action == mw2::LobbyAction::Join)
      pending->request.token = mw2::memberToken();
    const auto base = testBase;
    std::thread([pending, base] {
      pending->result = mw2::lobbyRequest(pending->request, base);
      pending->finished.store(true, std::memory_order_release);
    }).detach();
    job = pending;
    lastRequest = GetTickCount64();
    if (action == mw2::LobbyAction::Create)
      status = L"Creating your lobby...";
    if (action == mw2::LobbyAction::Join)
      status = L"Joining the lobby...";
    if (action == mw2::LobbyAction::Settings)
      status = L"Sharing mission settings...";
    controls();
  } catch (...) {
    status = L"Could not start the lobby request. Please retry.";
    controls();
  }
}
void selectionFromLobby() {
  for (std::size_t i = 0; i < std::size(mw2::MissionCatalog); i++)
    if (lobby.mission == mw2::MissionCatalog[i].id)
      selectedMission = static_cast<int>(i);
  for (int i = 0; i < 4; i++)
    if (lobby.difficulty == DiffIds[i])
      selectedDifficulty = i;
  if (SendMessageW(missionCombo, CB_GETCURSEL, 0, 0) != selectedMission)
    SendMessageW(missionCombo, CB_SETCURSEL, selectedMission, 0);
}
void finish() {
  if (!job || !job->finished.load(std::memory_order_acquire))
    return;
  auto completed = job;
  job.reset();
  const auto &reply = completed->result;
  const auto action = completed->request.action;
  if (reply.ok) {
    online = true;
    lastSuccess = GetTickCount64();
    lastRtt = reply.rtt;
    if (action == mw2::LobbyAction::Health)
      status = L"Lobby service ready. Choose a callsign, then host or join.";
    else if (action == mw2::LobbyAction::Leave) {
      lobby = {};
      settingsDirty = false;
      token.clear();
      SetWindowTextW(codeEdit, L"");
      status = L"You left the lobby.";
    } else {
      lobby = reply.lobby;
      token = completed->request.token;
      hostMode = lobby.host();
      if (value(codeEdit) != lobby.code)
        SetWindowTextW(codeEdit, lobby.code.c_str());
      if (action == mw2::LobbyAction::Create ||
          action == mw2::LobbyAction::Join)
        settingsDirty = false;
      if (action == mw2::LobbyAction::Settings &&
          completed->request.mission ==
              mw2::MissionCatalog[selectedMission].id &&
          completed->request.difficulty == DiffIds[selectedDifficulty])
        settingsDirty = false;
      if (!settingsDirty &&
          !SendMessageW(missionCombo, CB_GETDROPPEDSTATE, 0, 0))
        selectionFromLobby();
      status = lobby.host() ? L"Share your six-letter code. You control "
                              L"mission and difficulty."
                            : L"Connected. Mission and difficulty are "
                              L"controlled by the host.";
    }
  } else {
    status = reply.error;
    if (action == mw2::LobbyAction::Health ||
        action == mw2::LobbyAction::Heartbeat)
      online = false;
    if (action == mw2::LobbyAction::Heartbeat &&
        (reply.status == 403 || reply.status == 404)) {
      lobby = {};
      token.clear();
      SetWindowTextW(codeEdit, L"");
    }
    if (action == mw2::LobbyAction::Leave) {
      lobby = {};
      token.clear();
      SetWindowTextW(codeEdit, L"");
      status =
          L"Disconnected locally. The server lobby will expire automatically.";
    }
    if (action == mw2::LobbyAction::Settings && lobby.member()) {
      settingsDirty = false;
      selectionFromLobby();
    }
    if (action == mw2::LobbyAction::Create ||
        action == mw2::LobbyAction::Join ||
        action == mw2::LobbyAction::Settings)
      MessageBoxW(window, status.c_str(), L"Lobby request",
                  MB_OK | MB_ICONINFORMATION);
  }
  controls();
  if (closing)
    DestroyWindow(window);
}
void drawPlay(HDC dc) {
  int right = width() - 626;
  panel(dc, L"LOBBY SETUP", 24, 104, 550, 395);
  panel(dc, L"MISSION BRIEF", 590, 104, right + 12, 395);
  text(dc, L"CALLSIGN", 40, 190, 76, 23, 12, Muted, FW_SEMIBOLD);
  text(dc, L"CODE", 281, 190, 46, 23, 12, Muted, FW_SEMIBOLD);
  for (int i = 0; i < 4; i++) {
    const int y = 228 + i * 42;
    const auto &p = lobby.players[i];
    box(dc, 40, y, 38, 35, RGB(35, 43, 28));
    outline(dc, 40, y, 38, 35, Line);
    star(dc, 59, y + 18, 13, p.occupied ? Muted : Line, p.occupied);
    box(dc, 86, y, 472, 35, p.occupied ? RGB(35, 43, 28) : RGB(25, 31, 23));
    const bool fresh = online && GetTickCount64() - lastSuccess < 10000;
    HBRUSH brush =
        CreateSolidBrush(p.occupied && p.online && fresh ? Bright : Line);
    auto old = SelectObject(dc, brush);
    Ellipse(dc, px(538), px(y + 13), px(547), px(y + 22));
    SelectObject(dc, old);
    DeleteObject(brush);
  }
  line(dc, 40, 399, 558, 399, Line);
  text(dc, L"MISSION", 40, 416, 80, 24, 15, Ink);
  text(dc, L"DIFFICULTY", 40, 462, 82, 24, 14, Ink);
  box(dc,606,146,right-20,224,Field);
  auto preview=missionBitmap(static_cast<std::size_t>(selectedMission));
  if(preview)drawImageFit(dc,preview,{px(606),px(146),px(586+right),px(370)});
  else text(dc,L"MISSION IMAGE UNAVAILABLE\nSelect your installed MW2 folder in Settings.",622,224,right-52,65,16,Muted,FW_NORMAL,DT_CENTER|DT_WORDBREAK|DT_NOPREFIX);
  outline(dc, 606, 146, right - 20, 224, Muted);
  box(dc, 607, 347, right - 22, 22, Field);
  const wchar_t* imageLabel=mw2::MissionCatalog[selectedMission].concept?L"CUSTOM CONCEPT ART / NOT AN IN-GAME CAPTURE":preview?L"ORIGINAL MW2 MISSION IMAGE / LOCAL GAME FILES":L"LOCAL MISSION IMAGE NOT AVAILABLE";
  text(dc, imageLabel, 614, 352,
       right - 34, 20, 10, Muted, FW_SEMIBOLD);
  const auto &mission = mw2::MissionCatalog[selectedMission];
  std::wstring title = L"MISSION: " + std::wstring(mission.name);
  std::transform(title.begin(), title.end(), title.begin(), [](wchar_t c) {
    return c >= L'a' && c <= L'z' ? static_cast<wchar_t>(c - 32) : c;
  });
  text(dc, title, 606, 384, right - 20, 31, 24, Ink, FW_SEMIBOLD);
  text(dc, mission.brief, 606, 420, right - 24, 65, 16, Ink, FW_NORMAL,
       DT_WORDBREAK | DT_NOPREFIX);
  text(dc, L"4-player gameplay is still in development.", 606, 481, right - 24,
       20, 12, Amber);
  text(dc,
       L"CONNECTION: " +
           std::wstring(online ? L"LOBBY ONLINE" : L"OFFLINE / RETRYING"),
       606, 518, right - 24, 22, 16, online ? Green : Amber, FW_SEMIBOLD);
  text(dc, status, 606, 546, right - 24, 43, 13, Muted, FW_NORMAL,
       DT_WORDBREAK | DT_NOPREFIX);
  text(dc, L"GAME INTEGRATION PENDING", 40, 580, 520, 18, 10, Muted, FW_NORMAL,
       DT_CENTER | DT_NOPREFIX);
}
void drawSettings(HDC dc) {
  text(dc, L"INSTALLATION & DIAGNOSTICS", 48, 116, width() - 96, 30, 23, Ink,
       FW_SEMIBOLD);
  text(dc,
       L"Select your MW2 (2009) game folder. Checks inspect a private memory "
       L"copy only.",
       48, 143, width() - 96, 20, 13, Muted);
  text(dc, L"LOCAL REPORT / NO AUTOMATIC UPLOAD", 48, 259, width() - 96, 18, 12,
       Muted);
  text(dc,
       L"Original-game launch opens Steam. Launcher mission settings are not "
       L"applied to it.",
       48, 494, width() - 96, 22, 13, Amber);
}
void drawInfo(HDC dc) {
  if(brandArtwork){
    drawImageFit(dc,brandArtwork.get(),{px(width()-258),px(127),px(width()-48),px(337)});
    text(dc,L"YOUR LAUNCHER ARTWORK",width()-258,343,210,20,10,Muted,FW_NORMAL,DT_CENTER|DT_NOPREFIX);
  }
  text(dc, L"SPEC OPS / FOUR-PLAYER DEVELOPMENT", 48, 123, width() - 96, 35, 26,
       Ink, FW_SEMIBOLD);
  text(dc, L"BUILD " + std::wstring(mw2::DisplayVersion), 48, 177, width() - 96,
       30, 20, Green, FW_SEMIBOLD);
  text(dc, L"LIVE LAUNCHER LOBBY", 48, 235, width() - 96, 23, 16, Ink,
       FW_SEMIBOLD);
  text(dc,
       L"Create a six-letter code, invite up to three friends, and share a "
       L"mission and difficulty.\nPlayer names and heartbeat status come from "
       L"the OPS HUB service. Only the host can change settings.",
       48, 268, width() - 370, 66, 16, Muted, FW_NORMAL,
       DT_WORDBREAK | DT_NOPREFIX);
  text(dc, L"GAMEPLAY STATUS", 48, 354, width() - 96, 24, 16, Amber,
       FW_SEMIBOLD);
  text(dc,
       L"Four-player game networking and original mission integration are "
       L"unfinished. Launch Game stays disabled.\nOverkill is a concept, not a "
       L"shipped MW2 mission. Original mission images are read from your "
       L"game files.\nThe bottom response time measures the lobby service, not "
       L"peer-to-peer game ping.",
       48, 385, width() - 96, 90, 16, Muted, FW_NORMAL,
       DT_WORDBREAK | DT_NOPREFIX);
  text(dc,
       L"No game files are patched. Lobby identities are kept only in memory "
       L"by this launcher.",
       48, 486, width() - 96, 24, 13, Muted);
}
void paint(HDC dc) {
  const int w = width(), h = height();
  box(dc, 0, 0, w, h, Bg);
  for (int y = 0; y < h; y += 4)
    line(dc, 0, y, w, y, RGB(12, 15, 11));
  for (int i = 0; i < 110; i++) {
    int x = (i * 79) % w;
    box(dc, x, i % 2 ? 0 : h - 5, 8 + i % 16, 4,
        RGB(28 + i % 12, 33 + i % 14, 23 + i % 8));
  }
  badge(dc);
  text(dc, L"MW2 4-PLAYER SPEC OPS", 96, 29, 660, 45, 33, Ink, FW_BOLD);
  text(dc, L"OPS HUB  /  INDEPENDENT CO-OP PROJECT", 98, 72, 600, 19, 11, Muted,
       FW_SEMIBOLD);
  line(dc, 24, 94, w - 24, 94, Line);
  if (tab == Tab::Play)
    drawPlay(dc);
  else if (tab == Tab::Settings)
    drawSettings(dc);
  else
    drawInfo(dc);
  box(dc, 0, h - 24, w, 24, RGB(17, 22, 15));
  line(dc, 0, h - 25, w, h - 25, Line);
  text(dc, online ? L"ONLINE" : L"OFFLINE", 32, h - 19, 95, 18, 12,
       online ? Bright : Muted, FW_SEMIBOLD);
  int count = 0;
  for (const auto &p : lobby.players)
    if (p.occupied)
      ++count;
  std::wstring state = lobby.member() ? L"SQUAD " + std::to_wstring(count) +
                                            L" / 4   |   LOBBY SYNC: " +
                                            (online ? L"ACTIVE" : L"RETRYING")
                                      : L"NO ACTIVE LOBBY";
  if (online)
    state += L"   |   SERVICE " + std::to_wstring(lastRtt) + L" ms";
  text(dc, state, 126, h - 19, w - 445, 18, 11, Muted);
  text(dc, L"BUILD " + std::wstring(mw2::DisplayVersion), w - 310, h - 19, 285,
       18, 11, Muted, FW_NORMAL, DT_RIGHT | DT_NOPREFIX);
}
std::unique_ptr<Gdiplus::Bitmap> loadResourceArt(int id,IStream*& stream) {
  auto resource = FindResourceW(nullptr, MAKEINTRESOURCEW(id), RT_RCDATA);
  if (!resource)
    return nullptr;
  auto loaded = LoadResource(nullptr, resource);
  if (!loaded)
    return nullptr;
  DWORD size = SizeofResource(nullptr, resource);
  HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, size);
  if (!memory)
    return nullptr;
  auto bytes = GlobalLock(memory);
  if (!bytes) {
    GlobalFree(memory);
    return nullptr;
  }
  std::memcpy(bytes, LockResource(loaded), size);
  GlobalUnlock(memory);
  if (FAILED(CreateStreamOnHGlobal(memory, TRUE, &stream))) {
    GlobalFree(memory);
    return nullptr;
  }
  std::unique_ptr<Gdiplus::Bitmap> image(Gdiplus::Bitmap::FromStream(stream));
  if(image->GetLastStatus()!=Gdiplus::Ok)return nullptr;
  return image;
}
void loadArt(){
  artwork=loadResourceArt(101,artStream);
  brandArtwork=loadResourceArt(102,brandStream);
}
LRESULT CALLBACK procedure(HWND hwnd, UINT message, WPARAM wParam,
                           LPARAM lParam) {
  switch (message) {
  case WM_NCCALCSIZE:
    if (wParam)
      return 0;
    break;
  case WM_CREATE: {
    window = hwnd;
    dpi = GetDpiForWindow(hwnd);
    auto module=GetModuleHandleW(nullptr);
    auto big=LoadImageW(module,MAKEINTRESOURCEW(1),IMAGE_ICON,GetSystemMetricsForDpi(SM_CXICON,dpi),GetSystemMetricsForDpi(SM_CYICON,dpi),LR_SHARED);
    auto smallIcon=LoadImageW(module,MAKEINTRESOURCEW(1),IMAGE_ICON,GetSystemMetricsForDpi(SM_CXSMICON,dpi),GetSystemMetricsForDpi(SM_CYSMICON,dpi),LR_SHARED);
    SendMessageW(hwnd,WM_SETICON,ICON_BIG,reinterpret_cast<LPARAM>(big));
    SendMessageW(hwnd,WM_SETICON,ICON_SMALL,reinterpret_cast<LPARAM>(smallIcon));
    BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));
    fieldBrush = CreateSolidBrush(Field);
    occupiedBrush = CreateSolidBrush(RGB(35, 43, 28));
    waitingBrush = CreateSolidBrush(RGB(25, 31, 23));
    loadArt();
    nameEdit =
        CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"Operator",
                        WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL, 0, 0, 0, 0,
                        hwnd, reinterpret_cast<HMENU>(Name), nullptr, nullptr);
    SendMessageW(nameEdit, EM_SETLIMITTEXT, 24, 0);
    codeEdit = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_TABSTOP | ES_UPPERCASE | ES_AUTOHSCROLL, 0, 0, 0, 0, hwnd,
        reinterpret_cast<HMENU>(Code), nullptr, nullptr);
    SendMessageW(codeEdit, EM_SETLIMITTEXT, 8, 0);
    missionCombo = CreateWindowExW(
        0, L"COMBOBOX", L"Mission",
        WS_CHILD | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS, 0, 0, 0, 0, hwnd,
        reinterpret_cast<HMENU>(MissionBox), nullptr, nullptr);
    for (const auto &mission : mw2::MissionCatalog) {
      std::wstring label =
          std::wstring(mission.name) + (mission.concept ? L" [CONCEPT]" : L"");
      SendMessageW(missionCombo, CB_ADDSTRING, 0,
                   reinterpret_cast<LPARAM>(label.c_str()));
    }
    SendMessageW(missionCombo, CB_SETCURSEL, 0, 0);
    SendMessageW(missionCombo,CB_SETITEMHEIGHT,static_cast<WPARAM>(-1),px(30));
    SendMessageW(missionCombo,CB_SETITEMHEIGHT,0,px(42));
    button(ModeHost, L"HOST LOBBY");
    button(ModeJoin, L"JOIN LOBBY");
    button(LobbyButton, L"CREATE");
    for (int i = 0; i < 4; i++)
      button(Diff0 + i, DiffNames[i]);
    button(Launch, L"LAUNCH GAME");
    button(TabPlay, L"PLAY");
    button(TabSettings, L"SETTINGS");
    button(TabInfo, L"INFO");
    button(Minimize, L"_");
    button(Close, L"X");
    for (int i = 0; i < 4; i++)
      CreateWindowExW(
          0, L"STATIC", L"Waiting for player",
          WS_CHILD | SS_NOPREFIX | SS_OWNERDRAW, 0, 0, 0, 0, hwnd,
          reinterpret_cast<HMENU>(static_cast<INT_PTR>(Player0 + i)), nullptr,
          nullptr);
    pathEdit = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL,
        0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(GamePath), nullptr, nullptr);
    reportEdit = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_TABSTOP | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL |
            ES_READONLY,
        0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(Report), nullptr, nullptr);
    button(Browse, L"BROWSE...");
    button(Check, L"RUN COMPONENT CHECKS");
    button(Update, L"CHECK FOR UPDATES");
    button(Export, L"SAVE REPORT...");
    button(Original, L"LAUNCH ORIGINAL MW2 / STEAM");
    button(Website, L"OPEN OPS HUB");
    button(Source, L"PROJECT SOURCE");
    auto paths = mw2::discover();
    if (!paths.empty())
      SetWindowTextW(pathEdit, paths.front().c_str());
    initializing = false;
    layout();
    inspect(false);
    SetTimer(hwnd, 1, 250, nullptr);
    start(mw2::LobbyAction::Health);
    return 0;
  }
  case WM_COMMAND: {
    int id = LOWORD(wParam);
    if (id == GamePath && HIWORD(wParam) == EN_CHANGE && !initializing &&
        reportEdit) {
      diagnostics = {};
      SetWindowTextW(
          reportEdit,
          L"Folder changed. Run component checks to verify the installation.");
      controls();
      return 0;
    }
    if (id == MissionBox) {
      if (HIWORD(wParam) == CBN_DROPDOWN)
        missionBeforeDropdown = selectedMission;
      if (HIWORD(wParam) == CBN_SELCHANGE || HIWORD(wParam) == CBN_SELENDOK) {
        selectedMission =
            static_cast<int>(SendMessageW(missionCombo, CB_GETCURSEL, 0, 0));
        if (lobby.host())
          settingsDirty = true;
        InvalidateRect(hwnd, nullptr, FALSE);
      }
      if (HIWORD(wParam) == CBN_SELENDCANCEL) {
        selectedMission = missionBeforeDropdown;
        settingsDirty = false;
        InvalidateRect(hwnd, nullptr, FALSE);
      }
      return 0;
    }
    if (id >= Diff0 && id <= Diff3) {
      selectedDifficulty = id - Diff0;
      if (lobby.host())
        settingsDirty = true;
      controls();
      return 0;
    }
    switch (id) {
    case ModeHost:
      hostMode = true;
      SetWindowTextW(codeEdit, L"");
      controls();
      break;
    case ModeJoin:
      hostMode = false;
      controls();
      SetFocus(codeEdit);
      break;
    case LobbyButton:
      if (lobby.member())
        start(mw2::LobbyAction::Leave);
      else
        start(hostMode ? mw2::LobbyAction::Create : mw2::LobbyAction::Join);
      break;
    case TabPlay:
      tab = Tab::Play;
      controls();
      break;
    case TabSettings:
      tab = Tab::Settings;
      controls();
      break;
    case TabInfo:
      tab = Tab::Info;
      controls();
      break;
    case Minimize:
      ShowWindow(hwnd, SW_MINIMIZE);
      break;
    case Close:
      SendMessageW(hwnd, WM_CLOSE, 0, 0);
      break;
    case Browse:
      browse();
      break;
    case Check:
      inspect(true);
      break;
    case Update: {
      wchar_t executable[MAX_PATH]{};
      if (!GetModuleFileNameW(nullptr, executable, MAX_PATH))
        break;
      std::wstring script(executable);
      script = script.substr(0, script.find_last_of(L"\\/")) +
               L"\\update-launcher.ps1";
      if (GetFileAttributesW(script.c_str()) == INVALID_FILE_ATTRIBUTES) {
        MessageBoxW(hwnd, L"Update helper is missing. Extract the full launcher ZIP.",
                    L"MW2 launcher updates", MB_OK | MB_ICONWARNING);
        break;
      }
      const std::wstring args = L"-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File \"" +
                                script + L"\" -LauncherVersion \"" + mw2::Version +
                                L"\" -LauncherWindow " +
                                std::to_wstring(reinterpret_cast<std::uintptr_t>(hwnd));
      if (reinterpret_cast<std::intptr_t>(ShellExecuteW(
              hwnd, L"open", L"powershell.exe", args.c_str(), nullptr,
              SW_HIDE)) <= 32)
        MessageBoxW(hwnd, L"Could not start the update helper.",
                    L"MW2 launcher updates", MB_OK | MB_ICONERROR);
      break;
    }
    case Export:
      exportReport();
      break;
    case Original:
      open(L"steam://rungameid/10180");
      break;
    case Website:
      open(mw2::Site);
      break;
    case Source:
      open(mw2::Source);
      break;
    }
    return 0;
  }
  case WM_TIMER:
    finish();
    if (closing) {
      if (GetTickCount64() > closeAt)
        DestroyWindow(hwnd);
      return 0;
    }
    if (!job && settingsDirty && lobby.host() &&
        !SendMessageW(missionCombo, CB_GETDROPPEDSTATE, 0, 0))
      start(mw2::LobbyAction::Settings);
    else if (!job &&
             GetTickCount64() - lastRequest > (lobby.member() ? 3000 : 15000))
      start(lobby.member() ? mw2::LobbyAction::Heartbeat
                           : mw2::LobbyAction::Health);
    if (online && GetTickCount64() - lastSuccess > 20000) {
      online = false;
      InvalidateRect(hwnd, nullptr, FALSE);
    }
    return 0;
  case WM_CLOSE:
    if (lobby.member() && !job) {
      closing = true;
      closeAt = GetTickCount64() + 3500;
      start(mw2::LobbyAction::Leave);
    } else
      DestroyWindow(hwnd);
    return 0;
  case WM_SIZE:
    if (pathEdit)
      layout();
    return 0;
  case WM_GETMINMAXINFO: {
    auto info = reinterpret_cast<MINMAXINFO *>(lParam);
    info->ptMinTrackSize = {px(1100), px(620)};
    return 0;
  }
  case WM_DPICHANGED: {
    dpi = HIWORD(wParam);
    auto r = reinterpret_cast<RECT *>(lParam);
    SetWindowPos(hwnd, nullptr, r->left, r->top, r->right - r->left,
                 r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
    layout();
    return 0;
  }
  case WM_NCHITTEST: {
    POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    ScreenToClient(hwnd, &point);
    RECT r{};
    GetClientRect(hwnd, &r);
    int edge = px(6);
    if (point.y < edge) {
      if (point.x < edge)
        return HTTOPLEFT;
      if (point.x > r.right - edge)
        return HTTOPRIGHT;
      return HTTOP;
    }
    if (point.y > r.bottom - edge) {
      if (point.x < edge)
        return HTBOTTOMLEFT;
      if (point.x > r.right - edge)
        return HTBOTTOMRIGHT;
      return HTBOTTOM;
    }
    if (point.x < edge)
      return HTLEFT;
    if (point.x > r.right - edge)
      return HTRIGHT;
    if (point.y < px(90))
      return HTCAPTION;
    return HTCLIENT;
  }
  case WM_CTLCOLOREDIT:
  case WM_CTLCOLORSTATIC:
  case WM_CTLCOLORLISTBOX: {
    auto dc = reinterpret_cast<HDC>(wParam);
    int id = GetDlgCtrlID(reinterpret_cast<HWND>(lParam));
    if (id >= Player0 && id <= Player3) {
      bool occupied = lobby.players[id - Player0].occupied;
      SetTextColor(dc, occupied ? Ink : Muted);
      SetBkColor(dc, occupied ? RGB(35, 43, 28) : RGB(25, 31, 23));
      return reinterpret_cast<LRESULT>(occupied ? occupiedBrush : waitingBrush);
    }
    SetTextColor(dc, Ink);
    SetBkColor(dc, Field);
    return reinterpret_cast<LRESULT>(fieldBrush);
  }
  case WM_MEASUREITEM:{
    auto measure=reinterpret_cast<MEASUREITEMSTRUCT*>(lParam);
    if(measure->CtlID==MissionBox){measure->itemHeight=static_cast<UINT>(px(42));return TRUE;}
    break;
  }
  case WM_DRAWITEM: {
    auto item = reinterpret_cast<DRAWITEMSTRUCT *>(lParam);
    int id = static_cast<int>(item->CtlID);
    if(id==MissionBox){
      fill(item->hDC,item->rcItem,(item->itemState&ODS_SELECTED)?Header:Field);
      if(item->itemID<std::size(mw2::MissionCatalog)){
        const bool closed=(item->itemState&ODS_COMBOBOXEDIT)!=0;
        const int thumb=px(closed?44:64);
        RECT imageRect{item->rcItem.left+px(5),item->rcItem.top+px(3),item->rcItem.left+px(5)+thumb,item->rcItem.bottom-px(3)};
        auto image=missionBitmap(item->itemID);
        if(image)drawImageFit(item->hDC,image,imageRect);
        const auto& mission=mw2::MissionCatalog[item->itemID];
        std::wstring label=std::wstring(mission.name)+(mission.concept?L" [CONCEPT]":L"");
        RECT labelRect=item->rcItem;labelRect.left=imageRect.right+px(9);labelRect.right-=px(5);
        auto old=SelectObject(item->hDC,font?font:GetStockObject(DEFAULT_GUI_FONT));
        SetTextColor(item->hDC,Ink);SetBkMode(item->hDC,TRANSPARENT);
        DrawTextW(item->hDC,label.c_str(),-1,&labelRect,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
        SelectObject(item->hDC,old);
      }
      if(item->itemState&ODS_FOCUS)DrawFocusRect(item->hDC,&item->rcItem);
      return TRUE;
    }
    if (id >= Player0 && id <= Player3) {
      const bool occupied = lobby.players[id - Player0].occupied;
      fill(item->hDC, item->rcItem,
           occupied ? RGB(35, 43, 28) : RGB(25, 31, 23));
      auto old = SelectObject(item->hDC, rowFont);
      SetBkMode(item->hDC, TRANSPARENT);
      SetTextColor(item->hDC, occupied ? Ink : Muted);
      auto label = value(item->hwndItem);
      RECT bounds = item->rcItem;
      DrawTextW(item->hDC, label.c_str(), -1, &bounds,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
      SelectObject(item->hDC, old);
      return TRUE;
    }
    bool disabled = (item->itemState & ODS_DISABLED) != 0;
    bool selected =
        (id == ModeHost && hostMode) || (id == ModeJoin && !hostMode) ||
        (id == TabPlay && tab == Tab::Play) ||
        (id == TabSettings && tab == Tab::Settings) ||
        (id == TabInfo && tab == Tab::Info) ||
        (id >= Diff0 && id <= Diff3 && id - Diff0 == selectedDifficulty);
    COLORREF color = selected ? Header : Field;
    if (id == LobbyButton && !disabled)
      color = Header;
    if (item->itemState & ODS_SELECTED)
      color = Line;
    fill(item->hDC, item->rcItem, color);
    if (id == Launch) {
      for (int y = 0; y < 61; y += 4) {
        RECT scan{item->rcItem.left, item->rcItem.top + px(y),
                  item->rcItem.right, item->rcItem.top + px(y + 1)};
        fill(item->hDC, scan, RGB(26, 37, 21));
      }
      for (int i = 0; i < 12; i++) {
        int x = (i * 47) % 550;
        RECT camo{px(x), px((i * 13) % 42), px(x + 18), px((i * 13) % 42 + 10)};
        fill(item->hDC, camo, i % 2 ? RGB(35, 48, 26) : RGB(23, 34, 19));
      }
    }
    auto previous = SelectObject(item->hDC, font);
    SetBkMode(item->hDC, TRANSPARENT);
    SetTextColor(item->hDC, selected ? Ink : disabled ? Muted : Ink);
    RECT rect = item->rcItem;
    auto label = value(item->hwndItem);
    if (id == Launch) {
      HFONT large = CreateFontW(-px(30), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0,
                                L"Bahnschrift SemiCondensed");
      SelectObject(item->hDC, large);
      DrawTextW(item->hDC, label.c_str(), -1, &rect,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      SelectObject(item->hDC, previous);
      DeleteObject(large);
    } else {
      DrawTextW(item->hDC, label.c_str(), -1, &rect,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      SelectObject(item->hDC, previous);
    }
    HBRUSH border = CreateSolidBrush(selected ? Muted : Line);
    FrameRect(item->hDC, &item->rcItem, border);
    DeleteObject(border);
    if (item->itemState & ODS_FOCUS) {
      InflateRect(&rect, -px(3), -px(3));
      DrawFocusRect(item->hDC, &rect);
    }
    return TRUE;
  }
  case WM_ERASEBKGND:
    return 1;
  case WM_PAINT: {
    PAINTSTRUCT ps{};
    HDC dc = BeginPaint(hwnd, &ps);
    RECT r{};
    GetClientRect(hwnd, &r);
    if (r.right > 0 && r.bottom > 0) {
      HDC back = CreateCompatibleDC(dc);
      HBITMAP bitmap = CreateCompatibleBitmap(dc, r.right, r.bottom);
      auto old = SelectObject(back, bitmap);
      paint(back);
      BitBlt(dc, 0, 0, r.right, r.bottom, back, 0, 0, SRCCOPY);
      SelectObject(back, old);
      DeleteObject(bitmap);
      DeleteDC(back);
    }
    EndPaint(hwnd, &ps);
    return 0;
  }
  case WM_DESTROY:
    KillTimer(hwnd, 1);
    artwork.reset();
    brandArtwork.reset();missionPreviews.clear();missionPixels.clear();
    if(brandStream){brandStream->Release();brandStream=nullptr;}
    if (artStream) {
      artStream->Release();
      artStream = nullptr;
    }
    DeleteObject(font);
    DeleteObject(codeFont);
    DeleteObject(rowFont);
    DeleteObject(fieldBrush);
    DeleteObject(occupiedBrush);
    DeleteObject(waitingBrush);
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProcW(hwnd, message, wParam, lParam);
}
} // namespace
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int show) {
  int argc = 0;
  wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  if (argc == 5 && std::wstring(argv[1]) == L"--self-test" &&
      std::wstring(argv[3]) == L"--report") {
    int result = 2;
    try {
      auto data = mw2::inspect(argv[2], true);
      mw2::saveReport(argv[4], data.report);
      result = data.componentPassed ? 0 : 1;
    } catch (...) {
      result = 2;
    }
    LocalFree(argv);
    return result;
  }
  if (argc == 3 && std::wstring(argv[1]) == L"--test-lobby-base")
    testBase = argv[2];
  LocalFree(argv);
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  const auto apartment = RoInitialize(RO_INIT_SINGLETHREADED);
  Gdiplus::GdiplusStartupInput input;
  Gdiplus::GdiplusStartup(&graphicsToken, &input, nullptr);
  WNDCLASSW wc{};
  wc.hInstance = instance;
  wc.lpfnWndProc = procedure;
  wc.lpszClassName = L"BreezOpsMW2Lobby";
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1));
  SetCurrentProcessExplicitAppUserModelID(L"BreezOps.MW2SpecOps4.Launcher");
  RegisterClassW(&wc);
  dpi = GetDpiForSystem();
  HWND hwnd = CreateWindowExW(
      0, wc.lpszClassName, L"MW2 4-Player Spec Ops | OPS HUB",
      WS_POPUP | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN, CW_USEDEFAULT,
      CW_USEDEFAULT, px(1100), px(620), nullptr, nullptr, instance, nullptr);
  if (!hwnd) {
    Gdiplus::GdiplusShutdown(graphicsToken);
    if (SUCCEEDED(apartment))
      RoUninitialize();
    return 2;
  }
  ShowWindow(hwnd, show);
  UpdateWindow(hwnd);
  MSG message{};
  while (GetMessageW(&message, nullptr, 0, 0) > 0)
    if (!IsDialogMessageW(hwnd, &message)) {
      TranslateMessage(&message);
      DispatchMessageW(&message);
    }
  Gdiplus::GdiplusShutdown(graphicsToken);
  if (SUCCEEDED(apartment))
    RoUninitialize();
  return static_cast<int>(message.wParam);
}
