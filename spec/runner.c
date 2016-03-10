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

static void set_arg(lua_State* L, int argc, char **argv)
{
    int i, n;
    lua_createtable(L, 0, 0);
    for (i = 2, n = 1; i < argc; ++i, ++n) {
        lua_pushstring(L, argv[i]);
        lua_rawseti(L, -2, n);
    }
    lua_pushliteral(L, "-o");
    lua_rawseti(L, -2, n++);
    lua_pushliteral(L, "plainTerminal");
    lua_rawseti(L, -2, n++);

    lua_setglobal(L, "arg");
}

// runner spec/xxx_spec.lua [bustedoptions]
static int run(int argc, char **argv)
{
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    preload_lua_modules(L);
    set_arg(L, argc, argv);
    if (luaL_dofile(L, argc > 1 ? argv[1] : PROJECT_ROOT "/spec/pomelo_spec.lua") != 0)
        traceback(L);
    lua_close(L);
    return 0;
}

#include "pomelo.h"

static int runc()
{
    pc_client_t* client = (pc_client_t*)malloc(pc_client_size());
    pc_client_config_t config = PC_CLIENT_CONFIG_DEFAULT;
    config.enable_polling = 1;
    //pc_lib_init(NULL, NULL, NULL, "Test Client");
    pc_client_init(client, NULL, &config);
    pc_client_connect(client, "127.0.0.1", 3010, NULL);
    pc_client_disconnect(client);
    pc_client_cleanup(client);
    return 0;
}

int main(int argc, char* argv[])
{
    run(argc, argv);
    return runc();
}
