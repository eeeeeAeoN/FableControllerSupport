#include <windows.h>
#include <dinput.h>
#include <xinput.h>
#include <initguid.h> 
#include "MinHook.h"
#include <fstream>
#include <string>
#include <mutex>
#include <stdio.h> 
#include <cmath>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

std::ofstream g_Log;
std::mutex g_LogMutex;

void Log(const std::string& message) {
    std::lock_guard<std::mutex> lock(g_LogMutex);
    if (g_Log.is_open()) {
        g_Log << "[EgoCore] " << message << std::endl;
        g_Log.flush();
    }
}

typedef DWORD(WINAPI* XInputGetState_t)(DWORD dwUserIndex, XINPUT_STATE* pState);
XInputGetState_t DynXInputGetState = nullptr;

void InitializeXInput() {
    HMODULE hXInput = LoadLibraryA("xinput1_4.dll");
    if (!hXInput) hXInput = LoadLibraryA("xinput1_3.dll");
    if (!hXInput) hXInput = LoadLibraryA("xinput9_1_0.dll");

    if (hXInput) {
        DynXInputGetState = (XInputGetState_t)GetProcAddress(hXInput, "XInputGetState");
        if (DynXInputGetState) Log("XInput dynamically loaded successfully.");
    }
}

namespace Retail {
    constexpr ptrdiff_t OFFSET_CWORLD_IN_GUI = 0x04;
    constexpr ptrdiff_t OFFSET_DEFMANAGER_IN_WORLD = 0x10;
    constexpr ptrdiff_t OFFSET_CAMERAMANAGER_IN_WORLD = 0x30;
    constexpr ptrdiff_t OFFSET_PDRAW_QUICK_ACCESS = 0x1D8;
    constexpr ptrdiff_t OFFSET_PDRAW_ABILITY_BUTTONS = 0x200;
    constexpr ptrdiff_t OFFSET_ILLUSTRATORS = 0x2C0;
    constexpr ptrdiff_t OFFSET_SHOW_SPECIAL_BUTTONS = 0x250;

    constexpr uintptr_t ADDR_PGUIDEF_GLOBAL = 0x013B878C;
    constexpr uintptr_t ADDR_CPLAYERGUI_UPDATE = 0x0043A080;
    constexpr uintptr_t ADDR_CONSTRUCT_XBOX_ABILITY = 0x00651A7E;
    constexpr uintptr_t ADDR_CONSTRUCT_XBOX_QUICKACCESS = 0x00652F0A;
    constexpr uintptr_t ADDR_CCHARSTRING_CONSTRUCT = 0x0099EBF0;

    constexpr uintptr_t ADDR_CINPUTMANAGER_UPDATE = 0x009F57A0;
    constexpr uintptr_t ADDR_GFGETTIME = 0x009E1BC0;

    constexpr uintptr_t ADDR_GETDEF_CAMERA = 0x0043FF30;
    constexpr uintptr_t ADDR_SET_CAMERA_MANAGER_DEF = 0x0069A850;
    constexpr uintptr_t ADDR_GETDEF_CONTROLS = 0x00410820;
    constexpr uintptr_t ADDR_APPLY_VALUES = 0x00407370;
    constexpr uintptr_t ADDR_USER_PROFILE_GET = 0x0040D2A0;

    constexpr uintptr_t ADDR_ISTOGGLE_ZTARGET = 0x00445B80;
    constexpr uintptr_t ADDR_ISTOGGLE_SNEAK = 0x00000000;
    constexpr uintptr_t ADDR_ISTOGGLE_SPELLS = 0x00000000;

    constexpr uintptr_t ADDR_IS_QUICK_ACCESS = 0x004460C0;
    constexpr uintptr_t ADDR_IS_CONTEXT_SENSITIVE = 0x004461D0;
    constexpr uintptr_t ADDR_DRAW_ICON = 0x00645292;

    constexpr uintptr_t ADDR_IS_EVENT_QA = 0x00446000;
    constexpr uintptr_t ADDR_IS_EVENT_CS = 0x00446110;

    constexpr uintptr_t ADDR_BETTING_EVENT_CHECK = 0x007E52EE;
    constexpr uintptr_t ADDR_TAVERN_BETTING_EVENT_CHECK = 0x005EC3C6;

    constexpr uintptr_t ADDR_NUISYSTEM_GETINSTANCE = 0x0041E5F2;
    constexpr uintptr_t ADDR_BOASTUI_PROCESSINPUT = 0x00690E70;
}

bool g_IsControllerActive = false;
bool g_LastControllerState = false;
bool g_ShadowObjectsInitialized = false;

