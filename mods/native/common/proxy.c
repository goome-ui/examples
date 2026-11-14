#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winver.h>

int goome_mod_start(void);
void goome_mod_stop(void);

static HMODULE real;

static FARPROC pick(const char *name)
{
    return real ? GetProcAddress(real, name) : NULL;
}

#define FWD(ret, name, args, call) \
    __declspec(dllexport) ret WINAPI name args \
    { \
        typedef ret(WINAPI * fwd_t) args; \
        static fwd_t fwd; \
        if (!fwd) fwd = (fwd_t)(void (*)(void))pick(#name); \
        return fwd ? fwd call : (ret)0; \
    }

FWD(DWORD, GetFileVersionInfoSizeA, (LPCSTR a, LPDWORD b), (a, b))
FWD(DWORD, GetFileVersionInfoSizeW, (LPCWSTR a, LPDWORD b), (a, b))
FWD(DWORD, GetFileVersionInfoSizeExA, (DWORD a, LPCSTR b, LPDWORD c), (a, b, c))
FWD(DWORD, GetFileVersionInfoSizeExW, (DWORD a, LPCWSTR b, LPDWORD c), (a, b, c))
FWD(BOOL, GetFileVersionInfoA, (LPCSTR a, DWORD b, DWORD c, LPVOID d), (a, b, c, d))
FWD(BOOL, GetFileVersionInfoW, (LPCWSTR a, DWORD b, DWORD c, LPVOID d), (a, b, c, d))
FWD(BOOL, GetFileVersionInfoExA, (DWORD a, LPCSTR b, DWORD c, DWORD d, LPVOID e), (a, b, c, d, e))
FWD(BOOL, GetFileVersionInfoExW, (DWORD a, LPCWSTR b, DWORD c, DWORD d, LPVOID e), (a, b, c, d, e))
FWD(DWORD, VerLanguageNameA, (DWORD a, LPSTR b, DWORD c), (a, b, c))
FWD(DWORD, VerLanguageNameW, (DWORD a, LPWSTR b, DWORD c), (a, b, c))
FWD(BOOL, VerQueryValueA, (LPCVOID a, LPCSTR b, LPVOID *c, PUINT d), (a, b, c, d))
FWD(BOOL, VerQueryValueW, (LPCVOID a, LPCWSTR b, LPVOID *c, PUINT d), (a, b, c, d))
FWD(DWORD, VerFindFileA, (DWORD a, LPSTR b, LPSTR c, LPSTR d, LPSTR e, PUINT f, LPSTR g, PUINT h), (a, b, c, d, e, f, g, h))
FWD(DWORD, VerFindFileW, (DWORD a, LPWSTR b, LPWSTR c, LPWSTR d, LPWSTR e, PUINT f, LPWSTR g, PUINT h), (a, b, c, d, e, f, g, h))
FWD(DWORD, VerInstallFileA, (DWORD a, LPSTR b, LPSTR c, LPSTR d, LPSTR e, LPSTR f, LPSTR g, PUINT h), (a, b, c, d, e, f, g, h))
FWD(DWORD, VerInstallFileW, (DWORD a, LPWSTR b, LPWSTR c, LPWSTR d, LPWSTR e, LPWSTR f, LPWSTR g, PUINT h), (a, b, c, d, e, f, g, h))

static void load_real(void)
{
    wchar_t path[MAX_PATH];
    UINT n = GetSystemDirectoryW(path, MAX_PATH);
    if (!n || n > MAX_PATH - 16) return;
    lstrcatW(path, L"\\version.dll");
    real = LoadLibraryW(path);
}

BOOL WINAPI DllMain(HINSTANCE self, DWORD why, LPVOID reserved)
{
    (void)reserved;
    if (why == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(self);
        load_real();
        CloseHandle(CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)(void (*)(void))goome_mod_start, NULL, 0, NULL));
    } else if (why == DLL_PROCESS_DETACH) {
        goome_mod_stop();
    }
    return TRUE;
}
