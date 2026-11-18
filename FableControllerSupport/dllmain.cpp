#include <windows.h>
#include <intrin.h>
#include <dinput.h>
#include <xinput.h>
#include <initguid.h> 
#include "MinHook.h"
#include <fstream>
#include <string>
#include <mutex>
#include <stdio.h> 
#include <cmath>
#include <unordered_map>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

constexpr bool ENABLE_LOGGING = false;

std::ofstream g_Log;
std::mutex g_LogMutex;

static void Log(const std::string& message) {
    if (!ENABLE_LOGGING) return;
    std::lock_guard<std::mutex> lock(g_LogMutex);
    if (g_Log.is_open()) {
        g_Log << "[EgoCore] " << message << std::endl;
        g_Log.flush();
    }
}

static void LogFmt(const char* fmt, ...) {
    if (!ENABLE_LOGGING) return;
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    Log(buf);
}

typedef DWORD(WINAPI* XInputGetState_t)(DWORD dwUserIndex, XINPUT_STATE* pState);
XInputGetState_t DynXInputGetState = nullptr;

void InitializeXInput() {
    HMODULE hXInput = LoadLibraryA("xinput1_4.dll");
    if (!hXInput) hXInput = LoadLibraryA("xinput1_3.dll");
    if (!hXInput) hXInput = LoadLibraryA("xinput9_1_0.dll");

    if (hXInput) {
        DynXInputGetState = (XInputGetState_t)GetProcAddress(hXInput, "XInputGetState");
        //if (DynXInputGetState) Log("XInput dynamically loaded successfully.");
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
    constexpr uintptr_t ADDR_INPUT_EVENT_VEC_INSERT_ONE = 0x009F6AC0;
    constexpr uintptr_t ADDR_GFPLAY_FULLSCREEN_XBOX_MOVIE = 0x006286F0;
    constexpr uintptr_t ADDR_GFGETTIME = 0x009E1BC0;

    constexpr uintptr_t ADDR_GETDEF_CAMERA = 0x0043FF30;
    constexpr uintptr_t ADDR_SET_CAMERA_MANAGER_DEF = 0x0069A850;
    constexpr uintptr_t ADDR_GETDEF_CONTROLS = 0x00410820;
    constexpr uintptr_t ADDR_APPLY_VALUES = 0x00407370;
    constexpr uintptr_t ADDR_USER_PROFILE_GET = 0x0040D2A0;

    constexpr ptrdiff_t OFFSET_PDRAW_HEALTH        = 0x140;
    constexpr ptrdiff_t OFFSET_PDRAW_STAMINA       = 0x148;
    constexpr ptrdiff_t OFFSET_PDRAW_MELEE_EFFECTS = 0x220;
    constexpr uintptr_t ADDR_CLEAR_HEALTH_EMITTERS      = 0x00644644;
    constexpr uintptr_t ADDR_CLEAR_STAMINA_EMITTERS     = 0x0064464C;
    constexpr uintptr_t ADDR_CLEAR_ABILITY_EMITTERS     = 0x0064528A;
    constexpr uintptr_t ADDR_CLEAR_QUICKACCESS_EMITTERS = 0x00645605;
    constexpr uintptr_t ADDR_CLEAR_MELEE_EMITTERS       = 0x00645821;

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

    // Frontend Retail Addresses
    constexpr uintptr_t ADDR_CNEWFRONTEND_INPUT = 0x0042E3EE;
    constexpr uintptr_t ADDR_CFRONTLIST_PROCESSEVENT_VFT_SLOT = 0x01249200;
    constexpr uintptr_t ADDR_CFRONTLIST_SCROLLUP = 0x0054C4C0;
    constexpr uintptr_t ADDR_CFRONTLIST_SCROLLDOWN = 0x0054C810;
    constexpr uintptr_t ADDR_CFRONTBUTTON_PROCESSEVENT = 0x0054DBC0;
    constexpr uintptr_t ADDR_CFRONTSCREEN_PROCESSEVENT = 0x0054E280;
    constexpr uintptr_t ADDR_CSCROLLABLELIST_PROCESSEVENT = 0x00555CF0;
    constexpr uintptr_t ADDR_CSCROLLABLELIST_ONACTIVATE = 0x00556200;
    constexpr uintptr_t ADDR_CSCROLLABLELIST_ONDEACTIVATE = 0x005562A0;
    constexpr uintptr_t ADDR_CFRONTLIST_UPDATE = 0x0054C430;
    constexpr uintptr_t ADDR_CFRONTLIST_CHANGESTATE = 0x0054CBF0;
    constexpr uintptr_t ADDR_CFRONTENDMANAGER_GETINSTANCE = 0x00595582;
    constexpr uintptr_t ADDR_CFRONTENDMANAGER_GOTOPREVIOUSSCREEN = 0x00597BF2;
    constexpr uintptr_t ADDR_SCREENSTACK_SIZE = 0x0059A8F3;
    constexpr uintptr_t ADDR_SCREENSTACK_TOP = 0x0059B039;
    constexpr uintptr_t ADDR_CIME_GET = 0x00404A80;
    constexpr uintptr_t ADDR_CIME_ISACTIVE = 0x00404C50;
    constexpr uintptr_t ADDR_CMOUSECURSOR_DRAW_VFT_SLOT = 0x0124C234;
    constexpr uintptr_t ADDR_CMOUSECURSOR_DRAW_ORIG = 0x0041AFA0;
    constexpr uintptr_t ADDR_CSPRITE_DRAWWITHRM = 0x0041A980;
    constexpr uintptr_t ADDR_CSLIDER_CHANGESTATE = 0x0054A990;
    constexpr uintptr_t ADDR_CSLIDER_PROCESSEVENT = 0x0054AC90;
    constexpr uintptr_t ADDR_CTEXTSLIDER_CHANGESTATE = 0x00548F40;
    constexpr uintptr_t ADDR_CTEXTSLIDER_PROCESSEVENT = 0x00549440;
    constexpr uintptr_t ADDR_PMAINGAMECOMPONENT = 0x013B86A0;
    constexpr uintptr_t ADDR_CPROCESSEDINPUT_SETPERFORMED = 0x00A0D390;

    // In-Game UI Controller Support Addresses
    constexpr uintptr_t ADDR_CPLAYERGUI_DTOR = 0x0043A570;
    constexpr uintptr_t ADDR_GET_NUM_ABILITY_BUTTONS = 0x00445940;
    constexpr uintptr_t ADDR_TRANSFERVECTOR_CCHARSTRING = 0x0049B8D0;
    constexpr uintptr_t ADDR_TC_ABILITY_SERIALISE = 0x005F6E40;
    constexpr uintptr_t ADDR_CTCINVENTORY_SERIALISE = 0x00591A66;
    constexpr uintptr_t ADDR_STDVECTOR_LONG_RESIZE = 0x004428F0;
    constexpr uintptr_t ADDR_CPERSISTCONTEXT_ISSAVING = 0x004AEF50;
    constexpr uintptr_t ADDR_CMAINGAMECOMPONENT_SHUTDOWN = 0x004175E5;
    constexpr uintptr_t ADDR_CINPUTPROCESS_INVENTORY_PROCESSINPUT = 0x00689DF0;
    constexpr uintptr_t ADDR_CINPUTPROCESS_QUESTCOMPLETION_PROCESSINPUT = 0x00691130;
    constexpr uintptr_t ADDR_CTCINVENTORY_ISSAVEMENURECEIVINGINPUT = 0x0058376C;
    constexpr uintptr_t ADDR_CINPUTPROCESS_IS_EVENT_GAME_ACTION = 0x00687CF0;
    constexpr uintptr_t ADDR_CINPUTPROCESS_IS_BTN_PRESSED = 0x00687A90;
    constexpr uintptr_t ADDR_CINPUTPROCESS_IS_BTN_RELEASED = 0x00687B30;
    constexpr uintptr_t ADDR_CINPUTPROCESS_IS_BTN_HELD = 0x00687BD0;
    constexpr uintptr_t ADDR_CPLAYERMANAGER_GETMAINPLAYER = 0x00449970;
    constexpr uintptr_t ADDR_CPLAYER_GETPCONTROLLEDCREATURE = 0x00487DC0;
    constexpr uintptr_t ADDR_CVECTORMAP_LOWERBOUND = 0x0040F020;
    constexpr uintptr_t ADDR_CINPUTEVENT_GET_JOYSTICK_POS = 0x00A043A0;
    constexpr uintptr_t ADDR_STDLIST_CREATE_SENTINEL = 0x0042ABAF;
    constexpr uintptr_t ADDR_STDLIST_DESTROY = 0x0042ABCA;
    constexpr uintptr_t ADDR_SET_CONTROL_SCHEME = 0x00446EF0;
    constexpr uintptr_t ADDR_PACTIVE_REDEFINER = 0x013B8AC4;
    constexpr uintptr_t ADDR_PACTIVE_KEY_REDEFINER = 0x013B8AC8;
}

bool g_IsControllerActive = false;
bool g_HideMouseCursor = false;
bool g_LastControllerState = false;
bool g_ShadowObjectsInitialized = false;
bool g_UseXboxGuiDef = false; // true only while Xbox draw objects are installed (or being constructed)

void* g_pPlayerGui = nullptr;
void* g_LastPlayerGuiInstance = nullptr;
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

typedef void(__thiscall* SetControlScheme_t)(void* _this, uint32_t scheme);
static SetControlScheme_t o_SetControlScheme = nullptr;
static bool g_ApplyingPCCleanup = false;

typedef void(__thiscall* CPlayerGui_Update_t)(void* _this, bool paused);
CPlayerGui_Update_t o_CPlayerGui_Update = nullptr;

typedef int(__thiscall* CPlayerGui_Dtor_t)(void* _this);
CPlayerGui_Dtor_t o_CPlayerGui_Dtor = nullptr;

typedef int(__thiscall* CMainGameComponent_Shutdown_t)(void* _this);
CMainGameComponent_Shutdown_t o_CMainGameComponent_Shutdown = nullptr;

typedef void(__thiscall* CInputManager_Update_t)(void* _this);
CInputManager_Update_t o_CInputManager_Update = nullptr;

typedef bool(__fastcall* GFPlayFullScreenXBoxMovie_t)(
    const void* filename, uint32_t flags, uint32_t a3, uint32_t a4, uint32_t a5,
    const void* dataBank, void* engine, uint32_t a8, uint32_t a9, void* a10, void* a11, void* a12);
GFPlayFullScreenXBoxMovie_t o_GFPlayFullScreenXBoxMovie = nullptr;

typedef void* (__thiscall* CInputEvent_VecInsertOne_t)(
    void* vec, void* where, const EngineInputEvent* val, void* dummyIter, void* count, char dummy);
CInputEvent_VecInsertOne_t EngineVecInsertOne = (CInputEvent_VecInsertOne_t)Retail::ADDR_INPUT_EVENT_VEC_INSERT_ONE;

bool g_IsInFullscreenMovie = false;
WORD g_MoviePrevXInputButtons[4] = {};

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

static void SetCursorHidden(bool hide, const char* reason) {
    if (g_HideMouseCursor != hide) {
        g_HideMouseCursor = hide;
        LogFmt("CursorHide -> %d (%s)", hide ? 1 : 0, reason);
    }
}

static bool g_DPadFromXInput = false;

static WORD ExtractXInputDPadMask(WORD wButtons) {
    WORD mask = 0;
    if (wButtons & XINPUT_GAMEPAD_DPAD_UP)    mask |= DPAD_MASK_UP;
    if (wButtons & XINPUT_GAMEPAD_DPAD_DOWN)  mask |= DPAD_MASK_DOWN;
    if (wButtons & XINPUT_GAMEPAD_DPAD_LEFT)  mask |= DPAD_MASK_LEFT;
    if (wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) mask |= DPAD_MASK_RIGHT;
    return mask;
}

static bool IsXInputActive(const XINPUT_STATE& xState) {
    return (xState.Gamepad.wButtons != 0 ||
        xState.Gamepad.bLeftTrigger > 30 || xState.Gamepad.bRightTrigger > 30 ||
        abs(xState.Gamepad.sThumbLX) > 8000 || abs(xState.Gamepad.sThumbLY) > 8000 ||
        abs(xState.Gamepad.sThumbRX) > 8000 || abs(xState.Gamepad.sThumbRY) > 8000);
}

static bool PollXInputActivity(XINPUT_STATE* outState = nullptr) {
    if (!DynXInputGetState) return false;
    if (g_ActiveXInputSlot != (DWORD)-1) {
        XINPUT_STATE xState;
        if (DynXInputGetState(g_ActiveXInputSlot, &xState) == ERROR_SUCCESS) {
            if (IsXInputActive(xState)) {
                g_IsControllerActive = true;
                SetCursorHidden(true, "XInput");
                g_CurrentDPadMask = ExtractXInputDPadMask(xState.Gamepad.wButtons);
                g_DPadFromXInput = (g_CurrentDPadMask != 0);
            } else if (g_DPadFromXInput) {
                g_CurrentDPadMask = 0;
                g_DPadFromXInput = false;
            }
            if (outState) *outState = xState;
            return true;
        }
        g_ActiveXInputSlot = (DWORD)-1;
        if (g_DPadFromXInput) {
            g_CurrentDPadMask = 0;
            g_DPadFromXInput = false;
        }
    }

    XINPUT_STATE xState;
    for (DWORD i = 0; i < 4; i++) {
        if (DynXInputGetState(i, &xState) == ERROR_SUCCESS && IsXInputActive(xState)) {
            g_ActiveXInputSlot = i;
            g_IsControllerActive = true;
            SetCursorHidden(true, "XInput");
            g_CurrentDPadMask = ExtractXInputDPadMask(xState.Gamepad.wButtons);
            g_DPadFromXInput = (g_CurrentDPadMask != 0);
            if (outState) *outState = xState;
            return true;
        }
    }
    return false;
}

static void FE_SetCancelChanges(int cancel) {
    if (void* uiMgr = NUISystem_GetInstance()) {
        typedef void(__thiscall* SetChange_t)(void*, int);
        (*(SetChange_t*)(*(uintptr_t*)uiMgr + 128))(uiMgr, cancel);
    }
}

// Frontend Hook Typedefs & Helpers
struct GameCharString {
    void* PStringData;
};

typedef void(__thiscall* CNewFrontend_Input_t)(void* _this);
CNewFrontend_Input_t o_CNewFrontend_Input = nullptr;

typedef void(__thiscall* FrontEndList_Scroll_t)(void* _this);
static const FrontEndList_Scroll_t FrontEndList_ScrollUp = (FrontEndList_Scroll_t)Retail::ADDR_CFRONTLIST_SCROLLUP;
static const FrontEndList_Scroll_t FrontEndList_ScrollDown = (FrontEndList_Scroll_t)Retail::ADDR_CFRONTLIST_SCROLLDOWN;

typedef void(__thiscall* FrontEndButton_ProcessEvent_t)(void* pObserver, int eventId);
FrontEndButton_ProcessEvent_t o_CFrontEndButton_ProcessEvent = nullptr;

typedef void(__thiscall* FrontEndScreen_ProcessEvent_t)(void* pObserver, int eventId);
FrontEndScreen_ProcessEvent_t o_CFrontEndScreen_ProcessEvent = nullptr;

typedef void(__thiscall* ScrollableList_ProcessEvent_t)(void* pObserver, int eventId);
ScrollableList_ProcessEvent_t o_CScrollableList_ProcessEvent = nullptr;

typedef int(__thiscall* ScrollableList_OnActivate_t)(void* _this);
ScrollableList_OnActivate_t o_CScrollableList_OnActivate = nullptr;

typedef int(__thiscall* ScrollableList_OnDeactivate_t)(void* _this);
ScrollableList_OnDeactivate_t o_CScrollableList_OnDeactivate = nullptr;

typedef void(__thiscall* FrontEndList_Update_t)(void* _this, float dt);
FrontEndList_Update_t o_CFrontEndList_Update = nullptr;

typedef void(__thiscall* FrontEndList_ChangeState_t)(void* _this, uint32_t state);
FrontEndList_ChangeState_t o_CFrontEndList_ChangeState = nullptr;

typedef void* (*CFrontEndManager_GetInstance_t)();
static const CFrontEndManager_GetInstance_t CFrontEndManager_GetInstance =
    (CFrontEndManager_GetInstance_t)Retail::ADDR_CFRONTENDMANAGER_GETINSTANCE;

typedef void* (__thiscall* CFrontEndManager_GotoPreviousScreen_t)(void* _this, int canQuit);
static const CFrontEndManager_GotoPreviousScreen_t CFrontEndManager_GotoPreviousScreen =
    (CFrontEndManager_GotoPreviousScreen_t)Retail::ADDR_CFRONTENDMANAGER_GOTOPREVIOUSSCREEN;

typedef void(__thiscall* Slider_ChangeState_t)(void* _this, uint32_t state);
static Slider_ChangeState_t o_CSlider_ChangeState = nullptr;
static Slider_ChangeState_t o_CTextSlider_ChangeState = nullptr;

typedef void(__thiscall* Slider_ProcessEvent_t)(void* pObserver, int eventId);
static Slider_ProcessEvent_t o_CSlider_ProcessEvent = nullptr;
static Slider_ProcessEvent_t o_CTextSlider_ProcessEvent = nullptr;

typedef int(__thiscall* ScreenStack_Size_t)(void* backEnd, void* frontDeque);
static const ScreenStack_Size_t ScreenStack_Size = (ScreenStack_Size_t)Retail::ADDR_SCREENSTACK_SIZE;

typedef void** (__thiscall* ScreenStack_Top_t)(void* frontDeque);
static const ScreenStack_Top_t ScreenStack_Top = (ScreenStack_Top_t)Retail::ADDR_SCREENSTACK_TOP;

typedef void* (*CIME_Get_t)();
static const CIME_Get_t CIME_Get = (CIME_Get_t)Retail::ADDR_CIME_GET;
typedef bool(__thiscall* CIME_IsActive_t)(void* _this);
static const CIME_IsActive_t CIME_IsActive = (CIME_IsActive_t)Retail::ADDR_CIME_ISACTIVE;

static std::unordered_map<uintptr_t, int> g_ScrollableListSelection;
static std::unordered_map<uintptr_t, int> g_ScreenYesNoSelection;
static DWORD g_LastFrontendKeyEnterTick = 0;

typedef void(__thiscall* CSprite_Draw_t)(void* _this, void* engine, void* pHandle, int layer, int* pPrimitiveIdx, void* pParent);
static CSprite_Draw_t o_CMouseCursor_Draw = (CSprite_Draw_t)Retail::ADDR_CMOUSECURSOR_DRAW_ORIG;

typedef void(__thiscall* CSprite_DrawWithRM_t)(void* _this);
static CSprite_DrawWithRM_t o_CSprite_DrawWithRM = nullptr;

static void __fastcall Hooked_CMouseCursor_Draw(void* _this, void* edx, void* engine, void* pHandle, int layer, int* pPrimitiveIdx, void* pParent) {
    if (g_HideMouseCursor && _this) {
        uint8_t* pAlpha = (uint8_t*)((uintptr_t)_this + 151);
        uint8_t* pNoRender = (uint8_t*)((uintptr_t)_this + 368);
        uint8_t savedAlpha = *pAlpha;
        *pAlpha = 0;
        *pNoRender = 1;
        o_CMouseCursor_Draw(_this, engine, pHandle, layer, pPrimitiveIdx, pParent);
        *pAlpha = savedAlpha;
        return;
    }
    o_CMouseCursor_Draw(_this, engine, pHandle, layer, pPrimitiveIdx, pParent);
}

static void __fastcall Hooked_CSprite_DrawWithRM(void* _this, void* edx) {
    if (g_HideMouseCursor) {
        if (void* uiMgr = NUISystem_GetInstance()) {
            if (_this == *(void**)((uintptr_t)uiMgr + 0xB8)) return; // CManager::PMouseCursor
        }
    }
    o_CSprite_DrawWithRM(_this);
}

static void* FE_FindChild(void* comp, const char* name) {
    if (!comp || !name) return nullptr;
    GameCharString str = { nullptr };
    auto ctor = o_CCharString_Construct ? o_CCharString_Construct : (CCharString_Construct_t)Retail::ADDR_CCHARSTRING_CONSTRUCT;
    ctor(&str, name, -1);
    typedef void* (__thiscall* FindChild_t)(void*, GameCharString);
    auto fn = *(FindChild_t*)(*(uintptr_t*)comp + 12); // CUIInterface slot 3
    return fn(comp, str);
}

static uint32_t FE_GetType(void* comp) {
    if (!comp) return 0xFFFFFFFFu;
    typedef uint32_t(__thiscall* GetType_t)(void*);
    auto fn = *(GetType_t*)(*(uintptr_t*)comp + 260); // CUIInterface slot 65
    return fn(comp);
}

static void* FE_FindFirstChildOfType(void* comp, uint32_t uiType) {
    if (!comp) return nullptr;
    typedef void* (__thiscall* FindType_t)(void*, uint32_t);
    auto fn = *(FindType_t*)(*(uintptr_t*)comp + 36); // CUIInterface slot 9
    return fn(comp, uiType);
}

static void FE_ChangeState(void* comp, uint32_t state) {
    if (!comp) return;
    typedef void(__thiscall* ChangeState_t)(void*, uint32_t);
    auto fn = *(ChangeState_t*)(*(uintptr_t*)comp + 192); // CUIInterface slot 48
    fn(comp, state);
}

static void FE_ObserveEvent(void* comp, int eventId) {
    if (!comp) return;
    void* obs = (void*)((uintptr_t)comp + 4);
    typedef void(__thiscall* Observe_t)(void*, int);
    auto fn = *(Observe_t*)(*(uintptr_t*)obs + 12); // CObserver slot 3
    fn(obs, eventId);
}

static void FE_IgnoreEvent(void* comp, int eventId) {
    if (!comp) return;
    void* obs = (void*)((uintptr_t)comp + 4);
    typedef void(__thiscall* Ignore_t)(void*, int);
    auto fn = *(Ignore_t*)(*(uintptr_t*)obs + 16); // CObserver slot 4
    fn(obs, eventId);
}

static bool FE_IsPCStyle(void* comp) {
    if (!comp) return false;
    typedef void* (__thiscall* LoadDef_t)(void*, void**);
    auto fn = *(LoadDef_t*)(*(uintptr_t*)comp + 432); // CUIInterface slot 108
    void* pRef = nullptr;
    fn(comp, &pRef);
    void* defObj = pRef;
    bool pcStyle = defObj ? (*(uint8_t*)((uintptr_t)defObj + 545) != 0) : false;
    if (pRef) {
        int* refCount = (int*)((uintptr_t)pRef + 4);
        if (--(*refCount) == 0) {
            typedef void(__thiscall* Dtor_t)(void*);
            (*(Dtor_t*)(*(uintptr_t*)pRef + 4))(pRef);
        }
    }
    return pcStyle;
}

static bool FE_HasActionListEntries(void* actionList) {
    if (!actionList) return false;
    void* head = *(void**)actionList;
    return head != nullptr && (*(void**)head != head);
}

// Skip pure IME focus actions (3018 StartTyping, 3019 CloseOtherIMEs) attached to EditBox buttons
// so controller A on New Profile triggers Apply instead of re-focusing typing.
static bool FE_HasNonImeActionListEntries(void* actionList) {
    if (!actionList) return false;
    uintptr_t sentinel = *(uintptr_t*)actionList;
    if (!sentinel) return false;
    for (uintptr_t node = *(uintptr_t*)sentinel; node && node != sentinel; node = *(uintptr_t*)node) {
        void* pAct = *(void**)(node + 8);
        int actId = pAct ? *(int*)pAct : -1;
        if (actId != 3018 && actId != 3019) {
            return true;
        }
    }
    return false;
}

static bool FE_ExecuteActionList(void* comp, void* actionList) {
    if (!comp || !FE_HasActionListEntries(actionList)) return false;
    typedef void(__thiscall* ExecActions_t)(void*, void*);
    auto fn = *(ExecActions_t*)(*(uintptr_t*)comp + 524); // CUIInterface slot 131
    fn(comp, actionList);
    return true;
}

static bool FE_ExecuteActionVector(uintptr_t vecFirst, uintptr_t vecLast) {
    if (!vecFirst || !vecLast || vecLast <= vecFirst) return false;
    void* uiMgr = NUISystem_GetInstance();
    if (!uiMgr) return false;
    typedef void(__thiscall* ManagerAction_t)(void*, void*);
    auto mgrAction = *(ManagerAction_t*)(*(uintptr_t*)uiMgr + 56); // CManager slot 14
    bool ran = false;
    for (uintptr_t p = vecFirst; p + 8 <= vecLast; p += 8) {
        if (*(void**)p != nullptr) {
            mgrAction(uiMgr, (void*)p);
            ran = true;
        }
    }
    return ran;
}

static bool FE_FrontEndButtonHasActions(void* comp) {
    if (!comp || FE_GetType(comp) != 11) return false;
    void* pUnclicked = *(void**)((uintptr_t)comp + 0x17C);
    void* pClicked   = *(void**)((uintptr_t)comp + 0x174);
    uintptr_t actFirst = *(uintptr_t*)((uintptr_t)comp + 0x19C);
    uintptr_t actLast  = *(uintptr_t*)((uintptr_t)comp + 0x1A0);
    if (FE_HasNonImeActionListEntries(pUnclicked) || FE_HasNonImeActionListEntries(pClicked)) return true;
    for (uintptr_t p = actFirst; actFirst && p + 8 <= actLast; p += 8) {
        if (*(void**)p != nullptr) return true;
    }
    return false;
}

static bool FE_ActivateFrontEndButton(void* comp) {
    if (!comp) return false;
    uint32_t ty = FE_GetType(comp);
    if (ty != 11) return false; // UI_TYPE_FRONTEND_BUTTON

    void* pUnclicked = *(void**)((uintptr_t)comp + 0x17C);
    void* pClicked   = *(void**)((uintptr_t)comp + 0x174);
    uintptr_t actFirst = *(uintptr_t*)((uintptr_t)comp + 0x19C);
    uintptr_t actLast  = *(uintptr_t*)((uintptr_t)comp + 0x1A0);

    if (FE_HasNonImeActionListEntries(pUnclicked) && FE_ExecuteActionList(comp, pUnclicked)) return true;
    if (FE_HasNonImeActionListEntries(pClicked)   && FE_ExecuteActionList(comp, pClicked))   return true;
    if (FE_ExecuteActionVector(actFirst, actLast)) return true;
    return false;
}

static bool FE_ActivateNavButton(void* comp) {
    if (!comp) return false;
    // Preserve active state across synthetic OnLeftUnclicked only for frontend buttons when StateBeforeLeftClicked is unset
    uint32_t* pPrevState = (uint32_t*)((uintptr_t)comp + 0x16C);
    if (FE_GetType(comp) == 11u && *pPrevState == 0) {
        uint32_t curState = *(uint32_t*)((uintptr_t)comp + 0x144);
        *pPrevState = (curState != 0 && curState != 6) ? curState : 1u;
    }
    typedef void(__thiscall* OnLeftUnclicked_t)(void*);
    auto fn = *(OnLeftUnclicked_t*)(*(uintptr_t*)comp + 588); // CUIInterface slot 147
    fn(comp);
    return true;
}

static void* FE_GetParent(void* comp) {
    if (!comp) return nullptr;
    typedef void* (__thiscall* GetParent_t)(void*);
    auto fn = *(GetParent_t*)(*(uintptr_t*)comp + 208); // CUIInterface slot 52
    return fn(comp);
}

static bool FE_IsOnActiveFrontendScreen(void* comp) {
    if (!comp || *(void**)Retail::ADDR_PMAINGAMECOMPONENT != nullptr) return false;
    void* root = comp;
    for (int depth = 0; depth < 16; ++depth) {
        void* p = FE_GetParent(root);
        if (!p || p == root) break;
        root = p;
    }
    void* feMgr = CFrontEndManager_GetInstance();
    if (!feMgr) return true;
    if (ScreenStack_Size((void*)((uintptr_t)feMgr + 48), (void*)((uintptr_t)feMgr + 32)) <= 0) {
        return false;
    }
    void* topScreen = *ScreenStack_Top((void*)((uintptr_t)feMgr + 32));
    return topScreen == nullptr || root == topScreen;
}

static bool FE_IsRedefiningKeys(void* screenComp) {
    if (!screenComp) return false;
    void* redefList = FE_FindChild(screenComp, "UI_FRONTEND_LIST_REDEFINE_KEYS_MENU");
    if (!redefList) return false;
    // +0x184 is non-zero only while a CKeyRedefiner child is actively waiting for a key press (sub_556810).
    return *(uint8_t*)((uintptr_t)redefList + 0x184) != 0;
}

static bool FE_IsIMEOrRedefinerBusy(void* screenComp) {
    void* ime = CIME_Get();
    if (ime && CIME_IsActive(ime)) return true;
    return FE_IsRedefiningKeys(screenComp);
}

void __fastcall Hooked_CNewFrontend_Input(void* _this, void* edx) {
    static bool s_ResetCancelOnce = false;
    if (!s_ResetCancelOnce) {
        s_ResetCancelOnce = true;
        FE_SetCancelChanges(0);
    }

    o_CNewFrontend_Input(_this);

    if (g_pInputManager) {
        EngineInputEvent* first = *(EngineInputEvent**)((uintptr_t)g_pInputManager + 0x10);
        EngineInputEvent* last  = *(EngineInputEvent**)((uintptr_t)g_pInputManager + 0x14);
        if (first && last && last >= first) {
            for (EngineInputEvent* cur = first; cur < last; ++cur) {
                if (cur->DeviceType == 0 && (cur->EventType == 1 || cur->EventType == 15) && !g_IsInFullscreenMovie) {
                    SetCursorHidden(false, "FENewInput_Kbm");
                }
                else if (cur->DeviceType == 1 && cur->EventType == 19) {
                    g_IsControllerActive = true;
                    SetCursorHidden(true, "FENewInput_JoyBtn");
                }
                else if (cur->DeviceType == 1 && cur->EventType == 17) {
                    float lx = *(float*)((uintptr_t)cur + 0);
                    float ly = *(float*)((uintptr_t)cur + 4);
                    if (fabsf(lx) > 0.25f || fabsf(ly) > 0.25f) {
                        g_IsControllerActive = true;
                        SetCursorHidden(true, "FENewInput_LStick");
                    }
                }
                else if (cur->DeviceType == 1 && cur->EventType == 18) { // Right Stick IE_JOYSTICK_POSITION2
                    float ry = *(float*)((uintptr_t)cur + 4);
                    if (fabsf(ry) > 0.35f) {
                        g_IsControllerActive = true;
                        SetCursorHidden(true, "FENewInput_RStick");
                    }
                    void* uiMgr = NUISystem_GetInstance();
                    if (uiMgr) {
                        NUISystem_SendEvent_t sendEv = *(NUISystem_SendEvent_t*)(*(uintptr_t*)uiMgr);
                        if (ry > 0.35f)       sendEv(uiMgr, 7); // UI_EVENT_RIGHT_STICK_DOWN
                        else if (ry < -0.35f) sendEv(uiMgr, 6); // UI_EVENT_RIGHT_STICK_UP
                    }
                }
            }
        }
    }
}

static bool FE_AreStatesToDoEmpty(void* comp) {
    if (!comp) return true;
    uintptr_t head = *(uintptr_t*)((uintptr_t)comp + 0x13C);
    return !head || (*(uintptr_t*)head == head);
}

static void FE_ApplyFrontEndListSelectionVisuals(void* listComp) {
    if (!listComp) return;
    uintptr_t vecFirst = *(uintptr_t*)((uintptr_t)listComp + 0x164);
    uintptr_t vecLast  = *(uintptr_t*)((uintptr_t)listComp + 0x168);
    int count = (vecFirst && vecLast > vecFirst) ? (int)((vecLast - vecFirst) >> 2) : 0;
    int sel = *(int*)((uintptr_t)listComp + 0x15C);
    for (int i = 0; i < count; ++i) {
        void* child = *(void**)(vecFirst + 4 * i);
        if (!child) continue;
        uint32_t curState = *(uint32_t*)((uintptr_t)child + 0x144);
        uint32_t reqChild = *(uint32_t*)((uintptr_t)child + 0x14C);
        if (i == sel) {
            if (curState != 3 && curState != 8 && reqChild != 3 && reqChild != 8) {
                FE_ChangeState(child, 3);
            }
        }
        else if (curState != 4 && reqChild != 4) {
            FE_ChangeState(child, 4);
        }
    }
}

void __fastcall Hooked_CFrontEndList_ChangeState(void* _this, void* edx, uint32_t state) {
    o_CFrontEndList_ChangeState(_this, state);
    if (_this && *(void**)Retail::ADDR_PMAINGAMECOMPONENT == nullptr &&
        state == 3 && FE_AreStatesToDoEmpty(_this)) {
        FE_ApplyFrontEndListSelectionVisuals(_this);
    }
}

void __fastcall Hooked_CFrontEndList_Update(void* _this, void* edx, float dt) {
    o_CFrontEndList_Update(_this, dt);
    if (_this) {
        bool inFrontend = (*(void**)Retail::ADDR_PMAINGAMECOMPONENT == nullptr);
        uint32_t targetState = *(uint32_t*)((uintptr_t)_this + 0x14C);
        if (inFrontend && (targetState == 5 || targetState == 6) && FE_AreStatesToDoEmpty(_this)) {
            typedef bool(__thiscall* IsFinished_t)(void*);
            auto isFinished = *(IsFinished_t*)(*(uintptr_t*)_this + 196);
            if (isFinished(_this)) {
                Hooked_CFrontEndList_ChangeState(_this, nullptr, targetState == 5 ? 3u : 0u);
                if (targetState == 6) {
                    FE_SetCancelChanges(0);
                }
            }
        }
        if (inFrontend && *(uint32_t*)((uintptr_t)_this + 0x14C) == 3 && FE_AreStatesToDoEmpty(_this)) {
            FE_ApplyFrontEndListSelectionVisuals(_this);
        }
    }
}

void __fastcall Hooked_CFrontEndList_ProcessEvent(void* pObserver, void* edx, int eventId) {
    if (!pObserver) return;
    uintptr_t comp = (uintptr_t)pObserver - 4;
    if (!FE_IsOnActiveFrontendScreen((void*)comp)) return;

    if (eventId != 0 && eventId != 1) return;

    uintptr_t vecFirst = *(uintptr_t*)(comp + 0x164);
    uintptr_t vecLast  = *(uintptr_t*)(comp + 0x168);
    if (!vecFirst || vecLast <= vecFirst) return;
    int count = (int)((vecLast - vecFirst) >> 2);
    if (count <= 0) return;

    int oldIdx = *(int*)(comp + 0x15C);
    if (oldIdx >= 0 && oldIdx < count) {
        void* oldChild = *(void**)(vecFirst + 4 * oldIdx);
        FE_IgnoreEvent(oldChild, 22); // UI_EVENT_DELETE
        FE_IgnoreEvent(oldChild, 4);  // UI_EVENT_SELECT
    }

    if (eventId == 0) FrontEndList_ScrollUp((void*)comp);
    else              FrontEndList_ScrollDown((void*)comp);

    int newIdx = *(int*)(comp + 0x15C);
    if (newIdx >= 0 && newIdx < count) {
        void* newChild = *(void**)(vecFirst + 4 * newIdx);
        FE_ObserveEvent(newChild, 22);
        FE_ObserveEvent(newChild, 4);
    }
}

static void HandleSliderChangeState(void* _this, uint32_t state, Slider_ChangeState_t origFn) {
    origFn(_this, state);
    // Native CSlider/CTextSlider::ChangeState leaves the component in GlobalObservers on state 0 (INACTIVE).
    if (_this && state == 0 && *(void**)Retail::ADDR_PMAINGAMECOMPONENT == nullptr) {
        if (void* uiMgr = NUISystem_GetInstance()) {
            typedef void(__thiscall* IgnoreAll_t)(void*, void*);
            (*(IgnoreAll_t*)(*(uintptr_t*)uiMgr + 20))(uiMgr, (void*)((uintptr_t)_this + 4));
        }
    }
}

static void HandleSliderProcessEvent(void* pObserver, int eventId, Slider_ProcessEvent_t origFn) {
    if (!pObserver) return;
    void* comp = (void*)((uintptr_t)pObserver - 4);
    bool inFrontend = (*(void**)Retail::ADDR_PMAINGAMECOMPONENT == nullptr);
    if (inFrontend) {
        // PC sliders adjust inline via left/right (20/21); block Xbox modal popup actions (4/5 -> 86/87).
        if (eventId == 4 || eventId == 5 || !FE_IsOnActiveFrontendScreen(comp)) {
            return;
        }
    }
    origFn(pObserver, eventId);
}

void __fastcall Hooked_CSlider_ChangeState(void* _this, void* edx, uint32_t state) {
    HandleSliderChangeState(_this, state, o_CSlider_ChangeState);
}

void __fastcall Hooked_CTextSlider_ChangeState(void* _this, void* edx, uint32_t state) {
    HandleSliderChangeState(_this, state, o_CTextSlider_ChangeState);
}

void __fastcall Hooked_CSlider_ProcessEvent(void* pObserver, void* edx, int eventId) {
    HandleSliderProcessEvent(pObserver, eventId, o_CSlider_ProcessEvent);
}

void __fastcall Hooked_CTextSlider_ProcessEvent(void* pObserver, void* edx, int eventId) {
    HandleSliderProcessEvent(pObserver, eventId, o_CTextSlider_ProcessEvent);
}

void __fastcall Hooked_CFrontEndButton_ProcessEvent(void* pObserver, void* edx, int eventId) {
    if (!pObserver) return;
    uintptr_t comp = (uintptr_t)pObserver - 4;

    if ((eventId == 4 || eventId == 22) && FE_IsOnActiveFrontendScreen((void*)comp)) { // UI_EVENT_SELECT / UI_EVENT_DELETE
        float now   = *(float*)(comp + 0x30);
        float delay = *(float*)(comp + 0x18C);
        float last  = *(float*)(comp + 0x194);
        if (now - last >= delay) {
            if (eventId == 4) {
                FE_SetCancelChanges(0);
                if (FE_ActivateFrontEndButton((void*)comp)) {
                    *(float*)(comp + 0x194) = now;
                    return;
                }
            }
            else {
                uintptr_t delFirst = *(uintptr_t*)(comp + 0x1AC);
                uintptr_t delLast  = *(uintptr_t*)(comp + 0x1B0);
                if (FE_ExecuteActionVector(delFirst, delLast)) {
                    *(float*)(comp + 0x194) = now;
                    return;
                }
            }
        }
    }

    o_CFrontEndButton_ProcessEvent(pObserver, eventId);
}

int __fastcall Hooked_CScrollableList_OnActivate(void* _this, void* edx) {
    int res = o_CScrollableList_OnActivate(_this);
    bool inFrontend = (*(void**)Retail::ADDR_PMAINGAMECOMPONENT == nullptr);
    if (inFrontend && _this && FE_GetType(_this) == 43) { // UI_TYPE_SCROLLABLE_LIST (not CRedefinerList)
        FE_ObserveEvent(_this, 0); // UI_EVENT_UP
        FE_ObserveEvent(_this, 1); // UI_EVENT_DOWN
        FE_ObserveEvent(_this, 4); // UI_EVENT_SELECT
        g_ScrollableListSelection[(uintptr_t)_this] = 0;

        uintptr_t vecFirst = *(uintptr_t*)((uintptr_t)_this + 0xB0);
        uintptr_t vecLast  = *(uintptr_t*)((uintptr_t)_this + 0xB4);
        int count = (vecFirst && vecLast > vecFirst) ? (int)((vecLast - vecFirst) / 8) : 0;
        for (int i = 0; i < count; ++i) {
            void* child = *(void**)(vecFirst + 8 * i);
            if (!child) continue;
            if (i == 0) {
                FE_ChangeState(child, 3);
            } else if (*(uint32_t*)((uintptr_t)child + 0x144) == 3u) {
                FE_ChangeState(child, 4);
            }
        }
    }
    return res;
}

int __fastcall Hooked_CScrollableList_OnDeactivate(void* _this, void* edx) {
    bool inFrontend = (*(void**)Retail::ADDR_PMAINGAMECOMPONENT == nullptr);
    if (inFrontend && _this && FE_GetType(_this) == 43) {
        FE_IgnoreEvent(_this, 0);
        FE_IgnoreEvent(_this, 1);
        FE_IgnoreEvent(_this, 4);
    }
    return o_CScrollableList_OnDeactivate(_this);
}

void __fastcall Hooked_CScrollableList_ProcessEvent(void* pObserver, void* edx, int eventId) {
    if (!pObserver) return;
    uintptr_t comp = (uintptr_t)pObserver - 4;
    bool inFrontend = (*(void**)Retail::ADDR_PMAINGAMECOMPONENT == nullptr);

    if (!inFrontend || FE_GetType((void*)comp) != 43 || !FE_IsOnActiveFrontendScreen((void*)comp)) {
        o_CScrollableList_ProcessEvent(pObserver, eventId);
        return;
    }

    uintptr_t vecFirst = *(uintptr_t*)(comp + 0xB0);
    uintptr_t vecLast  = *(uintptr_t*)(comp + 0xB4);
    int count = (vecFirst && vecLast > vecFirst) ? (int)((vecLast - vecFirst) / 8) : 0;

    if ((eventId == 0 || eventId == 1) && count > 0) {
        int& sel = g_ScrollableListSelection[comp];
        if (sel < 0 || sel >= count) sel = 0;

        int next = sel;
        if (eventId == 0) next = (sel > 0) ? (sel - 1) : (count - 1);
        else              next = (sel + 1 < count) ? (sel + 1) : 0;

        if (next != sel) {
            void* oldChild = *(void**)(vecFirst + 8 * sel);
            void* newChild = *(void**)(vecFirst + 8 * next);
            FE_ChangeState(oldChild, 4); // UI_STATE_UNSELECTED
            sel = next;

            uint32_t visible = *(uint32_t*)(comp + 0x160);
            if (visible == 0) visible = 1;
            int safety = 0;
            while ((uint32_t)sel < *(uint32_t*)(comp + 0x15C) && safety++ < count + 2) {
                o_CScrollableList_ProcessEvent(pObserver, 36); // MOUSE_WHEEL_UP
            }
            safety = 0;
            while ((uint32_t)sel >= *(uint32_t*)(comp + 0x15C) + visible && safety++ < count + 2) {
                o_CScrollableList_ProcessEvent(pObserver, 37); // MOUSE_WHEEL_DOWN
            }

            FE_ChangeState(newChild, 3); // UI_STATE_SELECTED
        }
        return;
    }

    if (eventId == 4 && count > 0 && inFrontend) { // UI_EVENT_SELECT
        int sel = g_ScrollableListSelection[comp];
        if (sel >= 0 && sel < count) {
            void* child = *(void**)(vecFirst + 8 * sel);
            FE_ActivateFrontEndButton(child);
        }
        return;
    }

    o_CScrollableList_ProcessEvent(pObserver, eventId);
}

static std::unordered_map<uintptr_t, double> g_ScreenActionTime;

void __fastcall Hooked_CFrontEndScreen_ProcessEvent(void* pObserver, void* edx, int eventId) {
    if (!pObserver) return;
    uintptr_t comp = (uintptr_t)pObserver - 4;
    if (!FE_IsOnActiveFrontendScreen((void*)comp)) {
        o_CFrontEndScreen_ProcessEvent(pObserver, eventId);
        return;
    }

    double nowSec = GFGetTime();
    if (eventId == 4 || eventId == 5 || eventId == 22 || eventId == 23) {
        auto it = g_ScreenActionTime.find(comp);
        if (it != g_ScreenActionTime.end() && (nowSec - it->second) < 0.18) {
            if (eventId == 4 || eventId == 23) {
                LogFmt("Screen_ProcessEvent(%d): comp=%p debounced (dt=%.3f)", eventId, (void*)comp, nowSec - it->second);
            }
            return;
        }
    }

    float now = *(float*)(comp + 0x30);
    void* feMgr = CFrontEndManager_GetInstance();
    void* pLeftClickAction = *(void**)(comp + 0x160);

    // Handle Enter (KB_RETURN = 28) when not typing in an EditBox or capturing a keybind
    if (eventId == 33) {
        void* uiMgr = NUISystem_GetInstance();
        uint16_t key = 0;
        if (uiMgr) {
            typedef uint16_t(__thiscall* GetKeyPressed_t)(void*);
            key = (*(GetKeyPressed_t*)(*(uintptr_t*)uiMgr + 52))(uiMgr);
        }
        if (key == 28 && !FE_IsIMEOrRedefinerBusy((void*)comp) && !FE_FindChild((void*)comp, "UI_NEW_PROFILE_EDIT_BOX")) {
            DWORD tick = GetTickCount();
            if (tick - g_LastFrontendKeyEnterTick > 180) {
                g_LastFrontendKeyEnterTick = tick;
                if (pLeftClickAction && feMgr) {
                    typedef void(__thiscall* FEMgr_Action_t)(void*, void*);
                    auto fn = *(FEMgr_Action_t*)(*(uintptr_t*)feMgr + 32); // CFrontEndManager::Action
                    fn(feMgr, (void*)(comp + 0x160));
                    g_ScreenActionTime[comp] = nowSec;
                    *(float*)(comp + 0x15C) = now;
                    return;
                }
                eventId = 4; // Treat Enter like UI_EVENT_SELECT on confirmation/list screens
            }
        }
    }

    // Check binary Yes/No prompt buttons (Quit Prompt / Delete Profile Prompt)
    void* yesBtn = FE_FindChild((void*)comp, "UI_FRONTEND_QUIT_YES");
    void* noBtn  = FE_FindChild((void*)comp, "UI_FRONTEND_QUIT_NO");
    if (!yesBtn || !noBtn) {
        yesBtn = FE_FindChild((void*)comp, "UI_FRONTEND_DELETE_PROFILE_YES");
        noBtn  = FE_FindChild((void*)comp, "UI_FRONTEND_DELETE_PROFILE_NO");
    }

    if (yesBtn && noBtn && (eventId == 2 || eventId == 3 || eventId == 4 || eventId == 5)) {
        int& sel = g_ScreenYesNoSelection[comp];
        if (eventId == 2 || eventId == 3) {
            sel = (eventId == 3) ? 1 : 0; // Left = No, Right = Yes (matches X=20 vs X=362)
            FE_ChangeState(yesBtn, sel == 1 ? 3 : 4);
            FE_ChangeState(noBtn,  sel == 0 ? 3 : 4);
            *(float*)(comp + 0x15C) = now;
            return;
        }
        if (eventId == 4) {
            FE_ActivateNavButton(sel == 1 ? yesBtn : noBtn);
            g_ScreenActionTime[comp] = nowSec;
            *(float*)(comp + 0x15C) = now;
            return;
        }
        if (eventId == 5) {
            FE_ActivateNavButton(noBtn);
            g_ScreenActionTime[comp] = nowSec;
            *(float*)(comp + 0x15C) = now;
            return;
        }
    }

    if (eventId == 4) { // UI_EVENT_SELECT
        // Press-start / single-click screen fallback if no screen transition is already queued
        if (pLeftClickAction && feMgr && *(void**)((uintptr_t)feMgr + 152) == nullptr) {
            void* startBtn = FE_FindChild((void*)comp, "UI_FRONTEND_BUTTON_INVISIBLE");
            if (!startBtn) {
                typedef void(__thiscall* FEMgr_Action_t)(void*, void*);
                auto fn = *(FEMgr_Action_t*)(*(uintptr_t*)feMgr + 32); // CFrontEndManager::Action
                fn(feMgr, (void*)(comp + 0x160));
                g_ScreenActionTime[comp] = nowSec;
                *(float*)(comp + 0x15C) = now;
                return;
            }
        }

        if (void* okBtn = FE_FindChild((void*)comp, "UI_OK")) {
            FE_SetCancelChanges(0);
            FE_ActivateNavButton(okBtn);
            g_ScreenActionTime[comp] = nowSec;
            *(float*)(comp + 0x15C) = now;
            return;
        }

        // Options / Video / Audio / New Profile Apply button (only if current list item isn't an actionable button)
        typedef void* (__thiscall* GetPAccept_t)(void*);
        void* acceptBtn = (*(GetPAccept_t*)(*(uintptr_t*)comp + 568))((void*)comp);
        if (!acceptBtn) acceptBtn = FE_FindChild((void*)comp, "UI_ACCEPT");
        if (!acceptBtn) acceptBtn = FE_FindChild((void*)comp, "UI_ACCEPT_NEW_PROFILE");
        if (acceptBtn) {
            void* feList = FE_FindFirstChildOfType((void*)comp, 12); // UI_TYPE_FRONTEND_LIST
            bool activeRowIsActionButton = false;
            void* activeRow = nullptr;
            if (feList) {
                int selIdx = *(int*)((uintptr_t)feList + 0x15C);
                uintptr_t fFirst = *(uintptr_t*)((uintptr_t)feList + 0x164);
                uintptr_t fLast  = *(uintptr_t*)((uintptr_t)feList + 0x168);
                int cnt = (fFirst && fLast > fFirst) ? (int)((fLast - fFirst) >> 2) : 0;
                if (selIdx >= 0 && selIdx < cnt) {
                    activeRow = *(void**)(fFirst + 4 * selIdx);
                    if (FE_FrontEndButtonHasActions(activeRow)) activeRowIsActionButton = true;
                }
            }
            bool redef = FE_IsRedefiningKeys((void*)comp);
            if (!activeRowIsActionButton && !redef) {
                FE_SetCancelChanges(0);
                FE_ActivateNavButton(acceptBtn);
                g_ScreenActionTime[comp] = nowSec;
                *(float*)(comp + 0x15C) = now;
                return;
            }
        }
    }
    else if (eventId == 5) { // UI_EVENT_BACK
        bool redef = FE_IsRedefiningKeys((void*)comp);
        if (feMgr && !redef) {
            FE_SetCancelChanges(1);
            CFrontEndManager_GotoPreviousScreen(feMgr, 0);
            g_ScreenActionTime[comp] = nowSec;
            *(float*)(comp + 0x15C) = now;
            return;
        }
    }
    else if (eventId == 22) { // UI_EVENT_DELETE (Y button on Profiles screen)
        if (void* delBtn = FE_FindChild((void*)comp, "UI_DELETE")) {
            FE_ActivateNavButton(delBtn);
            g_ScreenActionTime[comp] = nowSec;
            *(float*)(comp + 0x15C) = now;
            return;
        }
    }
    else if (eventId == 23) { // UI_EVENT_X (Restore Defaults on Options screens)
        const char* defaultsNames[] = { "UI_DEFAULTS_GAME", "UI_DEFAULTS_AUDIO", "UI_DEFAULTS_VIDEO", "UI_RESET" };
        for (const char* dn : defaultsNames) {
            if (void* defBtn = FE_FindChild((void*)comp, dn)) {
                FE_ActivateNavButton(defBtn);
                g_ScreenActionTime[comp] = nowSec;
                *(float*)(comp + 0x15C) = now;
                return;
            }
        }
    }

    o_CFrontEndScreen_ProcessEvent(pObserver, eventId);
}

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
            SetCursorHidden(true, "DPad");
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

void InjectKeyboardReturnToInputManager(void* pInputManager) {
    if (!pInputManager) return;

    EngineInputEvent ev;
    memset(&ev, 0, sizeof(ev));

    float engineTime = (float)GFGetTime();
    *(uint32_t*)ev.Pad0 = 28; // KB_RETURN (0x1C)
    ev.DeviceType = 0;        // INPUT_DEVICE_TYPE_KEYBOARD
    ev.DeviceNumber = 0;
    ev.EventType = 1;         // IE_KEY_PRESSED
    ev.CurrentTime = engineTime;
    ev.StartTime = engineTime;

    EngineInputEvent** _Mylast = (EngineInputEvent**)((uintptr_t)pInputManager + 0x14);
    EngineInputEvent** _Myend  = (EngineInputEvent**)((uintptr_t)pInputManager + 0x18);
    int* NoStoredEvents        = (int*)((uintptr_t)pInputManager + 0x1C);

    if (*_Mylast < *_Myend && *_Mylast != nullptr) {
        memcpy(*_Mylast, &ev, sizeof(EngineInputEvent));
        *_Mylast = (EngineInputEvent*)((uintptr_t)(*_Mylast) + sizeof(EngineInputEvent));
    }
    else {
        uint8_t dummyIter = 0;
        void* pVec = (void*)((uintptr_t)pInputManager + 0x10);
        EngineVecInsertOne(pVec, *_Mylast, &ev, &dummyIter, (void*)1, 1);
    }
    (*NoStoredEvents)++;
}

void ProcessMovieSkipInput(void* pInputManager) {
    if (!g_IsInFullscreenMovie || !pInputManager) return;

    bool shouldSkip = false;

    // 1. DInput (and DInput-backed XInput) path via CInputManager::EventStore
    EngineInputEvent* first = *(EngineInputEvent**)((uintptr_t)pInputManager + 0x10);
    EngineInputEvent* last  = *(EngineInputEvent**)((uintptr_t)pInputManager + 0x14);
    if (first && last && last >= first) {
        for (EngineInputEvent* cur = first; cur < last; ++cur) {
            if (cur->DeviceType == 1 && cur->EventType == 19) { // INPUT_DEVICE_TYPE_JOYSTICK, IE_JOYSTICK_BUTTON_PRESSED
                if (cur->JoystickButton == 4 || cur->JoystickButton == 11) { // A (Okay) or Start
                    shouldSkip = true;
                    break;
                }
            }
        }
    }

    // 2. Direct XInput rising-edge poll (covers frames where DInput buffer hasn't fired)
    if (DynXInputGetState) {
        const WORD skipMask = XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_START;
        for (DWORD i = 0; i < 4; ++i) {
            XINPUT_STATE xState;
            if (DynXInputGetState(i, &xState) == ERROR_SUCCESS) {
                WORD curButtons = xState.Gamepad.wButtons;
                WORD pressed = curButtons & ~g_MoviePrevXInputButtons[i];
                g_MoviePrevXInputButtons[i] = curButtons;
                if (pressed & skipMask) {
                    g_ActiveXInputSlot = i;
                    g_IsControllerActive = true;
                    shouldSkip = true;
                }
            }
            else {
                g_MoviePrevXInputButtons[i] = 0;
            }
        }
    }

    if (shouldSkip) {
        g_IsControllerActive = true;
        InjectKeyboardReturnToInputManager(pInputManager);
    }
}

bool __fastcall Hooked_GFPlayFullScreenXBoxMovie(
    const void* filename, uint32_t flags, uint32_t a3, uint32_t a4, uint32_t a5,
    const void* dataBank, void* engine, uint32_t a8, uint32_t a9, void* a10, void* a11, void* a12)
{
    bool prevInMovie = g_IsInFullscreenMovie;
    for (DWORD i = 0; i < 4; ++i) {
        XINPUT_STATE xState;
        if (DynXInputGetState && DynXInputGetState(i, &xState) == ERROR_SUCCESS) {
            g_MoviePrevXInputButtons[i] = xState.Gamepad.wButtons;
        }
        else {
            g_MoviePrevXInputButtons[i] = 0;
        }
    }

    g_IsInFullscreenMovie = true;
    bool result = o_GFPlayFullScreenXBoxMovie(filename, flags, a3, a4, a5, dataBank, engine, a8, a9, a10, a11, a12);
    g_IsInFullscreenMovie = prevInMovie;
    return result;
}

void __fastcall Hooked_CInputManager_Update(void* _this, void* edx) {
    g_pInputManager = _this;
    g_LastDPadMask = g_CurrentDPadMask;
    g_InjectedThisFrame = false;

    o_CInputManager_Update(_this);

    // Recover controller focus/cursor state even if mouse/keyboard cleared g_IsControllerActive
    // or if DirectInput GetDeviceData returned 0 buffered items for an XInput controller.
    PollXInputActivity(nullptr);
    if (g_CurrentDPadMask != 0) {
        g_IsControllerActive = true;
        SetCursorHidden(true, "DPad");
    }
    if (_this) {
        EngineInputEvent* first = *(EngineInputEvent**)((uintptr_t)_this + 0x10);
        EngineInputEvent* last  = *(EngineInputEvent**)((uintptr_t)_this + 0x14);
        if (first && last && last >= first) {
            for (EngineInputEvent* cur = first; cur < last; ++cur) {
                if (cur->DeviceType == 1) {
                    if (cur->EventType == 19 || cur->EventType == 20) {
                        g_IsControllerActive = true;
                        SetCursorHidden(true, "InputMgr_JoyBtn");
                    }
                    else if (cur->EventType == 17 || cur->EventType == 18) {
                        float x = *(float*)((uintptr_t)cur + 0);
                        float y = *(float*)((uintptr_t)cur + 4);
                        if (fabsf(x) > 0.25f || fabsf(y) > 0.25f) {
                            g_IsControllerActive = true;
                            SetCursorHidden(true, "InputMgr_JoyAxis");
                        }
                    }
                }
            }
        }
    }

    // Allow the manual D-Pad event injector to run for ALL controllers, 
    // including XInput, since we are wiping the raw hardware buffer.
    if (!g_InjectedThisFrame) {
        ProcessDPadInjection();
        g_InjectedThisFrame = true; // Mark as injected to be perfectly safe against double-fires
    }

    ProcessMovieSkipInput(_this);
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
            typedef void(__thiscall* SetPerformed_t)(void*);
            if (processed_input) ((SetPerformed_t)Retail::ADDR_CPROCESSEDINPUT_SETPERFORMED)(processed_input);
            return;
        }
    }
    o_ProcessBoastInput(_this, event, processed_input);
}

