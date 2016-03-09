/*
 * This is a simple lua runner allows you to debug tests in C/C++ IDEs.
 */
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <stdlib.h>

#include "lua-pomelo.h"


static int traceback(lua_State *L) {
    // error at -1
    lua_getglobal(L, "debug"); // [error, debug]
    lua_getfield(L, -1, "traceback");// [error, debug, debug.traceback]

    lua_remove(L,-2); // remove the 'debug'

    lua_pushvalue(L, 1);
    lua_pushinteger(L, 2);
    lua_call(L, 2, 1);
    fprintf(stderr, "%s\n", lua_tostring(L, -1));
    return 1;
}

static const luaL_Reg modules[] = {
    { "pomelo", luaopen_pomelo },

    { NULL, NULL }
};

void preload_lua_modules(lua_State *L)
{
    // load extensions
    const luaL_Reg* lib = modules;
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "preload");
    for (; lib->func; lib++)
    {
        lua_pushcfunction(L, lib->func);
        lua_setfield(L, -2, lib->name);
    }
    lua_pop(L, 2);
}


static void createargtable(lua_State *L, char **argv, int argc, int script) {
  int i, narg;
  if (script == argc) script = 0;  /* no script name? */
  narg = argc - (script + 1);  /* number of positive indices */
  lua_createtable(L, narg, script + 1);
  for (i = 0; i < argc; i++) {
    lua_pushstring(L, argv[i]);
    lua_rawseti(L, -2, i - script);
  }
  lua_setglobal(L, "arg");
}

// runner spec/xxx_spec.lua [bustedoptions]
static void run(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("USAGE: runner spec/you_spec.lua [bustedoptions]\n");
        exit(EXIT_FAILURE);
    }
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    preload_lua_modules(L);
    createargtable(L, argv, argc, 1);
    if (luaL_dofile(L, argv[1])!=0)
        traceback(L);
    lua_close(L);
}

int main(int argc, char* argv[])
{
    run(argc, argv);

    return 0;
}
