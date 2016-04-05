#include "lua-pomelo.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <vector>
#include "pomelo.h"

std::vector<pc_client_t*>& all_clients()
{
    static std::vector<pc_client_t*> clients_;
    return clients_;
}

#if LUA_VERSION_NUM < 502
#define lua_rawlen(L, idx)      lua_objlen(L, idx)
#endif

static void setfuncs(lua_State* L, const luaL_Reg *funcs)
{
#if LUA_VERSION_NUM >= 502 // LUA 5.2 or above
    luaL_setfuncs(L, funcs, 0);
#else
    luaL_register(L, NULL, funcs);
#endif
}

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

static int iscallable(lua_State* L, int idx)
{
    int r;
    switch (lua_type(L, idx)) {
        case LUA_TFUNCTION:
            return 1;
        case LUA_TTABLE:
            luaL_getmetafield(L, idx, "__call");
            r = lua_isfunction(L, -1);
            lua_pop(L, 1);
            return r;
        default:
            return 0;
    }
}

static const char * const log_levels[] = {
    "DEBUG",
    "INFO",
    "WARN",
    "ERROR",
    "DISABLE",
    NULL
};


/*
 * local pomelo = require('pomelo')
 * pomelo.configure({
 *   log='WARN', -- log level, optional, one of 'DEBUG', 'INFO', 'WARN', 'ERROR', 'DISABLE', default to 'DISABLE'
 *   cafile = 'path/to/ca/file', -- optional
 *   capath = 'path/to/ca/path', -- optional
 * })
 */
static int lib_configure(lua_State* L)
{
    int log_level = PC_LOG_DISABLE;
    const char* ca_file = NULL;
    const char* ca_path = NULL;

    switch (lua_type(L, 1))
    {
    case LUA_TTABLE:
        lua_getfield(L, 1, "log");
        log_level = luaL_checkoption(L, -1, "DISABLE", log_levels);
        lua_pop(L, 1);

        lua_getfield(L, 1, "cafile");
        ca_file = luaL_optstring(L, -1, NULL);
        lua_pop(L, 1);

        lua_getfield(L, 1, "capath");
        ca_path = luaL_optstring(L, -1, NULL);
        lua_pop(L, 1);
        break;
    case LUA_TNONE: break;
    default:
        luaL_argerror(L, 1, "expected an optional table");
    }

#if !defined(PC_NO_UV_TLS_TRANS)
    if (ca_file || ca_path) {
        tr_uv_tls_set_ca_file(ca_file, ca_path);
    }
#endif

    pc_lib_set_default_log_level(log_level);

    return 0;
}

/*
* local pomelo = require('pomelo')
* pomelo.version() -- '0.3.5-release'
*/
static int lib_version(lua_State* L)
{
    lua_pushstring(L, pc_lib_version_str());
    return 1;
}


static int lib_poll(lua_State* L)
{
    std::vector<pc_client_t*> clients(all_clients()); // make a copy of all clients vector for safe against deletion
    for (std::vector<pc_client_t*>::size_type i = 0; i < clients.size(); ++i) {
        pc_client_poll(clients[i]);
    }
    return 0;
}

#define ClientMETA  "pomelo.Client"

static void pushClient(lua_State* L, pc_client_t* client)
{
    pc_client_t** w = (pc_client_t**)lua_newuserdata(L, sizeof(*w));
    *w = client;
    luaL_getmetatable(L, ClientMETA);
    lua_setmetatable(L, -2);
}

static inline pc_client_t** toClientp(lua_State* L){
    return (pc_client_t**)luaL_checkudata(L, 1, ClientMETA);
}

pc_client_t* toClient(lua_State* L) {
    pc_client_t** w = toClientp(L);
    if (*w == NULL)
        luaL_error(L, "Client already closed");
    return *w;
}

static const char* const transport_names[] = {
    "TCP", "TLS", "2", "3", "4", "5", "6", "DUMMY", NULL
};

static int streq(lua_State* L, int index, const char* v)
{
    return (lua_type(L, -1) == LUA_TSTRING
    && (strcmp(luaL_checkstring(L, -1), v) == 0));
}

static int optbool(lua_State* L, int idx, int def)
{
    return (lua_isnoneornil(L, idx)) ? def : lua_toboolean(L, idx);
}

static int recon_retry_max(lua_State* L, int idx) {
    int v;
    if (streq(L, idx, "ALWAYS"))
        return PC_ALWAYS_RETRY;
    v = (int)luaL_optinteger(L, idx, PC_ALWAYS_RETRY);
    return v;
}