void* g_pPlayerGui = nullptr;
float g_CrossfadeAlpha = 0.0f;
double g_LastFrameTime = 0.0;

bool g_OriginalTooltipEnabled = true;
bool g_OriginalTooltipNew = true;

static bool g_LT_State = false;
static bool g_RT_State = false;

DWORD g_ActiveXInputSlot = (DWORD)-1;

#pragma pack(push, 1)
struct EngineInputEvent {
    uint8_t  Pad0[8];
    uint8_t  JoystickButton;
    uint8_t  Pad1[3];
    float    JoystickPressure;
    uint8_t  Pad2[8];
    float    MousePosX;
    float    MousePosY;
    uint32_t DeviceType;
    uint32_t DeviceNumber;
    uint32_t EventType;
    float    CurrentTime;
    float    StartTime;
};
#pragma pack(pop)

#define DPAD_MASK_UP    0x0001
#define DPAD_MASK_DOWN  0x0002
#define DPAD_MASK_LEFT  0x0004
#define DPAD_MASK_RIGHT 0x0008

WORD g_CurrentDPadMask = 0;
WORD g_LastDPadMask = 0;
void* g_pInputManager = nullptr;
bool g_InjectedThisFrame = false;

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

char g_PCPassiveBackup[16];
char g_PCAggressiveBackup[16];

typedef void(__thiscall* CPlayerGui_Update_t)(void* _this, bool paused);
CPlayerGui_Update_t o_CPlayerGui_Update = nullptr;

typedef void(__thiscall* CInputManager_Update_t)(void* _this);
CInputManager_Update_t o_CInputManager_Update = nullptr;

typedef double(*GFGetTime_t)();
GFGetTime_t GFGetTime = (GFGetTime_t)Retail::ADDR_GFGETTIME;

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

typedef bool(__thiscall* IsEventInQueue_t)(void* _this, float item_index, int event_type, int device_number);
IsEventInQueue_t o_IsContextSensitive = nullptr;
IsEventInQueue_t o_IsQuickAccess = nullptr;

typedef void(__thiscall* DrawIcon_t)(void* _this, void* info, void* tlpos, float alpha, int primitive_index, int back_graphic, int icon_graphic, bool special);
DrawIcon_t o_DrawIcon = nullptr;

typedef char(__thiscall* IsEventQA_t)(void* _this, const void* event, int* item_index);
IsEventQA_t o_IsEventQA = nullptr;

typedef char(__thiscall* IsEventCS_t)(void* _this, const void* event, int* item_index);
IsEventCS_t o_IsEventCS = nullptr;

typedef void(__thiscall* ProcessBoastInput_t)(void* _this, const EngineInputEvent* event, void* processed_input);
ProcessBoastInput_t o_ProcessBoastInput = nullptr;

typedef void* (*NUISystem_GetInstance_t)();
NUISystem_GetInstance_t NUISystem_GetInstance = (NUISystem_GetInstance_t)Retail::ADDR_NUISYSTEM_GETINSTANCE;

typedef void(__thiscall* NUISystem_SendEvent_t)(void* _this, int eventId);

void PushEventToVector(void* pInputManager, uint8_t buttonId, uint32_t eventType, uint32_t deviceNum) {
    EngineInputEvent** _Myfirst = (EngineInputEvent**)((uintptr_t)pInputManager + 0x10);
    EngineInputEvent** _Mylast = (EngineInputEvent**)((uintptr_t)pInputManager + 0x14);
    EngineInputEvent** _Myend = (EngineInputEvent**)((uintptr_t)pInputManager + 0x18);
    int* NoStoredEvents = (int*)((uintptr_t)pInputManager + 0x1C);

    if (!_Mylast || !_Myend || *_Mylast == nullptr || *_Mylast >= *_Myend) return;

    EngineInputEvent* newEvent = *_Mylast;
    memset(newEvent, 0, sizeof(EngineInputEvent));

    float engineTime = (float)GFGetTime();
    float pressure = (eventType == 21) ? 0.0f : 1.0f;

    newEvent->JoystickButton = buttonId;
    newEvent->JoystickPressure = pressure;
    newEvent->DeviceType = 1;
    newEvent->DeviceNumber = deviceNum;
    newEvent->EventType = eventType;
    newEvent->CurrentTime = engineTime;
    newEvent->StartTime = engineTime;

    *_Mylast = (EngineInputEvent*)((uintptr_t)(*_Mylast) + sizeof(EngineInputEvent));
    (*NoStoredEvents)++;
}

