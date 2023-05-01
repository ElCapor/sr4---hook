#include "MinHook.h"
#include <Windows.h>
#include <iostream>
#include <sstream>
#pragma comment(lib, "minhook.lib")
extern "C"
{
#include "lua/lua.h"
#include "lua/lapi.h"
#include "lua/lauxlib.h"
#include "lua/lfunc.h"
}

#include <debugapi.h>

HINSTANCE DllHandle;

typedef __int64(__fastcall* luaL_loadbuffer_def)(__int64 a1, const char* buffer, size_t size, const char* name);
luaL_loadbuffer_def luaL_loadbufferOriginal = nullptr;
luaL_loadbuffer_def luaL_loadbufferTarget = reinterpret_cast<luaL_loadbuffer_def>(0x7FF75F9026A0); // function pointer before hook

typedef __int64(__fastcall* lua_pushcclosure_def)(__int64 L, lua_CFunction function, __int64 n);//https://www.lua.org/source/5.1/lapi.c.html#lua_pushcclosure
lua_pushcclosure_def lua_pushcclosure_original = nullptr;
lua_pushcclosure_def lua_pushcclosure_detour = reinterpret_cast<lua_pushcclosure_def>(0x7FF75F900A00); // function pointer before hook

typedef __int64(__fastcall* lua_setfield_def)(__int64 L, __int64 idx, const char* k);//https://www.lua.org/source/5.1/lapi.c.html#lua_pushcclosure
lua_setfield_def lua_setfield_original = nullptr;
lua_setfield_def lua_setfield_detour = reinterpret_cast<lua_setfield_def>(0x7FF75F901130); // function pointer before hook


bool isLoadedPayload = false;
bool canStartPayload = false;
bool shouldStop = false;
bool isInit = false;
lua_State* game_state;

int __cdecl debug_print(lua_State* lua_state)
{
    int v1; // esi
    const char* v2; // ebx
    const char* v3; // esi

    v1 = lua_gettop(lua_state);
    v2 = lua_tolstring(lua_state, -v1, 0);
    v3 = lua_tolstring(lua_state, 1 - v1, 0);
    std::cout << v3 << " " << v2 << std::endl;
    return 0;
}

__int64 loadbuffer(lua_State* L, const char* buffer, size_t size, const char* name)
{
    if (!isInit)
    {
        game_state = L;
        lua_pushcfunction(L, &debug_print);
        lua_setglobal(L, "debug_print");
        luaL_dostring(L, "debug_print('hi', 'test')");
        isInit = true;
    }
    //MH_DisableHook(reinterpret_cast<void**>(luaL_loadbufferTarget));
    
    //std::cout << "--------------[ST4 - LUA LOGGER - mogus#2891]-------------------" << std::endl;
    //std::cout << "Attempt to run code ! " << name << "State : " << std::hex << a1 << std::endl;
    //std::cout << buffer << std::endl;
    //std::cout << "---------------------[END -  " << name << " - State : " << std::hex << a1  << " ]--------------------------------------" << std::endl;
    __int64 result = luaL_loadbuffer(L, buffer, size, name);
    //MH_EnableHook(reinterpret_cast<void**>(luaL_loadbufferTarget));
    return result;
}
int i = 0;
__int64 setfield_detour(lua_State* L, int idx, const char* k)
{
    //MH_DisableHook(reinterpret_cast<void**>(lua_setfield_detour));
    //std::cout << "---------[ST4 - lua_setfield - START]-----------" << std::endl;
    i++;
    //std::cout << "lua_setfield(" << std::hex << L << "," << idx << "," << k << ")" << std::endl;
    //std::cout << "---------[ST4 - lua_setfield - END]-----------" << std::endl;
    lua_setfield(L, idx, k);
    //MH_EnableHook(reinterpret_cast<void**>(lua_setfield_detour));
    return 0;
}