static pc_client_config_t check_client_config(lua_State* L, int idx)
{
    pc_client_config_t config = PC_CLIENT_CONFIG_DEFAULT;
    int t = lua_type(L, idx);
    if (t != LUA_TTABLE && t != LUA_TNONE)
        luaL_error(L, "bad argument %d to pomelo.newClient (table|none expected, got %s)", idx, luaL_typename(L, idx));
    config.enable_polling = 1;
    if (t == LUA_TNONE)
        return config;

    lua_getfield(L, idx, "conn_timeout");
    config.conn_timeout = (int)luaL_optinteger(L, -1, config.conn_timeout);
    lua_pop(L, 1);

    lua_getfield(L, idx, "enable_reconn");
    config.enable_reconn = optbool(L, -1, config.enable_reconn);
    lua_pop(L, 1);

    lua_getfield(L, idx, "reconn_max_retry");
    config.reconn_max_retry = recon_retry_max(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, -1, "reconn_delay");
    config.reconn_delay = (int)luaL_optinteger(L, -1, config.reconn_delay);
    lua_pop(L, 1);

    lua_getfield(L, -1, "reconn_delay_max");
    config.reconn_delay_max = (int)luaL_optinteger(L, -1, config.reconn_delay_max);
    lua_pop(L, 1);

    lua_getfield(L, -1, "reconn_exp_backoff");
    config.reconn_exp_backoff = optbool(L, -1, config.reconn_exp_backoff);
    lua_pop(L, 1);

    lua_getfield(L, idx, "transport_name");
    config.transport_name = luaL_checkoption(L, -1, "TCP", transport_names);
    lua_pop(L, 1);

    return config;
}

static void push_client_config(lua_State* L, const pc_client_config_t* config)
{
    lua_createtable(L, 0, 4);

    lua_pushinteger(L, config->conn_timeout);
    lua_setfield(L, -2, "conn_timeout");

    lua_pushboolean(L, config->enable_reconn);
    lua_setfield(L, -2, "enable_reconn");

    if (config->reconn_max_retry == PC_ALWAYS_RETRY)
        lua_pushliteral(L, "ALWAYS");
    else
        lua_pushinteger(L, config->reconn_max_retry);
    lua_setfield(L, -2, "reconn_max_retry");

    lua_pushinteger(L, config->reconn_delay);
    lua_setfield(L, -2, "reconn_delay");

    lua_pushinteger(L, config->reconn_delay_max);
    lua_setfield(L, -2, "reconn_delay_max");

    lua_pushboolean(L, config->reconn_exp_backoff);
    lua_setfield(L, -2, "reconn_exp_backoff");

    if (config->transport_name == PC_TR_NAME_UV_TCP
        || config->transport_name == PC_TR_NAME_UV_TLS
        || config->transport_name == PC_TR_NAME_DUMMY)
    {
        lua_pushstring(L, transport_names[config->transport_name]);
    }
    else
        lua_pushinteger(L, config->transport_name);
    lua_setfield(L, -2, "transport_name");
}


static int create_registry(lua_State* L) {
    lua_createtable(L, 0, 8);
    return luaL_ref(L, LUA_REGISTRYINDEX);
}

static void load_registry(lua_State* L, int ref) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
}

static void load_registry(lua_State* L, void* ref) {
    load_registry(L, reinterpret_cast<intptr_t>(ref));
}

static void destroy_registry(lua_State* L, int ref) {
    luaL_unref(L, LUA_REGISTRYINDEX, ref);
}

/**
 * event handler callback and event types
 *
 * arg1 and arg2 are significant for the following events:
 *   PC_EV_USER_DEFINED_PUSH - arg1 as push route, arg2 as push msg
 *   PC_EV_CONNECT_ERROR - arg1 as short error description
 *   PC_EV_CONNECT_FAILED - arg1 as short reason description
 *   PC_EV_UNEXPECTED_DISCONNECT - arg1 as short reason description
 *   PC_EV_PROTO_ERROR - arg1 as short reason description
 *
 * For other events, arg1 and arg2 will be set to NULL.
 */