void ProcessDPadInjection() {
    if (!g_pInputManager || !g_IsControllerActive) return;

    auto EvaluateAndInject = [&](WORD mask, uint8_t btnId) {
        bool isHeld = (g_CurrentDPadMask & mask) != 0;
        bool wasHeld = (g_LastDPadMask & mask) != 0;
        uint32_t eventType = 0;

        if (isHeld && !wasHeld) eventType = 19;
        else if (isHeld && wasHeld) eventType = 20;
        else if (!isHeld && wasHeld) eventType = 21;

        if (eventType != 0) {
            PushEventToVector(g_pInputManager, btnId, eventType, 0);
            PushEventToVector(g_pInputManager, btnId, eventType, 1);
            PushEventToVector(g_pInputManager, btnId, eventType, 2);
        }
        };

    EvaluateAndInject(DPAD_MASK_UP, 13);
    EvaluateAndInject(DPAD_MASK_DOWN, 14);
    EvaluateAndInject(DPAD_MASK_LEFT, 15);
    EvaluateAndInject(DPAD_MASK_RIGHT, 16);
}

void __fastcall Hooked_CInputManager_Update(void* _this, void* edx) {
    g_pInputManager = _this;
    g_LastDPadMask = g_CurrentDPadMask;
    g_InjectedThisFrame = false;

    o_CInputManager_Update(_this);

    // Allow the manual D-Pad event injector to run for ALL controllers, 
    // including XInput, since we are wiping the raw hardware buffer.
    if (!g_InjectedThisFrame) {
        ProcessDPadInjection();
        g_InjectedThisFrame = true; // Mark as injected to be perfectly safe against double-fires
    }
}

void __fastcall Hooked_ProcessBoastInput(void* _this, void* edx, const EngineInputEvent* event, void* processed_input) {
    if (g_IsControllerActive && event->DeviceType == 1 && event->EventType == 19) {
        int nuiMessage = -1;
        switch (event->JoystickButton) {
        case 13: nuiMessage = 0; break;
        case 14: nuiMessage = 1; break;
        case 15: nuiMessage = 2; break;
        case 16: nuiMessage = 3; break;
        }

        if (nuiMessage != -1) {
            void* pNUIManager = NUISystem_GetInstance();
            if (pNUIManager) {
                void** vTable = *(void***)pNUIManager;
                NUISystem_SendEvent_t SendEvent = (NUISystem_SendEvent_t)vTable[0];
                SendEvent(pNUIManager, nuiMessage);
            }
            *(bool*)processed_input = true;
            return;
        }
    }
    o_ProcessBoastInput(_this, event, processed_input);
}

char __fastcall Hooked_IsEventQA(void* _this, void* edx, const void* event, int* item_index) {
    if (g_IsControllerActive && g_pPlayerGui) {
        bool showSpecial = *(bool*)((uintptr_t)g_pPlayerGui + Retail::OFFSET_SHOW_SPECIAL_BUTTONS);
        if (!showSpecial) { return 0; }
    }
    return o_IsEventQA(_this, event, item_index);
}

char __fastcall Hooked_IsEventCS(void* _this, void* edx, const void* event, int* item_index) {
    if (g_IsControllerActive && g_pPlayerGui) {
        bool showSpecial = *(bool*)((uintptr_t)g_pPlayerGui + Retail::OFFSET_SHOW_SPECIAL_BUTTONS);
        if (showSpecial) { return 0; }
    }
    return o_IsEventCS(_this, event, item_index);
}

bool __fastcall Hooked_IsContextSensitive(void* _this, void* edx, float item_index, int event_type, int device_number) {
    if (g_IsControllerActive && g_pPlayerGui) {
        bool showSpecial = *(bool*)((uintptr_t)g_pPlayerGui + Retail::OFFSET_SHOW_SPECIAL_BUTTONS);
        int index = (int)item_index;
        if (showSpecial) { if (index >= 0 && index <= 3) return false; }
    }
    return o_IsContextSensitive(_this, item_index, event_type, device_number);
}

bool __fastcall Hooked_IsQuickAccess(void* _this, void* edx, float item_index, int event_type, int device_number) {
    if (g_IsControllerActive && g_pPlayerGui) {
        bool showSpecial = *(bool*)((uintptr_t)g_pPlayerGui + Retail::OFFSET_SHOW_SPECIAL_BUTTONS);
        int index = (int)item_index;
        if (!showSpecial) { if (index >= 0 && index <= 3) return false; }
    }
    return o_IsQuickAccess(_this, item_index, event_type, device_number);
}

