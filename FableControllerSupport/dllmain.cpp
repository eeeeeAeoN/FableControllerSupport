// ---------------------------------------------------------
// Project EgoCore - Controller Support UI & Camera Injection
// Developer: AlbionSecrets
// ---------------------------------------------------------
#include <windows.h>
#include <dinput.h>
#include <Xinput.h>
#include <initguid.h> 
#include "MinHook.h"
#include <fstream>
#include <string>
#include <mutex>
#include <stdio.h> 

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "Xinput.lib")

// --- Logger ---
std::ofstream g_Log;
std::mutex g_LogMutex;

void Log(const std::string& message) {
    std::lock_guard<std::mutex> lock(g_LogMutex);
    if (g_Log.is_open()) {
        g_Log << "[EgoCore] " << message << std::endl;
        g_Log.flush();
    }
}

void LogHex(const std::string& prefix, DWORD value) {
    char buf[128];
    snprintf(buf, sizeof(buf), "%s0x%02X", prefix.c_str(), value);
    Log(buf);
}

// --- Engine Pointers & State ---
namespace Retail {
    constexpr ptrdiff_t OFFSET_CWORLD_IN_GUI = 0x04;
    constexpr ptrdiff_t OFFSET_DEFMANAGER_IN_WORLD = 0x10;
    constexpr ptrdiff_t OFFSET_CAMERAMANAGER_IN_WORLD = 0x30;
    constexpr ptrdiff_t OFFSET_PDRAW_QUICK_ACCESS = 0x1D8;
    constexpr ptrdiff_t OFFSET_PDRAW_ABILITY_BUTTONS = 0x200;
    constexpr ptrdiff_t OFFSET_ILLUSTRATORS = 0x2C0;

    constexpr uintptr_t ADDR_PGUIDEF_GLOBAL = 0x013B878C;

    constexpr uintptr_t ADDR_CPLAYERGUI_UPDATE = 0x0043A080;
    constexpr uintptr_t ADDR_CONSTRUCT_XBOX_ABILITY = 0x00651A7E;
    constexpr uintptr_t ADDR_CONSTRUCT_XBOX_QUICKACCESS = 0x00652F0A;
    constexpr uintptr_t ADDR_CCHARSTRING_CONSTRUCT = 0x0099EBF0;

    constexpr uintptr_t ADDR_GETDEF_CAMERA = 0x0043FF30;
    constexpr uintptr_t ADDR_SET_CAMERA_MANAGER_DEF = 0x0069A850;

    constexpr uintptr_t ADDR_GETDEF_CONTROLS = 0x00410820;
    constexpr uintptr_t ADDR_APPLY_VALUES = 0x00407370;
    constexpr uintptr_t ADDR_USER_PROFILE_GET = 0x0040D2A0;

    constexpr uintptr_t ADDR_ISTOGGLE_ZTARGET = 0x00445B80;
    constexpr uintptr_t ADDR_ISTOGGLE_SNEAK = 0x00000000;
    constexpr uintptr_t ADDR_ISTOGGLE_SPELLS = 0x00000000;
}

// --- Hotswap Global State ---
bool g_IsControllerActive = false;
bool g_LastControllerState = false;
bool g_ShadowObjectsInitialized = false;

static char g_XboxAbilityBuffer[256];
static char g_XboxQuickAccessBuffer[256];
void* g_PCAbilityButtons = nullptr;
void* g_PCQuickAccessIcons = nullptr;

void* g_CameraDef_XboxMain = nullptr;
void* g_CameraDef_XboxCombat = nullptr;
void* g_CameraDef_PCMain = nullptr;
void* g_CameraDef_PCCombat = nullptr;
void* g_CurrentActiveCameraDef = nullptr;

void* g_PCControlDef = nullptr;
void* g_XboxControlDef = nullptr;

char g_PCPassiveBackup[12];
char g_PCAggressiveBackup[12];