static bool IsSanePtr(const void* p) {
    uintptr_t a = (uintptr_t)p;
    return a >= 0x00010000u && a < 0x7FFF0000u;
}

// --- In-Game UI Navigation (Save/Load, Popups, Death/Fail) ---
typedef void(__thiscall* InGameProcessInput_t)(void* _this, const EngineInputEvent* inputEvent, void* processedInput);
static InGameProcessInput_t o_CInputProcessInventory_ProcessInput = nullptr;
static InGameProcessInput_t o_CInputProcessQuestCompletionUI_ProcessInput = nullptr;

typedef char(__thiscall* IsSaveMenuReceivingInput_t)(void* _this);
static IsSaveMenuReceivingInput_t o_CTCInventory_IsSaveMenuReceivingInput = nullptr;

static std::unordered_map<uintptr_t, int> g_InGamePairSelection;
static bool g_InGameStickLeftHeld = false;
static bool g_InGameStickRightHeld = false;

struct IGOptionsFocusState {
    bool inBottomZone = false;
    int  bottomIdx = 0;
};
static std::unordered_map<uintptr_t, IGOptionsFocusState> g_InGameOptionsFocus;
static int   g_InGameOptStickXDir = 0;
static int   g_InGameOptStickYDir = 0;
static DWORD g_InGameOptSliderRepeatTick = 0;
static DWORD g_InGameOptScrollRepeatTick = 0;
static int   g_InGameOptLastSelChild = -1;
static DWORD g_InGameOptRowChangeTick = 0;