void __fastcall Hooked_DrawIcon(void* _this, void* edx, void* info, void* tlpos, float alpha, int primitive_index, int back_graphic, int icon_graphic, bool special) {
    if (g_IsControllerActive && g_pPlayerGui) {
        if (special) { alpha = g_CrossfadeAlpha; }
        else { alpha = 1.0f - g_CrossfadeAlpha; }
    }
    o_DrawIcon(_this, info, tlpos, alpha, primitive_index, back_graphic, icon_graphic, special);
}

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

void __fastcall Hooked_CPlayerGui_Update(void* _this, void* edx, bool paused) {

    g_pPlayerGui = _this;

    static void* g_LastPlayerGuiInstance = nullptr;
    if (g_LastPlayerGuiInstance != _this) {
        Log("New Session Detected. Resetting Caches.");
        g_LastPlayerGuiInstance = _this;
        g_ShadowObjectsInitialized = false;
        g_PCAbilityButtons = nullptr;
        g_PCQuickAccessIcons = nullptr;
        g_CurrentActiveCameraDef = nullptr;
        g_LastControllerState = false;
        g_CrossfadeAlpha = 0.0f;
        g_ActiveXInputSlot = (DWORD)-1;
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

        void** pGlobalGuiDef = (void**)Retail::ADDR_PGUIDEF_GLOBAL;
        if (pGlobalGuiDef && *pGlobalGuiDef != nullptr) *pGlobalGuiDef = nullptr;

        void** currentAbilityPtr = (void**)((uintptr_t)_this + Retail::OFFSET_PDRAW_ABILITY_BUTTONS);
        void** currentQuickAccessPtr = (void**)((uintptr_t)_this + Retail::OFFSET_PDRAW_QUICK_ACCESS);

        DWORD oldProtect1, oldProtect2;
        if (VirtualProtect((LPVOID)Retail::ADDR_BETTING_EVENT_CHECK, 1, PAGE_EXECUTE_READWRITE, &oldProtect1) &&
            VirtualProtect((LPVOID)Retail::ADDR_TAVERN_BETTING_EVENT_CHECK, 1, PAGE_EXECUTE_READWRITE, &oldProtect2)) {

            if (requestedState) {
                *(uint8_t*)Retail::ADDR_BETTING_EVENT_CHECK = 0x11;
                *(uint8_t*)Retail::ADDR_TAVERN_BETTING_EVENT_CHECK = 0x11;
            }
            else {
                *(uint8_t*)Retail::ADDR_BETTING_EVENT_CHECK = 0x02;
                *(uint8_t*)Retail::ADDR_TAVERN_BETTING_EVENT_CHECK = 0x02;
            }
            VirtualProtect((LPVOID)Retail::ADDR_BETTING_EVENT_CHECK, 1, oldProtect1, &oldProtect1);
            VirtualProtect((LPVOID)Retail::ADDR_TAVERN_BETTING_EVENT_CHECK, 1, oldProtect2, &oldProtect2);
        }

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

        if (g_XboxControlDef && g_PCControlDef) {
            void* pProfileManager = ((GetUserProfileManager_t)Retail::ADDR_USER_PROFILE_GET)();
            if (pProfileManager) {
                void* xboxVector = (void*)((uintptr_t)g_XboxControlDef + 0x3C);
                void* passiveVector = (void*)((uintptr_t)pProfileManager + 0x54);
                void* aggressiveVector = (void*)((uintptr_t)pProfileManager + 0x64);

                bool* pTooltipEnabled = (bool*)((uintptr_t)pProfileManager + 0x19);
                bool* pTooltipNew = (bool*)((uintptr_t)pProfileManager + 0xDD);

                if (requestedState) {
                    memcpy(g_PCPassiveBackup, passiveVector, 16);
                    memcpy(g_PCAggressiveBackup, aggressiveVector, 16);
                    memcpy(passiveVector, xboxVector, 16);
                    memcpy(aggressiveVector, xboxVector, 16);

                    g_OriginalTooltipEnabled = *pTooltipEnabled;
                    g_OriginalTooltipNew = *pTooltipNew;
                    *pTooltipEnabled = false;
                    *pTooltipNew = false;

                    ((ApplyValues_t)Retail::ADDR_APPLY_VALUES)(pProfileManager);

                    memcpy(passiveVector, g_PCPassiveBackup, 16);
                    memcpy(aggressiveVector, g_PCAggressiveBackup, 16);
                }
                else {
                    *pTooltipEnabled = g_OriginalTooltipEnabled;
                    *pTooltipNew = g_OriginalTooltipNew;
                    ((ApplyValues_t)Retail::ADDR_APPLY_VALUES)(pProfileManager);
                }
            }
        }
    }

    double currentTime = GFGetTime();
    float deltaTime = (float)(currentTime - g_LastFrameTime);
    g_LastFrameTime = currentTime;

    if (deltaTime > 0.1f || deltaTime < 0.0f) deltaTime = 0.016f;

    bool showSpecial = *(bool*)((uintptr_t)_this + Retail::OFFSET_SHOW_SPECIAL_BUTTONS);

    float fadeSpeed = 9.0f;

    if (showSpecial) {
        g_CrossfadeAlpha += deltaTime * fadeSpeed;
        if (g_CrossfadeAlpha > 1.0f) g_CrossfadeAlpha = 1.0f;
    }
    else {
        g_CrossfadeAlpha -= deltaTime * fadeSpeed;
        if (g_CrossfadeAlpha < 0.0f) g_CrossfadeAlpha = 0.0f;
    }

    o_CPlayerGui_Update(_this, paused);
}

