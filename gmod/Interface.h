#ifndef GARRYSMOD_LUA_INTERFACE_H
#define GARRYSMOD_LUA_INTERFACE_H

#include "LuaBase.h"

namespace GarrysMod::Lua {
    struct lua_State
    {
    #if ( defined( _WIN32 ) || defined( __linux__ ) || defined( __APPLE__ ) ) && \
        !defined( __x86_64__ ) && !defined( _M_X64 )
        // Win32, Linux32 and macOS32
        unsigned char _ignore_this_common_lua_header_[48 + 22];
    #elif ( defined( _WIN32 ) || defined( __linux__ ) || defined( __APPLE__ ) ) && \
        ( defined( __x86_64__ ) || defined( _M_X64 ) )
        // Win64, Linux64 and macOS64 (not tested)
        unsigned char _ignore_this_common_lua_header_[92 + 22];
    #else
        #error Unsupported platform
    #endif

        GarrysMod::Lua::ILuaBase* luabase;
    };
}

#endif