static void IG_MarkPerformed(void* processedInput) {
    if (!processedInput) return;
    typedef void(__thiscall* SetPerformed_t)(void*);
    ((SetPerformed_t)Retail::ADDR_CPROCESSEDINPUT_SETPERFORMED)(processedInput);
}

static void IG_SendUIEvent(int eventId) {
    void* uiMgr = NUISystem_GetInstance();
    if (!uiMgr) return;
    void** vTable = *(void***)uiMgr;
    ((NUISystem_SendEvent_t)vTable[0])(uiMgr, eventId);
}

static bool IG_IsExclusivelyObserved() {
    void* uiMgr = NUISystem_GetInstance();
    if (!uiMgr) return false;
    typedef bool(__thiscall* IsExcl_t)(void*);
    auto fn = *(IsExcl_t*)(*(uintptr_t*)uiMgr + 32); // CManager slot 8
    return fn(uiMgr);
}

static uint32_t IG_GetTargetStateIndex(void* comp) {
    if (!IsSanePtr(comp)) return 0xFFFFFFFFu;
    typedef uint32_t(__thiscall* GetTargetState_t)(void*);
    auto fn = *(GetTargetState_t*)(*(uintptr_t*)comp + 348); // CUIInterface slot 87
    return fn(comp);
}

static bool IG_IsObservingEvent(void* comp, int eventId) {
    if (!IsSanePtr(comp)) return false;
    void* obs = (void*)((uintptr_t)comp + 4);
    typedef bool(__thiscall* IsObs_t)(void*, int);
    auto fn = *(IsObs_t*)(*(uintptr_t*)obs + 8); // CObserver slot 2
    return fn(obs, eventId);
}