typedef HRESULT(WINAPI* tGetDeviceData)(IDirectInputDevice8W* _this, DWORD cbObjectData, LPDIDEVICEOBJECTDATA rgdod, LPDWORD pdwInOut, DWORD dwFlags);
tGetDeviceData oGetDeviceData = nullptr;

typedef HRESULT(WINAPI* tGetDeviceState)(IDirectInputDevice8W* _this, DWORD cbData, LPVOID lpvData);
tGetDeviceState oGetDeviceState = nullptr;

typedef HRESULT(WINAPI* tSetCooperativeLevel)(IDirectInputDevice8W* _this, HWND hwnd, DWORD dwFlags);
tSetCooperativeLevel oSetCooperativeLevel = nullptr;


DWORD TranslateStadiaButtonOffset(DWORD rawOffset) {
    switch (rawOffset) {
    case 0x30: return 0x33; case 0x31: return 0x34; case 0x32: return 0x30;
    case 0x33: return 0x31; case 0x34: return 0x35; case 0x35: return 0x32;
    case 0x36: return 0x38; case 0x37: return 0x39; case 0x38: return 0x3B;
    case 0x39: return 0x3A; case 0x3B: return 0x37; case 0x3C: return 0x36;
    }
    return rawOffset;
}

void TrackStadiaPOV(DWORD povValue) {
    WORD oldMask = g_CurrentDPadMask;
    g_CurrentDPadMask = 0;
    if (LOWORD(povValue) != 0xFFFF) {
        if (povValue == 0 || povValue == 31500 || povValue == 4500) g_CurrentDPadMask |= DPAD_MASK_UP;
        if (povValue == 18000 || povValue == 13500 || povValue == 22500) g_CurrentDPadMask |= DPAD_MASK_DOWN;
        if (povValue == 27000 || povValue == 22500 || povValue == 31500) g_CurrentDPadMask |= DPAD_MASK_LEFT;
        if (povValue == 9000 || povValue == 4500 || povValue == 13500) g_CurrentDPadMask |= DPAD_MASK_RIGHT;
    }
}

// --- THE ALT-TAB SAVIOR ---
HRESULT WINAPI HookedSetCooperativeLevel(IDirectInputDevice8W* _this, HWND hwnd, DWORD dwFlags) {
    DWORD newFlags = DISCL_BACKGROUND | DISCL_NONEXCLUSIVE;
    return oSetCooperativeLevel(_this, hwnd, newFlags);
}

