// All user-visible texts, English and Bahasa Indonesia. Add a string to BOTH tables.
// "&" marks the keyboard shortcut (Alt + letter).
#pragma once

namespace tumon {

struct Strings {
    const wchar_t* cameraLabel;
    const wchar_t* refresh;
    const wchar_t* start;
    const wchar_t* stop;
    const wchar_t* statusLabel;
    const wchar_t* autoStart;
    const wchar_t* runAtLogin;
    const wchar_t* helpLabel;
    const wchar_t* help;
    const wchar_t* languageLabel;
    const wchar_t* languageAuto;
    const wchar_t* exitButton;
    const wchar_t* trayOpen;
    const wchar_t* trayExit;
    const wchar_t* stillRunningTitle;
    const wchar_t* stillRunningText;

    const wchar_t* statusStopped;
    const wchar_t* statusWaiting;
    const wchar_t* statusStreaming;     // %s camera, %u width, %u height
    const wchar_t* statusRate;          // %.0f pictures per second
    const wchar_t* statusProblem;       // %08X error
    const wchar_t* statusMissing;
    const wchar_t* statusAlreadyRunning;
    const wchar_t* statusNotInstalled;
    const wchar_t* statusNoCamera;
};

inline const Strings kEnglish = {
    L"&Camera source:",
    L"&Refresh list",
    L"&Start",
    L"S&top",
    L"S&tatus:",
    L"Start sending when &Tumon opens",
    L"Open Tumon when I sign in to &Windows",
    L"TeamTalk &setup:",
    L"In TeamTalk open Preferences, Video Capture.\r\nVideo Capture Device: Tumon Camera.\r\n"
    L"Video Resolution: 640x480, FPS 30. Image Format: YUY2.\r\nThen turn on video transmission.",
    L"&Language:",
    L"Same as Windows",
    L"E&xit",
    L"&Open Tumon",
    L"E&xit",
    L"Tumon is still running",
    L"Tumon is in the notification area. Open it there, or choose Exit to stop it.",

    L"Stopped. Press Start to send your camera to TeamTalk.",
    L"Ready. Your camera turns on when an app (such as TeamTalk) uses Tumon Camera.",
    L"Sending %s at %ux%u.",
    L" About %.0f pictures per second.",
    L"The camera could not be used (error 0x%08X). Tumon will try again. Check Windows Settings, Privacy, Camera, "
    L"and close other apps that use this camera.",
    L"The chosen camera is not connected. Tumon is looking for it.",
    L"Another Tumon program is already sending a camera. Close it first.",
    L"Tumon Camera is not installed on this PC. Run the Tumon installer again.",
    L"No camera found. Connect a camera, then choose Refresh list.",
};

inline const Strings kIndonesian = {
    L"Sumber &kamera:",
    L"Muat &ulang daftar",
    L"&Mulai",
    L"&Hentikan",
    L"S&tatus:",
    L"Langsung &kirim saat Tumon dibuka",
    L"Buka Tumon saat saya masuk ke &Windows",
    L"Pengaturan Team&Talk:",
    L"Di TeamTalk buka Preferences, Video Capture.\r\nVideo Capture Device: Tumon Camera.\r\n"
    L"Video Resolution: 640x480, FPS 30. Image Format: YUY2.\r\nLalu aktifkan transmisi video.",
    L"&Bahasa:",
    L"Sama dengan Windows",
    L"K&eluar",
    L"&Buka Tumon",
    L"K&eluar",
    L"Tumon masih berjalan",
    L"Tumon ada di area notifikasi. Buka dari sana, atau pilih Keluar untuk menghentikannya.",

    L"Berhenti. Tekan Mulai untuk mengirim kamera Anda ke TeamTalk.",
    L"Siap. Kamera Anda menyala saat ada aplikasi (misalnya TeamTalk) memakai Tumon Camera.",
    L"Mengirim %s pada %ux%u.",
    L" Sekitar %.0f gambar per detik.",
    L"Kamera tidak dapat dipakai (kesalahan 0x%08X). Tumon akan mencoba lagi. Periksa Pengaturan Windows, Privasi, "
    L"Kamera, dan tutup aplikasi lain yang memakai kamera ini.",
    L"Kamera yang dipilih tidak tersambung. Tumon sedang mencarinya.",
    L"Program Tumon lain sudah mengirim kamera. Tutup dulu program itu.",
    L"Tumon Camera belum terpasang di PC ini. Jalankan lagi pemasang Tumon.",
    L"Kamera tidak ditemukan. Sambungkan kamera, lalu pilih Muat ulang daftar.",
};

}  // namespace tumon
