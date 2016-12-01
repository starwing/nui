#define LUA_LIB
#define LBIND_STATIC_API
#define LBIND_DEFAULT_FLAG (LBIND_TRACK|LBIND_INTERN|LBIND_ACCESSOR)
#include "lbind.h"

#define NUI_IMPLEMENTATION
#include "nui.h"

LBIND_TYPE(lbT_State, "nui.State");
LBIND_TYPE(lbT_Timer, "nui.Timer");
LBIND_TYPE(lbT_Node,  "nui.Node");
LBIND_TYPE(lbT_Attr,  "nui.Attribute");
LBIND_TYPE(lbT_Event, "nui.Event");
LBIND_TYPE(lbT_Type,  "nui.Type");


/* utils */

static void ln_unref(lua_State *L, int *ref)
{ luaL_unref(L, LUA_REGISTRYINDEX, *ref); *ref = LUA_NOREF; }

static void ln_ref(lua_State *L, int idx, int *ref) {
    lua_pushvalue(L, idx);
    if (*ref <= 0) *ref = luaL_ref(L, LUA_REGISTRYINDEX);
    else lua_rawseti(L, LUA_REGISTRYINDEX, *ref);
}

static NUIkey *ln_checkkey(NUIstate *S, lua_State *L, int idx) {
    size_t len;
    const char *s = luaL_checklstring(L, idx, &len);
    return nui_newkey(S, s, len);
}

static NUIkey *ln_testkey(NUIstate *S, lua_State *L, int idx) {
    size_t len;
    const char *s = lua_tolstring(L, idx, &len);
    if (s == NULL) return NULL;
    return nui_newkey(S, s, len);
}

static NUItype *ln_checktype(NUIstate *S, lua_State *L, int idx) {
    NUIkey *key = ln_testkey(S, L, idx);
    if (key == NULL) return (NUItype*)lbind_check(L, idx, &lbT_Type);
    return nui_gettype(S, key);
}


/* global state for Lua */

#define LNUI_LUA_KEYS(X) \
    X(LuaState) X(target) X(child) X(parent) X(node) \
    X(delete_node)

enum LNUIluakeys {
#define X(str) LNUI_##str,
    LNUI_LUA_KEYS(X)
#undef  X
    LNUI_KEY_MAX
};

typedef struct LNUIlua {
    NUItype base;
    lua_State *L;
    NUIstate *S;
    int handlers;
    int objects;
    int events;
    NUIpool attrpool;
    NUIkey *keys[LNUI_KEY_MAX];
} LNUIlua;

typedef struct LNUIstate {
    NUIparams params;
    LNUIlua *ls;
    int time_ref;
    int wait_ref;
    NUItime (*default_time) (NUIparams *S);
    int     (*default_wait) (NUIparams *S, NUItime time);
} LNUIstate;

typedef struct LNUIattr {
    NUIattr base;
    LNUIlua *ls;
    int used_count;
    int ref;
    int setattr_ref;
    int getattr_ref;
    int delattr_ref;
} LNUIattr;

typedef struct LNUIevent {
    NUIevent *current;
    NUIevent  evt;
    LNUIlua  *ls;
} LNUIevent;

static NUIstate *ln_checkstate(lua_State *L, int idx)
{ return ((LNUIstate*)lbind_check(L, idx, &lbT_State))->params.S; }

static LNUIlua *ln_statefromS(NUIstate *S)
{ return (LNUIlua*)nui_gettype(S, NUI_(LuaState)); }

static void ln_closelua(NUIparams *params) {
    LNUIstate *LS = (LNUIstate*)params;
    LNUIlua *ls = LS->ls;
    lua_State *L = ls->L;
    int i;
    if (L == NULL) return;
#ifdef LNUI_delete_from_lua
    lua_rawgetp(L, LUA_REGISTRYINDEX, ls);
    if (lua_isuserdata(L, -1))
        *(NUIstate**)lua_touserdata(L, -1) = NULL;
    lua_pop(L, 1);
    lua_pushnil(L);
    lua_rawsetp(L, LUA_REGISTRYINDEX, ls);
#endif /* LNUI_delete_from_lua */
    lua_pushnil(L);
    lua_rawsetp(L, LUA_REGISTRYINDEX, ls->S);
    lua_rawgeti(L, LUA_REGISTRYINDEX, ls->objects);
    lua_pushnil(L); /* clear objects table */
    while (lua_next(L, -2)) {
        void *p = lua_touserdata(L, -2);
        lua_CFunction f = lua_tocfunction(L, -1);
        if (f) f(L);
        if (!lbind_retrieve(L, p)) { lua_pop(L, 1); continue; }
        lbind_delete(L, -1);
        lua_pop(L, 2);
    }
    lua_rawgeti(L, LUA_REGISTRYINDEX, ls->events);
    lua_pushnil(L); /* clear event table */
    while (lua_next(L, -2)) {
        LNUIevent *levt = (LNUIevent*)lbind_test(L, -1, &lbT_Event);
        if (levt->current == &levt->evt)
            nui_freeevent(levt->ls->S, levt->current);
        levt->current = NULL;
        lbind_delete(L, 1);
        lua_pop(L, 1);
    }
    ln_unref(L, &ls->objects);
    ln_unref(L, &ls->handlers);
    ln_unref(L, &ls->events);
    nui_freepool(ls->S, &ls->attrpool);
    for (i = 0; i < LNUI_KEY_MAX; ++i)
        nui_delkey(ls->S, ls->keys[i]);
    ls->L = NULL;
    ls->S = NULL;
}

#ifdef LNUI_delete_from_lua
static int ln_closeluafromL(lua_State *L) {
    LNUIstate **pLS = (LNUIstate**)lua_touserdata(L, 1);
    if (pLS && *pLS) ln_closelua(&(*pLS)->params);
    return 0;
}
#endif /* LNUI_delete_from_lua */

static void ln_nodedeletor(NUItype *t, NUInode *n, NUIcomp *comp) {
    LNUIlua *ls = (LNUIlua*)t;
    lua_State *L = ls->L;
    if (L == NULL) return;
    if (lbind_retrieve(L, n)) {
        lbind_delete(L, -1);
        lua_pop(L, 1);
    }
}