static bool IG_IsClickableType(uint32_t uiType) {
    return uiType == 7u || uiType == 11u || uiType == 34u || uiType == 35u ||
           uiType == 38u || uiType == 39u || uiType == 42u;
}

static void IG_ActivateClickable(void* comp) {
    if (!IsSanePtr(comp) || !IsSanePtr(*(void**)comp)) return;
    uint32_t uiType = FE_GetType(comp);
    if (!IG_IsClickableType(uiType)) {
        LogFmt("IG_ActivateClickable: skipped unexpected uiType=%u comp=%p", uiType, comp);
        return;
    }
    LogFmt("IG_ActivateClickable: comp=%p uiType=%u", comp, uiType);
    *(uint32_t*)((uintptr_t)comp + 0x16C) = 0; // Restore UI_STATE_INACTIVE after unclick
    typedef void(__thiscall* OnLeftUnclicked_t)(void*);
    auto fn = *(OnLeftUnclicked_t*)(*(uintptr_t*)comp + 588); // CClickable slot 147
    fn(comp);
}

static bool IG_IsEventGameAction(void* inputProcess, int gameAction, const void* ev) {
    typedef bool(__thiscall* IsAct_t)(void*, int, const void*);
    return ((IsAct_t)Retail::ADDR_CINPUTPROCESS_IS_EVENT_GAME_ACTION)(inputProcess, gameAction, ev);
}

static bool IG_IsBtnPressed(const void* ev) {
    typedef bool(__stdcall* Fn_t)(const void*);
    return ((Fn_t)Retail::ADDR_CINPUTPROCESS_IS_BTN_PRESSED)(ev);
}

static bool IG_IsBtnHeld(const void* ev) {
    typedef bool(__stdcall* Fn_t)(const void*);
    return ((Fn_t)Retail::ADDR_CINPUTPROCESS_IS_BTN_HELD)(ev);
}

static bool IG_IsBtnReleased(const void* ev) {
    typedef bool(__stdcall* Fn_t)(const void*);
    return ((Fn_t)Retail::ADDR_CINPUTPROCESS_IS_BTN_RELEASED)(ev);
}

static void* IG_GetControlledThingComponent(void* cPlayer, int tciType) {
    if (!IsSanePtr(cPlayer)) return nullptr;
    typedef void* (__thiscall* GetCreature_t)(void*);
    void* creature = ((GetCreature_t)Retail::ADDR_CPLAYER_GETPCONTROLLEDCREATURE)(cPlayer);
    if (!IsSanePtr(creature)) return nullptr;

    uint32_t wordIdx = (uint32_t)tciType >> 5;
    uint32_t bitMask = 1u << ((uint32_t)tciType & 31u);
    uint32_t* usingBits = (uint32_t*)((uintptr_t)creature + 32);
    if ((usingBits[wordIdx] & bitMask) == 0) return nullptr;

    void* vecMap = (void*)((uintptr_t)creature + 68);
    typedef int* (__thiscall* LowerBound_t)(void*, const int*);
    int* it = ((LowerBound_t)Retail::ADDR_CVECTORMAP_LOWERBOUND)(vecMap, &tciType);
    int* end = *(int**)((uintptr_t)creature + 72);
    if (!it || it == end || *it > tciType) return nullptr;
    return *(void**)(it + 1);
}

static void* IG_GetMainPlayer() {
    void* mainGame = *(void**)Retail::ADDR_PMAINGAMECOMPONENT;
    if (!IsSanePtr(mainGame)) return nullptr;
    void* playerMgr = *(void**)((uintptr_t)mainGame + 0x1C);
    if (!IsSanePtr(playerMgr)) return nullptr;
    uintptr_t vecFirst = *(uintptr_t*)((uintptr_t)playerMgr + 12);
    uintptr_t vecLast  = *(uintptr_t*)((uintptr_t)playerMgr + 16);
    if (!vecFirst || vecLast <= vecFirst) return nullptr;
    typedef void* (__thiscall* GetMain_t)(void*);
    return ((GetMain_t)Retail::ADDR_CPLAYERMANAGER_GETMAINPLAYER)(playerMgr);
}

static bool IG_IsWorldLoadingOrResetting() {
    void* mainGame = *(void**)Retail::ADDR_PMAINGAMECOMPONENT;
    if (!IsSanePtr(mainGame)) return true;
    void* world = *(void**)((uintptr_t)mainGame + 0x24);
    if (!IsSanePtr(world)) return true;
    uint32_t saveLoadState = *(uint32_t*)((uintptr_t)world + 248);
    uint32_t regionLoadState = *(uint32_t*)((uintptr_t)world + 260);
    return saveLoadState != 0 || regionLoadState != 0;
}

template <typename Fn>
static void IG_ForEachChildByName(void* rootComp, const char* name, Fn&& callback) {
    if (!IsSanePtr(rootComp) || !name) return;
    void* listHead = nullptr;
    char dummy = 0;
    typedef void* (__thiscall* CreateSentinel_t)(void**, void*);
    typedef void(__thiscall* DestroyList_t)(void**);
    ((CreateSentinel_t)Retail::ADDR_STDLIST_CREATE_SENTINEL)(&listHead, &dummy);

    GameCharString str = { nullptr };
    auto ctor = o_CCharString_Construct ? o_CCharString_Construct : (CCharString_Construct_t)Retail::ADDR_CCHARSTRING_CONSTRUCT;
    ctor(&str, name, -1);
    typedef void(__thiscall* FindChildren_t)(void*, GameCharString, void**);
    auto fnFind = *(FindChildren_t*)(*(uintptr_t*)rootComp + 20); // CUIInterface slot 5
    fnFind(rootComp, str, &listHead);

    if (listHead) {
        for (uintptr_t node = *(uintptr_t*)listHead; node && node != (uintptr_t)listHead; node = *(uintptr_t*)node) {
            void* child = *(void**)(node + 8);
            if (IsSanePtr(child)) {
                if (callback(child)) break;
            }
        }
    }
    ((DestroyList_t)Retail::ADDR_STDLIST_DESTROY)(&listHead);
}

// Finds active OK or Yes/No modal dialog inside CManager::ConcurrentExclusiveObservers (+0x0C).
static bool IG_FindActiveModalButtons(void*& outDlg, void*& outBtn0, void*& outBtn1, bool& outIsOkOnly) {
    outDlg = nullptr;
    outBtn0 = nullptr;
    outBtn1 = nullptr;
    outIsOkOnly = false;

    void* uiMgr = NUISystem_GetInstance();
    if (!IsSanePtr(uiMgr)) return false;

    uintptr_t head = *(uintptr_t*)((uintptr_t)uiMgr + 0x0C); // ConcurrentExclusiveObservers
    if (!IsSanePtr((void*)head) || *(uintptr_t*)head == head) return false;

    int guard = 0;
    for (uintptr_t node = *(uintptr_t*)head; IsSanePtr((void*)node) && node != head && guard < 256; node = *(uintptr_t*)node, ++guard) {
        void* pObs = *(void**)(node + 8);
        if (!IsSanePtr(pObs)) continue;
        void* comp = (void*)((uintptr_t)pObs - 4);
        if (!IsSanePtr(comp) || !IsSanePtr(*(void**)comp)) continue;
        if (FE_GetType(comp) != 25u) continue; // UI_TYPE_YESNO

        if (void* okBtn = FE_FindChild(comp, "PC_OK_GAME_OKBUTTON")) {
            outDlg = comp;
            outBtn0 = okBtn;
            outIsOkOnly = true;
            return true;
        }
        void* yesBtn = FE_FindChild(comp, "PC_YESNO_GAME_YESBUTTON");
        void* noBtn  = FE_FindChild(comp, "PC_YESNO_GAME_NOBUTTON");
        if (yesBtn && noBtn) {
            outDlg = comp;
            outBtn0 = yesBtn;
            outBtn1 = noBtn;
            outIsOkOnly = false;
            return true;
        }
    }
    return false;
}

