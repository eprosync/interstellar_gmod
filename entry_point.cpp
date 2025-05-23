#define NOMINMAX
#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__)
#include <dlfcn.h>
#include <sys/mman.h>
#endif

#include <unordered_set>
#include <iostream>
#include "gmod/Interface.h"
#include "gmod/LuaShared.h"
#include "gmod/LuaInterface.h"
#include "interstellar/interstellar.hpp"
#include "interstellar/interstellar_fs.hpp"
#include "interstellar/interstellar_memory.hpp"
#include "interstellar/interstellar_lxz.hpp"
#include "interstellar/interstellar_iot.hpp"
#include "interstellar/interstellar_sodium.hpp"

// This is just to grab the interfaces from the game engine
namespace Interface {
    typedef void* (*CreateInterface_fn)(const char* name, int* returncode);

    void** VTable(void* instance)
    {
        return *reinterpret_cast<void***>(instance);
    }

    template<typename T>
    T Interface(std::string module, std::string name)
    {
        #if defined(_WIN32)
            std::wstring stemp = std::wstring(module.begin(), module.end());
            HMODULE handle = GetModuleHandle(stemp.c_str());

            if (!handle)
                return nullptr;

            static CreateInterface_fn CreateInterface = (CreateInterface_fn)GetProcAddress(handle, "CreateInterface");

            if (!CreateInterface)
                return nullptr;

            return (T)CreateInterface(name.c_str(), 0);
        #elif defined(__linux)
            void* handle = dlopen(module.c_str(), RTLD_LAZY);
            if (!handle) {
                return nullptr;
            }

            CreateInterface_fn CreateInterface = (CreateInterface_fn)dlsym(handle, "CreateInterface");
            if (!CreateInterface) {
                dlclose(handle);
                return nullptr;
            }

            T result = (T)CreateInterface(name.c_str(), 0);

            dlclose(handle);
            return result;
        #endif
    }
}

using namespace Interstellar;
using namespace Interstellar::API;

// These are overrides so we can keep track of lua_State creation.
// Usually only needed for CLIENT realm.
typedef GarrysMod::Lua::ILuaInterface CLuaInterface;
typedef GarrysMod::Lua::ILuaShared CLuaShared;
typedef GarrysMod::Lua::lua_State GState;
CLuaShared* current_shared;
CLuaInterface* current_interface;

#ifdef __linux
typedef CLuaInterface* (*CreateLuaInterface_fn)(CLuaShared* self, unsigned char type, bool renew);
CreateLuaInterface_fn CreateLuaInterface_o;
CLuaInterface* CreateLuaInterface_h(CLuaShared* self, unsigned char type, bool renew)
#else
typedef CLuaInterface* (__thiscall* CreateLuaInterface_fn)(CLuaShared* self, unsigned char type, bool renew);
CreateLuaInterface_fn CreateLuaInterface_o;
CLuaInterface* __fastcall CreateLuaInterface_h(CLuaShared* self, unsigned char type, bool renew)
#endif
{
    CLuaInterface* state = CreateLuaInterface_o(self, type, renew);
    API::lua_State* L = (API::lua_State*)state->GetState();

    switch (type)
    {
        case GarrysMod::Lua::State::CLIENT:
            Tracker::listen(L, "client", true);
            break;
        case GarrysMod::Lua::State::SERVER:
            Tracker::listen(L, "server", true);
            break;
        case GarrysMod::Lua::State::MENU:
            Tracker::listen(L, "menu", true);
            break;
    };

    // Do note:
    // At this stage lua hasn't correctly initialized (missing some openlibs and stuff)
    // So you can't just grab things like the global table just yet, it isn't ready till luaopen_base is called.
    // If you need the API in the client-state, but you are in menu-state, just do L:api() L:pop(), this will create everything.
    // Be warned, exposing API's onto a state thats not entirely under your control leaves you open to high security risks.

    return state;
}

