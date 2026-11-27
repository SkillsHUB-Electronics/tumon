// The app's settings, stored for the current Windows user (HKEY_CURRENT_USER).
#pragma once

#include <string>

namespace tumon {

struct Settings {
    std::wstring camera;                 // chosen source camera (its name)
    bool startSendingOnOpen = false;     // press Start automatically
    std::wstring language = L"auto";     // "auto", "en" or "id"
    bool trayHintShown = false;          // the "still running" balloon was shown once
};

Settings LoadSettings();
void SaveSettings(const Settings& settings);

bool IsRunAtLogin();
bool SetRunAtLogin(bool enabled);  // HKCU\...\CurrentVersion\Run; returns false if it could not be changed

}  // namespace tumon