__int64 pushcclosure_detour(lua_State* L, lua_CFunction fn, __int64 n)
{
    //MH_DisableHook(reinterpret_cast<void**>(lua_pushcclosure_detour));
    //std::cout << "---------[ST4 - lua_pushcclosure - START]-----------" << std::endl;
    //std::cout << "lua_pushcclosure(" << std::hex << L << "," << fn << "," << n << ")" << std::endl;
    //std::cout << "---------[ST4 - lua_pushcclosure - END]-----------" << std::endl;
    
    lua_pushcclosure(L, fn, n); //lua_pushcclosure_original(L, fn, n);
    //MH_EnableHook(reinterpret_cast<void**>(lua_pushcclosure_detour));
    return 0;
}

DWORD __stdcall EjectThread(LPVOID lpParameter) {
    Sleep(100);
    FreeLibraryAndExitThread(DllHandle, 0);
    return 0;
}

void shutdown(FILE* fp, std::string reason) {

    MH_Uninitialize();
    std::cout << reason << std::endl;
    Sleep(1000);
    if (fp != nullptr)
        fclose(fp);
    FreeConsole();
    CreateThread(0, 0, EjectThread, 0, 0, 0);
    return;
}

void OutputDebugStringADetour(LPCSTR lpOutputString)
{
    std::cout << lpOutputString << std::endl;
}



DWORD WINAPI Menue(HINSTANCE hModule) {
    AllocConsole();
    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout); //sets cout to be used with our newly created console
    freopen_s(&fp, "CONIN$", "r", stdin); //sets cout to be used with our newly created console


    MH_STATUS status = MH_Initialize();
    if (status != MH_OK)
    {
        std::string sStatus = MH_StatusToString(status);
        shutdown(fp, "Minhook init failed!");
        return 0;
    }
    
    std::cout << std::hex << (uintptr_t)GetModuleHandle(0) << std::endl;
    if (MH_CreateHook(reinterpret_cast<void**>(luaL_loadbufferTarget), &loadbuffer, reinterpret_cast<void**>(&luaL_loadbufferOriginal)) != MH_OK) {
        shutdown(fp, "CreateHook failed!");
        return 1;
    }

    if (MH_EnableHook(reinterpret_cast<void**>(luaL_loadbufferTarget)) != MH_OK) {
        shutdown(fp, "Reload: EnableHook failed!");
        return 1;
    }
    
    if (MH_CreateHook(reinterpret_cast<void**>(lua_pushcclosure_detour), &pushcclosure_detour, reinterpret_cast<void**>(&lua_pushcclosure_original)) != MH_OK) {
        shutdown(fp, "CreateHook failed!");
        return 1;
    }

    if (MH_EnableHook(reinterpret_cast<void**>(lua_pushcclosure_detour)) != MH_OK) {
        shutdown(fp, "Reload: EnableHook failed!");
        return 1;
    }

    if (MH_CreateHook(reinterpret_cast<void**>(lua_setfield_detour), &setfield_detour, reinterpret_cast<void**>(&lua_setfield_original)) != MH_OK) {
        shutdown(fp, "CreateHook failed!");
        return 1;
    }

    if (MH_EnableHook(reinterpret_cast<void**>(lua_setfield_detour)) != MH_OK) {
        shutdown(fp, "Reload: EnableHook failed!");
        return 1;
    }

    if (MH_CreateHook(reinterpret_cast<void**>(OutputDebugStringA), &OutputDebugStringADetour, reinterpret_cast<void**>(0)) != MH_OK) {
        shutdown(fp, "CreateHook failed!");
        return 1;
    }

    if (MH_EnableHook(reinterpret_cast<void**>(OutputDebugStringA)) != MH_OK) {
        shutdown(fp, "Reload: EnableHook failed!");
        return 1;
    }
    std::cin.ignore();
    std::cout << std::hex << game_state << std::endl;
    
    std::cin.ignore();
    
    std::cin.ignore();

    shutdown(fp, "Byby");
    return 0;

}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        DllHandle = hModule;
        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)Menue, NULL, 0, NULL);
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