// Finds active Save/Load confirmation sub-list Apply & Cancel buttons when open.
static bool IG_FindActiveSaveLoadButtons(void* tcInventory, void*& outContainer, void*& outApplyBtn, void*& outCancelBtn) {
    outContainer = nullptr;
    outApplyBtn = nullptr;
    outCancelBtn = nullptr;
    if (!IsSanePtr(tcInventory)) return false;
    if (*(uint32_t*)((uintptr_t)tcInventory + 0x21C) != 4u) return false; // PC_OPTIONS menu

    uintptr_t vecFirst = *(uintptr_t*)((uintptr_t)tcInventory + 0x0C);
    uintptr_t vecLast  = *(uintptr_t*)((uintptr_t)tcInventory + 0x10);
    if (!vecFirst || vecLast <= vecFirst) return false;

    for (uintptr_t p = vecFirst; p + 4 <= vecLast; p += 4) {
        void* root = *(void**)p;
        if (!IsSanePtr(root)) continue;

        IG_ForEachChildByName(root, "COMPONENT_SAVE", [&](void* child) -> bool {
            // Only match PC_SUB_LIST (UI_TYPE_LIST = 8), ignoring empty-slot CMenuEntry (type 7)
            if (FE_GetType(child) == 8u && (IG_IsObservingEvent(child, 1) || IG_IsObservingEvent(child, 2))) {
                void* applyBtn  = FE_FindChild(child, "PC_OPTIONS_ACCEPT_CHANGES_SAVE_LOAD");
                void* cancelBtn = FE_FindChild(child, "PC_OPTIONS_BACK_SAVE_LOAD");
                if (applyBtn && cancelBtn) {
                    outContainer = child;
                    outApplyBtn = applyBtn;
                    outCancelBtn = cancelBtn;
                    return true;
                }
            }
            return false;
        });
        if (outContainer) return true;

        IG_ForEachChildByName(root, "PC_UI_MENU_ENTRY_LOAD_CONFIRM", [&](void* child) -> bool {
            if (IG_IsObservingEvent(child, 2)) {
                void* parent = FE_GetParent(child);
                void* applyBtn  = FE_FindChild(parent, "PC_OPTIONS_ACCEPT_CHANGES_SAVE_LOAD");
                void* cancelBtn = FE_FindChild(parent, "PC_OPTIONS_BACK_SAVE_LOAD");
                if (applyBtn && cancelBtn) {
                    outContainer = parent;
                    outApplyBtn = applyBtn;
                    outCancelBtn = cancelBtn;
                    return true;
                }
            }
            return false;
        });
        if (outContainer) return true;
    }
    return false;
}

static bool IG_FindQuestCompletionButtons(void* tcQC, void*& outRespawnBtn, void*& outQuitBtn) {
    outRespawnBtn = nullptr;
    outQuitBtn = nullptr;
    if (!IsSanePtr(tcQC)) return false;
    // Match sub_5E3E70: Active && !Quitting && !FadingOutToRespawn
    if (*(uint8_t*)((uintptr_t)tcQC + 12) == 0 ||
        *(uint8_t*)((uintptr_t)tcQC + 28) != 0 ||
        *(uint8_t*)((uintptr_t)tcQC + 192) != 0) {
        return false;
    }

    uint32_t completion = *(uint32_t*)((uintptr_t)tcQC + 32);
    if (completion != 2u && completion != 3u) return false; // QUEST_FAILURE / HERO_DIED

    uintptr_t vecFirst = *(uintptr_t*)((uintptr_t)tcQC + 0x10);
    uintptr_t vecLast  = *(uintptr_t*)((uintptr_t)tcQC + 0x14);
    if (!vecFirst || vecLast <= vecFirst) return false;

    for (uintptr_t p = vecFirst; p + 4 <= vecLast; p += 4) {
        void* comp = *(void**)p;
        if (!IsSanePtr(comp)) continue;
        if (!outRespawnBtn) outRespawnBtn = FE_FindChild(comp, "PC_RESPAWN_HELPER");
        if (!outQuitBtn)    outQuitBtn    = FE_FindChild(comp, "PC_QUIT_HELPER");
    }
    return outRespawnBtn != nullptr;
}

static int& IG_EnsurePairHighlight(void* containerKey, void* btn0, void* btn1) {
    int& sel = g_InGamePairSelection[(uintptr_t)containerKey];
    bool highlighted0 = (IG_GetTargetStateIndex(btn0) == 1u);
    bool highlighted1 = (btn1 != nullptr && IG_GetTargetStateIndex(btn1) == 1u);
    if (!highlighted0 && !highlighted1) {
        sel = 0;
        FE_ChangeState(btn0, 1u);
        if (btn1) FE_ChangeState(btn1, 0u);
    }
    else if (g_IsControllerActive) {
        if (!btn1 && !highlighted0) FE_ChangeState(btn0, 1u);
    }
    return sel;
}

static void IG_ApplyPairVisuals(void* btn0, void* btn1, int sel) {
    FE_ChangeState(btn0, sel == 0 ? 1u : 0u);
    if (btn1) FE_ChangeState(btn1, sel == 1 ? 1u : 0u);
}

static void IG_ResetPairVisuals(void* containerKey, void* btn0, void* btn1) {
    FE_ChangeState(btn0, 0u);
    if (btn1) FE_ChangeState(btn1, 0u);
    g_InGamePairSelection.erase((uintptr_t)containerKey);
}

enum IGPairMode {
    IG_PAIR_MODAL_OK = 0,
    IG_PAIR_MODAL_YESNO = 1,
    IG_PAIR_CLICKABLES_WITH_B = 2,
    IG_PAIR_CLICKABLES_NO_B = 3
};

static bool IG_HandlePairInput(
    void* inputProcess, const EngineInputEvent* ev, void* processedInput,
    void* containerKey, void* btn0, void* btn1, IGPairMode mode)
{
    int& sel = IG_EnsurePairHighlight(containerKey, btn0, btn1);
    if (btn1 && sel != 0 && sel != 1) sel = 0;

    if (ev->EventType == 17) { // IE_JOYSTICK_POSITION (Left Stick)
        float xy[2] = { 0.0f, 0.0f };
        typedef void(__thiscall* GetJoy_t)(const void*, float*);
        ((GetJoy_t)Retail::ADDR_CINPUTEVENT_GET_JOYSTICK_POS)(ev, xy);

        if (fabsf(xy[0]) < 0.35f) {
            g_InGameStickLeftHeld = false;
            g_InGameStickRightHeld = false;
        }
        else if (btn1 && xy[0] < -0.6f) {
            if (!g_InGameStickLeftHeld) {
                g_InGameStickLeftHeld = true;
                g_InGameStickRightHeld = false;
                sel = 0;
                IG_ApplyPairVisuals(btn0, btn1, sel);
            }
            IG_MarkPerformed(processedInput);
            return true;
        }
        else if (btn1 && xy[0] > 0.6f) {
            if (!g_InGameStickRightHeld) {
                g_InGameStickRightHeld = true;
                g_InGameStickLeftHeld = false;
                sel = 1;
                IG_ApplyPairVisuals(btn0, btn1, sel);
            }
            IG_MarkPerformed(processedInput);
            return true;
        }
        if (mode == IG_PAIR_MODAL_OK || mode == IG_PAIR_MODAL_YESNO) {
            IG_MarkPerformed(processedInput);
            return true;
        }
        return false;
    }

    bool isSelectAction = IG_IsEventGameAction(inputProcess, 25, ev) || IG_IsEventGameAction(inputProcess, 33, ev);
    bool isBackAction   = (mode != IG_PAIR_CLICKABLES_NO_B) && IG_IsEventGameAction(inputProcess, 34, ev);

    if (isSelectAction || isBackAction) {
        if (IG_IsBtnPressed(ev)) {
            const int selVal = sel; // copy: IG_ResetPairVisuals erases the map node `sel` references
            IG_ResetPairVisuals(containerKey, btn0, btn1);
            if (mode == IG_PAIR_MODAL_OK) {
                LogFmt("IG_HandlePairInput: MODAL_OK uiEvent=4 dlg=%p", containerKey);
                IG_SendUIEvent(4); // UI_EVENT_SELECT
            }
            else if (mode == IG_PAIR_MODAL_YESNO) {
                int evId = (isBackAction || selVal == 1) ? 5 : 4;
                LogFmt("IG_HandlePairInput: MODAL_YESNO uiEvent=%d sel=%d isBack=%d dlg=%p", evId, selVal, isBackAction ? 1 : 0, containerKey);
                IG_SendUIEvent(evId);
            }
            else {
                void* target = (isBackAction && btn1) ? btn1 : ((selVal == 1 && btn1) ? btn1 : btn0);
                LogFmt("IG_HandlePairInput: CLICKABLE mode=%d sel=%d isBack=%d target=%p container=%p", (int)mode, selVal, isBackAction ? 1 : 0, target, containerKey);
                IG_ActivateClickable(target);
            }
            IG_MarkPerformed(processedInput);
            return true;
        }
        if (IG_IsBtnHeld(ev) || IG_IsBtnReleased(ev)) {
            IG_MarkPerformed(processedInput);
            return true;
        }
    }

    if (IG_IsBtnPressed(ev)) {
        if (btn1 && IG_IsEventGameAction(inputProcess, 39, ev)) { // LEFT
            sel = 0;
            IG_ApplyPairVisuals(btn0, btn1, sel);
            IG_MarkPerformed(processedInput);
            return true;
        }
        if (btn1 && IG_IsEventGameAction(inputProcess, 40, ev)) { // RIGHT
            sel = 1;
            IG_ApplyPairVisuals(btn0, btn1, sel);
            IG_MarkPerformed(processedInput);
            return true;
        }
        if ((mode == IG_PAIR_MODAL_OK || mode == IG_PAIR_MODAL_YESNO) &&
            (IG_IsEventGameAction(inputProcess, 37, ev) || IG_IsEventGameAction(inputProcess, 38, ev) ||
             IG_IsEventGameAction(inputProcess, 39, ev) || IG_IsEventGameAction(inputProcess, 40, ev))) {
            IG_MarkPerformed(processedInput);
            return true;
        }
    }

    return false;
}

struct IGBottomButton {
    void* comp;
    bool isRestoreDefaults;
    bool isBack;
    float x;
};

static bool IG_IsCapturingRedefineKey() {
    void* pKeyRedef = *(void**)Retail::ADDR_PACTIVE_KEY_REDEFINER;
    if (IsSanePtr(pKeyRedef)) return true;
    void* pRedefList = *(void**)Retail::ADDR_PACTIVE_REDEFINER;
    if (IsSanePtr(pRedefList)) {
        if (*(uint8_t*)((uintptr_t)pRedefList + 380) != 0 &&
            *(uint8_t*)((uintptr_t)pRedefList + 388) != 0) {
            return true;
        }
    }
    return false;
}

static bool IG_FindActiveOptionsSubmenu(void* tcInventory, void*& outListComp, bool& outIsRedefineKeys) {
    outListComp = nullptr;
    outIsRedefineKeys = false;
    if (!IsSanePtr(tcInventory)) return false;
    if (*(uint32_t*)((uintptr_t)tcInventory + 0x21C) != 4u) return false; // PC_OPTIONS menu

    uintptr_t vecFirst = *(uintptr_t*)((uintptr_t)tcInventory + 0x0C);
    uintptr_t vecLast  = *(uintptr_t*)((uintptr_t)tcInventory + 0x10);
    if (!vecFirst || vecLast <= vecFirst) return false;

    static const struct { const char* name; bool isRedefine; } kMenus[] = {
        { "PC_GAMEPLAY_PREFS_LIST", false },
        { "PC_AUDIO_PREFS_LIST",    false },
        { "PC_VIDEO_PREFS_LIST",    false },
        { "PC_REDEFINE_KEYS_LIST",  true  },
    };

    for (uintptr_t p = vecFirst; p + 4 <= vecLast; p += 4) {
        void* root = *(void**)p;
        if (!IsSanePtr(root)) continue;
        for (const auto& m : kMenus) {
            void* comp = FE_FindChild(root, m.name);
            if (IsSanePtr(comp) && IG_GetTargetStateIndex(comp) == 3u) {
                outListComp = comp;
                outIsRedefineKeys = m.isRedefine;
                return true;
            }
        }
    }
    return false;
}

static int IG_CollectBottomButtons(void* listComp, IGBottomButton outBtns[3]) {
    static const struct { const char* name; bool isRestore; bool isBack; } defs[] = {
        { "PC_OPTIONS_ACCEPT_CHANGES",   false, false },
        { "PC_OPTIONS_BACK",             false, true  },
        { "PC_OPTIONS_RESTORE_DEFAULTS", true,  false },
    };
    int count = 0;
    for (const auto& d : defs) {
        void* btn = FE_FindChild(listComp, d.name);
        if (!IsSanePtr(btn)) continue;
        // PC_REDEFINE_KEYS_LIST hides PC_OPTIONS_RESTORE_DEFAULTS with Colour alpha 0 and disables its actions
        uint8_t curAlpha = *(uint8_t*)((uintptr_t)btn + 0x87);
        uint8_t tgtAlpha = *(uint8_t*)((uintptr_t)btn + 0x8B);
        if (curAlpha == 0 && tgtAlpha == 0) continue;
        float posX = *(float*)((uintptr_t)btn + 0x54); // CComponent::GetRenderPosition().X (+84)
        outBtns[count++] = { btn, d.isRestore, d.isBack, posX };
    }
    for (int i = 0; i < count; ++i) {
        for (int j = i + 1; j < count; ++j) {
            if (outBtns[j].x < outBtns[i].x) {
                IGBottomButton tmp = outBtns[i];
                outBtns[i] = outBtns[j];
                outBtns[j] = tmp;
            }
        }
    }
    return count;
}

static uint32_t IG_FindSliderTypeInSubtree(void* comp, int depth = 0) {
    if (!IsSanePtr(comp)) return 0u;
    uint32_t t = FE_GetType(comp);
    if (t == 15u || t == 16u) return t; // CSlider or CTextSlider
    if (depth >= 3) return 0u;
    uintptr_t vecFirst = *(uintptr_t*)((uintptr_t)comp + 0xB0);
    uintptr_t vecLast  = *(uintptr_t*)((uintptr_t)comp + 0xB4);
    if (!vecFirst || vecLast <= vecFirst) return 0u;
    for (uintptr_t p = vecFirst; p + 8 <= vecLast; p += 8) {
        uint32_t found = IG_FindSliderTypeInSubtree(*(void**)p, depth + 1);
        if (found != 0u) return found;
    }
    return 0u;
}

static void* IG_GetListSelectedRow(void* listComp, int& outSelIdx, int& outRowCount) {
    outSelIdx = -1;
    outRowCount = 0;
    if (!IsSanePtr(listComp)) return nullptr;
    uintptr_t vecFirst = *(uintptr_t*)((uintptr_t)listComp + 0x164);
    uintptr_t vecLast  = *(uintptr_t*)((uintptr_t)listComp + 0x168);
    if (!vecFirst || vecLast <= vecFirst) return nullptr;
    outRowCount = (int)((vecLast - vecFirst) / 4);
    int sel = *(int*)((uintptr_t)listComp + 0x15C);
    if (sel < 0 || sel >= outRowCount) return nullptr;
    outSelIdx = sel;
    void* row = *(void**)(vecFirst + 4 * sel);
    return IsSanePtr(row) ? row : nullptr;
}

static void IG_ApplyBottomButtonVisuals(const IGBottomButton* btns, int count, int activeIdx) {
    for (int i = 0; i < count; ++i) {
        FE_ChangeState(btns[i].comp, (i == activeIdx) ? 1u : 0u);
    }
}

static void IG_EnterBottomZone(IGOptionsFocusState& st, void* activeRow, const IGBottomButton* btns, int btnCount) {
    st.inBottomZone = true;
    if (st.bottomIdx < 0 || st.bottomIdx >= btnCount) st.bottomIdx = 0;
    // Use state 4 (UNSELECTED) so CSlider/CTextSlider unregisters input observers without reverting values
    if (activeRow) FE_ChangeState(activeRow, 4u);
    IG_ApplyBottomButtonVisuals(btns, btnCount, st.bottomIdx);
}

static void IG_ExitBottomZone(IGOptionsFocusState& st, void* activeRow, const IGBottomButton* btns, int btnCount) {
    st.inBottomZone = false;
    IG_ApplyBottomButtonVisuals(btns, btnCount, -1);
    if (activeRow) FE_ChangeState(activeRow, 3u);
}

static void IG_PruneStaleOptionsFocus(void* currentActiveList) {
    if (g_InGameOptionsFocus.empty()) return;
    if (!currentActiveList) {
        g_InGameOptionsFocus.clear();
        g_InGameOptLastSelChild = -1;
        return;
    }
    for (auto it = g_InGameOptionsFocus.begin(); it != g_InGameOptionsFocus.end(); ) {
        if (it->first != (uintptr_t)currentActiveList) {
            it = g_InGameOptionsFocus.erase(it);
        } else {
            ++it;
        }
    }
}