#ifdef __linux
typedef void (*CloseLuaInterface_fn)(CLuaShared* self, CLuaInterface* state);
CloseLuaInterface_fn CloseLuaInterface_o;
void CloseLuaInterface_h(CLuaShared* self, CLuaInterface* state)
#else
typedef void (__thiscall* CloseLuaInterface_fn)(CLuaShared* self, CLuaInterface* state);
CloseLuaInterface_fn CloseLuaInterface_o;
void __fastcall CloseLuaInterface_h(CLuaShared* self, CLuaInterface* state)
#endif
{
    if (current_interface == state)
        return CloseLuaInterface_o(self, state);
    API::lua_State* L = (API::lua_State*)state->GetState();
    Tracker::pre_remove(L);
    CloseLuaInterface_o(self, state);
    Tracker::post_remove(L);
}

int runtime_async(API::lua_State* L) {
    IOT::runtime();
    LXZ::runtime();
    FS::runtime();
    return 0;
}

#ifdef __linux
    #if defined(__x86_64__) || defined(_M_X64)
        #define BINARY "bin/linux64/lua_shared.so"
    #elif defined(__i386__) || defined(_M_IX86)
        #define BINARY "bin/linux32/lua_shared.so"
        #define BINARY2 "garrysmod/bin/lua_shared_srv.so"
    #endif
#else
    #if defined(__x86_64__) || defined(_M_X64)
        #define BINARY "bin/win64/lua_shared.dll"
    #elif defined(__i386__) || defined(_M_IX86)
        #define BINARY "bin/lua_shared.dll"
        #define BINARY2 "garrysmod/bin/lua_shared.dll"
    #endif
#endif