static LNUIlua *ln_newlua(lua_State *L, LNUIstate *LS) {
    NUIstate *S = LS->params.S;
    LNUIlua *ls = LS->ls;
    NUIkey *key = NUI_(LuaState);
    if (ls != NULL) return ls;
    ls = (LNUIlua*)nui_newtype(S, key, sizeof(LNUIlua), 0);
    if (ls == NULL) { /* already have a state? */
        ls = (LNUIlua*)nui_gettype(S, key);
        if (ls->L != NULL) luaL_error(L, "already have a LuaState(%p) "
                "bind with this NUIstate(%p)", ls->L, S);
    }
    ls->L = L;
    ls->S = S;
    ls->base.del_comp = ln_nodedeletor;
#ifdef LNUI_delete_from_lua
    *(LNUIstate**)lua_newuserdata(L, sizeof(LNUIstate*)) = LS;
    lua_pushcfunction(L, ln_closeluafromL);
    lbind_setmetafield(L, -2, "__gc");
    lua_rawsetp(L, LUA_REGISTRYINDEX, ls);
#endif /* LNUI_delete_from_lua */
    lua_newtable(L); ls->objects = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_newtable(L); ls->handlers = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_newtable(L); ls->events = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_pushstring(L, "v");
    lbind_setmetafield(L, -2, "__mode");
    nui_initpool(&ls->attrpool, sizeof(LNUIattr));
#define X(str) ls->keys[LNUI_##str] = nui_usekey(NUI_(str));
    LNUI_LUA_KEYS(X)
#undef  X
    lua_pushlightuserdata(L, ls);
    lua_rawsetp(L, LUA_REGISTRYINDEX, S);
    return LS->ls = ls;
}

static void ln_refobject(LNUIlua *ls, void *p, lua_CFunction deletor) {
    lua_State *L = ls->L;
    lua_rawgeti(L, LUA_REGISTRYINDEX, ls->objects);
    if (deletor) lua_pushcfunction(L, deletor);
    else lua_pushboolean(L, 1);
    lua_rawsetp(L, -2, p);
    lua_pop(L, 1);
}

static void ln_unrefobject(LNUIlua *ls, void *p) {
    lua_State *L = ls->L;
    lua_rawgeti(L, LUA_REGISTRYINDEX, ls->objects);
    lua_pushnil(L);
    lua_rawsetp(L, -2, p);
    lua_pop(L, 1);
}

static int ln_pushnode(lua_State *L, NUInode *n) {
    NUIstate *S;
    if (n == NULL) return 0;
    if (n == nui_rootnode(S = nui_state(n))) {
        LNUIlua *ls = ln_statefromS(S);
        lua_rawgeti(L, LUA_REGISTRYINDEX, ls->objects);
        if (lua53_rawgetp(L, -1, S) == LUA_TNIL) {
            lua_pop(L, 1);
            lbind_wrapraw(L, S, 0);
            lbind_setmetatable(L, &lbT_Node);
            lua_pushvalue(L, -1);
            lua_rawsetp(L, -3, S);
        }
        lua_remove(L, -2);
    }
    else if (!lbind_retrieve(L, n))
        lbind_wrap(L, n, &lbT_Node);
    return 1;
}


/* timer */

typedef struct LNUItimer {
    NUItimer *timer;
    LNUIlua *ls;
    int ref;
    int ontimer_ref;
    NUItime delayms;
} LNUItimer;

static NUItime ln_ontimer(void *ud, NUItimer *timer, NUItime elapsed) {
    LNUItimer *lt = (LNUItimer*)ud;
    lua_State *L = lt->ls->L;
    lua_rawgeti(L, LUA_REGISTRYINDEX, lt->ontimer_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, lt->ref);
    lua_pushinteger(L, (lua_Integer)elapsed);
    if (lbind_pcall(L, 2, 1) != LUA_OK) {
        fprintf(stderr, "%s\n", lua_tostring(L, -1));
        ln_unref(L, &lt->ref);
    }
    else if (lua_isnumber(L, -1)) {
        lua_Integer ret = lua_tointeger(L, -1);
        return ret >= 0 ? (NUItime)ret : 0;
    }
    else if (lua_toboolean(L, -1))
        nui_starttimer(timer, lt->delayms);
    else
        ln_unref(L, &lt->ref);
    lua_pop(L, 1);
    return 0;
}

static int Ltimer_new(lua_State *L) {
    NUIstate *S = ln_checkstate(L, 1);
    LNUIlua *ls = ln_statefromS(S);
    LNUItimer *lt;
    luaL_checktype(L, 2, LUA_TFUNCTION);
    lt = (LNUItimer*)lbind_new(L, sizeof(LNUItimer), &lbT_Timer);
    lt->timer = nui_newtimer(S, ln_ontimer, lt);
    lt->ls = ls;
    lt->ontimer_ref = LUA_NOREF;
    lt->ref = LUA_NOREF;
    ln_ref(L, 2, &lt->ontimer_ref);
    ln_refobject(ln_statefromS(S), lt, NULL);
    return 1;
}

static int Ltimer_delete(lua_State *L) {
    LNUItimer *lt = (LNUItimer*)lbind_test(L, 1, &lbT_Timer);
    if (lt && lt->timer != NULL) {
        ln_unrefobject(lt->ls, lt);
        nui_deltimer(lt->timer);
        lbind_delete(L, 1);
        ln_unref(L, &lt->ontimer_ref);
        ln_unref(L, &lt->ref);
        lt->timer = NULL;
    }
    return 0;
}

static int Ltimer_start(lua_State *L) {
    LNUItimer *lt = (LNUItimer*)lbind_check(L, 1, &lbT_Timer);
    lua_Integer delayms = luaL_optinteger(L, 2, 0);
    if (!lt->timer) return 0;
    if (delayms < 0) delayms = 0;
    lt->delayms = (NUItime)delayms;
    if (nui_starttimer(lt->timer, lt->delayms))
        ln_ref(L, 1, &lt->ref);
    lbind_returnself(L);
}

static int Ltimer_cancel(lua_State *L) {
    LNUItimer *lt = (LNUItimer*)lbind_check(L, 1, &lbT_Timer);
    if (!lt->timer) return 0;
    ln_unref(L, &lt->ref);
    nui_canceltimer(lt->timer);
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
    if (lbind_newmetatable(L, libs, &lbT_Timer))
        lbind_setlibcall(L, NULL);
}


/* type */

typedef struct LNUItype {
    NUItype base;
    LNUIlua *ls;
    lua_State *L;
} LNUItype;

typedef struct LNUIcomp {
    NUIcomp base;
    int ref;
} LNUIcomp;

static int ln_newcomp(NUItype *t, NUInode *n, NUIcomp *comp) {
    LNUIcomp *lc = (LNUIcomp*)comp;
    lua_State *L = ((LNUItype*)t)->L;
    int ret = 0;
    if (L == NULL) return 0;
    if (lbind_self(L, t, "newcomp", 1, NULL)) {
        if (!ln_pushnode(L, n)) lua_pushnil(L);
        if (lbind_pcall(L, 2, 1) != LUA_OK)
            fprintf(stderr, "%s\n", lua_tostring(L, -1));
        else if (lua_toboolean(L, -1)) {
            ln_ref(L, -1, &lc->ref);
            ret = 1;
        }
        lua_pop(L, 1);
    }
    return ret;
}