IDirectInputDevice8W* g_pMouse = nullptr;
IDirectInputDevice8W* g_pKeyboard = nullptr;
static DWORD g_LastDpadButtonOffset = 0;

// --- Function Typedefs ---
typedef void(__thiscall* CPlayerGui_Update_t)(void* _this, bool paused);
CPlayerGui_Update_t o_CPlayerGui_Update = nullptr;

typedef void* (__thiscall* CCharString_Construct_t)(void* _this, const char* str, long len);
CCharString_Construct_t o_CCharString_Construct = nullptr;

typedef void(__thiscall* ConstructAbility_t)(void* _this, void* playerGui, void* illustrators);
typedef void(__thiscall* ConstructQuick_t)(void* _this, void* playerGui, void* illustrators);

typedef void(__thiscall* GetDef_t)(void* _this, void* pString, void* pOutDef);
typedef void(__thiscall* SetCameraManagerDef_t)(void* _this, void** new_def);
SetCameraManagerDef_t o_SetCameraManagerDef = nullptr;

typedef void* (*GetUserProfileManager_t)();
typedef void(__thiscall* ApplyValues_t)(void* _this);

typedef bool(__thiscall* IsToggle_t)(void* _this);
IsToggle_t o_IsToggleZTarget = nullptr;
IsToggle_t o_IsToggleSneak = nullptr;
IsToggle_t o_IsToggleSpells = nullptr;


// =================================================================================
// 1. THE DEFINITION & LOGIC INTERCEPTORS 
// =================================================================================
bool __fastcall Hooked_IsToggleZTarget(void* _this, void* edx) {
    if (g_IsControllerActive) return false;
    return o_IsToggleZTarget(_this);
}

bool __fastcall Hooked_IsToggleSneak(void* _this, void* edx) {
    if (g_IsControllerActive) return false;
    return o_IsToggleSneak(_this);
}

bool __fastcall Hooked_IsToggleSpells(void* _this, void* edx) {
    if (g_IsControllerActive) return false;
    return o_IsToggleSpells(_this);
}

void* __fastcall Hooked_CCharString_Construct(void* _this, void* edx, const char* str, long len) {
    if (str && strcmp(str, "PLAYER_GUI_PC") == 0 && g_IsControllerActive) {
        return o_CCharString_Construct(_this, "PLAYER_GUI_DEFAULT", -1);
    }
    return o_CCharString_Construct(_this, str, len);
}

void __fastcall Hooked_SetCameraManagerDef(void* _this, void* edx, void** new_def) {
    if (!new_def || !*new_def) {
        o_SetCameraManagerDef(_this, new_def);
        return;
    }

    void* actual_def = *new_def;
    void* target_def = actual_def;

    if (g_IsControllerActive) {
        if (actual_def == g_CameraDef_PCMain && g_CameraDef_XboxMain) target_def = g_CameraDef_XboxMain;
        else if (actual_def == g_CameraDef_PCCombat && g_CameraDef_XboxCombat) target_def = g_CameraDef_XboxCombat;
    }
    else {
        if (actual_def == g_CameraDef_XboxMain && g_CameraDef_PCMain) target_def = g_CameraDef_PCMain;
        else if (actual_def == g_CameraDef_XboxCombat && g_CameraDef_PCCombat) target_def = g_CameraDef_PCCombat;
    }

    g_CurrentActiveCameraDef = target_def;
    void* override_def[1] = { target_def };
    o_SetCameraManagerDef(_this, override_def);
}


