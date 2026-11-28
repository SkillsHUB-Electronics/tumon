// Tumon: the app. Choose a camera, press Start, and Tumon Camera shows it to TeamTalk and other apps.
//
// A single dialog made of standard Windows controls (so screen readers understand it), a notification-area icon,
// and the feeder engine on its own thread. Options: --tray starts hidden in the notification area.
#include <windows.h>

#include <commctrl.h>
#include <shellapi.h>
#include <windowsx.h>
#include <ole2.h>  // the UI Automation headers need COM, which WIN32_LEAN_AND_MEAN leaves out
#include <UIAutomation.h>

#include <mfapi.h>

#include <memory>
#include <string>
#include <vector>

#include "../src/feeder/camera_feeder.h"
#include "resource.h"
#include "settings.h"
#include "strings.h"

#pragma comment(lib, "comctl32")
#pragma comment(lib, "shell32")
#pragma comment(lib, "uiautomationcore")
#pragma comment(lib, "oleaut32")

namespace {

constexpr UINT WM_APP_STATUS = WM_APP + 1;  // lParam: FeederStatus* (owned by the receiver)
constexpr UINT WM_APP_TRAY = WM_APP + 2;
constexpr UINT kTrayId = 1;
constexpr wchar_t kInstanceMutex[] = L"Local\\TumonApp";

UINT g_showMessage = 0;     // a second start asks the first one to show its window
UINT g_taskbarCreated = 0;  // the taskbar restarted: add the icon again

struct App {
    HWND window = nullptr;
    tumon::Settings settings;
    const tumon::Strings* text = &tumon::kEnglish;
    tumon::CameraFeeder feeder;
    tumon::FeederState shownState = tumon::FeederState::Stopped;
    tumon::FeederStatus lastStatus;
    bool installed = true;
    bool sending = false;
    bool startHidden = false;
};

App g_app;

const tumon::Strings* ChooseLanguage(const std::wstring& setting) {
    if (setting == L"id") return &tumon::kIndonesian;
    if (setting == L"en") return &tumon::kEnglish;
    return PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_INDONESIAN ? &tumon::kIndonesian : &tumon::kEnglish;
}

// Screen readers do not announce a changed text box by themselves: raise a UI Automation notification.
void Announce(HWND window, const std::wstring& message) {
    IRawElementProviderSimple* provider = nullptr;
    if (FAILED(UiaHostProviderFromHwnd(window, &provider)) || !provider) return;
    BSTR text = SysAllocString(message.c_str());
    BSTR id = SysAllocString(L"TumonStatus");
    UiaRaiseNotificationEvent(provider, NotificationKind_ActionCompleted, NotificationProcessing_ImportantMostRecent, text, id);
    SysFreeString(text);
    SysFreeString(id);
    provider->Release();
}

std::wstring DescribeStatus() {
    const tumon::Strings& t = *g_app.text;
    if (!g_app.installed) return t.statusNotInstalled;
    const tumon::FeederStatus& s = g_app.lastStatus;
    wchar_t buffer[600];
    switch (g_app.sending ? s.state : tumon::FeederState::Stopped) {
        case tumon::FeederState::Waiting: return t.statusWaiting;
        case tumon::FeederState::Streaming: {
            swprintf_s(buffer, t.statusStreaming, s.camera.c_str(), s.width, s.height);
            std::wstring out = buffer;
            if (s.picturesPerSecond > 0) {
                swprintf_s(buffer, t.statusRate, s.picturesPerSecond);
                out += buffer;
            }
            return out;
        }
        case tumon::FeederState::CameraProblem:
            swprintf_s(buffer, t.statusProblem, static_cast<unsigned>(s.error));
            return buffer;
        case tumon::FeederState::CameraMissing: return t.statusMissing;
        case tumon::FeederState::AlreadyRunning: return t.statusAlreadyRunning;
        case tumon::FeederState::Stopped: break;
    }
    if (ComboBox_GetCount(GetDlgItem(g_app.window, IDC_CAMERA)) == 0) return t.statusNoCamera;
    return t.statusStopped;
}

// The Tumon icon at the size Windows wants for small (tray, title bar) or large (Alt+Tab) use.
HICON AppIcon(bool large) {
    return static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_TUMON), IMAGE_ICON,
                                         GetSystemMetrics(large ? SM_CXICON : SM_CXSMICON),
                                         GetSystemMetrics(large ? SM_CYICON : SM_CYSMICON), LR_SHARED));
}

