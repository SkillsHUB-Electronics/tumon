#include "settings.h"

#include <windows.h>

namespace tumon {

namespace {

constexpr wchar_t kKey[] = L"Software\\SkillsHUB Electronics\\Tumon";
constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kRunValue[] = L"Tumon";

std::wstring ReadString(HKEY key, const wchar_t* name, const std::wstring& fallback) {
    wchar_t buffer[512];
    DWORD size = sizeof(buffer);
    if (RegGetValueW(key, nullptr, name, RRF_RT_REG_SZ, nullptr, buffer, &size) != ERROR_SUCCESS) return fallback;
    return buffer;
}

bool ReadBool(HKEY key, const wchar_t* name, bool fallback) {
    DWORD value = 0, size = sizeof(value);
    if (RegGetValueW(key, nullptr, name, RRF_RT_REG_DWORD, nullptr, &value, &size) != ERROR_SUCCESS) return fallback;
    return value != 0;
}

void WriteString(HKEY key, const wchar_t* name, const std::wstring& value) {
    RegSetValueExW(key, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()),
                   static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
}

void WriteBool(HKEY key, const wchar_t* name, bool value) {
    const DWORD d = value ? 1 : 0;
    RegSetValueExW(key, name, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&d), sizeof(d));
}

}  // namespace

Settings LoadSettings() {
    Settings s;
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kKey, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) return s;
    s.camera = ReadString(key, L"Camera", s.camera);
    s.startSendingOnOpen = ReadBool(key, L"StartSendingOnOpen", s.startSendingOnOpen);
    s.language = ReadString(key, L"Language", s.language);
    s.trayHintShown = ReadBool(key, L"TrayHintShown", s.trayHintShown);
    RegCloseKey(key);
    return s;
}

void SaveSettings(const Settings& s) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS) return;
    WriteString(key, L"Camera", s.camera);
    WriteBool(key, L"StartSendingOnOpen", s.startSendingOnOpen);
    WriteString(key, L"Language", s.language);
    WriteBool(key, L"TrayHintShown", s.trayHintShown);
    RegCloseKey(key);
}

bool IsRunAtLogin() {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) return false;
    const bool present = RegQueryValueExW(key, kRunValue, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS;
    RegCloseKey(key);
    return present;
}

bool SetRunAtLogin(bool enabled) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRunKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS) return false;
    LSTATUS status;
    if (enabled) {
        wchar_t path[MAX_PATH];
        const DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
        if (n == 0 || n >= MAX_PATH) {
            RegCloseKey(key);
            return false;
        }
        const std::wstring command = L"\"" + std::wstring(path) + L"\" --tray";
        status = RegSetValueExW(key, kRunValue, 0, REG_SZ, reinterpret_cast<const BYTE*>(command.c_str()),
                                static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
    } else {
        status = RegDeleteValueW(key, kRunValue);
        if (status == ERROR_FILE_NOT_FOUND) status = ERROR_SUCCESS;
    }
    RegCloseKey(key);
    return status == ERROR_SUCCESS;
}

}  // namespace tumon