// =================================================================================
// 2. THE CORE UPDATE LOOP & LIVE PUPPETEER
// =================================================================================
void __fastcall Hooked_CPlayerGui_Update(void* _this, void* edx, bool paused) {

    static void* g_LastPlayerGuiInstance = nullptr;
    if (g_LastPlayerGuiInstance != _this) {
        Log("New Session Detected. Resetting Caches.");
        g_LastPlayerGuiInstance = _this;
        g_ShadowObjectsInitialized = false;
        g_PCAbilityButtons = nullptr;
        g_PCQuickAccessIcons = nullptr;
        g_CurrentActiveCameraDef = nullptr;
        g_LastControllerState = false;
    }

    if (!g_ShadowObjectsInitialized) {
        void* illustrators = (void*)((uintptr_t)_this + Retail::OFFSET_ILLUSTRATORS);

        memset(g_XboxAbilityBuffer, 0, sizeof(g_XboxAbilityBuffer));
        memset(g_XboxQuickAccessBuffer, 0, sizeof(g_XboxQuickAccessBuffer));

        bool tempState = g_IsControllerActive;
        g_IsControllerActive = true;

        void** pGlobalGuiDef = (void**)Retail::ADDR_PGUIDEF_GLOBAL;
        void* cachedDef = *pGlobalGuiDef;
        *pGlobalGuiDef = nullptr;

        ((ConstructAbility_t)Retail::ADDR_CONSTRUCT_XBOX_ABILITY)(g_XboxAbilityBuffer, _this, illustrators);
        ((ConstructQuick_t)Retail::ADDR_CONSTRUCT_XBOX_QUICKACCESS)(g_XboxQuickAccessBuffer, _this, illustrators);

        g_IsControllerActive = tempState;
        *pGlobalGuiDef = cachedDef;

        void* pWorld = *(void**)((uintptr_t)_this + Retail::OFFSET_CWORLD_IN_GUI);
        if (pWorld) {
            void* pDefManager = *(void**)((uintptr_t)pWorld + Retail::OFFSET_DEFMANAGER_IN_WORLD);
            GetDef_t GetCamDef = (GetDef_t)Retail::ADDR_GETDEF_CAMERA;
            GetDef_t GetControlDef = (GetDef_t)Retail::ADDR_GETDEF_CONTROLS;

            char strBuf[64] = { 0 };
            void* outDef[2] = { nullptr, nullptr };

            auto FetchDef = [&](GetDef_t fetcher, const char* name) -> void* {
                o_CCharString_Construct(strBuf, name, -1);
                outDef[0] = nullptr;
                fetcher(pDefManager, strBuf, outDef);
                return outDef[0];
                };

            g_CameraDef_XboxMain = FetchDef(GetCamDef, "CAMERA_MANAGER_MAIN");
            g_CameraDef_XboxCombat = FetchDef(GetCamDef, "CAMERA_MANAGER_COMBAT_MAIN");
            g_CameraDef_PCMain = FetchDef(GetCamDef, "CAMERA_MANAGER_PC_MAIN");
            g_CameraDef_PCCombat = FetchDef(GetCamDef, "CAMERA_MANAGER_PC_MAIN_COMBAT");

            g_PCControlDef = FetchDef(GetControlDef, "FABLE_PC_CONTROL_SCHEME_GDD_WASD");
            g_XboxControlDef = FetchDef(GetControlDef, "FABLE_XBOX_CONTROL_SCHEME");
        }
        g_ShadowObjectsInitialized = true;
    }

    bool requestedState = g_IsControllerActive;

    if (requestedState != g_LastControllerState) {
        g_LastControllerState = requestedState;

        // --- 1. Swap the GUI ---
        void** pGlobalGuiDef = (void**)Retail::ADDR_PGUIDEF_GLOBAL;
        if (pGlobalGuiDef && *pGlobalGuiDef != nullptr) *pGlobalGuiDef = nullptr;

        void** currentAbilityPtr = (void**)((uintptr_t)_this + Retail::OFFSET_PDRAW_ABILITY_BUTTONS);
        void** currentQuickAccessPtr = (void**)((uintptr_t)_this + Retail::OFFSET_PDRAW_QUICK_ACCESS);

        if (requestedState) {
            if (!g_PCAbilityButtons) g_PCAbilityButtons = *currentAbilityPtr;
            if (!g_PCQuickAccessIcons) g_PCQuickAccessIcons = *currentQuickAccessPtr;

            *currentAbilityPtr = g_XboxAbilityBuffer;
            *currentQuickAccessPtr = g_XboxQuickAccessBuffer;
        }
        else {
            if (g_PCAbilityButtons) *currentAbilityPtr = g_PCAbilityButtons;
            if (g_PCQuickAccessIcons) *currentQuickAccessPtr = g_PCQuickAccessIcons;
        }

        // --- 2. Swap the Camera ---
        void* pWorld = *(void**)((uintptr_t)_this + Retail::OFFSET_CWORLD_IN_GUI);
        if (pWorld) {
            void* pCameraManager = *(void**)((uintptr_t)pWorld + Retail::OFFSET_CAMERAMANAGER_IN_WORLD);
            if (pCameraManager) {
                void* targetDef = nullptr;

                if (requestedState) {
                    if (g_CurrentActiveCameraDef == nullptr || g_CurrentActiveCameraDef == g_CameraDef_PCMain) targetDef = g_CameraDef_XboxMain;
                    else if (g_CurrentActiveCameraDef == g_CameraDef_PCCombat) targetDef = g_CameraDef_XboxCombat;
                }
                else {
                    if (g_CurrentActiveCameraDef == nullptr || g_CurrentActiveCameraDef == g_CameraDef_XboxMain) targetDef = g_CameraDef_PCMain;
                    else if (g_CurrentActiveCameraDef == g_CameraDef_XboxCombat) targetDef = g_CameraDef_PCCombat;
                }

                if (targetDef != nullptr && targetDef != g_CurrentActiveCameraDef) {
                    static void* mockDefPointer[1] = { nullptr };
                    mockDefPointer[0] = targetDef;
                    o_SetCameraManagerDef(pCameraManager, mockDefPointer);
                    g_CurrentActiveCameraDef = targetDef;
                }
            }
        }

        // --- 3. Swap the Control Scheme ---
        if (g_XboxControlDef && g_PCControlDef) {
            void* pProfileManager = ((GetUserProfileManager_t)Retail::ADDR_USER_PROFILE_GET)();
            if (pProfileManager) {
                void* xboxVector = (void*)((uintptr_t)g_XboxControlDef + 0x3C);
                void* passiveVector = (void*)((uintptr_t)pProfileManager + 0x54);
                void* aggressiveVector = (void*)((uintptr_t)pProfileManager + 0x60);

                if (requestedState) {
                    memcpy(g_PCPassiveBackup, passiveVector, 12);
                    memcpy(g_PCAggressiveBackup, aggressiveVector, 12);
                    memcpy(passiveVector, xboxVector, 12);
                    memcpy(aggressiveVector, xboxVector, 12);

                    ((ApplyValues_t)Retail::ADDR_APPLY_VALUES)(pProfileManager);

                    memcpy(passiveVector, g_PCPassiveBackup, 12);
                    memcpy(aggressiveVector, g_PCAggressiveBackup, 12);
                }
                else {
                    ((ApplyValues_t)Retail::ADDR_APPLY_VALUES)(pProfileManager);
                }
            }
        }
    }

    o_CPlayerGui_Update(_this, paused);
}