static void lua_event_cb(pc_client_t *client, int ev_type, void* ex_data, const char* arg1, const char* arg2)
{
    int nargs = 0, n, i, copy, a, start;
    lua_State* L = (lua_State*)ex_data;
    lua_checkstack(L, 8);
    load_registry(L, pc_client_ex_data(client));    // [event_registry]
    start = lua_gettop(L);

    switch (ev_type) {
        case PC_EV_USER_DEFINED_PUSH:
            lua_pushstring(L, arg1);                // [event_registry, route]
            lua_rawget(L, -2);                      // [event_registry, handlers]
            lua_pushstring(L, arg2);                // [event_registry, handlers, arg]
            nargs = 1;
            break;
        case PC_EV_CONNECTED:
            lua_pushliteral(L, "connected");        // [event_registry, connected]
            lua_rawget(L, -2);                      // [event_registry, handlers]
            break;
        case PC_EV_DISCONNECT:
            lua_pushliteral(L, "disconnect");       // [event_registry, disconnect]
            lua_rawget(L, -2);                      // [event_registry, handlers]
            break;
        case PC_EV_KICKED_BY_SERVER:
            lua_pushliteral(L, "kicked");           // [event_registry, kick]
            lua_rawget(L, -2);                      // [event_registry, handlers]
            break;
        case PC_EV_CONNECT_ERROR:
        case PC_EV_CONNECT_FAILED:
        case PC_EV_UNEXPECTED_DISCONNECT:
        case PC_EV_PROTO_ERROR:
            lua_pushliteral(L, "error");            // [event_registry, error]
            luaL_checktype(L, -2, LUA_TTABLE);
            lua_rawget(L, -2);                      // [event_registry, handlers]
            lua_pushstring(L, arg1);                // [event_registry, handlers, reason]
            nargs = 1;
            break;
        default:
            lua_pop(L, 1);
            return;
    }
                                                    // [event_registry, handlers, ...]
    n = lua_rawlen(L, start + 1);
    if (n > 0)
    {
        // make a copy of listeners so that safe against remove or add.
        lua_createtable(L, n, 0);                   // [event_registry, handlers, ..., copy]
        for (i = 1; i <= n; ++i) {
            lua_rawgeti(L, start + 1, i);           // [event_registry, handlers, ..., copy, listener]
            lua_rawseti(L, -2, i);                  // [event_registry, handlers, ..., copy]
        }
        copy = lua_gettop(L);
        for (i = 1; i <= n; ++i) {
            lua_rawgeti(L, copy, i);                // [event_registry, handlers, ..., copy, handler]
            for (a = 0; a < nargs; ++a)
                lua_pushvalue(L, start + 3 + a);
                                                    // [event_registry, handlers, ..., copy, handler, ...]
            if (lua_pcall(L, nargs, 0, 0) != 0)
                traceback(L);
        }
    }

    lua_settop(L, start - 1);
}

/*
 * local pomelo = require('pomelo')
 * local opts =  {
 *    conn_timeout = 30,
 *    enable_reconn = true,
 *    reconn_max_retry = 'ALWAYS',
 *    reconn_delay = 2,
 *    reconn_delay_max = 30,
 *    reconn_exp_backoff = true,
 *    enable_polling = true,
 *    transport_name = "TCP" -- 'TCP', 'TLS', 'DUMMY', or an integer id of you customized transport
 * }
 * local client = pomelo:newClient(opts)
 */
static int Client_new(lua_State* L)
{
    pc_client_config_t config = check_client_config(L, 1);
    pc_client_t* client = (pc_client_t* )malloc(pc_client_size());
    int listeners = LUA_NOREF;
    if (!client) {
        lua_pushnil(L);
        return 1;
    }

    // use lua table as events listener registry
    listeners = create_registry(L);
    if (pc_client_init(client, (void*)(ptrdiff_t)listeners, &config) != PC_RC_OK) {
        destroy_registry(L, listeners);
        free(client);
        lua_pushnil(L);
        return 1;
    }

    pc_client_add_ev_handler(client, lua_event_cb, L, NULL);

    pushClient(L, client);
    all_clients().push_back(client);
    return 1;
}

static void push_rcstring(lua_State* L, int rc) {
    lua_pushstring(L, pc_client_rc_str(rc) + 6); // + 6 for remove the leading "PC_RC_"
}

static int pushRC(lua_State* L, int rc)
{
    if (rc != PC_RC_OK)
    {
        lua_pushnil(L);
        push_rcstring(L, rc);
        return 2;
    }

    lua_pushboolean(L, 1);
    return 1;
}
/*
 *  ERROR
    TIMEOUT
    INVALID_JSON
    INVALID_ARG
    NO_TRANS
    INVALID_THREAD
    TRANS_ERROR
    INVALID_ROUTE
    INVALID_STATE
    NOT_FOUND
    RESET
 */