HRESULT WINAPI HookedGetDeviceState(IDirectInputDevice8W* _this, DWORD cbData, LPVOID lpvData) {
    HRESULT hr = oGetDeviceState(_this, cbData, lpvData);
    if (FAILED(hr)) return hr;

    if (cbData == sizeof(DIJOYSTATE) || cbData == sizeof(DIJOYSTATE2)) {
        DIJOYSTATE* js = (DIJOYSTATE*)lpvData;

        XINPUT_STATE xState;
        bool isXInputConnected = false;

        if (DynXInputGetState) {
            for (DWORD i = 0; i < 4; i++) {
                if (DynXInputGetState(i, &xState) == ERROR_SUCCESS) {
                    if (xState.Gamepad.wButtons != 0 ||
                        xState.Gamepad.bLeftTrigger > 30 || xState.Gamepad.bRightTrigger > 30 ||
                        abs(xState.Gamepad.sThumbLX) > 8000 || abs(xState.Gamepad.sThumbLY) > 8000 ||
                        abs(xState.Gamepad.sThumbRX) > 8000 || abs(xState.Gamepad.sThumbRY) > 8000)
                    {
                        g_ActiveXInputSlot = i;
                    }
                    if (g_ActiveXInputSlot == i) {
                        isXInputConnected = true;
                        break;
                    }
                }
            }
        }

        if (isXInputConnected) {
            g_IsControllerActive = true;
            ZeroMemory(js->rgbButtons, sizeof(js->rgbButtons));

            if (xState.Gamepad.wButtons & XINPUT_GAMEPAD_X)              js->rgbButtons[0] = 0x80;
            if (xState.Gamepad.wButtons & XINPUT_GAMEPAD_Y)              js->rgbButtons[1] = 0x80;
            if (xState.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) js->rgbButtons[2] = 0x80;
            if (xState.Gamepad.wButtons & XINPUT_GAMEPAD_A)              js->rgbButtons[3] = 0x80;
            if (xState.Gamepad.wButtons & XINPUT_GAMEPAD_B)              js->rgbButtons[4] = 0x80;
            if (xState.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER)  js->rgbButtons[5] = 0x80;

            if (xState.Gamepad.bLeftTrigger > 30)                        js->rgbButtons[6] = 0x80;
            if (xState.Gamepad.bRightTrigger > 30)                       js->rgbButtons[7] = 0x80;

            if (xState.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB)     js->rgbButtons[8] = 0x80;
            if (xState.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB)    js->rgbButtons[9] = 0x80;
            if (xState.Gamepad.wButtons & XINPUT_GAMEPAD_START)          js->rgbButtons[10] = 0x80;
            if (xState.Gamepad.wButtons & XINPUT_GAMEPAD_BACK)           js->rgbButtons[11] = 0x80;

            WORD xpadMask = 0;
            if (xState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP)    xpadMask |= DPAD_MASK_UP;
            if (xState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN)  xpadMask |= DPAD_MASK_DOWN;
            if (xState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT)  xpadMask |= DPAD_MASK_LEFT;
            if (xState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) xpadMask |= DPAD_MASK_RIGHT;
            g_CurrentDPadMask = xpadMask;

            if (xState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP)         js->rgdwPOV[0] = 0;
            else if (xState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) js->rgdwPOV[0] = 9000;
            else if (xState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN)  js->rgdwPOV[0] = 18000;
            else if (xState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT)  js->rgdwPOV[0] = 27000;
            else                                                          js->rgdwPOV[0] = -1;

            js->lX = xState.Gamepad.sThumbLX;
            js->lY = -xState.Gamepad.sThumbLY;
            js->lZ = xState.Gamepad.sThumbRX;
            js->lRz = xState.Gamepad.sThumbRY;

            return hr;
        }

        TrackStadiaPOV(js->rgdwPOV[0]);
        for (int i = 0; i < 32; i++) { if (js->rgbButtons[i] & 0x80) g_IsControllerActive = true; }
        for (int i = 0; i < 4; i++) { if (js->rgdwPOV[i] != -1 && js->rgdwPOV[i] != 65535) g_IsControllerActive = true; }
        if (!g_IsControllerActive) return DIERR_NOTACQUIRED;
    }
    return hr;
}