static bool IG_HandleOptionsSubmenuInput(void* inputProcess, void* tcInv, const EngineInputEvent* ev, void* processedInput) {
    if (IG_IsExclusivelyObserved() || IG_IsCapturingRedefineKey()) return false;

    void* listComp = nullptr;
    bool isRedefineKeys = false;
    if (!IG_FindActiveOptionsSubmenu(tcInv, listComp, isRedefineKeys)) {
        IG_PruneStaleOptionsFocus(nullptr);
        return false;
    }
    IG_PruneStaleOptionsFocus(listComp);

    IGBottomButton btns[3];
    int btnCount = IG_CollectBottomButtons(listComp, btns);
    IGOptionsFocusState& st = g_InGameOptionsFocus[(uintptr_t)listComp];
    if (btnCount <= 0) {
        st.inBottomZone = false;
    } else if (st.bottomIdx < 0 || st.bottomIdx >= btnCount) {
        st.bottomIdx = 0;
    }

    int selIdx = -1;
    int rowCount = 0;
    void* activeRow = isRedefineKeys ? nullptr : IG_GetListSelectedRow(listComp, selIdx, rowCount);
    if (!isRedefineKeys && selIdx != g_InGameOptLastSelChild) {
        g_InGameOptLastSelChild = selIdx;
        g_InGameOptRowChangeTick = GetTickCount();
    }

    auto findBackButton = [&]() -> void* {
        for (int i = 0; i < btnCount; ++i) {
            if (btns[i].isBack) return btns[i].comp;
        }
        return FE_FindChild(listComp, "PC_OPTIONS_BACK");
    };

    auto activateAndCloseButton = [&](void* btnComp, bool isRestoreDefaults) {
        if (isRestoreDefaults) {
            FE_ChangeState(btnComp, 1u);
            IG_ActivateClickable(btnComp);
            return;
        }
        IG_ApplyBottomButtonVisuals(btns, btnCount, -1);
        g_InGameOptionsFocus.erase((uintptr_t)listComp);
        // Restore active slider row to state 3 so it receives SEND_EVENT(4/5) from Apply/Cancel
        if (activeRow) FE_ChangeState(activeRow, 3u);
        IG_ActivateClickable(btnComp);
    };

    auto cancelMenu = [&]() {
        if (isRedefineKeys) {
            void* backBtn = findBackButton();
            IG_ApplyBottomButtonVisuals(btns, btnCount, -1);
            g_InGameOptionsFocus.erase((uintptr_t)listComp);
            if (backBtn) IG_ActivateClickable(backBtn);
            return;
        }
        IG_ApplyBottomButtonVisuals(btns, btnCount, -1);
        g_InGameOptionsFocus.erase((uintptr_t)listComp);
        if (activeRow) FE_ChangeState(activeRow, 3u);
        IG_SendUIEvent(5); // UI_EVENT_BACK reverts active sliders & pops CList::GPreviousSelected
    };

    auto getRedefinerScrollMetrics = [&](uint32_t& outTop, uint32_t& outVis, int& outTotal) {
        outTop = 0;
        outVis = 0;
        outTotal = 0;
        void* redefList = FE_FindChild(listComp, "PC_UI_REDEFINER_LIST");
        if (!IsSanePtr(redefList)) return false;
        outTop = *(uint32_t*)((uintptr_t)redefList + 0x15C);
        outVis = *(uint32_t*)((uintptr_t)redefList + 0x160);
        uintptr_t rf0 = *(uintptr_t*)((uintptr_t)redefList + 0xB0);
        uintptr_t rf1 = *(uintptr_t*)((uintptr_t)redefList + 0xB4);
        if (rf0 && rf1 >= rf0) outTotal = (int)((rf1 - rf0) / 8);
        return true;
    };

    // --- ZONE B: Bottom Apply / Cancel / Restore Defaults buttons ---
    if (st.inBottomZone) {
        IG_ApplyBottomButtonVisuals(btns, btnCount, st.bottomIdx);

        if (ev->EventType == 17) {
            float xy[2] = { 0.0f, 0.0f };
            typedef void(__thiscall* GetJoy_t)(const void*, float*);
            ((GetJoy_t)Retail::ADDR_CINPUTEVENT_GET_JOYSTICK_POS)(ev, xy);

            if (fabsf(xy[0]) < 0.35f) g_InGameOptStickXDir = 0;
            if (fabsf(xy[1]) < 0.35f) g_InGameOptStickYDir = 0;

            if (xy[0] < -0.6f) {
                if (g_InGameOptStickXDir != -1) {
                    g_InGameOptStickXDir = -1;
                    st.bottomIdx = (st.bottomIdx + btnCount - 1) % btnCount;
                    IG_ApplyBottomButtonVisuals(btns, btnCount, st.bottomIdx);
                }
            }
            else if (xy[0] > 0.6f) {
                if (g_InGameOptStickXDir != 1) {
                    g_InGameOptStickXDir = 1;
                    st.bottomIdx = (st.bottomIdx + 1) % btnCount;
                    IG_ApplyBottomButtonVisuals(btns, btnCount, st.bottomIdx);
                }
            }
            else if (xy[1] < -0.6f || xy[1] > 0.6f) {
                int yDir = (xy[1] < 0.0f) ? 1 : -1; // -Y is Up, +Y is Down in CInputEvent
                if (g_InGameOptStickYDir != yDir) {
                    g_InGameOptStickYDir = yDir;
                    IG_ExitBottomZone(st, activeRow, btns, btnCount);
                }
            }
            IG_MarkPerformed(processedInput);
            return true;
        }

        bool isSelect = IG_IsEventGameAction(inputProcess, 25, ev) || IG_IsEventGameAction(inputProcess, 33, ev);
        bool isBack   = IG_IsEventGameAction(inputProcess, 34, ev);
        bool isLeft   = IG_IsEventGameAction(inputProcess, 39, ev);
        bool isRight  = IG_IsEventGameAction(inputProcess, 40, ev);
        bool isUp     = IG_IsEventGameAction(inputProcess, 37, ev);
        bool isDown   = IG_IsEventGameAction(inputProcess, 38, ev);

        if (isSelect || isBack || isLeft || isRight || isUp || isDown) {
            if (IG_IsBtnPressed(ev)) {
                if (isLeft) {
                    st.bottomIdx = (st.bottomIdx + btnCount - 1) % btnCount;
                    IG_ApplyBottomButtonVisuals(btns, btnCount, st.bottomIdx);
                }
                else if (isRight) {
                    st.bottomIdx = (st.bottomIdx + 1) % btnCount;
                    IG_ApplyBottomButtonVisuals(btns, btnCount, st.bottomIdx);
                }
                else if (isUp || isDown) {
                    IG_ExitBottomZone(st, activeRow, btns, btnCount);
                }
                else if (isSelect) {
                    IGBottomButton chosen = btns[st.bottomIdx];
                    activateAndCloseButton(chosen.comp, chosen.isRestoreDefaults);
                }
                else if (isBack) {
                    cancelMenu();
                }
            }
            IG_MarkPerformed(processedInput);
            return true;
        }
        return false;
    }

    // --- ZONE A: Option rows / Redefiner list ---
    if (ev->EventType == 17) {
        float xy[2] = { 0.0f, 0.0f };
        typedef void(__thiscall* GetJoy_t)(const void*, float*);
        ((GetJoy_t)Retail::ADDR_CINPUTEVENT_GET_JOYSTICK_POS)(ev, xy);

        if (fabsf(xy[0]) < 0.35f) {
            g_InGameOptStickXDir = 0;
            g_InGameOptSliderRepeatTick = 0;
        }
        if (fabsf(xy[1]) < 0.35f) {
            g_InGameOptStickYDir = 0;
            g_InGameOptScrollRepeatTick = 0;
        }

        if (xy[0] < -0.6f || xy[0] > 0.6f) {
            int dir = (xy[0] < 0.0f) ? -1 : 1;
            if (!isRedefineKeys) {
                uint32_t sliderType = IG_FindSliderTypeInSubtree(activeRow);
                DWORD nowTick = GetTickCount();
                bool fire = false;
                if (g_InGameOptStickXDir != dir) {
                    g_InGameOptStickXDir = dir;
                    g_InGameOptSliderRepeatTick = nowTick;
                    fire = true;
                }
                else if (sliderType == 15u && (nowTick - g_InGameOptSliderRepeatTick) >= 110) {
                    g_InGameOptSliderRepeatTick = nowTick;
                    fire = true;
                }
                if (fire) {
                    IG_SendUIEvent(dir < 0 ? 20 : 21); // UI_EVENT_SLIDER_LEFT / RIGHT
                }
            }
            IG_MarkPerformed(processedInput);
            return true;
        }

        if (xy[1] < -0.6f || xy[1] > 0.6f) {
            bool isUp = (xy[1] < 0.0f); // -Y is Up, +Y is Down in CInputEvent
            int yDir = isUp ? 1 : -1;
            bool freshEdge = (g_InGameOptStickYDir != yDir);
            DWORD nowTick = GetTickCount();

            if (isRedefineKeys) {
                uint32_t topIdx = 0, visCnt = 0;
                int rfTotal = 0;
                getRedefinerScrollMetrics(topIdx, visCnt, rfTotal);
                bool atTop = (topIdx == 0u);
                bool atBottom = (rfTotal == 0 || visCnt == 0 || topIdx + visCnt >= (uint32_t)rfTotal);
                if (btnCount > 0 && freshEdge && ((isUp && atTop) || (!isUp && atBottom))) {
                    g_InGameOptStickYDir = yDir;
                    IG_EnterBottomZone(st, nullptr, btns, btnCount);
                }
                else if (freshEdge || (nowTick - g_InGameOptScrollRepeatTick) >= 110) {
                    if (!(isUp && atTop) && !(!isUp && atBottom)) {
                        g_InGameOptScrollRepeatTick = nowTick;
                        IG_SendUIEvent(isUp ? 36 : 37);
                    }
                    g_InGameOptStickYDir = yDir;
                }
                IG_MarkPerformed(processedInput);
                return true;
            }

            bool atBoundary = (rowCount > 0) && ((isUp && selIdx == 0) || (!isUp && selIdx == rowCount - 1));
            if (atBoundary && btnCount > 0) {
                bool settleOk = freshEdge || ((nowTick - g_InGameOptRowChangeTick) >= 250 && (nowTick - g_InGameOptScrollRepeatTick) >= 250);
                if (settleOk) {
                    g_InGameOptStickYDir = yDir;
                    IG_EnterBottomZone(st, activeRow, btns, btnCount);
                }
                IG_MarkPerformed(processedInput);
                return true;
            }
            g_InGameOptStickYDir = yDir;
            g_InGameOptScrollRepeatTick = nowTick;
        }
        return false;
    }

    // D-Pad Left / Right -> Slider adjustment (never Apply/Back!)
    bool isLeft  = IG_IsEventGameAction(inputProcess, 39, ev);
    bool isRight = IG_IsEventGameAction(inputProcess, 40, ev);
    if (isLeft || isRight) {
        if (!isRedefineKeys) {
            if (IG_IsBtnPressed(ev)) {
                g_InGameOptSliderRepeatTick = GetTickCount();
                IG_SendUIEvent(isLeft ? 20 : 21);
            }
            else if (IG_IsBtnHeld(ev) && IG_FindSliderTypeInSubtree(activeRow) == 15u) {
                DWORD nowTick = GetTickCount();
                if ((nowTick - g_InGameOptSliderRepeatTick) >= 110) {
                    g_InGameOptSliderRepeatTick = nowTick;
                    IG_SendUIEvent(isLeft ? 20 : 21);
                }
            }
            else if (IG_IsBtnReleased(ev)) {
                g_InGameOptSliderRepeatTick = 0;
            }
        }
        IG_MarkPerformed(processedInput);
        return true;
    }

    // D-Pad Up / Down
    bool isUp   = IG_IsEventGameAction(inputProcess, 37, ev);
    bool isDown = IG_IsEventGameAction(inputProcess, 38, ev);
    if (isUp || isDown) {
        if (isRedefineKeys) {
            uint32_t topIdx = 0, visCnt = 0;
            int rfTotal = 0;
            getRedefinerScrollMetrics(topIdx, visCnt, rfTotal);
            bool atTop = (topIdx == 0u);
            bool atBottom = (rfTotal == 0 || visCnt == 0 || topIdx + visCnt >= (uint32_t)rfTotal);
            if (IG_IsBtnPressed(ev) && btnCount > 0 && ((isUp && atTop) || (isDown && atBottom))) {
                IG_EnterBottomZone(st, nullptr, btns, btnCount);
            }
            else if (IG_IsBtnPressed(ev) || (IG_IsBtnHeld(ev) && (GetTickCount() - g_InGameOptScrollRepeatTick) >= 110)) {
                if (!(isUp && atTop) && !(!isUp && atBottom)) {
                    g_InGameOptScrollRepeatTick = GetTickCount();
                    IG_SendUIEvent(isUp ? 36 : 37);
                }
            }
            IG_MarkPerformed(processedInput);
            return true;
        }

        bool atBoundary = (rowCount > 0) && ((isUp && selIdx == 0) || (isDown && selIdx == rowCount - 1));
        if (atBoundary && btnCount > 0) {
            if (IG_IsBtnPressed(ev)) {
                IG_EnterBottomZone(st, activeRow, btns, btnCount);
            }
            IG_MarkPerformed(processedInput);
            return true;
        }
        return false; // Let native CList scroll intermediate rows
    }

    // Controller A on a row: cycle discrete toggles (CTextSlider), never Apply/exit the submenu
    bool isSelect = IG_IsEventGameAction(inputProcess, 25, ev) || IG_IsEventGameAction(inputProcess, 33, ev);
    if (isSelect) {
        if (IG_IsBtnPressed(ev) && !isRedefineKeys && IG_FindSliderTypeInSubtree(activeRow) == 16u) {
            IG_SendUIEvent(21);
        }
        IG_MarkPerformed(processedInput);
        return true;
    }

    // Controller B in Zone A: clean Cancel (especially needed in PC_REDEFINE_KEYS_LIST where CRedefinerList ignores raw event 5)
    bool isBack = IG_IsEventGameAction(inputProcess, 34, ev);
    if (isBack) {
        if (IG_IsBtnPressed(ev)) {
            cancelMenu();
        }
        IG_MarkPerformed(processedInput);
        return true;
    }

    return false;
}

static void IG_SyncControllerFocus() {
    if (!g_IsControllerActive || IG_IsWorldLoadingOrResetting()) return;

    void* dlg = nullptr;
    void* btn0 = nullptr;
    void* btn1 = nullptr;
    bool isOkOnly = false;
    if (IG_FindActiveModalButtons(dlg, btn0, btn1, isOkOnly)) {
        IG_EnsurePairHighlight(dlg, btn0, btn1);
        return;
    }

    void* player = IG_GetMainPlayer();
    if (!player) return;

    if (void* tcQC = IG_GetControlledThingComponent(player, 252)) {
        void* respawnBtn = nullptr;
        void* quitBtn = nullptr;
        if (IG_FindQuestCompletionButtons(tcQC, respawnBtn, quitBtn)) {
            IG_EnsurePairHighlight(tcQC, respawnBtn, quitBtn);
        }
    }

    if (void* tcInv = IG_GetControlledThingComponent(player, 17)) {
        void* container = nullptr;
        void* applyBtn = nullptr;
        void* cancelBtn = nullptr;
        if (IG_FindActiveSaveLoadButtons(tcInv, container, applyBtn, cancelBtn)) {
            IG_EnsurePairHighlight(container, applyBtn, cancelBtn);
        }
        void* optList = nullptr;
        bool isRedefine = false;
        if (!IG_IsExclusivelyObserved() && !IG_IsCapturingRedefineKey() &&
            IG_FindActiveOptionsSubmenu(tcInv, optList, isRedefine)) {
            IG_PruneStaleOptionsFocus(optList);
            auto it = g_InGameOptionsFocus.find((uintptr_t)optList);
            if (it != g_InGameOptionsFocus.end() && it->second.inBottomZone) {
                IGBottomButton btns[3];
                int btnCount = IG_CollectBottomButtons(optList, btns);
                if (btnCount > 0) {
                    if (it->second.bottomIdx < 0 || it->second.bottomIdx >= btnCount) {
                        it->second.bottomIdx = 0;
                    }
                    IG_ApplyBottomButtonVisuals(btns, btnCount, it->second.bottomIdx);
                }
            }
        }
        else {
            IG_PruneStaleOptionsFocus(nullptr);
        }
    }
}

char __fastcall Hooked_CTCInventory_IsSaveMenuReceivingInput(void* _this, void* edx) {
    if (!IsSanePtr(_this)) return 0;
    uintptr_t vecFirst = *(uintptr_t*)((uintptr_t)_this + 0x0C);
    uintptr_t vecLast  = *(uintptr_t*)((uintptr_t)_this + 0x10);
    if (!vecFirst || vecLast <= vecFirst) return 0;

    bool found = false;
    for (uintptr_t p = vecFirst; p + 4 <= vecLast; p += 4) {
        void* root = *(void**)p;
        if (!IsSanePtr(root)) continue;
        IG_ForEachChildByName(root, "COMPONENT_SAVE", [&](void* child) -> bool {
            // Only PC_SUB_LIST (UI_TYPE_LIST = 8) represents the overwrite/confirm sub-screen;
            // empty save slot CMenuEntry (UI_TYPE_MENU_ENTRY = 7) was also named "COMPONENT_SAVE".
            if (FE_GetType(child) == 8u && (IG_IsObservingEvent(child, 1) || IG_IsObservingEvent(child, 2))) {
                found = true;
                return true;
            }
            return false;
        });
        if (found) return 1;
    }
    return 0;
}

