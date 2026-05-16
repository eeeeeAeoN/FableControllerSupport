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

// --- Engine Pointers & State ---
namespace Retail {
    // Memory Offsets
    constexpr ptrdiff_t OFFSET_CWORLD_IN_GUI = 0x04;
    constexpr ptrdiff_t OFFSET_DEFMANAGER_IN_WORLD = 0x10;
    constexpr ptrdiff_t OFFSET_CAMERAMANAGER_IN_WORLD = 0x30;
    constexpr ptrdiff_t OFFSET_PDRAW_QUICK_ACCESS = 0x1D8;
    constexpr ptrdiff_t OFFSET_PDRAW_ABILITY_BUTTONS = 0x200;
    constexpr ptrdiff_t OFFSET_ILLUSTRATORS = 0x2C0;

    // Global / Static Pointers
    constexpr uintptr_t ADDR_PGUIDEF_GLOBAL = 0x013B878C;

    // Function Addresses
    constexpr uintptr_t ADDR_CPLAYERGUI_UPDATE = 0x0043A080;
    constexpr uintptr_t ADDR_CONSTRUCT_XBOX_ABILITY = 0x00651A7E;
    constexpr uintptr_t ADDR_CONSTRUCT_XBOX_QUICKACCESS = 0x00652F0A;
    constexpr uintptr_t ADDR_CCHARSTRING_CONSTRUCT = 0x0099EBF0;

    // Camera Addresses
    constexpr uintptr_t ADDR_GETDEF = 0x0043FF30;
    constexpr uintptr_t ADDR_SET_CAMERA_MANAGER_DEF = 0x0069A850;
}

// --- Hotswap Global State ---
bool g_IsControllerActive = false;
bool g_LastControllerState = false;
bool g_ShadowObjectsInitialized = false;

// UI Shadow Buffers
static char g_XboxAbilityBuffer[256];
static char g_XboxQuickAccessBuffer[256];
void* g_PCAbilityButtons = nullptr;
void* g_PCQuickAccessIcons = nullptr;

// Camera Definition Pointers
void* g_CameraDef_XboxMain = nullptr;
void* g_CameraDef_XboxCombat = nullptr;
void* g_CameraDef_PCMain = nullptr;
void* g_CameraDef_PCCombat = nullptr;
void* g_CurrentActiveCameraDef = nullptr;

// Device Trackers
IDirectInputDevice8W* g_pMouse = nullptr;
IDirectInputDevice8W* g_pKeyboard = nullptr;

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


// =================================================================================
// 1. THE DEFINITION INTERCEPTORS (GUI & CAMERA)
// =================================================================================
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

    // Create a mock CDefPointer struct on the stack to pass to the original function
    void* override_def[1] = { target_def };
    o_SetCameraManagerDef(_this, override_def);
}


// =================================================================================
// 2. THE CORE UPDATE LOOP & LIVE PUPPETEER
// =================================================================================
void __fastcall Hooked_CPlayerGui_Update(void* _this, void* edx, bool paused) {

    // Session Tracking
    static void* g_LastPlayerGuiInstance = nullptr;
    if (g_LastPlayerGuiInstance != _this) {
        Log("New CPlayerGui instance detected. Resetting UI & Camera Caches.");
        g_LastPlayerGuiInstance = _this;
        g_ShadowObjectsInitialized = false;
        g_PCAbilityButtons = nullptr;
        g_PCQuickAccessIcons = nullptr;
        g_LastControllerState = false;
    }

    if (!g_ShadowObjectsInitialized) {
        Log("Initializing Shadow Xbox UI & Fetching Camera Defs...");
        void* illustrators = (void*)((uintptr_t)_this + Retail::OFFSET_ILLUSTRATORS);

        // 1. Build Xbox UI Buffers
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

        // 2. Cache Camera Definitions
        void* pWorld = *(void**)((uintptr_t)_this + Retail::OFFSET_CWORLD_IN_GUI);
        if (pWorld) {
            void* pDefManager = *(void**)((uintptr_t)pWorld + Retail::OFFSET_DEFMANAGER_IN_WORLD);
            GetDef_t GetDef = (GetDef_t)Retail::ADDR_GETDEF;

            auto FetchDef = [&](const char* name) -> void* {
                char strBuf[64] = { 0 };
                o_CCharString_Construct(strBuf, name, -1);
                void* outDef[2] = { nullptr, nullptr };
                GetDef(pDefManager, strBuf, outDef);
                return outDef[0];
                };

            g_CameraDef_XboxMain = FetchDef("CAMERA_MANAGER_MAIN");
            g_CameraDef_XboxCombat = FetchDef("CAMERA_MANAGER_COMBAT_MAIN");
            g_CameraDef_PCMain = FetchDef("CAMERA_MANAGER_PC_MAIN");
            g_CameraDef_PCCombat = FetchDef("CAMERA_MANAGER_PC_COMBAT");

            Log("All 4 Camera Definitions successfully cached.");
        }

        g_ShadowObjectsInitialized = true;
    }

    // Input Hotswap Evaluation
    bool requestedState = g_IsControllerActive;
    if (paused) requestedState = false; // Protect save file serialization

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

        // --- 2. Swap the Camera (The Puppeteer) ---
        void* pWorld = *(void**)((uintptr_t)_this + Retail::OFFSET_CWORLD_IN_GUI);
        if (pWorld && g_CurrentActiveCameraDef) {
            void* pCameraManager = *(void**)((uintptr_t)pWorld + Retail::OFFSET_CAMERAMANAGER_IN_WORLD);
            if (pCameraManager) {
                void* targetDef = nullptr;

                if (requestedState) { // PC -> Xbox
                    if (g_CurrentActiveCameraDef == g_CameraDef_PCMain) targetDef = g_CameraDef_XboxMain;
                    else if (g_CurrentActiveCameraDef == g_CameraDef_PCCombat) targetDef = g_CameraDef_XboxCombat;
                    else targetDef = g_CurrentActiveCameraDef;
                }
                else { // Xbox -> PC
                    if (g_CurrentActiveCameraDef == g_CameraDef_XboxMain) targetDef = g_CameraDef_PCMain;
                    else if (g_CurrentActiveCameraDef == g_CameraDef_XboxCombat) targetDef = g_CameraDef_PCCombat;
                    else targetDef = g_CurrentActiveCameraDef;
                }

                if (targetDef != nullptr) {
                    Log("Input shift: Puppeteering active Camera Math update...");
                    void* mockDefPointer[1] = { targetDef };
                    o_SetCameraManagerDef(pCameraManager, mockDefPointer);
                }
            }
        }
    }

    o_CPlayerGui_Update(_this, paused);
}


// =================================================================================
// 3. THE DIRECTINPUT FILTER & TRANSLATION
// =================================================================================
typedef HRESULT(WINAPI* tGetDeviceData)(IDirectInputDevice8W* _this, DWORD cbObjectData, LPDIDEVICEOBJECTDATA rgdod, LPDWORD pdwInOut, DWORD dwFlags);
tGetDeviceData oGetDeviceData = nullptr;

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
        if (originalOffset >= 0x30 && originalOffset <= 0x3F) {
            rgdod[i].dwOfs = isXInput ? TranslateXboxDInputOffset(originalOffset) : TranslateStadiaButtonOffset(originalOffset);
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