static int Client_connect(lua_State* L)
{
    pc_client_t* client = toClient(L);
    const char* host = luaL_checkstring(L, 2);
    int port = (int)luaL_checkinteger(L, 3);
    const char* handshake_opts = luaL_optstring(L, 4, NULL);
    int rc = pc_client_connect(client, host, port, handshake_opts);
    return pushRC(L, rc);
}

static int Client_disconnect(lua_State* L)
{
    pc_client_t* client = toClient(L);
    int rc = pc_client_disconnect(client);
    return pushRC(L, rc);
}

static int Client_config(lua_State* L)
{
    pc_client_t* client = toClient(L);
    push_client_config(L, pc_client_config(client));
    return 1;
}

static int Client_state(lua_State* L)
{
    pc_client_t* client = toClient(L);
    const char* s = pc_client_state_str(pc_client_state(client)) + 6; // remove 'PC_ST_';
    lua_pushstring(L, s);
    return 1;
}


static int Client_conn_quality(lua_State* L)
{
    pc_client_t* client = toClient(L);
    lua_pushinteger(L, pc_client_conn_quality(client));
    return 1;
}

static int Client_gc(lua_State* L)
{
    pc_client_t** p = toClientp(L);
    if (!*p)
        return 0;
    std::vector<pc_client_t*>& clients = all_clients();
    clients.erase(std::remove(clients.begin(), clients.end(), *p), clients.end());
    pc_client_cleanup(*p);
    free(*p);
    *p = NULL;
    return 0;
}

static int Client_tostring(lua_State* L)
{
    pc_client_t** w = toClientp(L);
    if (*w)
        lua_pushfstring(L, "Client (%p)", *w);
    else
        lua_pushliteral(L, "Client (closed)");
    return 1;
}


static int Client_poll(lua_State* L)
{
    pc_client_t* client = toClient(L);
    int rc = pc_client_poll(client);
    return pushRC(L, rc);
}

typedef struct {
    lua_State* L;
    int ref;
} lua_cb_ex_t;

static lua_cb_ex_t* create_lua_cb_ex(lua_State* L, int index)
{
    lua_cb_ex_t* p = (lua_cb_ex_t*)malloc(sizeof(*p));
    if (!p)
        return NULL;

    lua_pushvalue(L, index);
    p->L = L;
    p->ref = luaL_ref(L, LUA_REGISTRYINDEX);
    return p;
}

static void destroy_lua_cb_ex(lua_cb_ex_t* p)
{
    luaL_unref(p->L, LUA_REGISTRYINDEX, p->ref);
    free(p);
}



#define copy_field(L, from, type, name, push) push(L, pc_##type##_##name(from)); lua_setfield(L, -2, #name)
/**
 * push the obj as a table copy on to lua stack
 */
#define push_as_table(L, type, obj) \
    lua_createtable(L, 0, 3); \
    copy_field(L, obj, type, route, lua_pushstring); \
    copy_field(L, obj, type, msg, lua_pushstring); \
    copy_field(L, obj, type, timeout, lua_pushinteger)


static lua_State* load_cb_env(void* cbex)
{
    lua_State* L = NULL;
    lua_cb_ex_t* ex = (lua_cb_ex_t*)(cbex);
    if (ex && ex->L) {
        L = ex->L;
        lua_rawgeti(L, LUA_REGISTRYINDEX, ex->ref);
        assert(iscallable(L, -1));
        destroy_lua_cb_ex(ex);
    }
    return L;
}

static void push_as_error(lua_State* L, int rc)
{
    if (rc == PC_RC_OK)
        lua_pushnil(L);
    else
        push_rcstring(L, rc);
}


static void lua_request_cb(const pc_request_t* req, int rc, const char* res)
{
    int top;
    lua_State* L = load_cb_env(pc_request_ex_data(req));
    if (!L) return;
    top = lua_gettop(L) - 1;

    push_as_error(L, rc);   // err
    push_as_table(L, request, req); // req
    lua_pushstring(L, res); // res

    // callback(err, req, res)
    lua_pcall(L, 3, 0, 0);
    lua_settop(L, top);
}