// ------------------------------------------------------------------------------------------ tray icon

NOTIFYICONDATAW TrayData() {
    NOTIFYICONDATAW data{};
    data.cbSize = sizeof(data);
    data.hWnd = g_app.window;
    data.uID = kTrayId;
    return data;
}

void TrayAdd() {
    NOTIFYICONDATAW data = TrayData();
    data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    data.uCallbackMessage = WM_APP_TRAY;
    data.hIcon = AppIcon(false);
    wcscpy_s(data.szTip, L"Tumon");
    Shell_NotifyIconW(NIM_ADD, &data);
    data.uVersion = NOTIFYICON_VERSION_4;  // keyboard users get NIN_KEYSELECT and WM_CONTEXTMENU
    Shell_NotifyIconW(NIM_SETVERSION, &data);
}

void TrayTip(const std::wstring& tip) {
    NOTIFYICONDATAW data = TrayData();
    data.uFlags = NIF_TIP | NIF_SHOWTIP;
    wcsncpy_s(data.szTip, (L"Tumon: " + tip).c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &data);
}

void TrayBalloon(const wchar_t* title, const wchar_t* body) {
    NOTIFYICONDATAW data = TrayData();
    data.uFlags = NIF_INFO;
    wcsncpy_s(data.szInfoTitle, title, _TRUNCATE);
    wcsncpy_s(data.szInfo, body, _TRUNCATE);
    data.dwInfoFlags = NIIF_INFO;
    Shell_NotifyIconW(NIM_MODIFY, &data);
}

void TrayRemove() {
    NOTIFYICONDATAW data = TrayData();
    Shell_NotifyIconW(NIM_DELETE, &data);
}

// ------------------------------------------------------------------------------------------ window

void ShowStatus(bool announce) {
    const std::wstring status = DescribeStatus();
    SetDlgItemTextW(g_app.window, IDC_STATUS, status.c_str());
    TrayTip(status);
    if (announce) Announce(g_app.window, status);
}

void UpdateButtons() {
    SetDlgItemTextW(g_app.window, IDC_STARTSTOP, g_app.sending ? g_app.text->stop : g_app.text->start);
    const bool canStart = g_app.installed && ComboBox_GetCount(GetDlgItem(g_app.window, IDC_CAMERA)) > 0;
    EnableWindow(GetDlgItem(g_app.window, IDC_STARTSTOP), g_app.sending || canStart);
}

void ApplyTexts() {
    const tumon::Strings& t = *g_app.text;
    HWND w = g_app.window;
    SetDlgItemTextW(w, IDC_CAMERA_LABEL, t.cameraLabel);
    SetDlgItemTextW(w, IDC_REFRESH, t.refresh);
    SetDlgItemTextW(w, IDC_STATUS_LABEL, t.statusLabel);
    SetDlgItemTextW(w, IDC_AUTOSTART, t.autoStart);
    SetDlgItemTextW(w, IDC_RUNATLOGIN, t.runAtLogin);
    SetDlgItemTextW(w, IDC_HELP_LABEL, t.helpLabel);
    SetDlgItemTextW(w, IDC_HELPTEXT, t.help);
    SetDlgItemTextW(w, IDC_LANG_LABEL, t.languageLabel);
    SetDlgItemTextW(w, IDC_EXIT, t.exitButton);

    HWND language = GetDlgItem(w, IDC_LANG);
    ComboBox_ResetContent(language);
    ComboBox_AddString(language, t.languageAuto);
    ComboBox_AddString(language, L"English");
    ComboBox_AddString(language, L"Bahasa Indonesia");
    ComboBox_SetCurSel(language, g_app.settings.language == L"en" ? 1 : g_app.settings.language == L"id" ? 2 : 0);
    UpdateButtons();
    ShowStatus(false);
}

std::wstring SelectedCamera() {
    HWND combo = GetDlgItem(g_app.window, IDC_CAMERA);
    const int index = ComboBox_GetCurSel(combo);
    if (index < 0) return L"";
    std::wstring name(static_cast<size_t>(ComboBox_GetLBTextLen(combo, index)), L'\0');
    ComboBox_GetLBText(combo, index, name.data());
    return name;
}