// =================================================================================
// 3. THE DIRECTINPUT FILTER & MASKING
// =================================================================================
typedef HRESULT(WINAPI* tGetDeviceData)(IDirectInputDevice8W* _this, DWORD cbObjectData, LPDIDEVICEOBJECTDATA rgdod, LPDWORD pdwInOut, DWORD dwFlags);
tGetDeviceData oGetDeviceData = nullptr;

typedef HRESULT(WINAPI* tGetDeviceState)(IDirectInputDevice8W* _this, DWORD cbData, LPVOID lpvData);
tGetDeviceState oGetDeviceState = nullptr;

typedef HRESULT(WINAPI* tGetCapabilities)(IDirectInputDevice8W* _this, LPDIDEVCAPS lpDIDevCaps);
tGetCapabilities oGetCapabilities = nullptr;

typedef HRESULT(WINAPI* tEnumObjects)(IDirectInputDevice8W* _this, LPDIENUMDEVICEOBJECTSCALLBACKW lpCallback, LPVOID pvRef, DWORD dwFlags);
tEnumObjects oEnumObjects = nullptr;

DWORD TranslateStadiaButtonOffset(DWORD rawOffset) {
    switch (rawOffset) {
    case 0x30: return 0x33; case 0x31: return 0x34; case 0x32: return 0x30;
    case 0x33: return 0x31; case 0x34: return 0x35; case 0x35: return 0x32;
    case 0x36: return 0x38; case 0x37: return 0x39; case 0x38: return 0x3B;
    case 0x39: return 0x3A; case 0x3B: return 0x37; case 0x3C: return 0x36;
    }
    return rawOffset;
}