static void lua_nofity_cb(const pc_notify_t* req, int rc)
{
    lua_State* L = load_cb_env(pc_notify_ex_data(req));
    if (!L) return;

    push_as_error(L, rc); // err
    push_as_table(L, notify, req); // req

    // callback(err, req)
    if (lua_pcall(L, 2, LUA_MULTRET, 0) != 0)
        traceback(L);
}

typedef struct {
    pc_client_t* client;
    const char* route;
    const char* msg;
    int timeout;
    lua_cb_ex_t* ex;
} lua_req_arg_t;


static lua_req_arg_t get_args(lua_State* L, int optional)
{
    lua_req_arg_t args;
    int cbindex = 5;
    size_t sz;
    args.client = toClient(L);
    args.route = luaL_checkstring(L, 2);
    args.msg = luaL_checklstring(L, 3, &sz);
    if (sz == 0)
        luaL_argerror(L, 3, "message should not be empty");
    args.timeout = PC_WITHOUT_TIMEOUT;

    switch (lua_type(L, 4)) {
        case LUA_TTABLE: // fall thought
        case LUA_TFUNCTION: cbindex = 4; break;
        case LUA_TNUMBER: args.timeout = lua_tointeger(L, 4); break;
        default:
            luaL_error(L, "bad argument %d (number|function expected, got %s)", 4, luaL_typename(L, 4));
    }
    if (!iscallable(L, cbindex) && !optional)
        luaL_error(L, "bad argument %d (function expected, got %s)", cbindex, luaL_typename(L, cbindex));
    args.ex = create_lua_cb_ex(L, cbindex);
    return args;
}

/**
 * client:request(route, message[, timeout], callback)
 * client:request('connector.get.ip', message, 30, function(err, req, res)
 *
 * end)
 */
static int Client_request(lua_State* L)
{
    lua_req_arg_t args = get_args(L, 0);
    int rc = pc_request_with_timeout(
        args.client, args.route, args.msg,
        args.ex, args.timeout, lua_request_cb);
    return pushRC(L, rc);
}


/**
 * client:notify(route, message[, timeout][, callback])
 * client:notify('connector.get.ip', message, 30, function(err, req)
 *
 * end)
 */
static int Client_notify(lua_State* L)
{
    lua_req_arg_t args = get_args(L, 1);

    int rc = pc_notify_with_timeout(args.client, args.route, args.msg,
        args.ex, args.timeout, lua_nofity_cb);
    return pushRC(L, rc);
}


/**
 * client:on(route, callback) --> client
 */
static int Client_on(lua_State* L)
{
    int n;
    // [client, route, callback]
    pc_client_t* client = toClient(L);
    luaL_checkstring(L, 2);
    iscallable(L, 3);

    load_registry(L, pc_client_ex_data(client)); // [client, route, callback, event_registry]

    /*
    local listeners = registry[route]
    if listeners == nil then
        listeners = {}
        registry[route] = listeners
    end
    */
    lua_pushvalue(L, 2); // [client, route, callback, event_registry, route]
    lua_rawget(L, -2); // [client, route, callback, event_registry, listeners]
    if (lua_isnil(L, -1))
    {
        lua_pop(L, 1); // [client, route, callback, event_registry]
        lua_createtable(L, 0, 0); // [client, route, callback, event_registry, new_listeners]
        lua_pushvalue(L, 2); // [client, route, callback, event_registry, new_listeners, route]
        lua_pushvalue(L, -2); // [client, route, callback, event_registry, new_listeners, route, new_listeners]
        lua_rawset(L, -4); // [client, route, callback, event_registry, new_listeners]
    }

    /*
    listeners[#listeners+1] = callback
    */
    n = lua_rawlen(L, -1);
    lua_pushvalue(L, 3); // [client, route, callback, event_registry, listeners, callback]
    lua_rawseti(L, -2, n + 1); // [client, route, callback, event_registry, listeners]

    /*
    return client
    */
    lua_pop(L, 4); // [client]
    return 1;
}


#define ONCE_CLIENT     1
#define ONCE_EVENT      2
#define ONCE_CALLBACK   3