static void ln_delcomp(NUItype *t, NUInode *n, NUIcomp *comp) {
    LNUIcomp *lc = (LNUIcomp*)comp;
    lua_State *L = ((LNUItype*)t)->L;
    if (L == NULL) return;
    if (lbind_self(L, t, "delcomp", 2, NULL)) {
        if (!ln_pushnode(L, n)) lua_pushnil(L);
        lua_rawseti(L, LUA_REGISTRYINDEX, lc->ref);
        if (lbind_pcall(L, 3, 0) != LUA_OK) {
            fprintf(stderr, "%s\n", lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    }
}

static void ln_closetype(NUItype *t, NUIstate *S) {
    lua_State *L = ((LNUItype*)t)->L;
    if (L == NULL) return;
    if (lbind_self(L, t, "close", 0, NULL)
            && lbind_pcall(L, 1, 0) != LUA_OK) {
        fprintf(stderr, "%s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
}

static void ln_freetype(lua_State *L, LNUItype *t) {
    lua_pushnil(L);
    lua_rawsetp(L, LUA_REGISTRYINDEX, t);
    t->base.new_comp = NULL;
    t->L = NULL;
}

static int ln_typedeletor(lua_State *L) {
    ln_freetype(L, (LNUItype*)lua_touserdata(L, -2));
    return 0;
}

static int Ltype_new(lua_State *L) {
    NUIstate *S = ln_checkstate(L, 1);
    LNUIlua *ls = ln_statefromS(S);
    NUIkey *key = ln_checkkey(S, L, 2);
    LNUItype *t = (LNUItype*)nui_newtype(S, key, sizeof(LNUItype), sizeof(LNUIcomp));
    t->ls = ls;
    t->L = L;
    t->base.new_comp = ln_newcomp;
    t->base.del_comp = ln_delcomp;
    t->base.close    = ln_closetype;
    lbind_wrap(L, t, &lbT_Type);
    lua_pushvalue(L, -1);
    lua_rawsetp(L, LUA_REGISTRYINDEX, t);
    ln_refobject(ls, t, ln_typedeletor);
    return 1;
}

static int Ltype_delete(lua_State *L) {
    LNUItype *t = (LNUItype*)lbind_test(L, 1, &lbT_Type);
    if (t) {
        ln_freetype(L, t);
        ln_unrefobject(t->ls, t);
        lbind_delete(L, 1);
        t->base.new_comp = NULL;
        t->L = NULL;
    }
    return 0;
}

static int Ltype_setenv(lua_State *L) {
    lbind_check(L, 1, &lbT_Type);
    luaL_checktype(L, 2, LUA_TTABLE);
    lua_setuservalue(L, 1);
    lbind_returnself(L);
}

static int Ltype_get(lua_State *L) {
    NUIstate *S = ln_checkstate(L, 1);
    LNUIlua *ls = ln_statefromS(S);
    NUIkey *key = ln_testkey(S, L, 2);
    NUItype *t = nui_gettype(S, key);
    if (t == NULL) return 0;
    lua_rawgeti(L, LUA_REGISTRYINDEX, ls->objects);
    if (lua_rawgetp(L, -1, t) == LUA_TNIL)
        lua_pushlightuserdata(L, t);
    else if (!lbind_retrieve(L, t)) {
        ln_refobject(ls, t, NULL);
        lbind_wrap(L, t, &lbT_Type);
    }
    return 1;
}

static int Lnode_addcomp(lua_State *L) {
    NUInode *n = (NUInode*)lbind_check(L, 1, &lbT_Node);
    LNUIlua *ls = ln_statefromS(nui_state(n));
    NUItype *t = ln_checktype(nui_state(n), L, 2);
    if (t == NULL) return 0;
    lua_rawgeti(L, LUA_REGISTRYINDEX, ls->objects);
    if (lua_rawgetp(L, -1, t) == LUA_TNIL)
        lua_pushlightuserdata(L, nui_addcomp(n, t));
    else {
        LNUIcomp *lc = (LNUIcomp*)nui_addcomp(n, t);
        lua_rawgeti(L, LUA_REGISTRYINDEX, lc->ref);
    }
    return 1;
}

static int Lnode_getcomp(lua_State *L) {
    NUInode *n = (NUInode*)lbind_check(L, 1, &lbT_Node);
    LNUIlua *ls = ln_statefromS(nui_state(n));
    NUItype *t = ln_checktype(ls->S, L, 2);
    if (t == NULL) return 0;
    lua_rawgeti(L, LUA_REGISTRYINDEX, ls->objects);
    if (lua_rawgetp(L, -1, t) == LUA_TNIL)
        lua_pushlightuserdata(L, nui_getcomp(n, t));
    else {
        LNUIcomp *lc = (LNUIcomp*)nui_getcomp(n, t);
        lua_rawgeti(L, LUA_REGISTRYINDEX, lc->ref);
    }
    return 1;
}

static void open_type(lua_State *L) {
    luaL_Reg libs[] = {
        { "__call", Ltype_setenv },
        { "__gc", Ltype_delete },
#define ENTRY(name) { #name, Ltype_##name }
        ENTRY(new),
        ENTRY(delete),
        ENTRY(get),
#undef  ENTRY
        { NULL, NULL }
    };
    if (lbind_newmetatable(L, libs, &lbT_Type))
        lbind_setlibcall(L, NULL);
}


/* attributes */

static void ln_refattr(lua_State *L, LNUIattr *lattr, int idx)
{ if (lattr->used_count++ <= 0) ln_ref(L, idx, &lattr->ref); }

static void ln_unrefattr(lua_State *L, LNUIattr *lattr)
{ if (--lattr->used_count <= 0) ln_unref(L, &lattr->ref); }

static NUIdata *ln_getattr(NUIattr *attr, NUInode *node, NUIkey *key) {
    LNUIattr *lattr = (LNUIattr*)attr;
    lua_State *L = lattr->ls->L;
    NUIdata *ret = NULL;
    if (lattr->getattr_ref <= 0) return NULL;
    lua_rawgeti(L, LUA_REGISTRYINDEX, lattr->getattr_ref);
    ln_pushnode(L, node);
    lua_pushstring(L, (const char*)key);
    if (lbind_pcall(L, 2, 1) != LUA_OK)
        fprintf(stderr, "%s\n", lua_tostring(L, -1));
    else {
        size_t len;
        const char *s = lua_tolstring(L, -1, &len);
        if (s) ret = nui_newdata(lattr->ls->S, s, len);
    }
    lua_pop(L, 1);
    return ret;
}

static int ln_setattr(NUIattr *attr, NUInode *node, NUIkey *key, const char *v) {
    LNUIattr *lattr = (LNUIattr*)attr;
    lua_State *L = lattr->ls->L;
    int ret = 0;
    if (lattr->setattr_ref <= 0) return 0;
    lua_rawgeti(L, LUA_REGISTRYINDEX, lattr->setattr_ref);
    ln_pushnode(L, node);
    lua_pushstring(L, (const char*)key);
    lua_pushstring(L, v);
    if (lbind_pcall(L, 4, 1) != LUA_OK)
        fprintf(stderr, "%s\n", lua_tostring(L, -1));
    else
        ret = lua_toboolean(L, -1);
    lua_pop(L, 1);
    return ret;
}

static void ln_delattr(NUIattr *attr, NUInode *node) {
    LNUIattr *lattr = (LNUIattr*)attr;
    LNUIlua *ls = lattr->ls;
    lua_State *L = ls->L;
    if (lattr->delattr_ref <= 0) return;
    lua_rawgeti(L, LUA_REGISTRYINDEX, lattr->delattr_ref);
    ln_pushnode(L, node);
    if (lbind_pcall(L, 1, 0) != LUA_OK) {
        fprintf(stderr, "%s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    ln_unrefattr(L, lattr);
    return;
}

static void ln_freeattr(lua_State *L, LNUIattr *lattr) {
    assert(lattr->used_count == 0);
    ln_unrefobject(lattr->ls, lattr);
    ln_unref(L, &lattr->getattr_ref);
    ln_unref(L, &lattr->setattr_ref);
    ln_unref(L, &lattr->delattr_ref);
    lattr->base.get_attr = NULL;
    lattr->base.set_attr = NULL;
    lattr->base.del_attr = NULL;
    nui_pfree(&lattr->ls->attrpool, lattr);
}

static int ln_attrdeletor(lua_State *L) {
    ln_freeattr(L, (LNUIattr*)lua_touserdata(L, -2));
    return 0;
}

static LNUIattr *ln_newattr(lua_State *L, NUIstate *S, int idx) {
    LNUIlua *ls = ln_statefromS(S);
    LNUIattr *lattr = (LNUIattr*)nui_palloc(S, &ls->attrpool);
    int type, top = idx;
    assert(idx > 0);
    lua_settop(L, idx+3);
    memset(lattr, 0, sizeof(*lattr));
    lattr->ls = ls;
    if ((type = lua_type(L, idx++)) != LUA_TNIL) {
        if (type != LUA_TFUNCTION) goto not_func;
        lua_pushvalue(L, 3);
        lattr->getattr_ref = luaL_ref(L, LUA_REGISTRYINDEX);
        lattr->base.get_attr = ln_getattr;
    }
    if ((type = lua_type(L, idx++)) != LUA_TNIL) {
        if (type != LUA_TFUNCTION) goto not_func;
        lua_pushvalue(L, idx);
        lattr->setattr_ref = luaL_ref(L, LUA_REGISTRYINDEX);
        lattr->base.set_attr = ln_setattr;
    }
    if ((type = lua_type(L, idx++)) != LUA_TNIL) {
        if (type != LUA_TFUNCTION) goto not_func;
        lua_pushvalue(L, idx);
        lattr->delattr_ref = luaL_ref(L, LUA_REGISTRYINDEX);
        lattr->base.del_attr = ln_delattr;
    }
    lua_settop(L, top);
    lbind_wrap(L, lattr, &lbT_Attr);
    ln_refobject(ls, lattr, ln_attrdeletor);
    return lattr;

not_func:
    lua_pushfstring(L, "function expected, got %s", lua_typename(L, type));
    luaL_argerror(L, idx, lua_tostring(L, -1));
    return NULL;
}

static int Lattr_new(lua_State *L) {
    ln_newattr(L, ln_checkstate(L, 1), 2);
    return 1;
}

static int Lattr_delete(lua_State *L) {
    LNUIattr *lattr = (LNUIattr*)lbind_test(L, 1, &lbT_Attr);
    if (lattr && lattr->used_count <= 0) {
        ln_freeattr(L, lattr);
        lbind_delete(L, 1);
    }
    return 0;
}

static int Lattr_getattr(lua_State *L) {
    LNUIattr *lattr = (LNUIattr*)lbind_check(L, 1, &lbT_Attr);
    if (lua_gettop(L) == 2) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, lattr->getattr_ref);
        return 1;
    }
    luaL_checktype(L, 3, LUA_TFUNCTION);
    ln_ref(L, 3, &lattr->getattr_ref);
    return 0;
}

static int Lattr_setattr(lua_State *L) {
    LNUIattr *lattr = (LNUIattr*)lbind_check(L, 1, &lbT_Attr);
    if (lua_gettop(L) == 2) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, lattr->setattr_ref);
        return 1;
    }
    luaL_checktype(L, 3, LUA_TFUNCTION);
    ln_ref(L, 3, &lattr->setattr_ref);
    return 0;
}

static int Lattr_delattr(lua_State *L) {
    LNUIattr *lattr = (LNUIattr*)lbind_check(L, 1, &lbT_Attr);
    if (lua_gettop(L) == 2) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, lattr->delattr_ref);
        return 1;
    }
    luaL_checktype(L, 3, LUA_TFUNCTION);
    ln_ref(L, 3, &lattr->delattr_ref);
    return 0;
}