DWORD TranslateXboxDInputOffset(DWORD rawOffset) {
    switch (rawOffset) {
    case 0x30: return 0x33; case 0x31: return 0x34; case 0x32: return 0x30;
    case 0x33: return 0x31; case 0x34: return 0x35; case 0x35: return 0x32;
    case 0x36: return 0x3B; case 0x37: return 0x3A; case 0x38: return 0x38;
    case 0x39: return 0x39;
    }
    return rawOffset;
}

// --- VTABLE [3] CAPABILITIES HOOK ---
HRESULT WINAPI HookedGetCapabilities(IDirectInputDevice8W* _this, LPDIDEVCAPS lpDIDevCaps) {
    HRESULT hr = oGetCapabilities(_this, lpDIDevCaps);
    if (SUCCEEDED(hr) && _this != g_pMouse && _this != g_pKeyboard) {
        // Force the engine to allocate enough memory to cover the Xbox Button Enums
        if (lpDIDevCaps->dwButtons < 32) {
            lpDIDevCaps->dwButtons = 32;
        }
    }
    return hr;
}

// --- VTABLE [4] ENUM OBJECTS HOOK ---
// Thread-local variables to safely intercept Fable's object enumeration callback
thread_local LPDIENUMDEVICEOBJECTSCALLBACKW tl_OriginalCallback = nullptr;
thread_local int tl_MaxButtonIndex = -1;

BOOL CALLBACK InterceptedEnumCallback(LPCDIDEVICEOBJECTINSTANCEW lpddoi, LPVOID pvRef) {
    if (lpddoi->guidType == GUID_Button) {
        int btnIndex = DIDFT_GETINSTANCE(lpddoi->dwType);
        if (btnIndex > tl_MaxButtonIndex) tl_MaxButtonIndex = btnIndex;
    }
    return tl_OriginalCallback(lpddoi, pvRef);
}

HRESULT WINAPI HookedEnumObjects(IDirectInputDevice8W* _this, LPDIENUMDEVICEOBJECTSCALLBACKW lpCallback, LPVOID pvRef, DWORD dwFlags) {
    tl_OriginalCallback = lpCallback;
    tl_MaxButtonIndex = -1;

    HRESULT hr = oEnumObjects(_this, InterceptedEnumCallback, pvRef, dwFlags);

    if (SUCCEEDED(hr) && _this != g_pMouse && _this != g_pKeyboard) {
        // Synthesize any missing buttons past the hardware limit so Fable doesn't drop our emulated D-Pad events
        for (int i = tl_MaxButtonIndex + 1; i < 32; i++) {
            DIDEVICEOBJECTINSTANCEW doi = { 0 };
            doi.dwSize = sizeof(DIDEVICEOBJECTINSTANCEW);
            doi.guidType = GUID_Button;
            doi.dwOfs = 0x30 + i; // DIJOFS_BUTTON(i)
            doi.dwType = DIDFT_BUTTON | DIDFT_MAKEINSTANCE(i);
            wcscpy_s(doi.tszName, L"Virtual Enum Button");

            if (tl_OriginalCallback(&doi, pvRef) == DIENUM_STOP) break;
        }
    }
    return hr;
}


