#define LUA_LIB
#define LBIND_STATIC_API
#define LBIND_DEFAULT_FLAG (LBIND_TRACK|LBIND_ACCESSOR)
#include "lbind.h"

#define NUI_STATIC_API
#include "nui.h"

LBIND_TYPE(lbT_State, "nui.State");
LBIND_TYPE(lbT_Timer, "nui.Timer");
LBIND_TYPE(lbT_Node,  "nui.Node");
LBIND_TYPE(lbT_Attr,  "nui.Attribute");
LBIND_TYPE(lbT_Type,  "nui.Type");


/* utils */

typedef struct LNUIstate {
    NUIparams params;
    lua_State *L;
    NUIstate *S;
    int ref_time;
    int ref_wait;
} LNUIstate;

static NUIstate *ln_checkstate(lua_State *L, int idx)
{ return ((LNUIstate*)lbind_check(L, idx, &lbT_State))->S; }

static void ln_ref(lua_State *L, int idx, int *ref) {
    lua_pushvalue(L, idx);
    if (*ref == LUA_NOREF)
        *ref = luaL_ref(L, LUA_REGISTRYINDEX);
    else
        lua_rawseti(L, LUA_REGISTRYINDEX, *ref);
}

static void ln_unref(lua_State *L, int *ref) {
    luaL_unref(L, LUA_REGISTRYINDEX, *ref);
    *ref = LUA_NOREF;
}

static int ln_pushnode(lua_State *L, NUInode *n) {
    if (n == NULL) return 0;
    if (!lua53_rawgetp(L, LUA_REGISTRYINDEX, n))
        lbind_wrap(L, n, &lbT_Node);
    return 1;
}

static int ln_pushkey(lua_State *L, NUIkey *key) {
    lua_pushlstring(L, (const char*)key, nui_keylen(key));
    return 1;
}


/* timer */

typedef struct LNUItimer {
    NUItimer *timer;
    lua_State *L;
    int ontimer_ref;
    int ref;
    unsigned delayms;
} LNUItimer;

static NUItime lnui_ontimer(void *ud, NUItimer *timer, NUItime elapsed) {
    LNUItimer *obj = (LNUItimer*)ud;
    lua_State *L = obj->L;
    lua_rawgeti(L, LUA_REGISTRYINDEX, obj->ontimer_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, obj->ref);
    lua_pushinteger(L, (lua_Integer)elapsed);
    if (lbind_pcall(L, 2, 1) != LUA_OK) {
        fprintf(stderr, "%s\n", lua_tostring(L, -1));
        ln_unref(L, &obj->ref);
    }
    else if (lua_isnumber(L, -1)) {
        lua_Integer ret = lua_tointeger(L, -1);
        return ret >= 0 ? (NUItime)ret : 0;
    }
    else if (lua_toboolean(L, -1))
        nui_starttimer(timer, obj->delayms);
    else
        ln_unref(L, &obj->ref);
    lua_pop(L, 1);
    return 0;
}

static int Ltimer_new(lua_State *L) {
    NUIstate *S = (NUIstate*)lbind_check(L, 1, &lbT_State);
    LNUItimer *obj;
    luaL_checktype(L, 2, LUA_TFUNCTION);
    obj = (LNUItimer*)lbind_new(L, sizeof(LNUItimer), &lbT_Timer);
    obj->timer = nui_newtimer(S, lnui_ontimer, obj);
    obj->L = L;
    obj->ontimer_ref = LUA_NOREF;
    obj->ref = LUA_NOREF;
    if (obj->timer != NULL) {
        ln_ref(L, 2, &obj->ontimer_ref);
        return 1;
    }
    return 0;
}

static int Ltimer_delete(lua_State *L) {
    LNUItimer *obj = (LNUItimer*)lbind_test(L, 1, &lbT_Timer);
    if (obj->timer != NULL) {
        nui_deltimer(obj->timer);
        lbind_delete(L, 1);
        ln_unref(L, &obj->ontimer_ref);
        ln_unref(L, &obj->ref);
        obj->timer = NULL;
    }
    return 0;
}