bool opened = false;
int module_open() {
    if (opened) return 1;
    opened = true;

    std::cout << "Interstellar - ";

    #if defined(_WIN32)
        std::cout << "Windows ";
    #elif defined(__linux__)
        std::cout << "Linux ";
    #endif

    #if defined(__x86_64__) || defined(_M_X64)
        std::cout << "x64 ";
    #elif defined(__i386__) || defined(_M_IX86)
        std::cout << "x86 ";
    #endif

    #ifdef GMSV
        std::cout << "Server/Menu";
    #elif GMCL
        std::cout << "Client";
    #endif

    std::cout << " - " __TIME__ " " __DATE__;
    std::cout << std::endl;
    std::cout << "An interstellar gateway to enhancing the 'RCE' & 'ACE' experience." << std::endl;

    #ifdef BINARY2
    const char* binary = BINARY2;
    auto shared = Interface::Interface<CLuaShared*>(BINARY2, "LUASHARED003");
    if (!shared) {
        binary = BINARY;
        shared = Interface::Interface<CLuaShared*>(BINARY, "LUASHARED003");
    }
    #else
    const char* binary = BINARY;
    auto shared = Interface::Interface<CLuaShared*>(BINARY, "LUASHARED003");
    #endif

    if (!shared) {
        std::cout << "[ERROR] Interstellar couldn't initialize properly, couldn't find lua_shared/interface." << std::endl;
        return 0;
    }

    current_shared = shared;
    auto vtable = Interface::VTable(shared);
    int errors = Interstellar::init(binary);

    if (errors != 0) {
        std::cout << "[ERROR] Interstellar couldn't initialize properly, code: " << std::to_string(errors) << std::endl;
        return errors;
    }

    // I noticed that the beta builds have srcds in odd places, so this solves that
    static const std::unordered_set<std::string> bin_variants = {
        "bin", "linux32", "linux64", "win32", "win64"
    };

    std::filesystem::path path = Interstellar::FS::get_root();
    if (path.filename().string() == "bin") {
        path = path.parent_path();
    }
    else if (bin_variants.count(path.filename().string())
        && path.parent_path().filename().string() == "bin") {
        path = path.parent_path().parent_path();
    }

    Interstellar::FS::api(path.string());
    Interstellar::Memory::api();
    Interstellar::LXZ::api();
    Interstellar::IOT::api();
    Interstellar::Sodium::api();

    Interstellar::FS::add_error("entry_point", [](API::lua_State* L, std::string error) {
        CLuaInterface* LUA = (CLuaInterface*)((GState*)L)->luabase;
        if (!LUA) {
            // TODO: support errors of unique states
            return;
        }
        LUA->ErrorNoHalt((error + "\n").c_str());
    });

    Interstellar::LXZ::add_error("entry_point", [](API::lua_State* L, std::string error) {
        CLuaInterface* LUA = (CLuaInterface*)((GState*)L)->luabase;
        if (!LUA) {
            // TODO: support errors of unique states
            return;
        }
        LUA->ErrorNoHalt((error + "\n").c_str());
    });

    Interstellar::IOT::add_error("entry_point", [](API::lua_State* L, std::string type, std::string error) {
        CLuaInterface* LUA = (CLuaInterface*)((GState*)L)->luabase;
        if (!LUA) {
            // TODO: support errors of unique states
            return;
        }
        std::string format = "[" + type + "] " + error + "\n";
        LUA->ErrorNoHalt(format.c_str());
    });

    #ifdef GMSV
        CLuaInterface* lua_interface = shared->GetLuaInterface(GarrysMod::Lua::State::SERVER);
        Interstellar::API::lua_State* L;
        if (lua_interface) {
            L = (Interstellar::API::lua_State*)lua_interface->GetState();
            Interstellar::Tracker::listen(L, "server", true);
        }
        else {
            lua_interface = shared->GetLuaInterface(GarrysMod::Lua::State::MENU);

            if (!lua_interface) {
                std::cout << "[ERROR] Interstellar couldn't initialize properly, couldn't find GMSV lua_State." << std::endl;
                return 0;
            }

            L = (Interstellar::API::lua_State*)lua_interface->GetState();
            Interstellar::Tracker::listen(L, "menu", true);
        }

        current_interface = lua_interface;
        Interstellar::Reflection::push(L);
        Interstellar::API::lua::pop(L);
    #elif defined(GMCL)
        CLuaInterface* lua_interface = shared->GetLuaInterface(GarrysMod::Lua::State::CLIENT);

        if (!lua_interface) {
            std::cout << "[ERROR] Interstellar couldn't initialize properly, couldn't find GMCL lua_State." << std::endl;
            return 0;
        }

        current_interface = lua_interface;
        Interstellar::API::lua_State* L = (Interstellar::API::lua_State*)lua_interface->GetState();
        Interstellar::Tracker::listen(L, "client", true);
        Interstellar::Reflection::push(L);
        Interstellar::API::lua::pop(L);
    #endif
    
    #ifndef GMCL
        if (shared->GetLuaInterface(GarrysMod::Lua::State::MENU)) {
            #ifdef _WIN32
                DWORD oldProtect;
                VirtualProtect(vtable, sizeof(void*) * 6, PAGE_EXECUTE_READWRITE, &oldProtect);
            #elif __linux
                size_t page_size = sysconf(_SC_PAGE_SIZE);
                uintptr_t base = reinterpret_cast<uintptr_t>(vtable);
                uintptr_t aligned_base = base & ~(page_size - 1);
                mprotect(reinterpret_cast<void*>(aligned_base), page_size, PROT_READ | PROT_WRITE | PROT_EXEC);
            #endif
    
            CreateLuaInterface_o = (CreateLuaInterface_fn)vtable[4];
            vtable[4] = (void*)CreateLuaInterface_h;
            CloseLuaInterface_o = (CloseLuaInterface_fn)vtable[5];
            vtable[5] = (void*)CloseLuaInterface_h;

            #ifdef _WIN32
                DWORD __oldProtect;
                VirtualProtect(vtable, sizeof(void*) * 6, oldProtect, &__oldProtect);
            #elif __linux
                mprotect(reinterpret_cast<void*>(aligned_base), page_size, PROT_READ | PROT_EXEC);
            #endif
        }
    #endif

    lua::pushvalue(L, indexer::global);

    // This is what allows some task scheduled or parallelism to exist
    // I wish I didn't have to do it this way...
    {
        // timer method
        lua::getfield(L, -1, "timer");
        if (!lua::istable(L, -1)) {
            lua::pop(L, 2);
            std::cout << "[ERROR] Interstellar couldn't initialize properly, timer.Create is missing." << std::endl;
            return 0;
        }

        lua::getfield(L, -1, "Create");
        if (!lua::isfunction(L, -1)) {
            lua::pop(L, 3);
            std::cout << "[ERROR] Interstellar couldn't initialize properly, timer.Create is missing." << std::endl;
            return 0;
        }

        lua::pushstring(L, "__interstellar");
        lua::pushnumber(L, 0);
        lua::pushnumber(L, 0);
        lua::pushcfunction(L, runtime_async);
        if (lua::tcall(L, 4, 0)) {
            std::string err = lua::tocstring(L, -1);
            lua::pop(L, 3);
            std::cout << "[ERROR] Interstellar couldn't initialize properly, timer.Create failure:\n" << err << std::endl;
            return 1;
        }
        lua::pop(L, 2);

        // hook method
        /*lua::getfield(L, -1, "hook");
        if (!lua::istable(L, -1)) {
            lua::pop(L, 2);
            std::cout << "[ERROR] Interstellar couldn't initialize properly, hook.Add is missing." << std::endl;
            return 1;
        }

        lua::getfield(L, -1, "Add");
        if (!lua::isfunction(L, -1)) {
            lua::pop(L, 3);
            std::cout << "[ERROR] Interstellar couldn't initialize properly, hook.Add is missing." << std::endl;
            return 1;
        }

        lua::pushstring(L, "Think");
        lua::pushstring(L, "__interstellar");
        lua::pushcfunction(L, runtime_dethreader);
        if (lua::tcall(L, 3, 0)) {
            std::string err = lua::tocstring(L, -1);
            lua::pop(L, 3);
            std::cout << "[ERROR] Interstellar couldn't initialize properly, hook.Add failure:\n" << err << std::endl;
            return 1;
        }
        lua::pop(L, 2);*/
    }

    return 1;
}