static int Lnode_setattr(lua_State *L) {
    NUInode *n = (NUInode*)lbind_check(L, 1, &lbT_Node);
    LNUIlua *ls = ln_statefromS(nui_state(n));
    NUIkey *key = ln_checkkey(nui_state(n), L, 2);
    LNUIattr *lattr = (LNUIattr*)lbind_test(L, 3, &lbT_Attr);
    lua_settop(L, 3);
    if (!lattr) lattr = ln_newattr(L, ls->S, 3);
    if (!nui_setattr(n, key, &lattr->base)) return 0;
    ln_refattr(L, lattr, 3);
    return 1;
}

static int Lnode_getattr(lua_State *L) {
    NUInode *n = (NUInode*)lbind_check(L, 1, &lbT_Node);
    NUIkey *key = ln_checkkey(nui_state(n), L, 2);
    LNUIattr *lattr = (LNUIattr*)nui_getattr(n, key);
    if (lattr == NULL) return 0;
    if (!lbind_retrieve(L, lattr))
        lua_pushlightuserdata(L, lattr);
    return 1;
}

static int Lnode_delattr(lua_State *L) {
    NUInode *n = (NUInode*)lbind_check(L, 1, &lbT_Node);
    NUIkey *key = ln_checkkey(nui_state(n), L, 2);
    NUIattr *attr = nui_delattr(n, key);
    if (attr == NULL) return 0;
    if (!lbind_retrieve(L, attr))
        lua_pushlightuserdata(L, attr);
    return 1;
}