void __fastcall Hooked_CInputProcessInventory_ProcessInput(
    void* _this, void* edx, const EngineInputEvent* ev, void* processedInput)
{
    if (g_IsControllerActive && ev && ev->DeviceType == 1) {
        // Priority 1: Active OK / Yes-No popup modals in ConcurrentExclusiveObservers
        void* dlg = nullptr;
        void* btn0 = nullptr;
        void* btn1 = nullptr;
        bool isOkOnly = false;
        if (IG_FindActiveModalButtons(dlg, btn0, btn1, isOkOnly)) {
            if (IG_HandlePairInput(_this, ev, processedInput, dlg, btn0, btn1,
                isOkOnly ? IG_PAIR_MODAL_OK : IG_PAIR_MODAL_YESNO)) {
                return;
            }
        }

        // Priority 2: Overwrite Save / Confirm Save / Confirm Load Apply & Cancel
        void* cPlayer = *(void**)((uintptr_t)_this + 24);
        void* tcInv = IG_GetControlledThingComponent(cPlayer, 17);
        void* subList = nullptr;
        void* applyBtn = nullptr;
        void* cancelBtn = nullptr;
        if (IG_FindActiveSaveLoadButtons(tcInv, subList, applyBtn, cancelBtn)) {
            if (IG_HandlePairInput(_this, ev, processedInput, subList, applyBtn, cancelBtn,
                IG_PAIR_CLICKABLES_WITH_B)) {
                return;
            }
        }

        // Priority 3: Options submenus (Gameplay, Audio, Video sliders/toggles & Redefine Keys + Apply/Cancel/Defaults)
        if (IG_HandleOptionsSubmenuInput(_this, tcInv, ev, processedInput)) {
            return;
        }

        // Priority 4: PC Pause / Save / Load / Options root list (CurrentOpenMenu == 4)
        // Map controller A -> UI_EVENT_SELECT (4) and B -> UI_EVENT_BACK (5); never allow D-Pad Right (40) to Apply (4)
        if (tcInv && *(uint32_t*)((uintptr_t)tcInv + 0x21C) == 4u && !IG_IsExclusivelyObserved() && !IG_IsCapturingRedefineKey()) {
            if (IG_IsEventGameAction(_this, 40, ev)) {
                IG_MarkPerformed(processedInput);
                return;
            }
            bool isSelect = IG_IsEventGameAction(_this, 25, ev) || IG_IsEventGameAction(_this, 33, ev);
            bool isBack   = IG_IsEventGameAction(_this, 34, ev);
            if (isSelect || isBack) {
                if (IG_IsBtnPressed(ev)) {
                    IG_SendUIEvent(isBack ? 5 : 4);
                    IG_MarkPerformed(processedInput);
                    return;
                }
                if (IG_IsBtnHeld(ev) || IG_IsBtnReleased(ev)) {
                    IG_MarkPerformed(processedInput);
                    return;
                }
            }
        }
    }

    o_CInputProcessInventory_ProcessInput(_this, ev, processedInput);
}

void __fastcall Hooked_CInputProcessQuestCompletionUI_ProcessInput(
    void* _this, void* edx, const EngineInputEvent* ev, void* processedInput)
{
    if (g_IsControllerActive && ev && ev->DeviceType == 1) {
        void* cPlayer = *(void**)((uintptr_t)_this + 24);
        void* tcQC = IG_GetControlledThingComponent(cPlayer, 252);
        void* respawnBtn = nullptr;
        void* quitBtn = nullptr;
        if (IG_FindQuestCompletionButtons(tcQC, respawnBtn, quitBtn)) {
            if (IG_HandlePairInput(_this, ev, processedInput, tcQC, respawnBtn, quitBtn,
                IG_PAIR_CLICKABLES_NO_B)) {
                return;
            }
        }
    }

    o_CInputProcessQuestCompletionUI_ProcessInput(_this, ev, processedInput);
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
        if (special) { alpha *= g_CrossfadeAlpha; }
        else { alpha *= (1.0f - g_CrossfadeAlpha); }
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
    // Only swap to the Xbox HUD def while the Xbox draw objects are actually installed in CPlayerGui.
    // Using g_IsControllerActive here made a freshly created CPlayerGui (PC draw objects) load the
    // Xbox def before the first Update could swap the objects -> crash in CDrawQuickAccessIconsPC::Draw.
    if (g_UseXboxGuiDef && str && strcmp(str, "PLAYER_GUI_PC") == 0) {
        return o_CCharString_Construct(_this, "PLAYER_GUI_DEFAULT", -1);
    }
    return o_CCharString_Construct(_this, str, len);
}

// ---- Save-load guard: sub_5F6E40 loops i < GetNumberOfAbilityButtons() over the *saved* "AbilityButton"
// vector without a bounds check. The count depends on the active control scheme (we swap it for
// controllers), so it can exceed the saved vector length -> garbage CCharString -> crash.
static bool  g_AbilityLoadActive = false;
static int   g_SavedAbilityBtnCount = -1;
static bool  g_InventoryLoadActive = false;
static void* g_CurrentTCInventory = nullptr;

typedef int(__thiscall* GetNumAbilityButtons_t)(void* _this);
static GetNumAbilityButtons_t o_GetNumAbilityButtons = nullptr;
typedef void(__thiscall* TransferVectorCharString_t)(void* _this, const char* name, int* vec);
static TransferVectorCharString_t o_TransferVectorCharString = nullptr;
typedef int(__thiscall* TCAbilitySerialise_t)(void* _this, void* a2, int a3);
static TCAbilitySerialise_t o_TCAbilitySerialise = nullptr;
typedef void(__thiscall* CTCInventorySerialise_t)(void* _this, void* a2, int a3);
static CTCInventorySerialise_t o_CTCInventory_Serialise = nullptr;

int __fastcall Hooked_GetNumAbilityButtons(void* _this, void* edx) {
    int real = o_GetNumAbilityButtons(_this);
    if (g_AbilityLoadActive && g_SavedAbilityBtnCount >= 0 && real > g_SavedAbilityBtnCount) {
        LogFmt("GetNumAbilityButtons clamped %d -> %d (saved vector size)", real, g_SavedAbilityBtnCount);
        return g_SavedAbilityBtnCount;
    }
    return real;
}

void __fastcall Hooked_TransferVectorCharString(void* _this, void* edx, const char* name, int* vec) {
    o_TransferVectorCharString(_this, name, vec);
    if (!name || !vec) return;
    if (g_AbilityLoadActive && strcmp(name, "AbilityButton") == 0) {
        g_SavedAbilityBtnCount = (vec[1] - vec[0]) >> 2;
    }
    else if (g_InventoryLoadActive && IsSanePtr(g_CurrentTCInventory) && strcmp(name, "QuickAccessItemNames") == 0) {
        uint32_t savedCount = ((uint32_t)vec[1] > (uint32_t)vec[0]) ? (((uint32_t)vec[1] - (uint32_t)vec[0]) >> 2) : 0;
        int* invVec = (int*)((uintptr_t)g_CurrentTCInventory + 0x148);
        uint32_t curCount = ((uint32_t)invVec[1] > (uint32_t)invVec[0]) ? (((uint32_t)invVec[1] - (uint32_t)invVec[0]) >> 2) : 0;
        if (savedCount > curCount) {
            typedef void(__thiscall* StdVectorLongResize_t)(int* vecPtr, unsigned int newSize, const long* fillVal);
            long fillZero = 0;
            ((StdVectorLongResize_t)Retail::ADDR_STDVECTOR_LONG_RESIZE)(invVec, savedCount, &fillZero);
            LogFmt("CTCInventory::QuickAccessItemNames resized %u -> %u", curCount, savedCount);
        }
    }
}

int __fastcall Hooked_TCAbilitySerialise(void* _this, void* edx, void* a2, int a3) {
    g_AbilityLoadActive = true;
    g_SavedAbilityBtnCount = -1;
    int r = o_TCAbilitySerialise(_this, a2, a3);
    g_AbilityLoadActive = false;
    g_SavedAbilityBtnCount = -1;
    return r;
}

void __fastcall Hooked_CTCInventory_Serialise(void* _this, void* edx, void* a2, int a3) {
    typedef BOOL(__thiscall* IsSaving_t)(void*);
    bool isSaving = IsSanePtr(a2) && (((IsSaving_t)Retail::ADDR_CPERSISTCONTEXT_ISSAVING)(a2) != 0);
    if (!isSaving) {
        g_InventoryLoadActive = true;
        g_CurrentTCInventory = _this;
    }
    o_CTCInventory_Serialise(_this, a2, a3);
    g_InventoryLoadActive = false;
    g_CurrentTCInventory = nullptr;
}

void __fastcall Hooked_SetCameraManagerDef(void* _this, void* edx, void** new_def) {
    if (!new_def || !*new_def) {
        o_SetCameraManagerDef(_this, new_def);
        return;
    }

    void* actual_def = *new_def;
    void* target_def = actual_def;

    if (g_ShadowObjectsInitialized && *(void**)Retail::ADDR_PMAINGAMECOMPONENT != nullptr) {
        if (g_IsControllerActive) {
            if (actual_def == g_CameraDef_PCMain && g_CameraDef_XboxMain) target_def = g_CameraDef_XboxMain;
            else if (actual_def == g_CameraDef_PCCombat && g_CameraDef_XboxCombat) target_def = g_CameraDef_XboxCombat;
        }
        else {
            if (actual_def == g_CameraDef_XboxMain && g_CameraDef_PCMain) target_def = g_CameraDef_PCMain;
            else if (actual_def == g_CameraDef_XboxCombat && g_CameraDef_PCCombat) target_def = g_CameraDef_PCCombat;
        }
    }

    g_CurrentActiveCameraDef = target_def;
    void* override_def[1] = { target_def };
    o_SetCameraManagerDef(_this, override_def);
}

static void ClearPlayerGuiParticles(void* playerGui) {
    if (!IsSanePtr(playerGui)) return;
    typedef int(__thiscall* ClearEmitters_t)(void*);
    struct Entry { ptrdiff_t offset; uintptr_t fnAddr; };
    const Entry entries[] = {
        { Retail::OFFSET_PDRAW_HEALTH,        Retail::ADDR_CLEAR_HEALTH_EMITTERS },
        { Retail::OFFSET_PDRAW_STAMINA,       Retail::ADDR_CLEAR_STAMINA_EMITTERS },
        { Retail::OFFSET_PDRAW_ABILITY_BUTTONS, Retail::ADDR_CLEAR_ABILITY_EMITTERS },
        { Retail::OFFSET_PDRAW_QUICK_ACCESS,  Retail::ADDR_CLEAR_QUICKACCESS_EMITTERS },
        { Retail::OFFSET_PDRAW_MELEE_EFFECTS, Retail::ADDR_CLEAR_MELEE_EMITTERS },
    };
    for (const auto& e : entries) {
        void* sub = *(void**)((uintptr_t)playerGui + e.offset);
        if (IsSanePtr(sub)) {
            ((ClearEmitters_t)e.fnAddr)(sub);
        }
    }
}

// NOTE: intentionally does NOT touch the def refcount. Releasing it freed defs that the engine
// still reads (PGuiDef -> CDrawQuickAccessIconsPC::Draw crash, heap corruption on quit).
// The original behaviour (drop our pointer, leak the reference) is the safe one.
static void ReleaseCountedDef(void** pDefPtr) {
    if (pDefPtr) *pDefPtr = nullptr;
}

static void ReleaseShadowIllustrators(void* playerGui) {
    if (!g_ShadowObjectsInitialized) return;
    if (IsSanePtr(playerGui)) {
        void** currentAbilityPtr = (void**)((uintptr_t)playerGui + Retail::OFFSET_PDRAW_ABILITY_BUTTONS);
        void** currentQuickAccessPtr = (void**)((uintptr_t)playerGui + Retail::OFFSET_PDRAW_QUICK_ACCESS);
        if (*currentAbilityPtr == g_XboxAbilityBuffer && g_PCAbilityButtons) {
            *currentAbilityPtr = g_PCAbilityButtons;
        }
        if (*currentQuickAccessPtr == g_XboxQuickAccessBuffer && g_PCQuickAccessIcons) {
            *currentQuickAccessPtr = g_PCQuickAccessIcons;
        }
    }
    // Do not call the draw objects' virtual destructors here: they touch player/TC state and
    // the static buffers are never freed by the engine anyway.
}

static void ResetInGameSessionState(const char* reason, void* playerGui) {
    LogFmt("ResetInGameSessionState (%s) gui=%p lastGui=%p init=%d",
        reason, playerGui, g_LastPlayerGuiInstance, g_ShadowObjectsInitialized ? 1 : 0);

    ReleaseShadowIllustrators(playerGui);

    ReleaseCountedDef(&g_CameraDef_XboxMain);
    ReleaseCountedDef(&g_CameraDef_XboxCombat);
    ReleaseCountedDef(&g_CameraDef_PCMain);
    ReleaseCountedDef(&g_CameraDef_PCCombat);
    ReleaseCountedDef(&g_PCControlDef);
    ReleaseCountedDef(&g_XboxControlDef);

    g_pPlayerGui = nullptr;
    g_LastPlayerGuiInstance = nullptr;
    g_ShadowObjectsInitialized = false;
    g_UseXboxGuiDef = false;
    g_PCAbilityButtons = nullptr;
    g_PCQuickAccessIcons = nullptr;
    g_CurrentActiveCameraDef = nullptr;
    g_LastControllerState = false;
    g_CrossfadeAlpha = 0.0f;
    g_LastFrameTime = 0.0;
    g_InGamePairSelection.clear();
    g_InGameStickLeftHeld = false;
    g_InGameStickRightHeld = false;
    g_InGameOptionsFocus.clear();
    g_InGameOptStickXDir = 0;
    g_InGameOptStickYDir = 0;
    g_InGameOptSliderRepeatTick = 0;
    g_InGameOptScrollRepeatTick = 0;
    g_InGameOptLastSelChild = -1;
    g_InGameOptRowChangeTick = 0;
    g_ApplyingPCCleanup = false;
    memset(g_XboxAbilityBuffer, 0, sizeof(g_XboxAbilityBuffer));
    memset(g_XboxQuickAccessBuffer, 0, sizeof(g_XboxQuickAccessBuffer));
}

int __fastcall Hooked_CPlayerGui_Dtor(void* _this, void* edx) {
    ResetInGameSessionState("CPlayerGui_Dtor", _this);
    return o_CPlayerGui_Dtor(_this);
}

int __fastcall Hooked_CMainGameComponent_Shutdown(void* _this, void* edx) {
    ResetInGameSessionState("CMainGameComponent_Shutdown", g_pPlayerGui);
    int r = o_CMainGameComponent_Shutdown(_this);
    Log("CMainGameComponent_Shutdown: original returned");
    return r;
}

void __fastcall Hooked_CGamePlayerInterface_SetControlScheme(void* _this, void* edx, uint32_t scheme) {
    if (g_IsControllerActive && g_XboxControlDef && !g_ApplyingPCCleanup) {
        void* pProfileManager = ((GetUserProfileManager_t)Retail::ADDR_USER_PROFILE_GET)();
        if (pProfileManager) {
            void* xboxVector = (void*)((uintptr_t)g_XboxControlDef + 0x3C);
            void* passiveVector = (void*)((uintptr_t)pProfileManager + 0x54);
            void* aggressiveVector = (void*)((uintptr_t)pProfileManager + 0x64);
            uint8_t savedPassive[16];
            uint8_t savedAggressive[16];
            memcpy(savedPassive, passiveVector, 16);
            memcpy(savedAggressive, aggressiveVector, 16);
            memcpy(passiveVector, xboxVector, 16);
            memcpy(aggressiveVector, xboxVector, 16);
            o_SetControlScheme(_this, scheme);
            memcpy(passiveVector, savedPassive, 16);
            memcpy(aggressiveVector, savedAggressive, 16);
            return;
        }
    }
    o_SetControlScheme(_this, scheme);
}