int module_close() {
    if (!opened) return 1;
    opened = false;
    
    if (!current_shared) {
        std::cout << "[ERROR] Interstellar couldn't destruct properly, couldn't find lua_shared/interface." << std::endl;
        return 1;
    }

    auto vtable = Interface::VTable(current_shared);

    if (CreateLuaInterface_o && CloseLuaInterface_o) {
        #ifdef _WIN32
            DWORD oldProtect;
            VirtualProtect(vtable, sizeof(void*) * 6, PAGE_EXECUTE_READWRITE, &oldProtect);
        #elif __linux
            size_t page_size = sysconf(_SC_PAGE_SIZE);
            uintptr_t base = reinterpret_cast<uintptr_t>(vtable);
            uintptr_t aligned_base = base & ~(page_size - 1);
            mprotect(reinterpret_cast<void*>(aligned_base), page_size, PROT_READ | PROT_WRITE | PROT_EXEC);
        #endif

        vtable[4] = CreateLuaInterface_o;
        vtable[5] = CloseLuaInterface_o;

        #ifdef _WIN32
            DWORD __oldProtect;
            VirtualProtect(vtable, sizeof(void*) * 6, oldProtect, &__oldProtect);
        #elif __linux
            mprotect(reinterpret_cast<void*>(aligned_base), page_size, PROT_READ | PROT_EXEC);
        #endif
    }

    auto list = Tracker::all();
    for (auto& state : list) {
        Tracker::pre_remove(state.second);
        Tracker::post_remove(state.second);
    }

    return 1;
}

// Used for testing with injections
#ifdef _WIN32
BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved) {
    DisableThreadLibraryCalls(module);

    if (reason == DLL_PROCESS_ATTACH) {
        module_open();
        return 1;
    }
    else if (reason == DLL_PROCESS_DETACH) {
        module_close();
        return 1;
    }

    return 0;
}
#else
__attribute__((constructor))
void onLibraryLoad() {
    module_open();
}

__attribute__((destructor))
void onLibraryUnload() {
    module_close();
}
#endif

#ifdef _WIN32
    #define GMOD_DLL_EXPORT extern "C" __declspec( dllexport )
#else
    #define GMOD_DLL_EXPORT extern "C" __attribute__((visibility("default")))
#endif

GMOD_DLL_EXPORT int gmod13_open( [[maybe_unused]] API::lua_State* L ) {
    module_open();
    return 0;
}
GMOD_DLL_EXPORT int gmod13_close( [[maybe_unused]] API::lua_State* L ) {
    module_close();
    return 0;
}