static int Lnode_addattrhandler(lua_State *L) {
    NUInode *n = (NUInode*)lbind_check(L, 1, &lbT_Node);
    LNUIattr *lattr = (LNUIattr*)lbind_check(L, 2, &lbT_Attr);
    nui_addattrhandler(n, &lattr->base);
    ln_refattr(L, lattr, 2);
    lbind_returnself(L);
}

static int Lnode_delattrhandler(lua_State *L) {
    NUInode *n = (NUInode*)lbind_check(L, 1, &lbT_Node);
    LNUIattr *lattr = (LNUIattr*)lbind_check(L, 2, &lbT_Attr);
    if (!nui_delattrhandler(n, &lattr->base)) return 0;
    ln_unrefattr(L, lattr);
    lbind_returnself(L);
}

static int Lnode_hashf(lua_State *L) {
    NUInode *n = (NUInode*)lbind_check(L, 1, &lbT_Node);
    NUIkey *key = ln_checkkey(nui_state(n), L, 2);
    if (lua_gettop(L) == 2) {
        NUIdata *ret = nui_get(n, key);
        if (!ret) return -1;
        lua_pushlstring(L, (char*)ret, nui_len(ret));
        return 1;
    }
    else {
        const char *v = luaL_checkstring(L, 3);
        if (!nui_set(n, key, v)) return -1;
        return 0;
    }
}

static void open_attr(lua_State *L) {
#define ENTRY(name) { #name, Lattr_##name }
    luaL_Reg libs[] = {
        { "__gc", Lattr_delete },
        ENTRY(new),
        ENTRY(delete),
        { NULL, NULL }
    };
    luaL_Reg props[] = {
        ENTRY(getattr),
        ENTRY(setattr),
        ENTRY(delattr),
        { NULL, NULL }
    };
#undef  ENTRY
    if (lbind_newmetatable(L, libs, &lbT_Attr)) {
        lbind_setmaptable(L, props, LBIND_INDEX | LBIND_NEWINDEX);
        lbind_setlibcall(L, NULL);
    }
}


/* events */

static LNUIevent *ln_retrieve(lua_State *L, NUIstate *S, const NUIevent *evt) {
    LNUIlua *ls = ln_statefromS(S);
    LNUIevent *levt;
    lua_rawgeti(L, LUA_REGISTRYINDEX, ls->events);
    lua_rawgetp(L, -1, evt);
    if ((levt = (LNUIevent*)lbind_object(L, -1)) == NULL)
        lua_pop(L, 2);
    else
        lua_remove(L, -2);
    return levt;
}

static NUIevent *ln_checkevent(lua_State *L, int idx) {
    LNUIevent *levt = lbind_check(L, idx, &lbT_Event);
    if (levt->current == NULL)
        luaL_argerror(L, idx, "expired event object");
    return levt->current;
}

static void ln_registerevent(lua_State *L, LNUIlua *ls, NUIevent *evt) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, ls->events);
    lua_pushvalue(L, -2);
    lua_rawsetp(L, -2, evt);
    lua_pop(L, 1);
}

static int Levent_tostring(lua_State *L) {
    LNUIevent *levt = lbind_test(L, 1, &lbT_Event);
    if (levt == NULL) { lbind_tostring(L, 1); return 1; }
    lua_pushfstring(L, "%s%s: %p",
            lbT_Event.name,
            levt->current == NULL ? "[D]" : "",
            levt->current);
    return 1;
}

static int Levent_new(lua_State *L) {
    NUIstate *S = ln_checkstate(L, 1);
    LNUIlua *ls = ln_statefromS(S);
    NUIkey *key = ln_checkkey(S, L, 2);
    int bubbles = lua_toboolean(L, 3);
    int cancelable = lua_toboolean(L, 4);
    LNUIevent *levt = (LNUIevent*)lbind_new(L, sizeof(LNUIevent), &lbT_Event);
    nui_initevent(&levt->evt, key, bubbles, cancelable);
    levt->current = &levt->evt;
    levt->ls = ls;
    ln_registerevent(L, ls, levt->current);
    return 1;
}

static int Levent_delete(lua_State *L) {
    LNUIevent *levt = (LNUIevent*)lbind_test(L, 1, &lbT_Event);
    if (levt != NULL) {
        if (levt->current == &levt->evt)
            nui_freeevent(levt->ls->S, levt->current);
        lbind_delete(L, 1);
        levt->current = NULL;
    }
    return 0;
}

static int Levent_hashf(lua_State *L) {
    LNUIevent *levt = lbind_check(L, 1, &lbT_Event);
    LNUIlua *ls = levt->ls;
    NUIstate *S = ls->S;
    NUIptrentry *e;
    int i, isnode = 0;
    NUIkey *key;
    if (levt->current == NULL)
        luaL_argerror(L, 1, "expired event object");
    key = ln_checkkey(S, L, 2);
    e = (NUIptrentry*)nui_gettable(nui_eventdata(levt->current), key);
    for (i = 1; i < LNUI_KEY_MAX; ++i)
        if (key == ls->keys[i]) { isnode = 1; break; }
    if (lua_gettop(L) == 2) {
        if (e->value == NULL) return 0;
        if (isnode) return ln_pushnode(L, e->value);
        lua_pushlstring(L, e->value, nui_len((NUIdata*)e->value));
        return 1;
    }
    if (isnode) {
        NUInode *node = lbind_check(L, 3, &lbT_Node);
        e->value = node;
    }
    /* XXX: how to hold string's life time? */
    e->value = (void*)luaL_checkstring(L, 3);
    return 0;
}

static int Levent_type(lua_State *L) {
    NUIevent *evt = ln_checkevent(L, 1);
    lbind_checkreadonly(L);
    lua_pushstring(L, (const char*)nui_eventtype(evt));
    return 1;
}

static int Levent_node(lua_State *L) {
    NUIevent *evt = ln_checkevent(L, 1);
    lbind_checkreadonly(L);
    ln_pushnode(L, nui_eventnode(evt));
    return 1;
}

static int Levent_time(lua_State *L) {
    NUIevent *evt = ln_checkevent(L, 1);
    lbind_checkreadonly(L);
    lua_pushinteger(L, nui_eventtime(evt));
    return 1;
}