HRESULT WINAPI HookedGetDeviceState(IDirectInputDevice8W* _this, DWORD cbData, LPVOID lpvData) {
    HRESULT hr = oGetDeviceState(_this, cbData, lpvData);

    if (_this != nullptr && _this != g_pMouse && _this != g_pKeyboard) {
        XINPUT_STATE xState;
        if (XInputGetState(0, &xState) == ERROR_SUCCESS) {
            if (xState.Gamepad.wButtons != 0 ||
                abs(xState.Gamepad.sThumbLX) > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE ||
                abs(xState.Gamepad.sThumbLY) > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE ||
                abs(xState.Gamepad.sThumbRX) > XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE ||
                abs(xState.Gamepad.sThumbRY) > XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE)
            {
                g_IsControllerActive = true;
            }
        }
        else if (SUCCEEDED(hr) && cbData == sizeof(DIJOYSTATE)) {
            DIJOYSTATE* js = (DIJOYSTATE*)lpvData;
            for (int i = 0; i < 32; i++) { if (js->rgbButtons[i] & 0x80) g_IsControllerActive = true; }
            for (int i = 0; i < 4; i++) { if (js->rgdwPOV[i] != -1 && js->rgdwPOV[i] != 65535) g_IsControllerActive = true; }

            // POV Hat to Button Emulation
            DWORD pov = js->rgdwPOV[0];
            if (LOWORD(pov) != 0xFFFF) {
                if (pov >= 31500 || pov <= 4500)  js->rgbButtons[13] = 0x80; // 0x3D (UP)
                else if (pov >= 4500 && pov <= 13500) js->rgbButtons[16] = 0x80; // 0x40 (RIGHT)
                else if (pov >= 13500 && pov <= 22500) js->rgbButtons[14] = 0x80; // 0x3E (DOWN)
                else if (pov >= 22500 && pov <= 31500) js->rgbButtons[15] = 0x80; // 0x3F (LEFT)
            }
        }

        if (!g_IsControllerActive) return DIERR_NOTACQUIRED;
    }
    return hr;
}

HRESULT WINAPI HookedGetDeviceData(IDirectInputDevice8W* _this, DWORD cbObjectData, LPDIDEVICEOBJECTDATA rgdod, LPDWORD pdwInOut, DWORD dwFlags) {
    HRESULT hr = oGetDeviceData(_this, cbObjectData, rgdod, pdwInOut, dwFlags);

    if (FAILED(hr) || rgdod == nullptr || pdwInOut == nullptr || *pdwInOut == 0) return hr;

    if (_this == g_pMouse) {
        bool significantInput = false;
        for (DWORD i = 0; i < *pdwInOut; i++) {
            DWORD ofs = rgdod[i].dwOfs;
            int data = (int)rgdod[i].dwData;
            if (ofs >= 0x0C && ofs <= 0x0F && data != 0) significantInput = true;
            if ((ofs == 0x00 || ofs == 0x04) && (data > 2 || data < -2)) significantInput = true;
        }
        if (significantInput && g_IsControllerActive) g_IsControllerActive = false;
        return hr;
    }

    if (_this == g_pKeyboard) {
        bool keyPressed = false;
        for (DWORD i = 0; i < *pdwInOut; i++) {
            if ((rgdod[i].dwData & 0x80) != 0) keyPressed = true;
        }
        if (keyPressed && g_IsControllerActive) g_IsControllerActive = false;
        return hr;
    }

    if (!g_IsControllerActive) g_IsControllerActive = true;

    XINPUT_STATE xState;
    bool isXInput = (XInputGetState(0, &xState) == ERROR_SUCCESS);

    for (DWORD i = 0; i < *pdwInOut; i++) {
        DWORD originalOffset = rgdod[i].dwOfs;

        // 1. Standard Button Translation
        if (originalOffset >= 0x30 && originalOffset <= 0x3F) {
            rgdod[i].dwOfs = isXInput ? TranslateXboxDInputOffset(originalOffset) : TranslateStadiaButtonOffset(originalOffset);
        }

        // 2. POV Hat to Engine Enum Translation
        else if (originalOffset == 0x20 /* DIJOFS_POV(0) */) {
            int povValue = (int)rgdod[i].dwData;

            constexpr DWORD FABLE_DPAD_UP = 0x3D;
            constexpr DWORD FABLE_DPAD_DOWN = 0x3E;
            constexpr DWORD FABLE_DPAD_LEFT = 0x3F;
            constexpr DWORD FABLE_DPAD_RIGHT = 0x40;

            if (LOWORD(povValue) == 0xFFFF) {
                if (g_LastDpadButtonOffset != 0) {
                    rgdod[i].dwOfs = g_LastDpadButtonOffset;
                    rgdod[i].dwData = 0x00;
                    g_LastDpadButtonOffset = 0;
                }
                else {
                    rgdod[i].dwOfs = 0;
                    rgdod[i].dwData = 0;
                }
            }
            else {
                DWORD targetOffset = 0;

                if (povValue >= 31500 || povValue <= 4500)  targetOffset = FABLE_DPAD_UP;
                else if (povValue >= 4500 && povValue <= 13500) targetOffset = FABLE_DPAD_RIGHT;
                else if (povValue >= 13500 && povValue <= 22500) targetOffset = FABLE_DPAD_DOWN;
                else if (povValue >= 22500 && povValue <= 31500) targetOffset = FABLE_DPAD_LEFT;

                if (targetOffset != 0) {
                    rgdod[i].dwOfs = targetOffset;
                    rgdod[i].dwData = 0x80;
                    g_LastDpadButtonOffset = targetOffset;
                }
            }
        }
    }
    return hr;
}