static int Ltimer_start(lua_State *L) {
    LNUItimer *obj = (LNUItimer*)lbind_check(L, 1, &lbT_Timer);
    lua_Integer delayms = luaL_optinteger(L, 2, 0);
    if (!obj->timer) return 0;
    if (delayms < 0) delayms = 0;
    obj->delayms = (unsigned)delayms;
    if (nui_starttimer(obj->timer, obj->delayms))
        ln_ref(L, 1, &obj->ref);
    lbind_returnself(L);
}

static int Ltimer_cancel(lua_State *L) {
    LNUItimer *obj = (LNUItimer*)lbind_check(L, 1, &lbT_Timer);
    if (!obj->timer) return 0;
    ln_unref(L, &obj->ref);
    nui_canceltimer(obj->timer);
    lbind_returnself(L);
}

static void open_timer(lua_State *L) {
    luaL_Reg libs[] = {
        { "__gc", Ltimer_delete },
#define ENTRY(name) { #name, Ltimer_##name }
        ENTRY(new),
        ENTRY(delete),
        ENTRY(start),
        ENTRY(cancel),
#undef  ENTRY
        { NULL, NULL }
    };
    lbind_newmetatable(L, libs, &lbT_Timer);
}


/* type */

typedef struct LNUItype {
    NUItype base;
    lua_State *L;
    int ref_newcomp;
    int ref_delete_node;
} LNUItype;

static int Ltype_new(lua_State *L) {
    /*NUIstate *S = ln_checkstate(L, 1);*/
    /*size_t len;*/
    /*const char *s = luaL_checklstring(L, 2, &len);*/
    /*luaL_checktype(L, 3, LUA_TTABLE);*/

    return 1;
}

static int Lnode_addcomp(lua_State *L) { return 0; }
static int Lnode_getcomp(lua_State *L) { return 0; }

static void open_type(lua_State *L) {}


/* node attributes */

typedef struct LNUIattr {
    NUIattr base;
    lua_State *L;
    int ref_setattr;
    int ref_getattr;
    int ref_delattr;
} LNUIattr;

static int ln_checkvalue(lua_State *L, int idx, NUIvalue *pv) {
    switch (lua_type(L, idx)) {
    case LUA_TNIL:
        pv->type = NUI_TNIL;
        break;
    case LUA_TBOOLEAN:
        pv->type = NUI_TBOOL;
        pv->subtype = lua_toboolean(L, idx);
        break;
    case LUA_TNUMBER:
        if (!lua_isinteger(L, idx)) {
            lua_Number nv = lua_tonumber(L, idx);
            pv->type = NUI_TNUM;
            pv->u.d = nv;
        }
        else {
            lua_Integer iv = lua_tointeger(L, idx);
            pv->type = NUI_TINT;
            pv->subtype = 1;
            pv->u.ll = iv;
        }
        break;
    case LUA_TSTRING:
        pv->type = NUI_TSTR;
        {
            size_t len;
            pv->u.s = lua_tolstring(L, idx, &len);
            pv->subtype = len;
        }
        break;
    default:
        return 0;
    }
    return 1;
}

static int ln_pushvalue(lua_State *L, const NUIvalue *pv) {
    switch (pv->type) {
    case NUI_TNIL: lua_pushnil(L); break;
    case NUI_TBOOL: lua_pushboolean(L, pv->subtype); break;
    case NUI_TINT: lua_pushinteger(L, pv->u.ll); break;
    case NUI_TNUM: lua_pushnumber(L, pv->u.d); break;
    case NUI_TSTR: lua_pushlstring(L, pv->u.s, pv->subtype); break;
    default: return 0;
    }
    return 1;
}

static int ln_getattr(NUIattr *attr, NUInode *node, NUIkey *key, NUIvalue *pv) {
    LNUIattr *lattr = (LNUIattr*)attr;
    lua_State *L = lattr->L;
    if (lattr->ref_getattr <= 0) return NUI_TNIL;
    lua_rawgeti(L, LUA_REGISTRYINDEX, lattr->ref_getattr);
    ln_pushnode(L, node);
    ln_pushkey(L, key);
    if (lbind_pcall(L, 2, 1) == LUA_OK)
        ln_checkvalue(L, -1, pv);
    else {
        fprintf(stderr, "%s\n", lua_tostring(L, -1));
        pv->type = NUI_TNIL;
    }
    lua_pop(L, 1);
    return pv->type;
}