static int Levent_bubbles(lua_State *L) {
    NUIevent *evt = ln_checkevent(L, 1);
    lbind_checkreadonly(L);
    lua_pushboolean(L, nui_eventstatus(evt, NUI_BUBBLES));
    return 1;
}

static int Levent_cancelable(lua_State *L) {
    NUIevent *evt = ln_checkevent(L, 1);
    lbind_checkreadonly(L);
    lua_pushboolean(L, nui_eventstatus(evt, NUI_CANCELABLE));
    return 1;
}

static int Levent_stopped(lua_State *L) {
    NUIevent *evt = ln_checkevent(L, 1);
    lbind_checkreadonly(L);
    lua_pushboolean(L, nui_eventstatus(evt, NUI_STOPPED));
    return 1;
}

static int Levent_canceled(lua_State *L) {
    NUIevent *evt = ln_checkevent(L, 1);
    lbind_checkreadonly(L);
    lua_pushboolean(L, nui_eventstatus(evt, NUI_CANCELED));
    return 1;
}

static int Levent_phase(lua_State *L) {
    NUIevent *evt = ln_checkevent(L, 1);
    const char *rets[] = { "none", "capture", "target", "bubble" };
    int retidx = nui_eventstatus(evt, NUI_PHASE);
    lbind_checkreadonly(L);
    if (retidx >= 0 && retidx <= 3) {
        lua_pushstring(L, rets[retidx]);
        return 1;
    }
    return 0;
}

static int Levent_emit(lua_State *L) {
    NUIevent *evt = ln_checkevent(L, 1);
    NUInode *n = lbind_check(L, 2, &lbT_Node);
    lua_pushinteger(L, nui_emitevent(n, evt));
    return 1;
}

static int Levent_cancel(lua_State *L) {
    NUIevent *evt = ln_checkevent(L, 1);
    nui_cancelevent(evt);
    lbind_returnself(L);
}

static int Levent_stop(lua_State *L) {
    NUIevent *evt = ln_checkevent(L, 1);
    int stopnow = lua_toboolean(L, 2);
    nui_stopevent(evt, stopnow);
    lbind_returnself(L);
}

static void ln_eventhandler(void *ud, NUInode *n, const NUIevent *evt) {
    NUIstate *S = nui_state(n);
    LNUIlua *ls = ln_statefromS(S);
    LNUIevent *levt;
    lua_State *L = ls->L;
    if (L == NULL) return;
    lua_rawgeti(L, LUA_REGISTRYINDEX, ls->handlers);
    if (lua53_rawgetp(L, -1, ud) != LUA_TFUNCTION)
    { lua_pop(L, 2); return; }
    ln_pushnode(L, n);
    if ((levt =ln_retrieve(L, S, evt)) == NULL) {
        levt = lbind_new(L, sizeof(LNUIevent), &lbT_Event);
        memset(levt, 0, sizeof(LNUIevent));
        ln_registerevent(L, ls, (NUIevent*)evt);
        levt->ls = ls;
    }
    levt->current = (NUIevent*)evt;
    if (lbind_pcall(L, 2, 0) != LUA_OK) {
        fprintf(stderr, "%s\n", lua_tostring(L, -1));
        lua_pushboolean(L, 0);
        lua_rawsetp(L, -3, ud);
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
    if (evt != &levt->evt) levt->current = NULL;
}

static int Lnode_addhandler(lua_State *L) {
    NUInode *n = lbind_check(L, 1, &lbT_Node);
    LNUIlua *ls = ln_statefromS(nui_state(n));
    NUIkey *type = ln_checkkey(ls->S, L, 2);
    void *ud = (void*)lua_topointer(L, 3);
    int capture = lua_toboolean(L, 4);
    luaL_checktype(L, 3, LUA_TFUNCTION);
    lua_rawgeti(L, LUA_REGISTRYINDEX, ls->handlers);
    lua_pushvalue(L, 3);
    lua_rawsetp(L, -2, ud);
    nui_addhandler(n, type, capture, ln_eventhandler, ud);
    lua_pushvalue(L, 3);
    return 1;
}

static int Lnode_delhandler(lua_State *L) {
    NUInode *n = lbind_check(L, 1, &lbT_Node);
    LNUIlua *ls = ln_statefromS(nui_state(n));
    NUIkey *type = ln_checkkey(ls->S, L, 2);
    void *ud = (void*)lua_topointer(L, 3);
    int capture = lua_toboolean(L, 4);
    luaL_checktype(L, 3, LUA_TFUNCTION);
    lua_rawgeti(L, LUA_REGISTRYINDEX, ls->handlers);
    if (!ud || lua53_rawgetp(L, -2, ud) == LUA_TNIL)
        return 0;
    lua_pushnil(L);
    lua_rawsetp(L, -3, ud);
    nui_delhandler(n, type, capture, ln_eventhandler, ud);
    return 1;
}

static void open_event(lua_State *L) {
    luaL_Reg libs[] = {
        { "__gc", Levent_delete },
        { "__tostring", Levent_tostring },
#define ENTRY(name) { #name, Levent_##name }
        ENTRY(new),
        ENTRY(delete),
        ENTRY(emit),
        ENTRY(stop),
        ENTRY(cancel),
        { NULL, NULL }
    };
    luaL_Reg props[] = {
        ENTRY(type),
        ENTRY(node),
        ENTRY(time),
        ENTRY(bubbles),
        ENTRY(cancelable),
        ENTRY(stopped),
        ENTRY(canceled),
        ENTRY(phase),
#undef  ENTRY
        { NULL, NULL }
    };
    if (lbind_newmetatable(L, libs, &lbT_Event)) {
        lbind_setmaptable(L, props, LBIND_INDEX);
        lbind_sethashf(L, Levent_hashf, LBIND_INDEX | LBIND_NEWINDEX);
        lbind_setlibcall(L, NULL);
    }
}


/* node */

static void ln_setparent(lua_State *L, NUInode *n, int nidx, int pidx) {
    int values = 0;
    if (pidx && lua53_getuservalue(L, pidx) != LUA_TNIL) {
        lua_pushvalue(L, lbind_relindex(nidx, 1));
        lua_rawsetp(L, -2, n);
        ++values;
    }
    if (lbind_retrieve(L, nui_parent(n))) {
        if (lua53_getuservalue(L, -1) == LUA_TTABLE) {
            lua_pushnil(L);
            lua_rawsetp(L, -2, n);
        }
        values += 2;
    }
    if (values) lua_pop(L, values);
}

static int Lnode_new(lua_State *L) {
    NUIstate *S = ln_checkstate(L, 1);
    LNUIlua *ls = ln_statefromS(S);
    NUInode *n = nui_newnode(S);
    int i, top = lua_gettop(L);
    nui_addcomp(n, &ls->base);
    for (i = 2; i <= top; ++i) {
        NUIkey *name = ln_checkkey(S, L, i);
        NUItype *t = nui_gettype(S, name);
        if (name && t != NULL) nui_addcomp(n, t);
    }
    lbind_wrap(L, n, &lbT_Node);
    nui_retain(n);
    return 1;
}

static int Lnode_delete(lua_State *L) {
    NUInode *n = (NUInode*)lbind_test(L, 1, &lbT_Node);
    if (n) nui_release(n);
    return 0;
}

static int Lnode_setenv(lua_State *L) {
    NUInode *n = (NUInode*)lbind_test(L, 1, &lbT_Node);
    NUIstate *S = nui_state(n);
    NUInode *child;
    int i;
    luaL_checktype(L, 2, LUA_TTABLE);
    nui_setchildren(n, NULL);
    for (i = 1; lua53_rawgeti(L, 2, i) != LUA_TNIL
            && (child = (NUInode*)lbind_test(L, -1, &lbT_Node)) != NULL; ++i) {
        nui_setparent(child, n);
        lua_rawsetp(L, 2, child);
        lua_pushnil(L);
        lua_rawseti(L, 2, i);
    }
    lua_pop(L, 1);
    if (i == 1) lua_pushnil(L);
    else lua_pushinteger(L, i-1);
    while (lua_next(L, 2)) {
        const char *v = lua_tostring(L, -1);
        if (v && lua_type(L, -2) == LUA_TSTRING) {
            NUIkey *key = ln_testkey(S, L, -2);
            nui_set(n, key, v);
            lua_pushvalue(L, -2);
            lua_pushnil(L);
            lua_rawset(L, 2);
        }
        lua_pop(L, 1);
    }
    lua_pushvalue(L, 2);
    lua_setuservalue(L, 1);
    lbind_returnself(L);
}

static int Lnode_parent(lua_State *L) {
    NUInode *parent, *n = (NUInode*)lbind_check(L, 1, &lbT_Node);
    if (lua_gettop(L) == 2) return ln_pushnode(L, nui_parent(n));
    parent = (NUInode*)lbind_opt(L, 3, NULL, &lbT_Node);
    ln_setparent(L, n, 1, parent ? 3 : 0);
    nui_setparent(n, parent);
    return 0;
}

static int Lnode_children(lua_State *L) {
    NUInode *n = (NUInode*)lbind_check(L, 1, &lbT_Node);
    NUInode *child = nui_nextchild(n, NULL);
    NUInode *i, *next, *newnode, *parent;
    if (lua_gettop(L) == 2) return ln_pushnode(L, child);
    newnode = (NUInode*)lbind_opt(L, 3, NULL, &lbT_Node);
    if (lua_getuservalue(L, 1) == LUA_TTABLE) {
        for (i = child; i != NULL; i = next) {
            next = nui_nextchild(n, i);
            lua_pushnil(L);
            lua_rawsetp(L, -2, i);
        }
    }
    lua_pop(L, 1);
    parent = nui_parent(newnode);
    if (parent && lbind_retrieve(L, parent) &&
            lua_getuservalue(L, -1) == LUA_TTABLE) {
        for (i = nui_nextchild(parent, NULL); i != NULL; i = next) {
            next = nui_nextchild(parent, i);
            lua_pushnil(L);
            lua_rawsetp(L, -2, i);
        }
    }
    nui_setchildren(n, newnode);
    if (lua_getuservalue(L, 1) != LUA_TTABLE) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setuservalue(L, -3);
    }
    for (i = nui_nextchild(n, NULL); i != NULL; i = next) {
        next = nui_nextchild(n, i);
        ln_pushnode(L, i);
        lua_rawsetp(L, -2, i);
    }
    return 0;
}