HRESULT WINAPI HookedGetDeviceData(IDirectInputDevice8W* _this, DWORD cbObjectData, LPDIDEVICEOBJECTDATA rgdod, LPDWORD pdwInOut, DWORD dwFlags) {
    HRESULT hr = oGetDeviceData(_this, cbObjectData, rgdod, pdwInOut, dwFlags);
    if (FAILED(hr) || rgdod == nullptr || pdwInOut == nullptr || *pdwInOut == 0) return hr;

    DIDEVICEINSTANCEW diDevInfo;
    diDevInfo.dwSize = sizeof(DIDEVICEINSTANCEW);
    DWORD devType = 0;
    if (SUCCEEDED(_this->GetDeviceInfo(&diDevInfo))) {
        devType = diDevInfo.dwDevType & 0xFF;
    }

    if (devType == DI8DEVTYPE_MOUSE) {
        bool significantInput = false;
        for (DWORD i = 0; i < *pdwInOut; i++) {
            DWORD ofs = rgdod[i].dwOfs;
            int data = (int)rgdod[i].dwData;
            if (ofs >= 0x0C && ofs <= 0x0F && data != 0) significantInput = true;
            if ((ofs == 0x00 || ofs == 0x04) && (data > 2 || data < -2)) significantInput = true;
        }
        if (significantInput && g_IsControllerActive) {
            g_IsControllerActive = false;
            g_ActiveXInputSlot = (DWORD)-1;
        }
        return hr;
    }

    if (devType == DI8DEVTYPE_KEYBOARD) {
        bool keyPressed = false;
        for (DWORD i = 0; i < *pdwInOut; i++) {
            if ((rgdod[i].dwData & 0x80) != 0) keyPressed = true;
        }
        if (keyPressed && g_IsControllerActive) {
            g_IsControllerActive = false;
            g_ActiveXInputSlot = (DWORD)-1;
        }
        return hr;
    }

    XINPUT_STATE xState;
    bool isXInputConnected = false;

    if (DynXInputGetState) {
        for (DWORD i = 0; i < 4; i++) {
            if (DynXInputGetState(i, &xState) == ERROR_SUCCESS) {
                if (xState.Gamepad.wButtons != 0 ||
                    xState.Gamepad.bLeftTrigger > 30 || xState.Gamepad.bRightTrigger > 30 ||
                    abs(xState.Gamepad.sThumbLX) > 8000 || abs(xState.Gamepad.sThumbLY) > 8000 ||
                    abs(xState.Gamepad.sThumbRX) > 8000 || abs(xState.Gamepad.sThumbRY) > 8000)
                {
                    g_ActiveXInputSlot = i;
                }
                if (g_ActiveXInputSlot == i) {
                    isXInputConnected = true;
                    break;
                }
            }
        }
    }

    if (!g_IsControllerActive) g_IsControllerActive = true;

    for (DWORD i = 0; i < *pdwInOut; i++) {
        DWORD originalOffset = rgdod[i].dwOfs;
        DWORD originalData = rgdod[i].dwData;

        if (isXInputConnected) {
            if (originalOffset == 0x0C) {
                rgdod[i].dwOfs = 0x08;
            }
            else if (originalOffset == 0x10) {
                rgdod[i].dwOfs = 0x14;
                rgdod[i].dwData = originalData;
            }
            else if (originalOffset == 0x08) {
                bool currentLT = (xState.Gamepad.bLeftTrigger > 30);
                bool currentRT = (xState.Gamepad.bRightTrigger > 30);

                if (currentLT != g_LT_State) {
                    g_LT_State = currentLT;
                    rgdod[i].dwOfs = 0x36;
                    rgdod[i].dwData = currentLT ? 0x80 : 0x00;
                }
                else if (currentRT != g_RT_State) {
                    g_RT_State = currentRT;
                    rgdod[i].dwOfs = 0x37;
                    rgdod[i].dwData = currentRT ? 0x80 : 0x00;
                }
                else {
                    if (!currentLT && !currentRT && (g_LT_State || g_RT_State)) {
                        rgdod[i].dwOfs = g_RT_State ? 0x37 : 0x36;
                        rgdod[i].dwData = 0x00;
                        g_LT_State = false;
                        g_RT_State = false;
                    }
                    else {
                        rgdod[i].dwOfs = 0xFFFFFFFF;
                    }
                }
            }
            else if (originalOffset >= 0x30 && originalOffset <= 0x3F) {
                DWORD targetedOffset = originalOffset;
                switch (originalOffset) {
                case 0x30: targetedOffset = 0x33; break;
                case 0x31: targetedOffset = 0x34; break;
                case 0x32: targetedOffset = 0x30; break;
                case 0x33: targetedOffset = 0x31; break;
                case 0x34: targetedOffset = 0x35; break;
                case 0x35: targetedOffset = 0x32; break;
                case 0x36: targetedOffset = 0x3B; break;
                case 0x37: targetedOffset = 0x3A; break;
                case 0x38: targetedOffset = 0x38; break;
                case 0x39: targetedOffset = 0x39; break;
                }
                rgdod[i].dwOfs = targetedOffset;
            }
            else if (originalOffset == DIJOFS_POV(0)) {
                TrackStadiaPOV(originalData);
            }
        }
        else {
            if (originalOffset >= 0x30 && originalOffset <= 0x3F) {
                rgdod[i].dwOfs = TranslateStadiaButtonOffset(originalOffset);
            }
            else if (originalOffset == DIJOFS_POV(0)) {
                TrackStadiaPOV(originalData);
            }
        }
    }
    return hr;
}

