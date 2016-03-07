#ifndef __LUA_POMELO_H__
#define __LUA_POMELO_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <lua.h>
#include <lauxlib.h>

LUALIB_API int luaopen_pomelo(lua_State *L);

#ifdef __cplusplus
}
#endif

#endif // __LUA_POMELO_H__