static int Lnode_append(lua_State *L) {
    NUInode *n = (NUInode*)lbind_check(L, 1, &lbT_Node);
    NUInode *newnode = (NUInode*)lbind_check(L, 2, &lbT_Node);
    NUInode *parent = nui_parent(n);
    if (lbind_retrieve(L, parent))
        ln_setparent(L, newnode, 2, -1);
    nui_append(n, newnode);
    lbind_returnself(L);
}

static int Lnode_insert(lua_State *L) {
    NUInode *n = (NUInode*)lbind_check(L, 1, &lbT_Node);
    NUInode *newnode = (NUInode*)lbind_check(L, 2, &lbT_Node);
    if (lbind_retrieve(L, nui_parent(n)))
        ln_setparent(L, newnode, 2, -1);
    nui_insert(n, newnode);
    lbind_returnself(L);
}

static int Lnode_detach(lua_State *L) {
    NUInode *n = (NUInode*)lbind_check(L, 1, &lbT_Node);
    if (lbind_retrieve(L, nui_parent(n)))
        ln_setparent(L, n, 1, 0);
    nui_detach(n);
    lbind_returnself(L);
}

static int Lnode_arrayf(lua_State *L) {
    NUInode *n = (NUInode*)lbind_check(L, 1, &lbT_Node);
    int idx = (int)luaL_checkinteger(L, 2);
    NUInode *old = nui_indexnode(n, idx > 0 ? idx-1 : idx), *newnode;
    if (lua_gettop(L) == 2) return ln_pushnode(L, old);
    if (old == NULL && idx > nui_childcount(n)+1)
        lbind_argferror(L, 2, "index#%d out of range", idx);
    newnode = (NUInode*)lbind_check(L, 3, &lbT_Node);
    if (lbind_retrieve(L, old))
        ln_setparent(L, old, -1, 0);
    ln_setparent(L, newnode, 3, 1);
    if (old == NULL)
        nui_setparent(n, newnode);
    else {
        nui_insert(old, newnode);
        nui_detach(old);
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
    lua_pushinteger(L, nui_release(n));
    return 1;
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
    if (!lbind_retrieve(L, S))
        lua_pushlightuserdata(L, S);
    return 1;
}

static int Lnode_childcount(lua_State *L) {
    NUInode *n = (NUInode*)lbind_check(L, 1, &lbT_Node);
    lbind_checkreadonly(L);
    lua_pushinteger(L, nui_childcount(n));
    return 1;
}

static int Lnode_index(lua_State *L) {
    NUInode *n = (NUInode*)lbind_check(L, 1, &lbT_Node);
    int idx;
    lbind_checkreadonly(L);
    if ((idx = nui_nodeindex(n)) < 0)
        return 0;
    lua_pushinteger(L, idx + 1);
    return 1;
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
        ENTRY(nextchild),
        ENTRY(prevchild),
        ENTRY(nextleaf),
        ENTRY(prevleaf),
        ENTRY(append),
        ENTRY(insert),
        ENTRY(detach),
        ENTRY(setattr),
        ENTRY(getattr),
        ENTRY(delattr),
        ENTRY(addattrhandler),
        ENTRY(delattrhandler),
        ENTRY(addhandler),
        ENTRY(delhandler),
        ENTRY(addcomp),
        ENTRY(getcomp),
        { NULL, NULL }
    };
    luaL_Reg props[] = {
        ENTRY(parent),
        ENTRY(children),
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
        lbind_setlibcall(L, NULL);
    }
}