void HookDInputVTable() {
    HMODULE hDI8 = GetModuleHandleA("dinput8.dll");
    if (!hDI8) hDI8 = LoadLibraryA("dinput8.dll");
    if (!hDI8) return;

    typedef HRESULT(WINAPI* tDirectInput8Create)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);
    tDirectInput8Create pDirectInput8Create = (tDirectInput8Create)GetProcAddress(hDI8, "DirectInput8Create");
    if (!pDirectInput8Create) return;

    IDirectInput8W* pDI = nullptr;
    if (FAILED(pDirectInput8Create(GetModuleHandle(nullptr), 0x0800, IID_IDirectInput8W, (LPVOID*)&pDI, nullptr))) return;

    IDirectInputDevice8W* pDevice = nullptr;

    if (SUCCEEDED(pDI->CreateDevice(GUID_SysMouse, &pDevice, nullptr))) {
        void** vTable = *(void***)pDevice;

        if (MH_CreateHook(vTable[9], &HookedGetDeviceState, (LPVOID*)&oGetDeviceState) == MH_OK) {
            MH_EnableHook(vTable[9]);
            Log("Successfully hooked IDirectInputDevice8::GetDeviceState");
        }
        if (MH_CreateHook(vTable[10], &HookedGetDeviceData, (LPVOID*)&oGetDeviceData) == MH_OK) {
            MH_EnableHook(vTable[10]);
            Log("Successfully hooked IDirectInputDevice8::GetDeviceData");
        }
        if (MH_CreateHook(vTable[13], &HookedSetCooperativeLevel, (LPVOID*)&oSetCooperativeLevel) == MH_OK) {
            MH_EnableHook(vTable[13]);
            Log("Successfully hooked IDirectInputDevice8::SetCooperativeLevel (Alt-Tab Fix)");
        }
        pDevice->Release();
    }
    else {
        Log("CRITICAL: Failed to create dummy DirectInput device. Hooks aborted.");
    }
    pDI->Release();
}

void InjectGUIHotswapHooks() {
    if (MH_CreateHook((LPVOID)Retail::ADDR_CINPUTMANAGER_UPDATE, &Hooked_CInputManager_Update, (LPVOID*)&o_CInputManager_Update) == MH_OK) {
        MH_EnableHook((LPVOID)Retail::ADDR_CINPUTMANAGER_UPDATE);
    }
    if (MH_CreateHook((LPVOID)Retail::ADDR_CPLAYERGUI_UPDATE, &Hooked_CPlayerGui_Update, (LPVOID*)&o_CPlayerGui_Update) == MH_OK) {
        MH_EnableHook((LPVOID)Retail::ADDR_CPLAYERGUI_UPDATE);
    }
    if (MH_CreateHook((LPVOID)Retail::ADDR_CCHARSTRING_CONSTRUCT, &Hooked_CCharString_Construct, (LPVOID*)&o_CCharString_Construct) == MH_OK) {
        MH_EnableHook((LPVOID)Retail::ADDR_CCHARSTRING_CONSTRUCT);
    }
    if (MH_CreateHook((LPVOID)Retail::ADDR_SET_CAMERA_MANAGER_DEF, &Hooked_SetCameraManagerDef, (LPVOID*)&o_SetCameraManagerDef) == MH_OK) {
        MH_EnableHook((LPVOID)Retail::ADDR_SET_CAMERA_MANAGER_DEF);
    }
    if (MH_CreateHook((LPVOID)Retail::ADDR_IS_QUICK_ACCESS, &Hooked_IsQuickAccess, (LPVOID*)&o_IsQuickAccess) == MH_OK) {
        MH_EnableHook((LPVOID)Retail::ADDR_IS_QUICK_ACCESS);
    }
    if (MH_CreateHook((LPVOID)Retail::ADDR_IS_CONTEXT_SENSITIVE, &Hooked_IsContextSensitive, (LPVOID*)&o_IsContextSensitive) == MH_OK) {
        MH_EnableHook((LPVOID)Retail::ADDR_IS_CONTEXT_SENSITIVE);
    }
    if (MH_CreateHook((LPVOID)Retail::ADDR_IS_EVENT_QA, &Hooked_IsEventQA, (LPVOID*)&o_IsEventQA) == MH_OK) {
        MH_EnableHook((LPVOID)Retail::ADDR_IS_EVENT_QA);
    }
    if (MH_CreateHook((LPVOID)Retail::ADDR_IS_EVENT_CS, &Hooked_IsEventCS, (LPVOID*)&o_IsEventCS) == MH_OK) {
        MH_EnableHook((LPVOID)Retail::ADDR_IS_EVENT_CS);
    }
    if (MH_CreateHook((LPVOID)Retail::ADDR_DRAW_ICON, &Hooked_DrawIcon, (LPVOID*)&o_DrawIcon) == MH_OK) {
        MH_EnableHook((LPVOID)Retail::ADDR_DRAW_ICON);
    }

    if (Retail::ADDR_BOASTUI_PROCESSINPUT != 0x0) {
        if (MH_CreateHook((LPVOID)Retail::ADDR_BOASTUI_PROCESSINPUT, &Hooked_ProcessBoastInput, (LPVOID*)&o_ProcessBoastInput) == MH_OK) {
            MH_EnableHook((LPVOID)Retail::ADDR_BOASTUI_PROCESSINPUT);
            Log("Boast UI Input Hook Deployed.");
        }
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
        InitializeXInput();
        InjectGUIHotswapHooks();
        HookDInputVTable();
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