typedef HRESULT(WINAPI* tDirectInput8Create)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);
tDirectInput8Create oDirectInput8Create = nullptr;

typedef HRESULT(WINAPI* tCreateDevice)(IDirectInput8W*, REFGUID, LPDIRECTINPUTDEVICE8W*, LPUNKNOWN);
tCreateDevice oCreateDevice = nullptr;

HRESULT WINAPI HookedCreateDevice(IDirectInput8W* _this, REFGUID rguid, LPDIRECTINPUTDEVICE8W* lplpDirectInputDevice, LPUNKNOWN punkOuter) {
    HRESULT hr = oCreateDevice(_this, rguid, lplpDirectInputDevice, punkOuter);
    if (SUCCEEDED(hr) && lplpDirectInputDevice != nullptr && *lplpDirectInputDevice != nullptr) {

        if (rguid == GUID_SysMouse) g_pMouse = *lplpDirectInputDevice;
        else if (rguid == GUID_SysKeyboard) g_pKeyboard = *lplpDirectInputDevice;

        void** vTable = *(void***)(*lplpDirectInputDevice);

        // Install Capabilities Hooks to bypass 10-button limit
        if (oGetCapabilities == nullptr) {
            MH_CreateHook(vTable[3], &HookedGetCapabilities, (LPVOID*)&oGetCapabilities);
            MH_EnableHook(vTable[3]);
        }
        if (oEnumObjects == nullptr) {
            MH_CreateHook(vTable[4], &HookedEnumObjects, (LPVOID*)&oEnumObjects);
            MH_EnableHook(vTable[4]);
        }

        // Install Data Hooks
        if (oGetDeviceState == nullptr) {
            MH_CreateHook(vTable[9], &HookedGetDeviceState, (LPVOID*)&oGetDeviceState);
            MH_EnableHook(vTable[9]);
        }
        if (oGetDeviceData == nullptr) {
            MH_CreateHook(vTable[10], &HookedGetDeviceData, (LPVOID*)&oGetDeviceData);
            MH_EnableHook(vTable[10]);
        }
    }
    return hr;
}