/* state */

static NUItime ln_time(NUIparams *params) {
    LNUIstate *LS = (LNUIstate*)params;
    lua_State *L = LS->ls->L;
    lua_rawgeti(L, LUA_REGISTRYINDEX, LS->time_ref);
    if (lbind_pcall(L, 0, 1) != LUA_OK) {
        fprintf(stderr, "%s\n", lua_tostring(L, -1));
        ln_unref(L, &LS->time_ref);
    }
    else if (lua_isnumber(L, -1)) {
        lua_Integer ret = lua_tointeger(L, -1);
        return ret >= 0 ? (NUItime)ret : 0;
    }
    lua_pop(L, 1);
    return 0;
}

static int ln_wait(NUIparams *params, NUItime time) {
    LNUIstate *LS = (LNUIstate*)params;
    lua_State *L = LS->ls->L;
    int ret = 0;
    lua_rawgeti(L, LUA_REGISTRYINDEX, LS->wait_ref);
    lua_pushinteger(L, (lua_Integer)time);
    if (lbind_pcall(L, 1, 1) != LUA_OK) {
        fprintf(stderr, "%s\n", lua_tostring(L, -1));
        ln_unref(L, &LS->time_ref);
    }
    else if (lua_isnumber(L, -1))
        ret = (int)lua_tointeger(L, -1);
    lua_pop(L, 1);
    return ret;
}

static int Lstate_new(lua_State *L) {
    LNUIstate *LS = (LNUIstate*)lbind_new(L, sizeof(LNUIstate), &lbT_State);
    memset(LS, 0, sizeof(LNUIstate));
    LS->params.close = ln_closelua;
    if (nui_newstate(&LS->params) == NULL)
        return 0;
    LS->default_time = LS->params.time;
    LS->default_wait = LS->params.wait;
    LS->time_ref = LUA_NOREF;
    LS->wait_ref = LUA_NOREF;
    ln_newlua(L, LS);
    return 1;
}

static int Lstate_delete(lua_State *L) {
    LNUIstate *LS = lbind_test(L, 1, &lbT_State);
    if (LS && LS->params.S) {
        nui_close(LS->params.S);
        assert(LS->params.S == NULL);
        ln_unref(L, &LS->time_ref);
        ln_unref(L, &LS->wait_ref);
        lbind_delete(L, 1);
    }
    return 0;
}

static int Lstate_pollevents(lua_State *L) {
    NUIstate *S = ln_checkstate(L, 1);
    lua_pushboolean(L, nui_waitevents(S, 0));
    return 1;
}

static int Lstate_waitevents(lua_State *L) {
    NUIstate *S = ln_checkstate(L, 1);
    lua_Integer delayms = luaL_optinteger(L, 2, NUI_FOREVER);
    lua_pushboolean(L, nui_waitevents(S, (NUItime)delayms));
    return 1;
}

static int Lstate_loop(lua_State *L) {
    NUIstate *S = ln_checkstate(L, 1);
    lua_pushboolean(L, nui_loop(S));
    return 1;
}

static int Lstate_rootnode(lua_State *L) {
    NUIstate *S = ln_checkstate(L, 1);
    ln_pushnode(L, nui_rootnode(S));
    return 1;
}

static int Lstate_wait(lua_State *L) {
    LNUIstate *LS = (LNUIstate*)lbind_check(L, 1, &lbT_State);
    if (lua_gettop(L) == 2) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, LS->wait_ref);
        return 1;
    }
    if (lua_isnil(L, 3)) {
        ln_unref(L, &LS->wait_ref);
        LS->params.wait = LS->default_wait;
    }
    else {
        luaL_checktype(L, 3, LUA_TFUNCTION);
        ln_ref(L, 3, &LS->wait_ref);
        LS->params.wait = ln_wait;
    }
    return 0;
}

static int Lstate_time(lua_State *L) {
    LNUIstate *LS = (LNUIstate*)lbind_check(L, 1, &lbT_State);
    if (lua_gettop(L) == 2) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, LS->time_ref);
        return 1;
    }
    if (lua_isnil(L, 3)) {
        ln_unref(L, &LS->time_ref);
        LS->params.time = LS->default_time;
    }
    else {
        luaL_checktype(L, 3, LUA_TFUNCTION);
        ln_ref(L, 3, &LS->time_ref);
        LS->params.time = ln_time;
    }
    return 0;
}

static void open_state(lua_State *L) {
    luaL_Reg libs[] = {
#define ENTRY(name) { #name, Lstate_##name }
        ENTRY(new),
        ENTRY(delete),
        ENTRY(pollevents),
        ENTRY(waitevents),
        ENTRY(loop),
        { NULL, NULL }
    };
    luaL_Reg props[] = {
        ENTRY(time),
        ENTRY(wait),
        ENTRY(rootnode),
#undef  ENTRY
        { NULL, NULL }
    };
    if (lbind_newmetatable(L, libs, &lbT_State)) {
        lbind_setmaptable(L, props, LBIND_INDEX | LBIND_NEWINDEX);
        lbind_setlibcall(L, NULL);
    }
}


/* entry point */

LUALIB_API int luaopen_nui(lua_State *L) {
    struct {
        const char *name;
        void (*f)(lua_State *L);
    } metas[] = {
#define ENTRY(name) { #name, open_##name }
        ENTRY(timer),
        ENTRY(node),
        ENTRY(attr),
        ENTRY(event),
        ENTRY(type),
#undef  ENTRY
        { NULL, NULL }
    }, *p = metas;
    open_state(L);
    for (; p->name != NULL; ++p) {
        p->f(L);
        lua_setfield(L, -2, p->name);
    }
    return 1;
}

/* win32cc: flags+='-mdll -s -O3 -DLUA_BUILD_AS_DLL'
 * win32cc: libs+='-llua53' output='nui.dll'
 * maccc: flags+='-bundle -ggdb -O0 -undefined dynamic_lookup' output='nui.so'
 * cc: run='lua -- test.lua' */