void RefreshCameras() {
    g_app.installed = tumon::IsTumonCameraInstalled();
    HWND combo = GetDlgItem(g_app.window, IDC_CAMERA);
    ComboBox_ResetContent(combo);
    int selected = -1;
    for (const std::wstring& name : tumon::ListSourceCameras()) {
        const int index = ComboBox_AddString(combo, name.c_str());
        if (name == g_app.settings.camera) selected = index;
    }
    // Keep showing a saved camera that is unplugged right now: the feeder waits for it.
    if (selected < 0 && !g_app.settings.camera.empty()) selected = ComboBox_AddString(combo, g_app.settings.camera.c_str());
    if (selected < 0 && ComboBox_GetCount(combo) > 0) selected = 0;
    ComboBox_SetCurSel(combo, selected);
    UpdateButtons();
    ShowStatus(false);
}

void StopSending() {
    g_app.feeder.Stop();
    g_app.sending = false;
    g_app.lastStatus = tumon::FeederStatus{};
    g_app.shownState = tumon::FeederState::Stopped;
    UpdateButtons();
    ShowStatus(true);
}

void StartSending() {
    const std::wstring camera = SelectedCamera();
    if (camera.empty() || !g_app.installed) {
        ShowStatus(true);
        return;
    }
    g_app.settings.camera = camera;
    tumon::SaveSettings(g_app.settings);

    tumon::FeederOptions options;
    options.camera = camera;
    g_app.sending = true;
    g_app.lastStatus = tumon::FeederStatus{};
    g_app.lastStatus.state = tumon::FeederState::Waiting;
    g_app.shownState = tumon::FeederState::Stopped;

    const HWND window = g_app.window;
    g_app.feeder.Start(options, [window](const tumon::FeederStatus& status) {
        auto copy = std::make_unique<tumon::FeederStatus>(status);
        if (PostMessageW(window, WM_APP_STATUS, 0, reinterpret_cast<LPARAM>(copy.get()))) copy.release();
    });
    UpdateButtons();
    ShowStatus(true);
}

void ShowMainWindow() {
    ShowWindow(g_app.window, SW_SHOW);
    ShowWindow(g_app.window, SW_RESTORE);
    SetForegroundWindow(g_app.window);
}

void HideToTray() {
    ShowWindow(g_app.window, SW_HIDE);
    if (!g_app.settings.trayHintShown) {
        TrayBalloon(g_app.text->stillRunningTitle, g_app.text->stillRunningText);
        g_app.settings.trayHintShown = true;
        tumon::SaveSettings(g_app.settings);
    }
}

void TrayMenu(int x, int y) {
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, IDM_TRAY_OPEN, g_app.text->trayOpen);
    AppendMenuW(menu, MF_STRING, IDM_TRAY_STARTSTOP, g_app.sending ? g_app.text->stop : g_app.text->start);
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, IDM_TRAY_EXIT, g_app.text->trayExit);
    SetMenuDefaultItem(menu, IDM_TRAY_OPEN, FALSE);
    SetForegroundWindow(g_app.window);  // so the menu closes when focus moves away
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, x, y, 0, g_app.window, nullptr);
    DestroyMenu(menu);
}