static int once_call(lua_State* L)
{
    int narg;
                                        // [self, ...]
    luaL_checktype(L, 1, LUA_TTABLE);
    narg = lua_gettop(L) - 1;
    lua_rawgeti(L, 1, ONCE_CALLBACK);   // [self, ..., callback]
    lua_insert(L, 2);                   // [self, callback, ...]
    if (lua_pcall(L, narg, 0, 0) != 0)  // [self] -- just discard the return value of callback
        traceback(L);

    // remove the callback
    lua_rawgeti(L, 1, ONCE_CLIENT);     // [self, client]
    lua_getfield(L, -1, "off");         // [self, client, client.off]
    lua_pushvalue(L, -2);               // [self, client, client.off, client]
    lua_rawgeti(L, 1, ONCE_EVENT);      // [self, client, client.off, client, event]
    lua_pushvalue(L, 1);                // [self, client, client.off, client, event, self]
    if (lua_pcall(L, 3, 0, 0))          // [self, client]
        traceback(L);
    return 0;
}

#define OnceMeta "pomelo.Once"
static void createOnceMeta(lua_State* L)
{
    // create the Request class Metatable
    luaL_newmetatable(L, OnceMeta);
    lua_pushcfunction(L, once_call);
    lua_setfield(L, -2, "__call");
    lua_pop(L, 1);
}

// requires stack: [client, event, callback]
// returns stack: [client, event, callback, once]
static void wrapOnce(lua_State* L)
{
    assert(lua_gettop(L) == 3);
    lua_createtable(L, 3, 0);           // [client, event, callback, once]

    lua_pushvalue(L, 1);                // [client, event, callback, once, client]
    lua_rawseti(L, -2, ONCE_CLIENT);    // [client, event, callback, once]

    lua_pushvalue(L, 2);                // [client, event, callback, once, event]
    lua_rawseti(L, -2, ONCE_EVENT);     // [client, event, callback, once]

    lua_pushvalue(L, 3);                // [client, event, callback, once, callback]
    lua_rawseti(L, -2, ONCE_CALLBACK);  // [client, event, callback, once]

                                        // [client, event, callback, once]
    luaL_getmetatable(L, OnceMeta);     // [client, event, callback, once, meta]
    lua_setmetatable(L, -2);            // [client, event, callback, once]
}


// client:once(route, callback) --> client
static int Client_once(lua_State* L)
{
                                // [self, event, callback]
    toClient(L);
    luaL_checkstring(L, 2);
    iscallable(L, 3);
    lua_settop(L, 3);
    wrapOnce(L);                // [self, event, once]

    lua_getfield(L, 1, "on");   // [self, event, callback, once, self.on]
    lua_pushvalue(L, 1);        // [self, event, callback, once, self.on, self]
    lua_pushvalue(L, 2);        // [self, event, callback, once, self.on, self, event]
    lua_pushvalue(L, 4);        // [self, event, callback, once, self.on, self, event, once]
    lua_call(L, 3, 1);          // [self, event, callback, once, self] -- self:on() returns self.

    return 1;
}


static int fneq(lua_State* L, int cur, int check)
{
    int eq;
    switch (lua_type(L, cur)) {
        case LUA_TFUNCTION:
            return lua_rawequal(L, cur, check);
        case LUA_TTABLE:
            lua_rawgeti(L, cur, 3);
            eq = lua_rawequal(L, -1, check);
            lua_pop(L, 1);
            return eq;
        default:
            return 0;
    }
}

static int tbeq(lua_State* L, int cur, int check)
{
    int eq;
    if (!lua_istable(L, cur))
        return 0;

    if (lua_rawequal(L, cur, check))
        return 1;

    lua_rawgeti(L, cur, ONCE_CALLBACK);
    eq = lua_rawequal(L, -1, check);
    lua_pop(L, 1);
    return eq;
}