HRESULT WINAPI HookedDirectInput8Create(HINSTANCE hinst, DWORD dwVersion, REFIID riidltf, LPVOID* ppvOut, LPUNKNOWN punkOuter) {
    HRESULT hr = oDirectInput8Create(hinst, dwVersion, riidltf, ppvOut, punkOuter);
    if (SUCCEEDED(hr) && ppvOut != nullptr && *ppvOut != nullptr) {
        void** vTable = *(void***)(*ppvOut);
        if (oCreateDevice == nullptr) {
            MH_CreateHook(vTable[3], &HookedCreateDevice, (LPVOID*)&oCreateDevice);
            MH_EnableHook(vTable[3]);
        }
    }
    return hr;
}

// =================================================================================
// 4. DEPLOYMENT & MAIN THREAD
// =================================================================================
void InjectGUIHotswapHooks() {
    if (MH_CreateHook((LPVOID)Retail::ADDR_CPLAYERGUI_UPDATE, &Hooked_CPlayerGui_Update, (LPVOID*)&o_CPlayerGui_Update) == MH_OK) {
        MH_EnableHook((LPVOID)Retail::ADDR_CPLAYERGUI_UPDATE);
    }
    if (MH_CreateHook((LPVOID)Retail::ADDR_CCHARSTRING_CONSTRUCT, &Hooked_CCharString_Construct, (LPVOID*)&o_CCharString_Construct) == MH_OK) {
        MH_EnableHook((LPVOID)Retail::ADDR_CCHARSTRING_CONSTRUCT);
    }
    if (MH_CreateHook((LPVOID)Retail::ADDR_SET_CAMERA_MANAGER_DEF, &Hooked_SetCameraManagerDef, (LPVOID*)&o_SetCameraManagerDef) == MH_OK) {
        MH_EnableHook((LPVOID)Retail::ADDR_SET_CAMERA_MANAGER_DEF);
    }

    if (Retail::ADDR_ISTOGGLE_ZTARGET != 0x0) {
        if (MH_CreateHook((LPVOID)Retail::ADDR_ISTOGGLE_ZTARGET, &Hooked_IsToggleZTarget, (LPVOID*)&o_IsToggleZTarget) == MH_OK) {
            MH_EnableHook((LPVOID)Retail::ADDR_ISTOGGLE_ZTARGET);
        }
    }
    if (Retail::ADDR_ISTOGGLE_SNEAK != 0x0) {
        if (MH_CreateHook((LPVOID)Retail::ADDR_ISTOGGLE_SNEAK, &Hooked_IsToggleSneak, (LPVOID*)&o_IsToggleSneak) == MH_OK) {
            MH_EnableHook((LPVOID)Retail::ADDR_ISTOGGLE_SNEAK);
        }
    }
    if (Retail::ADDR_ISTOGGLE_SPELLS != 0x0) {
        if (MH_CreateHook((LPVOID)Retail::ADDR_ISTOGGLE_SPELLS, &Hooked_IsToggleSpells, (LPVOID*)&o_IsToggleSpells) == MH_OK) {
            MH_EnableHook((LPVOID)Retail::ADDR_ISTOGGLE_SPELLS);
        }
    }
}

static DWORD WINAPI MainThread(LPVOID) {
    g_Log.open("EgoCore_Hook.log", std::ios::out | std::ios::trunc);
    Log("EgoCore Engine Initializing...");

    if (MH_Initialize() == MH_OK) {
        InjectGUIHotswapHooks();

        HMODULE hDI8 = GetModuleHandleA("dinput8.dll");
        if (!hDI8) hDI8 = LoadLibraryA("dinput8.dll");
        if (hDI8) {
            void* pDirectInput8Create = GetProcAddress(hDI8, "DirectInput8Create");
            if (pDirectInput8Create) {
                MH_CreateHook(pDirectInput8Create, &HookedDirectInput8Create, (LPVOID*)&oDirectInput8Create);
                MH_EnableHook(pDirectInput8Create);
            }
        }
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, MainThread, hModule, 0, nullptr);
    }
    else if (reason == DLL_PROCESS_DETACH) {
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
        if (g_Log.is_open()) g_Log.close();
    }
    return TRUE;
}