void __fastcall Hooked_CPlayerGui_Update(void* _this, void* edx, bool paused) {

    g_pPlayerGui = _this;

    if (g_LastPlayerGuiInstance != _this) {
        if (g_LastPlayerGuiInstance != nullptr || g_ShadowObjectsInitialized) {
            ResetInGameSessionState("NewPlayerGuiInstance", nullptr);
        }
        g_pPlayerGui = _this;
        g_LastPlayerGuiInstance = _this;
        LogFmt("New CPlayerGui session gui=%p ctrl=%d", _this, g_IsControllerActive ? 1 : 0);
    }

    if (!g_ShadowObjectsInitialized) {
        void* illustrators = (void*)((uintptr_t)_this + Retail::OFFSET_ILLUSTRATORS);
        memset(g_XboxAbilityBuffer, 0, sizeof(g_XboxAbilityBuffer));
        memset(g_XboxQuickAccessBuffer, 0, sizeof(g_XboxQuickAccessBuffer));

        bool tempState = g_IsControllerActive;
        g_IsControllerActive = true;
        g_UseXboxGuiDef = true; // construct the Xbox draw objects against PLAYER_GUI_DEFAULT

        void** pGlobalGuiDef = (void**)Retail::ADDR_PGUIDEF_GLOBAL;
        void* cachedDef = *pGlobalGuiDef;
        *pGlobalGuiDef = nullptr;

        ((ConstructAbility_t)Retail::ADDR_CONSTRUCT_XBOX_ABILITY)(g_XboxAbilityBuffer, _this, illustrators);
        ((ConstructQuick_t)Retail::ADDR_CONSTRUCT_XBOX_QUICKACCESS)(g_XboxQuickAccessBuffer, _this, illustrators);

        g_IsControllerActive = tempState;
        g_UseXboxGuiDef = false;
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
        g_UseXboxGuiDef = requestedState; // PGuiDef is reloaded lazily after the next line nulls it

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

        ClearPlayerGuiParticles(_this);

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
                    g_ApplyingPCCleanup = true;
                    ((ApplyValues_t)Retail::ADDR_APPLY_VALUES)(pProfileManager);
                    g_ApplyingPCCleanup = false;
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
    IG_SyncControllerFocus();
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
        bool isXInputConnected = PollXInputActivity(&xState);

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

            g_CurrentDPadMask = ExtractXInputDPadMask(xState.Gamepad.wButtons);
            g_DPadFromXInput = (g_CurrentDPadMask != 0);

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

        g_DPadFromXInput = false;
        TrackStadiaPOV(js->rgdwPOV[0]);
        for (int i = 0; i < 32; i++) {
            if (js->rgbButtons[i] & 0x80) {
                g_IsControllerActive = true;
                SetCursorHidden(true, "GetDevState_JoyBtn");
            }
        }
        for (int i = 0; i < 4; i++) {
            if (js->rgdwPOV[i] != -1 && js->rgdwPOV[i] != 65535) {
                g_IsControllerActive = true;
                SetCursorHidden(true, "GetDevState_JoyPOV");
            }
        }
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
        if (significantInput) {
            SetCursorHidden(false, "GetDevData_Mouse");
            if (g_IsControllerActive) {
                g_IsControllerActive = false;
                g_ActiveXInputSlot = (DWORD)-1;
            }
        }
        return hr;
    }

    if (devType == DI8DEVTYPE_KEYBOARD) {
        bool keyPressed = false;
        for (DWORD i = 0; i < *pdwInOut; i++) {
            if ((rgdod[i].dwData & 0x80) != 0) keyPressed = true;
        }
        if (keyPressed) {
            SetCursorHidden(false, "GetDevData_Kbd");
            if (g_IsControllerActive) {
                g_IsControllerActive = false;
                g_ActiveXInputSlot = (DWORD)-1;
            }
        }
        return hr;
    }

    XINPUT_STATE xState;
    bool isXInputConnected = PollXInputActivity(&xState);

    if (isXInputConnected && IsXInputActive(xState) && !g_IsControllerActive) {
        g_IsControllerActive = true;
    }

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
                    rgdod[i].dwOfs = 0xFFFFFFFF;
                }
            }
            else if (originalOffset >= 0x30 && originalOffset <= 0x3F) {
                if (originalData & 0x80) {
                    g_IsControllerActive = true;
                    SetCursorHidden(true, "GetDevData_XInputBtn");
                }
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
                g_DPadFromXInput = false;
                if (LOWORD(originalData) != 0xFFFF) {
                    g_IsControllerActive = true;
                    SetCursorHidden(true, "GetDevData_XInputPOV");
                }
                TrackStadiaPOV(originalData);
            }
        }
        else {
            if (originalOffset >= 0x30 && originalOffset <= 0x3F) {
                if (originalData & 0x80) {
                    g_IsControllerActive = true;
                    SetCursorHidden(true, "GetDevData_DInputBtn");
                }
                rgdod[i].dwOfs = TranslateStadiaButtonOffset(originalOffset);
            }
            else if (originalOffset == DIJOFS_POV(0)) {
                g_DPadFromXInput = false;
                if (LOWORD(originalData) != 0xFFFF) {
                    g_IsControllerActive = true;
                    SetCursorHidden(true, "GetDevData_DInputPOV");
                }
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
            //Log("Successfully hooked IDirectInputDevice8::GetDeviceState");
        }
        if (MH_CreateHook(vTable[10], &HookedGetDeviceData, (LPVOID*)&oGetDeviceData) == MH_OK) {
            MH_EnableHook(vTable[10]);
            //Log("Successfully hooked IDirectInputDevice8::GetDeviceData");
        }
        if (MH_CreateHook(vTable[13], &HookedSetCooperativeLevel, (LPVOID*)&oSetCooperativeLevel) == MH_OK) {
            MH_EnableHook(vTable[13]);
            //Log("Successfully hooked IDirectInputDevice8::SetCooperativeLevel (Alt-Tab Fix)");
        }
        pDevice->Release();
    }
    else {
        //Log("CRITICAL: Failed to create dummy DirectInput device. Hooks aborted.");
    }
    pDI->Release();
}

void InjectGUIHotswapHooks() {
    if (MH_CreateHook((LPVOID)Retail::ADDR_CINPUTMANAGER_UPDATE, &Hooked_CInputManager_Update, (LPVOID*)&o_CInputManager_Update) == MH_OK) {
        MH_EnableHook((LPVOID)Retail::ADDR_CINPUTMANAGER_UPDATE);
    }
    if (MH_CreateHook((LPVOID)Retail::ADDR_GFPLAY_FULLSCREEN_XBOX_MOVIE, &Hooked_GFPlayFullScreenXBoxMovie, (LPVOID*)&o_GFPlayFullScreenXBoxMovie) == MH_OK) {
        MH_EnableHook((LPVOID)Retail::ADDR_GFPLAY_FULLSCREEN_XBOX_MOVIE);
    }

    // Frontend Controller Support Hooks
    if (MH_CreateHook((LPVOID)Retail::ADDR_CNEWFRONTEND_INPUT, &Hooked_CNewFrontend_Input, (LPVOID*)&o_CNewFrontend_Input) == MH_OK) {
        MH_EnableHook((LPVOID)Retail::ADDR_CNEWFRONTEND_INPUT);
    }
    if (MH_CreateHook((LPVOID)Retail::ADDR_CFRONTBUTTON_PROCESSEVENT, &Hooked_CFrontEndButton_ProcessEvent, (LPVOID*)&o_CFrontEndButton_ProcessEvent) == MH_OK) {
        MH_EnableHook((LPVOID)Retail::ADDR_CFRONTBUTTON_PROCESSEVENT);
    }
    if (MH_CreateHook((LPVOID)Retail::ADDR_CFRONTSCREEN_PROCESSEVENT, &Hooked_CFrontEndScreen_ProcessEvent, (LPVOID*)&o_CFrontEndScreen_ProcessEvent) == MH_OK) {
        MH_EnableHook((LPVOID)Retail::ADDR_CFRONTSCREEN_PROCESSEVENT);
    }
    if (MH_CreateHook((LPVOID)Retail::ADDR_CSCROLLABLELIST_PROCESSEVENT, &Hooked_CScrollableList_ProcessEvent, (LPVOID*)&o_CScrollableList_ProcessEvent) == MH_OK) {
        MH_EnableHook((LPVOID)Retail::ADDR_CSCROLLABLELIST_PROCESSEVENT);
    }
    if (MH_CreateHook((LPVOID)Retail::ADDR_CSCROLLABLELIST_ONACTIVATE, &Hooked_CScrollableList_OnActivate, (LPVOID*)&o_CScrollableList_OnActivate) == MH_OK) {
        MH_EnableHook((LPVOID)Retail::ADDR_CSCROLLABLELIST_ONACTIVATE);
    }
    if (MH_CreateHook((LPVOID)Retail::ADDR_CSCROLLABLELIST_ONDEACTIVATE, &Hooked_CScrollableList_OnDeactivate, (LPVOID*)&o_CScrollableList_OnDeactivate) == MH_OK) {
        MH_EnableHook((LPVOID)Retail::ADDR_CSCROLLABLELIST_ONDEACTIVATE);
    }
    if (MH_CreateHook((LPVOID)Retail::ADDR_CFRONTLIST_UPDATE, &Hooked_CFrontEndList_Update, (LPVOID*)&o_CFrontEndList_Update) == MH_OK) {
        MH_EnableHook((LPVOID)Retail::ADDR_CFRONTLIST_UPDATE);
    }
    if (MH_CreateHook((LPVOID)Retail::ADDR_CFRONTLIST_CHANGESTATE, &Hooked_CFrontEndList_ChangeState, (LPVOID*)&o_CFrontEndList_ChangeState) == MH_OK) {
        MH_EnableHook((LPVOID)Retail::ADDR_CFRONTLIST_CHANGESTATE);
    }
    if (MH_CreateHook((LPVOID)Retail::ADDR_CSLIDER_CHANGESTATE, &Hooked_CSlider_ChangeState, (LPVOID*)&o_CSlider_ChangeState) == MH_OK) {
        MH_EnableHook((LPVOID)Retail::ADDR_CSLIDER_CHANGESTATE);
    }
    if (MH_CreateHook((LPVOID)Retail::ADDR_CSLIDER_PROCESSEVENT, &Hooked_CSlider_ProcessEvent, (LPVOID*)&o_CSlider_ProcessEvent) == MH_OK) {
        MH_EnableHook((LPVOID)Retail::ADDR_CSLIDER_PROCESSEVENT);
    }
    if (MH_CreateHook((LPVOID)Retail::ADDR_CTEXTSLIDER_CHANGESTATE, &Hooked_CTextSlider_ChangeState, (LPVOID*)&o_CTextSlider_ChangeState) == MH_OK) {
        MH_EnableHook((LPVOID)Retail::ADDR_CTEXTSLIDER_CHANGESTATE);
    }
    if (MH_CreateHook((LPVOID)Retail::ADDR_CTEXTSLIDER_PROCESSEVENT, &Hooked_CTextSlider_ProcessEvent, (LPVOID*)&o_CTextSlider_ProcessEvent) == MH_OK) {
        MH_EnableHook((LPVOID)Retail::ADDR_CTEXTSLIDER_PROCESSEVENT);
    }
    // Patch CFrontEndList::ProcessEvent vtable slot (3-byte ret 4 stub in .text)
    DWORD oldProt = 0;
    void** pFrontEndListSlot = (void**)Retail::ADDR_CFRONTLIST_PROCESSEVENT_VFT_SLOT;
    if (VirtualProtect(pFrontEndListSlot, sizeof(void*), PAGE_READWRITE, &oldProt)) {
        *pFrontEndListSlot = (void*)&Hooked_CFrontEndList_ProcessEvent;
        VirtualProtect(pFrontEndListSlot, sizeof(void*), oldProt, &oldProt);
    }
    // Hide frontend CMouseCursor while controller is in use (also gate IME cursor + in-game CDrawMouseCursor)
    void** pMouseCursorDrawSlot = (void**)Retail::ADDR_CMOUSECURSOR_DRAW_VFT_SLOT;
    if (VirtualProtect(pMouseCursorDrawSlot, sizeof(void*), PAGE_READWRITE, &oldProt)) {
        o_CMouseCursor_Draw = (CSprite_Draw_t)*pMouseCursorDrawSlot;
        *pMouseCursorDrawSlot = (void*)&Hooked_CMouseCursor_Draw;
        VirtualProtect(pMouseCursorDrawSlot, sizeof(void*), oldProt, &oldProt);
    }
    if (MH_CreateHook((LPVOID)Retail::ADDR_CSPRITE_DRAWWITHRM, &Hooked_CSprite_DrawWithRM, (LPVOID*)&o_CSprite_DrawWithRM) == MH_OK) {
        MH_EnableHook((LPVOID)Retail::ADDR_CSPRITE_DRAWWITHRM);
    }
    if (MH_CreateHook((LPVOID)Retail::ADDR_CPLAYERGUI_UPDATE, &Hooked_CPlayerGui_Update, (LPVOID*)&o_CPlayerGui_Update) == MH_OK) {
        MH_EnableHook((LPVOID)Retail::ADDR_CPLAYERGUI_UPDATE);
    }
    if (MH_CreateHook((LPVOID)Retail::ADDR_CPLAYERGUI_DTOR, &Hooked_CPlayerGui_Dtor, (LPVOID*)&o_CPlayerGui_Dtor) == MH_OK) {
        MH_EnableHook((LPVOID)Retail::ADDR_CPLAYERGUI_DTOR);
    }
    if (MH_CreateHook((LPVOID)Retail::ADDR_GET_NUM_ABILITY_BUTTONS, &Hooked_GetNumAbilityButtons, (LPVOID*)&o_GetNumAbilityButtons) == MH_OK) {
        MH_EnableHook((LPVOID)Retail::ADDR_GET_NUM_ABILITY_BUTTONS);
    }
    if (MH_CreateHook((LPVOID)Retail::ADDR_TRANSFERVECTOR_CCHARSTRING, &Hooked_TransferVectorCharString, (LPVOID*)&o_TransferVectorCharString) == MH_OK) {
        MH_EnableHook((LPVOID)Retail::ADDR_TRANSFERVECTOR_CCHARSTRING);
    }
    if (MH_CreateHook((LPVOID)Retail::ADDR_TC_ABILITY_SERIALISE, &Hooked_TCAbilitySerialise, (LPVOID*)&o_TCAbilitySerialise) == MH_OK) {
        MH_EnableHook((LPVOID)Retail::ADDR_TC_ABILITY_SERIALISE);
    }
    if (MH_CreateHook((LPVOID)Retail::ADDR_CTCINVENTORY_SERIALISE, &Hooked_CTCInventory_Serialise, (LPVOID*)&o_CTCInventory_Serialise) == MH_OK) {
        MH_EnableHook((LPVOID)Retail::ADDR_CTCINVENTORY_SERIALISE);
    }
    if (MH_CreateHook((LPVOID)Retail::ADDR_CMAINGAMECOMPONENT_SHUTDOWN, &Hooked_CMainGameComponent_Shutdown, (LPVOID*)&o_CMainGameComponent_Shutdown) == MH_OK) {
        MH_EnableHook((LPVOID)Retail::ADDR_CMAINGAMECOMPONENT_SHUTDOWN);
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
            //Log("Boast UI Input Hook Deployed.");
        }
    }

    // In-Game UI Navigation Hooks (Save/Load, Modals, Death/Quest Fail, Options Submenus)
    if (MH_CreateHook((LPVOID)Retail::ADDR_SET_CONTROL_SCHEME, &Hooked_CGamePlayerInterface_SetControlScheme, (LPVOID*)&o_SetControlScheme) == MH_OK) {
        MH_EnableHook((LPVOID)Retail::ADDR_SET_CONTROL_SCHEME);
    }
    if (MH_CreateHook((LPVOID)Retail::ADDR_CTCINVENTORY_ISSAVEMENURECEIVINGINPUT, &Hooked_CTCInventory_IsSaveMenuReceivingInput, (LPVOID*)&o_CTCInventory_IsSaveMenuReceivingInput) == MH_OK) {
        MH_EnableHook((LPVOID)Retail::ADDR_CTCINVENTORY_ISSAVEMENURECEIVINGINPUT);
    }
    if (MH_CreateHook((LPVOID)Retail::ADDR_CINPUTPROCESS_INVENTORY_PROCESSINPUT, &Hooked_CInputProcessInventory_ProcessInput, (LPVOID*)&o_CInputProcessInventory_ProcessInput) == MH_OK) {
        MH_EnableHook((LPVOID)Retail::ADDR_CINPUTPROCESS_INVENTORY_PROCESSINPUT);
    }
    if (MH_CreateHook((LPVOID)Retail::ADDR_CINPUTPROCESS_QUESTCOMPLETION_PROCESSINPUT, &Hooked_CInputProcessQuestCompletionUI_ProcessInput, (LPVOID*)&o_CInputProcessQuestCompletionUI_ProcessInput) == MH_OK) {
        MH_EnableHook((LPVOID)Retail::ADDR_CINPUTPROCESS_QUESTCOMPLETION_PROCESSINPUT);
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

// Crash diagnostics: logs the faulting EIP, registers and a heuristic call stack for fatal exceptions.
static LONG WINAPI CrashLogger(EXCEPTION_POINTERS* ep) {
    DWORD code = ep->ExceptionRecord->ExceptionCode;
    if (code != EXCEPTION_ACCESS_VIOLATION && code != 0xC0000409 && code != EXCEPTION_ILLEGAL_INSTRUCTION &&
        code != EXCEPTION_INT_DIVIDE_BY_ZERO && code != EXCEPTION_STACK_OVERFLOW)
        return EXCEPTION_CONTINUE_SEARCH;
    static LONG s_Count = 0;
    if (InterlockedIncrement(&s_Count) > 3) return EXCEPTION_CONTINUE_SEARCH;
    CONTEXT* c = ep->ContextRecord;
    LogFmt("!!! CRASH code=%08X eip=%08X (file+%08X) accessAddr=%08X",
        code, (DWORD)(uintptr_t)ep->ExceptionRecord->ExceptionAddress,
        (DWORD)((uintptr_t)ep->ExceptionRecord->ExceptionAddress - 0x400000),
        ep->ExceptionRecord->NumberParameters > 1 ? (DWORD)ep->ExceptionRecord->ExceptionInformation[1] : 0);
    LogFmt("!!! eax=%08X ebx=%08X ecx=%08X edx=%08X esi=%08X edi=%08X ebp=%08X esp=%08X",
        c->Eax, c->Ebx, c->Ecx, c->Edx, c->Esi, c->Edi, c->Ebp, c->Esp);
    char line[512]; int n = 0; line[0] = 0;
    DWORD* sp = (DWORD*)c->Esp;
    for (int i = 0; i < 512 && n < 24; ++i) {
        if (IsBadReadPtr(sp + i, 4)) break;
        DWORD v = sp[i];
        if (v > 0x401000 && v < 0x1400000 && !IsBadReadPtr((void*)(v - 5), 5)) {
            BYTE b = *(BYTE*)(v - 5);
            BYTE b2 = *(BYTE*)(v - 2);
            if (b == 0xE8 || b2 == 0xFF || *(BYTE*)(v - 3) == 0xFF || *(BYTE*)(v - 6) == 0xFF) {
                n++;
                char tmp[32]; snprintf(tmp, sizeof(tmp), "%08X ", v);
                if (strlen(line) + strlen(tmp) < sizeof(line) - 1) strcat_s(line, tmp);
                if (n % 8 == 0) { LogFmt("!!! stack: %s", line); line[0] = 0; }
            }
        }
    }
    if (line[0]) LogFmt("!!! stack: %s", line);
    return EXCEPTION_CONTINUE_SEARCH;
}

static DWORD WINAPI MainThread(LPVOID) {
    if (ENABLE_LOGGING) {
        g_Log.open("FableControllerSupport.log", std::ios::out | std::ios::trunc);
        Log("FableControllerSupport Initializing...");
        AddVectoredExceptionHandler(1, CrashLogger);
    }

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