/**
 client:off(event, callback) --> client
Removes the specified listener from the listener array for the specified event.


local callback = function(stream) {
  console.log('someone connected!')
};
client:on('connection', callback)
-- ...
client:off('connection', callback)

`off` will remove, at most, one instance of a listener from the listener array.
If any single listener has been added multiple times to the listener array for
the specified event, then removeListener must be called multiple times to remove
each instance.

Because listeners are managed using an internal array, calling this will change
the position indices of any listener registered after the listener being removed.
This will not impact the order in which listeners are called, but it will means
that any copies of the listener array as returned by the `client:listeners()`
method will need to be recreated.

Returns a reference to the client so calls can be chained.
*/
static int Client_off(lua_State* L)
{
    int t, n, i, j;
    int (*eq)(lua_State*, int, int);
                                                        // [self, event, callback]
    pc_client_t* client = toClient(L);
    luaL_checkstring(L, 2);
    t = lua_type(L, 3);
    if (t != LUA_TFUNCTION && t != LUA_TTABLE)
        luaL_error(L, "bad argument 3 to client.off (function/table expected, got %s)", luaL_typename(L, 3));

    lua_checkstack(L, 8);

    load_registry(L, pc_client_ex_data(client));        // [client, route, callback, event_registry]
    lua_pushvalue(L, 2);                                // [client, route, callback, event_registry, route]
    lua_rawget(L, -2);                                  // [client, route, callback, event_registry, listeners]
    if (lua_isnil(L, -1) || (n = lua_rawlen(L, -1)) == 0) {
        lua_pop(L, 2);                                  // [client, route, callback]
        lua_pushvalue(L, 1);                            // [client, route, callback, client]
        return 1;
    }

                                                        // [client, route, callback, event_registry, listeners]
    eq = (t == LUA_TFUNCTION) ? fneq : tbeq;
    for (i = n; i > 0; --i)
    {
        lua_rawgeti(L, -1, i);                          // [client, route, callback, event_registry, listeners, listener]
        if (eq(L, -1, 3)) // found the listener
        {
            for (j = i; j <= n; ++j)
            {
                lua_rawgeti(L, -2, j+1);                // [client, route, callback, event_registry, listeners, listener, listener]
                lua_rawseti(L, -3, j);                  // [client, route, callback, event_registry, listeners, listener]
            }
            break;
        }
    }
    lua_pop(L, 3);                                      // [client, route, callback]
    lua_pushvalue(L, 1);                                // [client, route, callback, client]

    return 1;
}

// Returns a copy of the array of listeners for the specified event.
static int Client_listeners(lua_State* L)
{
    int n = 0, i;
                                                        // [client, route]
    pc_client_t* client = toClient(L);
    luaL_checkstring(L, 2);
    load_registry(L, pc_client_ex_data(client));        // [client, route, event_registry]
    lua_pushvalue(L, 2);                                // [client, route, event_registry, route]
    lua_rawget(L, -2);                                  // [client, route, event_registry, listeners]
    if (lua_istable(L, -1))
        n = lua_rawlen(L, -1);
    lua_createtable(L, n, 0);                           // [client, route, event_registry, listeners, result]
    if (n > 0) {
        for (i = 1; i <= n; ++i)
        {
            lua_rawgeti(L, -2, i);                      // [client, route, event_registry, listeners, result, listener]
            lua_rawseti(L, -2, i);                      // [client, route, event_registry, listeners, result]
        }
    }
    lua_replace(L, 3);                                  // [client, route, result, listeners]
    lua_pop(L, 1);                                      // [client, route, result]
    return 1;
}


static const luaL_Reg client_methods[] = {
    {"connect", Client_connect},
    {"disconnect", Client_disconnect},
    {"request", Client_request},
    {"notify", Client_notify},

    {"on", Client_on},
    {"addListener", Client_on},     // Alias for on
    {"once", Client_once},
    {"off", Client_off},
    {"removeListener", Client_off}, // Alias for off
    {"listeners", Client_listeners},

    {"config", Client_config},
    {"state", Client_state},
    {"conn_quality", Client_conn_quality},
    {"connQuality", Client_conn_quality},   // Alias for conn_quality

    {"poll", Client_poll},

    {"close", Client_gc},

    {"__tostring", Client_tostring},
    {"__gc", Client_gc},

    {NULL, NULL}
};


static void createClassMetatable(lua_State* L, const char* name, const luaL_Reg* methods)
{
    // create the Request class Metatable
    luaL_newmetatable(L, name);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    setfuncs(L, methods);
    lua_pop(L, 1);
}

static const luaL_Reg lib[] = {
    {"configure", lib_configure},
    {"version", lib_version},
    {"newClient", Client_new},
    {"createClient", Client_new},   // Alias for newClient().
    {"poll", lib_poll},

    {NULL, NULL}
};


static int initialized = 0;

LUALIB_API int luaopen_pomelo(lua_State *L)
{
    if (!initialized) {
        initialized = 1;
        pc_lib_set_default_log_level(PC_LOG_DISABLE);
        pc_lib_init(NULL, NULL, NULL, "Lua Client");
    }
    createClassMetatable(L, ClientMETA, client_methods);
    createOnceMeta(L);
    lua_createtable(L, 0, sizeof(lib)/sizeof(lib[0])-1);
    setfuncs(L, lib);
    return 1;
}