INT_PTR CALLBACK MainDialog(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == g_showMessage && g_showMessage != 0) {
        ShowMainWindow();
        return TRUE;
    }
    if (message == g_taskbarCreated && g_taskbarCreated != 0) {
        TrayAdd();
        ShowStatus(false);
        return TRUE;
    }

    switch (message) {
        case WM_INITDIALOG: {
            g_app.window = window;
            SendMessageW(window, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(AppIcon(false)));
            SendMessageW(window, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(AppIcon(true)));
            TrayAdd();
            CheckDlgButton(window, IDC_AUTOSTART, g_app.settings.startSendingOnOpen ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(window, IDC_RUNATLOGIN, tumon::IsRunAtLogin() ? BST_CHECKED : BST_UNCHECKED);
            RefreshCameras();
            ApplyTexts();
            if (g_app.settings.startSendingOnOpen && g_app.installed) StartSending();
            return TRUE;  // let the dialog set the focus on the first control
        }

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_STARTSTOP:
                case IDM_TRAY_STARTSTOP:
                    if (g_app.sending) StopSending();
                    else StartSending();
                    return TRUE;
                case IDC_REFRESH:
                    RefreshCameras();
                    return TRUE;
                case IDC_CAMERA:
                    if (HIWORD(wParam) == CBN_SELCHANGE) {
                        g_app.settings.camera = SelectedCamera();
                        tumon::SaveSettings(g_app.settings);
                        if (g_app.sending) StartSending();  // switch to the newly chosen camera
                    }
                    return TRUE;
                case IDC_AUTOSTART:
                    g_app.settings.startSendingOnOpen = IsDlgButtonChecked(window, IDC_AUTOSTART) == BST_CHECKED;
                    tumon::SaveSettings(g_app.settings);
                    return TRUE;
                case IDC_RUNATLOGIN: {
                    const bool wanted = IsDlgButtonChecked(window, IDC_RUNATLOGIN) == BST_CHECKED;
                    tumon::SetRunAtLogin(wanted);
                    CheckDlgButton(window, IDC_RUNATLOGIN, tumon::IsRunAtLogin() ? BST_CHECKED : BST_UNCHECKED);
                    return TRUE;
                }
                case IDC_LANG:
                    if (HIWORD(wParam) == CBN_SELCHANGE) {
                        const int index = ComboBox_GetCurSel(GetDlgItem(window, IDC_LANG));
                        g_app.settings.language = index == 1 ? L"en" : index == 2 ? L"id" : L"auto";
                        tumon::SaveSettings(g_app.settings);
                        g_app.text = ChooseLanguage(g_app.settings.language);
                        ApplyTexts();
                    }
                    return TRUE;
                case IDM_TRAY_OPEN:
                    ShowMainWindow();
                    return TRUE;
                case IDC_EXIT:
                case IDM_TRAY_EXIT:
                    DestroyWindow(window);
                    return TRUE;
                case IDCANCEL:  // Escape
                    HideToTray();
                    return TRUE;
            }
            break;

        case WM_CLOSE:
            HideToTray();
            return TRUE;

        case WM_APP_STATUS: {
            std::unique_ptr<tumon::FeederStatus> status(reinterpret_cast<tumon::FeederStatus*>(lParam));
            if (!g_app.sending) return TRUE;  // a late message from a feeder that was already stopped
            const bool changed = status->state != g_app.shownState;
            g_app.lastStatus = *status;
            g_app.shownState = status->state;
            if (status->state == tumon::FeederState::AlreadyRunning) {
                g_app.feeder.Stop();
                const std::wstring text = DescribeStatus();
                g_app.sending = false;
                UpdateButtons();
                SetDlgItemTextW(window, IDC_STATUS, text.c_str());
                Announce(window, text);
                return TRUE;
            }
            ShowStatus(changed);  // announce changes of state, not the picture count every two seconds
            return TRUE;
        }

        case WM_APP_TRAY:
            switch (LOWORD(lParam)) {
                case NIN_SELECT:
                case NIN_KEYSELECT:
                    ShowMainWindow();
                    return TRUE;
                case WM_CONTEXTMENU:
                    TrayMenu(GET_X_LPARAM(wParam), GET_Y_LPARAM(wParam));
                    return TRUE;
            }
            break;

        case WM_DESTROY:
            g_app.sending = false;
            g_app.feeder.Stop();
            TrayRemove();
            PostQuitMessage(0);
            return TRUE;
    }
    return FALSE;
}

}  // namespace

int APIENTRY wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE, _In_ LPWSTR commandLine, _In_ int) {
    g_showMessage = RegisterWindowMessageW(L"Tumon.ShowMainWindow");
    g_taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    g_app.startHidden = wcsstr(commandLine, L"--tray") != nullptr;

    HANDLE instanceMutex = CreateMutexW(nullptr, FALSE, kInstanceMutex);
    if (instanceMutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        AllowSetForegroundWindow(ASFW_ANY);
        PostMessageW(HWND_BROADCAST, g_showMessage, 0, 0);
        return 0;
    }

    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&controls);
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)) || FAILED(MFStartup(MF_VERSION))) {
        MessageBoxW(nullptr, L"Tumon could not start Windows Media Foundation.", L"Tumon", MB_ICONERROR);
        return 1;
    }

    g_app.settings = tumon::LoadSettings();
    g_app.text = ChooseLanguage(g_app.settings.language);

    const HWND window = CreateDialogParamW(instance, MAKEINTRESOURCEW(IDD_MAIN), nullptr, MainDialog, 0);
    if (!window) return 1;
    if (!g_app.startHidden) ShowWindow(window, SW_SHOW);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(window, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    MFShutdown();
    CoUninitialize();
    if (instanceMutex) CloseHandle(instanceMutex);
    return 0;
}