static int ln_setattr(NUIattr *attr, NUInode *node, NUIkey *key, const NUIvalue *pv) {
    LNUIattr *lattr = (LNUIattr*)attr;
    lua_State *L = lattr->L;
    int ret = pv->type;
    if (lattr->ref_setattr <= 0) return NUI_TNIL;
    lua_rawgeti(L, LUA_REGISTRYINDEX, lattr->ref_setattr);
    ln_pushnode(L, node);
    ln_pushkey(L, key);
    ln_pushvalue(L, pv);
    if (lbind_pcall(L, 3, 1) != LUA_OK) {
        fprintf(stderr, "%s\n", lua_tostring(L, -1));
        ret = NUI_TNIL;
    }
    else if (!lua_toboolean(L, -1))
        ret = NUI_TNIL;
    lua_pop(L, 1);
    return ret;
}

static void ln_delattr(NUIattr *attr, NUInode *node) {
    LNUIattr *lattr = (LNUIattr*)attr;
    lua_State *L = lattr->L;
    if (lattr->ref_delattr <= 0) return;
    lua_rawgeti(L, LUA_REGISTRYINDEX, lattr->ref_delattr);
    ln_pushnode(L, node);
    if (lbind_pcall(L, 1, 0) != LUA_OK) {
        fprintf(stderr, "%s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    return;
}

static int Lnode_addattr(lua_State *L) {
    int type, idx;
    size_t len;
    NUInode *n = (NUInode*)lbind_check(L, 1, &lbT_Node);
    const char *s = luaL_checklstring(L, 2, &len);
    NUIkey *key = nui_newkey(nui_state(n), s, len);
    LNUIattr *attr;
    lua_settop(L, 5);
    attr = lbind_new(L, sizeof(LNUIattr), &lbT_Attr);
    memset(attr, 0, sizeof(*attr));
    if (!nui_addattr(n, key, &attr->base)) {
        luaL_argerror(L, 2, "attribute already exists");
        return 0;
    }
    if ((type = lua_type(L, idx = 3)) != LUA_TNIL) {
        if (type != LUA_TFUNCTION) goto err;
        lua_pushvalue(L, 3);
        attr->ref_getattr = luaL_ref(L, LUA_REGISTRYINDEX);
        attr->base.get_attr = ln_getattr;
    }
    if ((type = lua_type(L, idx = 4)) != LUA_TNIL) {
        if (type != LUA_TFUNCTION) goto err;
        lua_pushvalue(L, idx);
        attr->ref_getattr = luaL_ref(L, LUA_REGISTRYINDEX);
        attr->base.set_attr = ln_setattr;
    }
    if ((type = lua_type(L, idx = 5)) != LUA_TNIL) {
        if (type != LUA_TFUNCTION) goto err;
        lua_pushvalue(L, idx);
        attr->ref_getattr = luaL_ref(L, LUA_REGISTRYINDEX);
        attr->base.del_attr = ln_delattr;
    }
    return 1;

err:
    nui_delattr(n, key);
    lua_pushfstring(L, "function expected, got %s", lua_typename(L, type));
    luaL_argerror(L, idx, lua_tostring(L, -1));
    return 0;
}

static int Lnode_getattr(lua_State *L) { return 0; }

static int Lnode_delattr(lua_State *L) { return 0; }

static int Lnode_hashf(lua_State *L) { return 0; }


/* node listeners */

static int Lnode_addlistener(lua_State *L) { return 0; }
static int Lnode_dellistener(lua_State *L) { return 0; }


/* node lifetime */

typedef struct LNUInodefreer {
    NUIlistener base;
    lua_State *L;
} LNUInodefreer;

static void ln_removenode(NUIlistener *li, NUInode *n) {
    lua_State *L = ((LNUInodefreer*)li)->L;
    lua_pushnil(L);
    lua_rawsetp(L, LUA_REGISTRYINDEX, n);
}

static NUIlistener *ln_getfreer(lua_State *L) {
    return NULL;
}


/* node */

static int Lnode_new(lua_State *L) {
    NUIstate *S = (NUIstate*)lbind_check(L, 1, &lbT_State);
    int i, count = 0, top = lua_gettop(L);
    NUItype *t[32];
    if (top > 33) top = 33;
    for (i = 2; i <= top; ++i) {
        size_t len;
        const char *s = luaL_checklstring(L, i, &len);
        NUIkey *name = nui_getkey(S, s, len);
        if (name && (t[count] = nui_gettype(S, name)) != NULL)
            ++count;
    }
    lbind_wrap(L, nui_newnode(S, t, count), &lbT_Node);
    return 1;
}

static int Lnode_delete(lua_State *L) {
    NUInode *n = (NUInode*)lbind_test(L, 1, &lbT_Node);
    if (n) {
        nui_release(n);
        lbind_delete(L, 1);
    }
    return 0;
}

static int Lnode_setenv(lua_State *L) {
    NUInode *n = (NUInode*)lbind_test(L, 1, &lbT_Node);
    NUIstate *S = nui_state(n);
    NUInode *cur;
    int i = 1;
    luaL_checktype(L, 2, LUA_TTABLE);
    nui_setchildren(n, NULL);
    while (lua53_rawgeti(L, 2, i) == LUA_TUSERDATA
            && (cur = (NUInode*)lbind_test(L, -1, &lbT_Node)) != NULL) {
        nui_setparent(cur, n);
        lua_pushnil(L);
        lua_rawseti(L, 2, i);
        ++i;
    }
    lua_pop(L, 1);
    if (i == 1) lua_pushnil(L);
    else lua_pushinteger(L, i-1);
    while (lua_next(L, 2)) {
        if (lua_type(L, -2) == LUA_TSTRING) {
            size_t len;
            const char *s = lua_tolstring(L, -2, &len);
            NUIkey *key = nui_getkey(S, s, len);
            NUIvalue *v = NULL; /*XXX*/
            if (!key) {
                nui_set(n, key, v);
                lua_pushvalue(L, -2);
                lua_pushnil(L);
                lua_rawset(L, 2);
            }
        }
        lua_pop(L, 1);
    }
    lua_pushvalue(L, 2);
    lua_setuservalue(L, 1);
    lbind_returnself(L);
}

static int Lnode_arrayf(lua_State *L) {
    NUInode *n = (NUInode*)lbind_check(L, 1, &lbT_Node);
    lua_Integer idx = luaL_checkinteger(L, 2);
    NUInode *old = nui_indexnode(n, (int)idx), *newnode;
    if (lua_isnoneornil(L, 3)) return ln_pushnode(L, old);
    if (old == NULL) luaL_argerror(L, 2, "index out of range");
    newnode = (NUInode*)lbind_check(L, 3, &lbT_Node);
    nui_insert(old, newnode);
    nui_detach(old);
    if (lua53_getuservalue(L, 1) != LUA_TNIL) {
        lua_pushnil(L);
        lua_rawsetp(L, -2, old);
        lua_pushvalue(L, 3);
        lua_rawsetp(L, -2, newnode);
    }
    return 0;
}

static int Lnode_retain(lua_State *L) {
    NUInode *n = (NUInode*)lbind_check(L, 1, &lbT_Node);
    nui_retain(n);
    lbind_returnself(L);
}

static int Lnode_release(lua_State *L) {
    NUInode *n = (NUInode*)lbind_check(L, 1, &lbT_Node);
    if (nui_release(n) == 0)
        lbind_delete(L, 1);
    lbind_returnself(L);
}

static int Lnode_nextchild(lua_State *L) {
    NUInode *n = (NUInode*)lbind_check(L, 1, &lbT_Node);
    NUInode *curr = (NUInode*)lbind_opt(L, 2, NULL, &lbT_Node);
    ln_pushnode(L, nui_nextchild(n, curr));
    return 1;
}

static int Lnode_prevchild(lua_State *L) {
    NUInode *n = (NUInode*)lbind_check(L, 1, &lbT_Node);
    NUInode *curr = (NUInode*)lbind_opt(L, 2, NULL, &lbT_Node);
    ln_pushnode(L, nui_prevchild(n, curr));
    return 1;
}

static int Lnode_nextleaf(lua_State *L) {
    NUInode *n = (NUInode*)lbind_check(L, 1, &lbT_Node);
    NUInode *curr = (NUInode*)lbind_opt(L, 2, NULL, &lbT_Node);
    ln_pushnode(L, nui_nextleaf(n, curr));
    return 1;
}

static int Lnode_prevleaf(lua_State *L) {
    NUInode *n = (NUInode*)lbind_check(L, 1, &lbT_Node);
    NUInode *curr = (NUInode*)lbind_opt(L, 2, NULL, &lbT_Node);
    ln_pushnode(L, nui_prevleaf(n, curr));
    return 1;
}

static int Lnode_firstchild(lua_State *L) {
    NUInode *n = (NUInode*)lbind_check(L, 1, &lbT_Node);
    lbind_checkreadonly(L);
    return ln_pushnode(L, nui_nextchild(n, NULL));
}

static int Lnode_lastchild(lua_State *L) {
    NUInode *n = (NUInode*)lbind_check(L, 1, &lbT_Node);
    lbind_checkreadonly(L);
    return ln_pushnode(L, nui_prevchild(n, NULL));
}

static int Lnode_prevsibling(lua_State *L) {
    NUInode *n = (NUInode*)lbind_check(L, 1, &lbT_Node);
    lbind_checkreadonly(L);
    return ln_pushnode(L, nui_prevsibling(NULL, n));
}

static int Lnode_nextsibling(lua_State *L) {
    NUInode *n = (NUInode*)lbind_check(L, 1, &lbT_Node);
    lbind_checkreadonly(L);
    return ln_pushnode(L, nui_nextsibling(NULL, n));
}

static int Lnode_root(lua_State *L) {
    NUInode *n = (NUInode*)lbind_check(L, 1, &lbT_Node);
    lbind_checkreadonly(L);
    return ln_pushnode(L, nui_root(n));
}

static int Lnode_state(lua_State *L) {
    NUInode *n = (NUInode*)lbind_check(L, 1, &lbT_Node);
    NUIstate *S = nui_state(n);
    lbind_checkreadonly(L);
    if (lbind_retrieve(L, S))
        lbind_wrap(L, S, &lbT_State);
    return 1;
}

static int Lnode_index(lua_State *L) {
    NUInode *n = (NUInode*)lbind_check(L, 1, &lbT_Node);
    lbind_checkreadonly(L);
    lua_pushinteger(L, nui_childcount(n));
    return 1;
}

static int Lnode_childcount(lua_State *L) {
    NUInode *n = (NUInode*)lbind_check(L, 1, &lbT_Node);
    int idx;
    lbind_checkreadonly(L);
    if ((idx = nui_nodeindex(n)) < 0)
        return 0;
    lua_pushinteger(L, idx + 1);
    return 1;
}

static int Lnode_parent(lua_State *L) {
    NUInode *n = (NUInode*)lbind_check(L, 1, &lbT_Node);
    NUInode *parent;
    if (lua_isnoneornil(L, 3))
        return ln_pushnode(L, nui_parent(n));
    parent = (NUInode*)lbind_check(L, 3, &lbT_Node);
    nui_setparent(n, parent);
    if (lua53_getuservalue(L, 3) != LUA_TNIL) {
        lua_pushvalue(L, 1);
        lua_rawsetp(L, -2, parent);
    }
    return 0;
}

static int Lnode_append(lua_State *L) {
    NUInode *n = (NUInode*)lbind_check(L, 1, &lbT_Node);
    NUInode *newnode = (NUInode*)lbind_check(L, 2, &lbT_Node);
    nui_append(n, newnode);
    lbind_returnself(L);
}

static int Lnode_insert(lua_State *L) {
    NUInode *n = (NUInode*)lbind_check(L, 1, &lbT_Node);
    NUInode *newnode = (NUInode*)lbind_check(L, 2, &lbT_Node);
    nui_insert(n, newnode);
    lbind_returnself(L);
}

static int Lnode_detach(lua_State *L) {
    NUInode *n = (NUInode*)lbind_check(L, 1, &lbT_Node);
    nui_detach(n);
    lbind_returnself(L);
}

static void open_node(lua_State *L) {
#define ENTRY(name) { #name, Lnode_##name }
    luaL_Reg libs[] = {
        { "__call", Lnode_setenv },
        { "__gc", Lnode_delete },
        { "__len", Lnode_childcount },
        ENTRY(new),
        ENTRY(delete),
        ENTRY(setenv),
        ENTRY(retain),
        ENTRY(release),
        ENTRY(addlistener),
        ENTRY(dellistener),
        ENTRY(nextchild),
        ENTRY(prevchild),
        ENTRY(nextleaf),
        ENTRY(prevleaf),
        ENTRY(append),
        ENTRY(insert),
        ENTRY(detach),
        { NULL, NULL }
    };
    luaL_Reg props[] = {
        ENTRY(parent),
        ENTRY(firstchild),
        ENTRY(lastchild),
        ENTRY(prevsibling),
        ENTRY(nextsibling),
        ENTRY(root),
        ENTRY(state),
        ENTRY(index),
        ENTRY(childcount),
        { NULL, NULL }
    };
#undef  ENTRY
    if (lbind_newmetatable(L, libs, &lbT_Node)) {
        lbind_setmaptable(L, props, LBIND_INDEX | LBIND_NEWINDEX);
        lbind_sethashf(L, Lnode_hashf, LBIND_INDEX | LBIND_NEWINDEX);
        lbind_setarrayf(L, Lnode_arrayf, LBIND_INDEX | LBIND_NEWINDEX);
    }
}


/* state */

static NUItime ln_time(NUIstate *S) { return 0; }
static int ln_wait(NUIstate *S, NUItime time) { return 0; }

static int Lstate_new(lua_State *L) {
    LNUIstate *LS;
    lua_settop(L, 1);
    LS = (LNUIstate*)lbind_new(L, sizeof(LNUIstate), &lbT_State);
    memset(LS, 0, sizeof(LNUIstate));
    LS->L = L;
    if (lua_isnoneornil(L, 1)) {
        LS->ref_time = LUA_NOREF;
        LS->ref_wait = LUA_NOREF;
    }
    else {
        luaL_checktype(L, 1, LUA_TTABLE);
        if (lua53_getfield(L, 1, "time") != LUA_TNIL) {
            LS->ref_time = luaL_ref(L, LUA_REGISTRYINDEX);
            LS->params.time = ln_time;
        }
        if (lua53_getfield(L, 1, "wait") != LUA_TNIL) {
            LS->ref_wait = luaL_ref(L, LUA_REGISTRYINDEX);
            LS->params.wait = ln_wait;
        }
        lua_pop(L, 2);
    }
    LS->S = nui_newstate(&LS->params);
    return 1;
}

static int Lstate_delete(lua_State *L) { return 0; }
static int Lstate_addpoller(lua_State *L) { return 0; }
static int Lstate_delpoller(lua_State *L) { return 0; }
static int Lstate_pollevents(lua_State *L) { return 0; }
static int Lstate_waitevents(lua_State *L) { return 0; }
static int Lstate_loop(lua_State *L) { return 0; }
static int Lstate_breakloop(lua_State *L) { return 0; }

static void open_state(lua_State *L) {
    luaL_Reg libs[] = {
#define ENTRY(name) { #name, Lstate_##name }
        ENTRY(new),
        ENTRY(delete), ENTRY(addpoller),
        ENTRY(delpoller),
        ENTRY(pollevents),
        ENTRY(waitevents),
        ENTRY(loop),
        ENTRY(breakloop),
#undef  ENTRY
        { NULL, NULL }
    };
    lbind_newmetatable(L, libs, &lbT_Node);
}


LUALIB_API int luaopen_nui(lua_State *L) {
    open_timer(L);
    open_node(L);
    open_type(L);
    open_state(L);
    return 1;
}
/* win32cc: flags+='-mdll -s -O3 -DLUA_BUILD_AS_DLL'
 * win32cc: libs+='-llua53' output='nui.dll'
 * maccc: flags+='-bundle -O2 -undefined dynamic_lookup'
 * maccc: libs+='-llua53' output='nui.dll' */
