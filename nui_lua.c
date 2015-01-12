#define NUI_CORE
#include "nui.h"
#include "nui_backend.h"

#define LUA_LIB
#define LBIND_DEFAULT_FLAG (LBIND_TRACK|LBIND_INTERN)
#define LB_API NUI_INLINE
#include "lbind.h"
#include "lbind.c"
#undef LB_API
#define LB_API LUA_API


#include <stdlib.h>
#include <string.h>


LBIND_TYPE(lbT_Action, "nui.Action");
LBIND_TYPE(lbT_Class, "nui.Class");
LBIND_TYPE(lbT_Node, "nui.Node");
LBIND_TYPE(lbT_State, "nui.State");
LBIND_TYPE(lbT_String, "nui.String");


static NUIclass *check_class(lua_State *L, int idx);

static int getbox(lua_State *L, void *idx) {
  lua_rawgetp(L, LUA_REGISTRYINDEX, idx);
  if (lua_isnil(L, -1)) {
    lua_pop(L, 1);
    lua_newtable(L);
    lua_pushvalue(L, -1);
    lua_rawsetp(L, LUA_REGISTRYINDEX, idx);
    return 1;
  }
  return 0;
}


/* nui values conversion */

#if 0
#define NUI_STRINGBOX 0x21579807

static int get_stringbox(lua_State *L) {
    if (getbox(L, NUI_STRINGBOX)) {
        lua_pushliteral(L, "k");
        lbind_setmetafield(L, -2, "__mode");
        return 1;
    }
}

static NUIstring *test_string(NUIstate *S, lua_State *L, int idx) {
    size_t len;
    const char *p;
    NUIstring *s = lbind_test(L, idx, &lbT_String);
    if (s != NULL)
        return s;
    if (lua_type(L, idx) != LUA_TSTRING)
        return NULL;
    get_stringbox(L);
    lua_pushvalue(L, lbind_relindex(idx, 1));
    lua_rawget(L, -2);
    if (!lua_isnil(L, -1)) {
        s = lbind_test(L, -1, &lbT_String);
        lua_pop(L, 2);
        return s;
    }
    lua_pop(L, 1);
    lua_pushvalue(L, lbind_relindex(idx, 1));
    p = luaL_checklstring(L, idx, &len);
    s = nui_newlstr(S, p, len);
    lbind_wrap(L, s, &lbT_String);
    lua_rawset(L, -3);
    lua_pop(L, 1);
    return s;
}
#endif

static NUIstring *test_string(NUIstate *S, lua_State *L, int idx) {
    size_t len;
    const char *s;
    if (lua_type(L, idx) != LUA_TSTRING)
        return NULL;
    s = lua_tolstring(L, idx, &len);
    return nui_newlstr(S, s, len);
}

static NUIstring *check_string(NUIstate *S, lua_State *L, int idx) {
    NUIstring *s = test_string(S, L, idx);
    if (s == NULL)
        lbind_typeerror(L, idx, "string");
    return s;
}

static int push_string(lua_State *L, NUIstring *s) {
    lua_pushlstring(L, (char*)s, nui_strlen(s));
    return 1;
}


/* nui value routines */

#define POINT_SIGN   "\033PT"
#define POINT_STRLEN (sizeof(POINT_SIGN)+sizeof(NUIpoint))
#define SIZE_SIGN   "\033SZ"
#define SIZE_STRLEN (sizeof(SIZE_SIGN)+sizeof(NUIsize))

static int retrieve_node(lua_State *L, NUInode *n);
static NUIstate *get_default_state(lua_State *L);

static int retrieve_or_wrap(lua_State *L, void *ptr, lbind_Type *t) {
    if (ptr == NULL) return 0;
    if (!lbind_retrieve(L, ptr)) {
        lbind_wrap(L, ptr, t);
        lbind_untrack(L, -1);
    }
    return 1;
}

static int test_string_or_data(lua_State *L, int idx, NUIvalue *pv) {
    size_t len;
    const char *s = lua_tolstring(L, idx, &len);
    if (len == POINT_STRLEN && memcmp(POINT_SIGN, s, 4) == 0) {
        *pv = nui_pointvalue(*(NUIpoint*)(s+4));
        return 1;
    }
    if (len == SIZE_STRLEN && memcmp(SIZE_SIGN, s, 4) == 0) {
        *pv = nui_sizevalue(*(NUIsize*)(s+4));
        return 1;
    }
    *pv = nui_stringvalue(nui_newlstr(get_default_state(L), s, len));
    return 1;
}

static int test_value(lua_State *L, int idx, NUIvalue *pv) {
    switch (lua_type(L, idx)) {
    case LUA_TNIL: *pv = nui_nilvalue(); break;
    case LUA_TBOOLEAN: *pv = nui_integervalue(lua_toboolean(L, idx)); break;
    case LUA_TNUMBER: {
        lua_Number n = lua_tonumber(L, idx);
        if ((double)(long)n == n)
            *pv = nui_integervalue((long)n);
        else
            *pv = nui_numbervalue(n);
        break;
    }
    case LUA_TSTRING:
        return test_string_or_data(L, idx, pv);

    case LUA_TUSERDATA: {
        void *p;
        if ((p = lbind_test(L, idx, &lbT_Action)))
            *pv = nui_actionvalue(p);
        else if ((p = lbind_test(L, idx, &lbT_Node)))
            *pv = nui_nodevalue(p);
        if (!p) return 0;
        break;
    }
    default:
        return 0;
    }
    return 1;
}

static int push_value(lua_State *L, NUIvalue v) {
    switch (v.type) {
    case NUI_TNIL: lua_pushnil(L); return 1;
    case NUI_TINTEGER: lua_pushinteger(L, v.integer); return 1;
    case NUI_TNUMBER: lua_pushnumber(L, v.number); return 1;
    case NUI_TSTRING: return push_string(L, v.string);
    case NUI_TACTION: return retrieve_or_wrap(L, v.pointer, &lbT_Action);
    case NUI_TNODE: return retrieve_or_wrap(L, v.pointer, &lbT_Node);
    case NUI_TPOINT: {
        luaL_Buffer b;
        luaL_buffinit(L, &b);
        luaL_addlstring(&b, POINT_SIGN, 4);
        luaL_addlstring(&b, (char*)&v.point, sizeof(NUIpoint));
        luaL_pushresult(&b);
        return 1;
    }
    case NUI_TSIZE: {
        luaL_Buffer b;
        luaL_buffinit(L, &b);
        luaL_addlstring(&b, SIZE_SIGN, 4);
        luaL_addlstring(&b, (char*)&v.size, sizeof(NUIsize));
        luaL_pushresult(&b);
        return 1;
    }

    default:
        return 0;
    }
}


/* nui state routines */

#define NUI_STATE     0x20157A7E

#define NUI_STATE_CALLBACKS(X) \
    X(error)  \
    X(poll)   \
    X(quit)   \
    X(time)   \
    X(wait)

typedef enum NUIstatecallbacks {
#define X(name) NUI_SCB_##name,
    NUI_STATE_CALLBACKS(X)
#undef  X
    NUI_SCB_COUNT
} NUIstatecallbacks;

typedef struct LNUIparams {
    NUIparams base;
    lua_State *L;
    int refs[NUI_SCB_COUNT];
} LNUIparams;

static void scb_deletor(NUIparams *params) {
    LNUIparams *lp = (LNUIparams*)params;
    int i;
    for (i = 0; i < NUI_SCB_COUNT; ++i)
        if (lp->refs[i] != LUA_REFNIL)
            luaL_unref(lp->L, LUA_REGISTRYINDEX, LUA_REFNIL);
    free(lp);
}

static NUIstate *get_default_state(lua_State *L) {
    NUIstate *S;
    lua_rawgeti(L, LUA_REGISTRYINDEX, NUI_STATE);
    S = lbind_test(L, -1, &lbT_State);
    lua_pop(L, 1);
    if (S == NULL) {
        LNUIparams *lp = malloc(sizeof(LNUIparams));
        lp->base.deletor = scb_deletor;
        lp->base.error = NULL;
        lp->base.time = NULL;
        lp->base.poll = NULL;
        lp->base.wait = NULL;
        lp->base.quit = NULL;
        S = nui_newstate(&lp->base);
        lbind_wrap(L, S, &lbT_State);
        lua_rawseti(L, LUA_REGISTRYINDEX, NUI_STATE);
    }
    return S;
}

static int handle_error(lua_State *L, int res, const char *msg) {
    if (res != LUA_OK) {
        if (msg)
            fprintf(stderr, "ERROR: %s\n%s", msg, lua_tostring(L, -1));
        else
            fprintf(stderr, "ERROR: %s", lua_tostring(L, -1));
        lua_pop(L, 1);
        return 1;
    }
    return 0;
}

static void scb_error(NUIparams *params, const char *msg) {
    LNUIparams *lp = (LNUIparams*)params;
    lua_State *L = lp->L;
    if (lp->refs[NUI_SCB_error] == LUA_REFNIL)
        return;
    lua_rawgeti(L, LUA_REGISTRYINDEX, lp->refs[NUI_SCB_error]);
    lua_pushstring(L, msg);
    handle_error(L, lbind_pcall(L, 1, 0), msg);
}

static unsigned scb_time(NUIparams *params) {
    LNUIparams *lp = (LNUIparams*)params;
    lua_State *L = lp->L;
    unsigned time;
    if (lp->refs[NUI_SCB_time] == LUA_REFNIL)
        return 0;
    lua_rawgeti(L, LUA_REGISTRYINDEX, lp->refs[NUI_SCB_time]);
    if (handle_error(L, lbind_pcall(L, 0, 1), NULL))
        return 0;
    time = (float)lua_tonumber(L, -1);
    lua_pop(L, 1);
    return time;
}

static int scb_poll(NUIparams *params) {
    LNUIparams *lp = (LNUIparams*)params;
    lua_State *L = lp->L;
    int res;
    if (lp->refs[NUI_SCB_poll] == LUA_REFNIL)
        return 0;
    lua_rawgeti(L, LUA_REGISTRYINDEX, lp->refs[NUI_SCB_poll]);
    if (handle_error(L, lbind_pcall(L, 0, 1), NULL))
        return 0;
    res = lua_toboolean(L, -1);
    lua_pop(L, 1);
    return res;
}

static int scb_wait(NUIparams *params, unsigned waittime) {
    LNUIparams *lp = (LNUIparams*)params;
    lua_State *L = lp->L;
    int res;
    if (lp->refs[NUI_SCB_wait] == LUA_REFNIL)
        return 0;
    lua_rawgeti(L, LUA_REGISTRYINDEX, lp->refs[NUI_SCB_wait]);
    lua_pushnumber(L, (lua_Number)waittime);
    if (handle_error(L, lbind_pcall(L, 1, 1), NULL))
        return 0;
    res = lua_toboolean(L, -1);
    lua_pop(L, 1);
    return res;
}

static void scb_quit(NUIparams *params) {
    LNUIparams *lp = (LNUIparams*)params;
    lua_State *L = lp->L;
    if (lp->refs[NUI_SCB_quit] == LUA_REFNIL)
        return;
    lua_rawgeti(L, LUA_REGISTRYINDEX, lp->refs[NUI_SCB_quit]);
    handle_error(L, lbind_pcall(L, 0, 0), NULL);
}

static int Lstate_new(lua_State *L) {
    NUIstate *S;
    LNUIparams *lp = malloc(sizeof(LNUIparams));
    lp->base.deletor = scb_deletor;
    lp->base.error = NULL;
    lp->base.time = NULL;
    lp->base.poll = NULL;
    lp->base.wait = NULL;
    lp->base.quit = NULL;
    S = nui_newstate(&lp->base);
    lbind_wrap(L, S, &lbT_State);
    return 1;
}

static int Lstate_delete(lua_State *L) {
    NUIstate *S = lbind_test(L, 1, &lbT_State);
    if (S != NULL) {
        NUIstate *oS = get_default_state(L);
        if (oS == S) {
            lua_pushnil(L);
            lua_rawseti(L, LUA_REGISTRYINDEX, NUI_STATE);
        }
        lbind_delete(L, 1);
        nui_close(S);
    }
    return 0;
}

static int Lstate_callback(lua_State *L) {
    static lbind_EnumItem items[] = {
#define X(name) { #name, NUI_SCB_##name },
        NUI_STATE_CALLBACKS(X)
#undef  X
        { NULL, 0 }
    };
    static lbind_Enum et = LBIND_INITENUM("callback", items);
    NUIstate *S = get_default_state(L);
    LNUIparams *lp = (LNUIparams*)nui_stateparams(S);
    int idx = lbind_checkenum(L, 1, &et);
    if (lua_isnone(L, 2)) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, lp->refs[idx]);
        return 1;
    }
    if (lua_isnil(L, 2))
        lp->refs[idx] = LUA_REFNIL;
    else {
        lua_pushvalue(L, 2);
        lp->refs[idx] = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    switch (idx) {
#define X(name) case NUI_SCB_##name: lp->base.name = \
        (lp->refs[idx] == LUA_REFNIL ? NULL : scb_##name); break;
    NUI_STATE_CALLBACKS(X)
#undef  X
    default: break;
    }
    return 0;
}

static int Lstate_state(lua_State *L) {
    if (lua_isnone(L, 3)) {
        NUIstate *S = get_default_state(L);
        return retrieve_or_wrap(L, S, &lbT_State);
    }
    lbind_check(L, 3, &lbT_State);
    lua_pushvalue(L, 3);
    lua_rawseti(L, LUA_REGISTRYINDEX, NUI_STATE);
    return 0;
}

static int Lstate_loop(lua_State *L) {
    NUIstate *S = lbind_check(L, 1, &lbT_State);
    if (nui_loop(S)) lbind_returnself(L);
    return 0;
}

static int Lstate_pollevents(lua_State *L) {
    NUIstate *S = lbind_check(L, 1, &lbT_State);
    if (nui_pollevents(S)) lbind_returnself(L);
    return 0;
}

static int Lstate_waitevents(lua_State *L) {
    NUIstate *S = lbind_check(L, 1, &lbT_State);
    if (nui_waitevents(S)) lbind_returnself(L);
    return 0;
}

static int Lstate_breakloop(lua_State *L) {
    nui_breakloop(lbind_check(L, 1, &lbT_State));
    return 0;
}

static int Lstate_time(lua_State *L) {
    NUIstate *S = lbind_check(L, 1, &lbT_State);
    lua_pushnumber(L, (double)nui_time(S)/1000.0);
    return 1;
}

static void open_state(lua_State *L) {
#define ENTRY(name) { #name, Lstate_##name }
    luaL_Reg libs[] = {
        ENTRY(new),
        ENTRY(delete),
        ENTRY(callback),
        ENTRY(loop),
        ENTRY(pollevents),
        ENTRY(waitevents),
        ENTRY(breakloop),
        ENTRY(time),
        { NULL, NULL }
    };
    luaL_Reg libacc[] = {
        ENTRY(state),
        { NULL, NULL }
    };
#undef  ENTRY
    if (lbind_newmetatable(L, libs, &lbT_State)) {
        lua_newtable(L);
        lbind_setmaptable(L, libacc, LBIND_INDEX|LBIND_NEWINDEX);
        lua_setmetatable(L, -2);
    }
}


/* nui action routines */

/* wk { f=action_pointer } */
#define NUI_ACTIONBOX 0xAC710B03

typedef struct LNUIaction {
    NUIaction base;
    NUIaction *first_after;
    NUIaction *last_before;
    NUIaction *freed_actions;
    lua_State *L;
    int count;
} LNUIaction;

typedef struct LNUIsubaction {
    NUIaction base;
    lua_State *L;
    int func_ref;
} LNUIsubaction;

static void get_actionbox(lua_State *L) {
    getbox(L, (void*)NUI_ACTIONBOX);
}

static void subaction_deletor(NUIstate *S, NUIaction *a) {
    LNUIsubaction *suba = (LNUIsubaction*)a;
    if (suba->L != NULL) {
        lua_State *L = suba->L;
        /* should never get into this */
        get_actionbox(L);
        lua_rawgeti(L, LUA_REGISTRYINDEX, suba->func_ref);
        lua_pushnil(L);
        lua_rawset(L, -3);
        lua_pop(L, 1);
        luaL_unref(L, LUA_REGISTRYINDEX, suba->func_ref);
        suba->func_ref = LUA_REFNIL;
        suba->L = NULL;
    }
}

static int subaction_emit(NUIstate *S, NUIaction *a, NUInode *n, float dt) {
    LNUIsubaction *suba = (LNUIsubaction*)a;
    lua_State *L = suba->L;
    int res, i, nargs = nui_gettop(S);
    if (suba->func_ref == LUA_REFNIL) return 0;
    lua_rawgeti(L, LUA_REGISTRYINDEX, suba->func_ref);
    retrieve_or_wrap(L, suba, &lbT_Action);
    lua_pushnumber(L, (lua_Number)dt);
    for (i = 1; i <= nargs; ++i) {
        NUIvalue v;
        nui_getvalue(S, i, &v);
        if (!push_value(L, v))
            lua_pushnil(L);
    }
    if (lbind_pcall(L, nargs + 2, 1) != LUA_OK)
        luaL_error(L, "action(%p, Node: %p): %s",
                suba, n, lua_tostring(L, -1));
    res = lua_toboolean(L, -1);
    lua_pop(L, 1);
    return res;
}

static int subaction_copy(NUIstate *S, NUIaction *from, NUIaction *to) {
    LNUIsubaction *lfrom = (LNUIsubaction*)from;
    LNUIsubaction *lto = (LNUIsubaction*)to;
    lua_State *L;
    if (from->deletor != subaction_deletor)
        return 0;
    L = lfrom->L;
    lua_rawgeti(L, LUA_REGISTRYINDEX, lfrom->func_ref);
    lto->func_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    lto->L = L;
    return 1;
}

static LNUIsubaction *init_subaction(LNUIsubaction *suba, lua_State *L) {
    suba->L = L;
    suba->func_ref = LUA_REFNIL;
    suba->base.deletor = subaction_deletor;
    suba->base.emit = subaction_emit;
    suba->base.copy = subaction_copy;
    return suba;
}

static NUIaction *new_subaction(NUIstate *S, lua_State *L, int idx) {
    NUIaction *a = NULL;

    switch (lua_type(L, idx)) {
    case LUA_TFUNCTION: {
        LNUIsubaction *suba = (LNUIsubaction*)nui_action(S, sizeof(LNUIsubaction));
        init_subaction(suba, L);
        lua_pushvalue(L, idx);
        suba->func_ref = luaL_ref(L, LUA_REGISTRYINDEX);
        get_actionbox(L);
        lua_pushvalue(L, lbind_relindex(idx, 1));
        lua_pushlightuserdata(L, suba);
        lua_rawset(L, -3);
        lua_pop(L, 1);
        a = &suba->base;
        break;
    }

    case LUA_TSTRING: {
        NUIstring *s = test_string(S, L, idx);
        if ((a = nui_namedaction(S, s)) == NULL)
            lbind_argferror(L, idx,
                    "bad named action: %s", (char*)s);
        break;
    }

    case LUA_TUSERDATA:
        if ((a = lbind_test(L, idx, &lbT_Action)) == NULL)
            lbind_typeerror(L, idx, "action");
        break;

    default:
        lbind_typeerror(L, idx, "action/function/string");
        break;
    }

    return a;
}

static void action_deletor(NUIstate *S, NUIaction *a) {
    LNUIaction *la = (LNUIaction*)a;
    NUIaction *i;
    lua_State *L = la->L;

    i = NULL;
    while ((i = nui_nextaction(la->freed_actions, i)) != NULL)
        nui_dropaction(i);

    i = NULL;
    get_actionbox(L);
    while ((i = nui_nextaction(&la->base, i)) != NULL) {
        if (i->deletor == subaction_deletor) {
            LNUIsubaction *suba = (LNUIsubaction*)i;
            lua_rawgeti(L, LUA_REGISTRYINDEX, suba->func_ref);
            lua_pushnil(L);
            lua_rawset(L, -3);
            luaL_unref(L, LUA_REGISTRYINDEX, suba->func_ref);
            suba->func_ref = LUA_REFNIL;
            suba->L = NULL;
        }
        nui_dropaction(i);
    }
    lua_pop(L, 1);

    /* clean action attributes */
    la->base.deletor = NULL;
    la->base.emit = NULL;
    la->L = NULL;
}

static LNUIaction *new_action(NUIstate *S, lua_State *L) {
    LNUIaction *la = (LNUIaction*)nui_action(S, sizeof(LNUIaction));
    la->first_after = NULL;
    la->last_before = NULL;
    la->freed_actions = NULL;
    la->L = L;
    la->count = 0;
    la->base.deletor = action_deletor;
    lbind_wrap(L, la, &lbT_Action);
    return la;
}

static int Laction_new(lua_State *L) {
    NUIstate *S = get_default_state(L);
    lbind_wrap(L, new_action(S, L), &lbT_Action);
    return 1;
}

static int Laction_delete(lua_State *L) {
    NUIaction *a = lbind_test(L, 1, &lbT_Action);
    if (a != NULL) {
        nui_dropaction(a);
        lbind_delete(L, 1);
    }
    return 0;
}

static NUIaction *insert_action(lua_State *L, NUIaction *a, int pos, int idx) {
    NUIaction *indexd, *newaction;
    indexd = nui_indexaction(a, pos);
    if (indexd == NULL) {
        NUIaction *last = NULL;
        if (pos > 0)
            nui_indexaction(a, pos-2);
        if (last == NULL)
            lbind_argferror(L, 2, "index #%d out of bound", pos);
    }
    newaction = new_subaction(get_default_state(L), L, idx);
    if (indexd == NULL)
        nui_linkaction(a, newaction);
    else
        nui_linkaction(indexd, newaction);
    return indexd;
}

static int Laction_insert(lua_State *L) {
    LNUIaction *la = lbind_check(L, 1, &lbT_Action);
    int idx, isint;
    idx = lua_tointegerx(L, 2, &isint);
    if (!isint) {
        NUIaction *newaction = new_subaction(get_default_state(L), L, 2);
        if (la->first_after == NULL)
            nui_linkaction(&la->base, newaction);
        lua_pushvalue(L, 2);
        return 1;
    }
    insert_action(L, &la->base, idx, 3);
    lua_pushvalue(L, 3);
    return 1;
}

static void recycle_subaction(lua_State *L, LNUIaction *la, LNUIsubaction *suba) {
    if (la->freed_actions == NULL)
        la->freed_actions = &suba->base;
    else
        nui_linkaction(la->freed_actions, &suba->base);
    lua_rawgeti(L, LUA_REGISTRYINDEX, suba->func_ref);
    get_actionbox(L);
    lua_pushvalue(L, -2);
    lua_pushnil(L);
    lua_rawset(L, -3);
    lua_pop(L, 1);
}

static int remove_action(lua_State *L, LNUIaction *la, NUIaction *removed) {
    NUIaction *i = NULL;
    while ((i = nui_nextaction(&la->base, i)) != NULL
            && i != removed)
        ;
    if (i == removed) {
        nui_unlinkaction(i);
        if (i->deletor == subaction_deletor) {
            recycle_subaction(L, la, (LNUIsubaction*)i);
            return 1;
        }
        return 0;
    }
    return -1;
}

static int Laction_remove(lua_State *L) {
    LNUIaction *la = lbind_check(L, 1, &lbT_Action);
    NUIaction *removed = NULL;
    int res;
    switch (lua_type(L, 2)) {
    case LUA_TNONE:
    case LUA_TNIL: /* remove last one */
        removed = nui_prevaction(&la->base, NULL);
        break;

    case LUA_TNUMBER: /* remove index */
        removed = nui_indexaction(&la->base, lua_tointeger(L, 2));
        break;

    case LUA_TFUNCTION: /* remove with this function */
        get_actionbox(L);
        lua_pushvalue(L, 2);
        lua_rawget(L, -2);
        removed = lua_touserdata(L, -1);
        lua_pop(L, 2);
        break;

    case LUA_TUSERDATA: /* remove action object */
        removed = lbind_test(L, 2, &lbT_Action);
        break;
    }

    if (removed == NULL
            || (res = remove_action(L, la, removed)) < 0)
        return 0;

    if (res == 0)
        lua_pushvalue(L, 2);
    return 1;
}

static int Laction_before(lua_State *L) {
    /* XXX */
    return 0;
}

static int Laction_after(lua_State *L) {
    /* XXX */
    return 0;
}

static int Laction___len(lua_State *L) {
    NUIaction *a = lbind_check(L, 1, &lbT_Action);
    NUIaction *i = NULL;
    size_t len = 0;
    while ((i = nui_nextaction(a, i)) != NULL)
        ++len;
    lua_pushinteger(L, len);
    return 1;
}

static int action_iter(lua_State *L) {
    NUIaction *n = lbind_check(L, 1, &lbT_Action);
    NUIaction *curr = lbind_opt(L, 2, NULL, &lbT_Action);
    return retrieve_or_wrap(L,
            nui_nextaction(n, curr), &lbT_Action);
}

static int action_riter(lua_State *L) {
    NUIaction *n = lbind_check(L, 1, &lbT_Action);
    NUIaction *curr = lbind_opt(L, 2, NULL, &lbT_Action);
    return retrieve_or_wrap(L,
            nui_prevaction(n, curr), &lbT_Action);
}

static int Laction_iter(lua_State *L) {
    lbind_check(L, 1, &lbT_Action);
    if (!lua_toboolean(L, 2))
        lua_pushcfunction(L, action_iter);
    else
        lua_pushcfunction(L, action_riter);
    lua_pushvalue(L, 1);
    return 2;
}

static int Laction_emit(lua_State *L) {
    NUIstate *S = get_default_state(L);
    NUIaction *a = lbind_check(L, 1, &lbT_Action);
    int i, top = lua_gettop(L);
    for (i = 2; i <= top; ++i) {
        NUIvalue v = nui_nilvalue();
        if (test_value(L, i, &v))
            nui_pushvalue(S, v);
        else
            nui_pushvalue(S, v);
    }
    nui_emitaction(a, top-1);
    lbind_returnself(L);
}

static int Laction_starttimer(lua_State *L) {
    NUIaction *a = lbind_check(L, 1, &lbT_Action);
    unsigned delayed = (unsigned)(luaL_checknumber(L, 2)/1000.0);
    unsigned interval = (unsigned)(luaL_optnumber(L, 2, 0.0)/1000.0);
    nui_starttimer(a, delayed, interval);
    lbind_returnself(L);
}

static int Laction_stoptimer(lua_State *L) {
    NUIaction *a = lbind_check(L, 1, &lbT_Action);
    nui_stoptimer(a);
    lbind_returnself(L);
}

static int action_arrayf(lua_State *L) {
    LNUIaction *la = lbind_check(L, 1, &lbT_Action);
    NUIaction *res = NULL;
    int isint, idx = lua_tointegerx(L, 2, &isint);
    if (!isint) return 0;
    res = nui_indexaction(&la->base, idx > 0 ? idx-1 : idx);
    if (lua_isnone(L, 3)) /* get */
        return retrieve_or_wrap(L, res, &lbT_Action);
    if ((res = insert_action(L, &la->base, idx, 3)) != NULL) {
        nui_unlinkaction(res);
        if (res->deletor == subaction_deletor)
            recycle_subaction(L, la, (LNUIsubaction*)res);
    }
    return 0;
}

static void open_action(lua_State *L) {
    luaL_Reg libs[] = {
#define ENTRY(name) { #name, Laction_##name }
#define Laction___call Laction_emit
        ENTRY(__call),
#undef  Laction___call
        ENTRY(__len),
        ENTRY(new),
        ENTRY(delete),
#define Laction_add Laction_insert
        ENTRY(add),
#undef  Laction_add
        ENTRY(insert),
        ENTRY(remove),
        ENTRY(before),
        ENTRY(after),
        ENTRY(iter),
        ENTRY(emit),
        ENTRY(starttimer),
        ENTRY(stoptimer),
#undef  ENTRY
        { NULL, NULL }
    };
    if (lbind_newmetatable(L, libs, &lbT_Action)) {
        lbind_setarrayf(L, action_arrayf, LBIND_INDEX|LBIND_NEWINDEX);
        lbind_setlibcall(L, "new");
    }
    lua_setfield(L, -2, "action");
}


/* nui node routines */

/* { node=parent } */
#define NUI_NODEBOX 0x220DEB03

static void get_nodebox(lua_State *L) {
    if (getbox(L, (void*)NUI_NODEBOX)) {
        lua_pushliteral(L, "v");
        lbind_setmetafield(L, -2, "__mode");
    }
}

static void intern_node(lua_State *L, int node_idx, int parent_idx) {
    get_nodebox(L);
    lua_pushvalue(L, lbind_relindex(node_idx, 1));
    if (parent_idx != 0)
        lua_pushvalue(L, lbind_relindex(parent_idx, 2));
    else
        lua_pushnil(L);
    lua_rawset(L, -3);
    lua_pop(L, 1);
}

static int retrieve_node(lua_State *L, NUInode *n) {
    if (n == NULL) return 0;
    /*printf("  retrieve: %p\n", n);*/
    if (!lbind_retrieve(L, n)) {
        /*printf("!!can not retrieve: %p\n", n);*/
        NUInode *parent;
        lbind_wrap(L, n, &lbT_Node);
        lbind_untrack(L, -1);
        parent = nui_parent(n);
        if (parent != NULL) {
            retrieve_node(L, parent);
            intern_node(L, -2, -1);
            lua_pop(L, 1);
        }
    }
    return 1;
}

static int Lnode_new(lua_State *L) {
    NUIstate *S = get_default_state(L);
    NUIstring *s = NULL;
    NUInode *n;
    size_t i, count;
    int table_idx = 1;
    int type = lua_type(L, 1);
    if (type == LUA_TNIL || type == LUA_TSTRING) {
        s = check_string(S, L, 1);
        type = lua_type(L, table_idx = 2);
    }

    n = nui_node(S, s);
    printf("node.new: %p", n);
    if (type != LUA_TTABLE) {
        lbind_wrap(L, n, &lbT_Node);
        printf("\n");
        return 1;
    }
    printf(" {\n");

    /* get children */
    for (i = 1;; ++i) {
        NUInode *child;
        lua_rawgeti(L, table_idx, i);
        if (lua_isnil(L, -1)) {
            count = i-1;
            lua_pop(L, 1);
            break;
        }
        child = lbind_test(L, -1, &lbT_Node);
        printf("  [%d]: %p,\n", i, child);
        if (child == NULL) {
            const char *tname = lbind_type(L, -1);
            return lbind_argferror(L, table_idx,
                    "invalid node in table #%d, got %s", i,
                    tname != NULL ? tname : luaL_typename(L, -1));
        }
        nui_setparent(child, n);
        lua_pop(L, 1);
    }

    /* get options */
    lua_pushinteger(L, count);
    /* lua_pushnil(L); */
    while (lua_next(L, table_idx)) {
        int idx;
        int keytype = lua_type(L, -2);
        NUIstring *key;
        NUIvalue v;
        printf("  %s",   lbind_tostring(L, -2)); lua_pop(L, 1);
        printf(": %s,\n", lbind_tostring(L, -1)); lua_pop(L, 1);
        if (keytype == LUA_TNUMBER
                && (idx = lua_tointeger(L, -2)) >= 1
                && (size_t)idx <= count)
            continue;
        if (lua_type(L, -2) != LUA_TSTRING)
            return lbind_argferror(L, table_idx,
                    "invalid key in table: %s", lbind_tostring(L, -2));
        if (!test_value(L, -1, &v))
            return lbind_argferror(L, table_idx,
                    "invalid value in table: %s", lbind_tostring(L, -1));
        key = check_string(S, L, -2);
        if (nui_setattr(n, key, v)) {
            lua_pushvalue(L, -2);
            lua_pushnil(L);
            lua_rawset(L, table_idx);
        }
        lua_pop(L, 1);
    }

    lbind_wrap(L, n, &lbT_Node);
    for (i = 1; i <= count; ++i) {
        lua_rawgeti(L, table_idx, i);
        intern_node(L, -1, -2);
        lua_pop(L, 1);
        lua_pushnil(L);
        lua_rawseti(L, table_idx, i);
    }

    lua_pushvalue(L, table_idx);
    lua_setuservalue(L, -2);
    printf("}\n");
    return 1;
}

static int Lnode_delete(lua_State *L) {
    NUInode *n = lbind_test(L, 1, &lbT_Node);
    if (n != NULL) {
        /*printf("!!!! node deletor: %p\n", n);*/
        intern_node(L, 1, 0);
        lbind_delete(L, 1);
        nui_dropnode(n);
    }
    return 0;
}

static int Lnode___len(lua_State *L) {
    NUInode *n = lbind_check(L, 1, &lbT_Node);
    lua_pushinteger(L, nui_childcount(n));
    return 1;
}

static int Lnode_isinstance(lua_State *L) {
    NUInode *n = lbind_check(L, 1, &lbT_Node);
    NUIclass *c = check_class(L, 2);
    lua_pushboolean(L, nui_matchclass(n, c->name));
    return 1;
}

static int Lnode_map(lua_State *L) {
    NUInode *n = lbind_check(L, 1, &lbT_Node);
    if (nui_mapnode(n))
        lbind_returnself(L);
    return 0;
}

static int Lnode_unmap(lua_State *L) {
    NUInode *n = lbind_check(L, 1, &lbT_Node);
    if (nui_unmapnode(n))
        lbind_returnself(L);
    return 0;
}

static int children_iter(lua_State *L) {
    NUInode *n = lbind_check(L, 1, &lbT_Node);
    NUInode *curr = lbind_opt(L, 2, NULL, &lbT_Node);
    return retrieve_node(L, nui_nextchild(n, curr));
}

static int children_riter(lua_State *L) {
    NUInode *n = lbind_check(L, 1, &lbT_Node);
    NUInode *curr = lbind_opt(L, 2, NULL, &lbT_Node);
    return retrieve_node(L, nui_prevchild(n, curr));
}

static int Lnode_iter(lua_State *L) {
    lbind_check(L, 1, &lbT_Node);
    if (!lua_toboolean(L, 2))
        lua_pushcfunction(L, children_iter);
    else
        lua_pushcfunction(L, children_riter);
    lua_pushvalue(L, 1);
    return 2;
}

static int leafs_iter(lua_State *L) {
    NUInode *n = lbind_check(L, 1, &lbT_Node);
    NUInode *curr = lbind_opt(L, 2, NULL, &lbT_Node);
    return retrieve_node(L, nui_nextleaf(n, curr));
}

static int leafs_riter(lua_State *L) {
    NUInode *n = lbind_check(L, 1, &lbT_Node);
    NUInode *curr = lbind_opt(L, 2, NULL, &lbT_Node);
    return retrieve_node(L, nui_prevleaf(n, curr));
}

static int Lnode_walk(lua_State *L) {
    lbind_check(L, 1, &lbT_Node);
    if (!lua_toboolean(L, 2))
        lua_pushcfunction(L, leafs_iter);
    else
        lua_pushcfunction(L, leafs_riter);
    lua_pushvalue(L, 1);
    return 2;
}

static int Lnode_append(lua_State *L) {
    NUInode *n = lbind_check(L, 1, &lbT_Node);
    NUInode *newnode = lbind_check(L, 2, &lbT_Node);
    nui_append(n, newnode);
    retrieve_node(L, nui_parent(n));
    intern_node(L, 2, -1);
    lbind_returnself(L);
}

static int Lnode_insert(lua_State *L) {
    NUInode *n = lbind_check(L, 1, &lbT_Node);
    NUInode *newnode = lbind_check(L, 2, &lbT_Node);
    nui_insert(n, newnode);
    retrieve_node(L, nui_parent(n));
    intern_node(L, 2, -1);
    lbind_returnself(L);
}

static int Lnode_detach(lua_State *L) {
    NUInode *n = lbind_check(L, 1, &lbT_Node);
    nui_detach(n);
    intern_node(L, 1, 0);
    lbind_returnself(L);
}

static int Lnode_frompos(lua_State *L) {
    NUInode *n = lbind_check(L, 1, &lbT_Node);
    NUInode *res;
    NUIpoint pt;
    pt.x = (float)luaL_checknumber(L, 2);
    pt.y = (float)luaL_checknumber(L, 3);
    res = nui_nodefrompos(n, pt);
    return retrieve_node(L, res);
}

static int Lnode_abspos(lua_State *L) {
    NUInode *n = lbind_check(L, 1, &lbT_Node);
    NUIpoint pt = nui_abspos(n);
    lua_pushnumber(L, (lua_Number)pt.x);
    lua_pushnumber(L, (lua_Number)pt.y);
    return 2;
}

static int Lnode_position(lua_State *L) {
    NUInode *n = lbind_check(L, 1, &lbT_Node);
    NUIpoint pt = nui_position(n);
    lua_pushnumber(L, (lua_Number)pt.x);
    lua_pushnumber(L, (lua_Number)pt.y);
    return 2;
}

static int Lnode_usersize(lua_State *L) {
    NUInode *n = lbind_check(L, 1, &lbT_Node);
    NUIsize sz = nui_usersize(n);
    lua_pushnumber(L, (lua_Number)sz.width);
    lua_pushnumber(L, (lua_Number)sz.height);
    return 2;
}

static int Lnode_naturalsize(lua_State *L) {
    NUInode *n = lbind_check(L, 1, &lbT_Node);
    NUIsize sz = nui_naturalsize(n);
    lua_pushnumber(L, (lua_Number)sz.width);
    lua_pushnumber(L, (lua_Number)sz.height);
    return 2;
}

static int Lnode_move(lua_State *L) {
    NUInode *n = lbind_check(L, 1, &lbT_Node);
    NUIpoint pt;
    pt.x = (float)luaL_checknumber(L, 2);
    pt.y = (float)luaL_checknumber(L, 3);
    nui_move(n, pt);
    lbind_returnself(L);
}

static int Lnode_resize(lua_State *L) {
    NUInode *n = lbind_check(L, 1, &lbT_Node);
    NUIsize sz;
    sz.width  = (float)luaL_checknumber(L, 2);
    sz.height = (float)luaL_checknumber(L, 3);
    nui_resize(n, sz);
    lbind_returnself(L);
}

static int Lnode_show(lua_State *L) {
    NUInode *n = lbind_check(L, 1, &lbT_Node);
    nui_show(n);
    lbind_returnself(L);
}

static int Lnode_hide(lua_State *L) {
    NUInode *n = lbind_check(L, 1, &lbT_Node);
    nui_hide(n);
    lbind_returnself(L);
}

static int node_index(lua_State *L) {
    NUInode *n = lbind_check(L, 1, &lbT_Node);
    lua_CFunction f;
    int type;

    /* check metatable field */
    if (lua_getmetatable(L, 1)) {
        lua_pushvalue(L, 2);
        lua_rawget(L, -2);
        if (!lua_isnil(L, -1))
            return 1;
    }

    /* check node property */
    lua_pushvalue(L, 2);
    lua_rawget(L, lua_upvalueindex(1));
    f = lua_tocfunction(L, -1);
    if (f != NULL) {
        lua_settop(L, 2);
        return f(L);
    }

    /* check index */
    if ((type = lua_type(L, 2)) == LUA_TNUMBER) {
        NUInode *res;
        int idx = lua_tointeger(L, 2);
        if (idx == 0) return 0;
        res = nui_indexnode(n, idx > 0 ? idx-1 : idx);
        return retrieve_node(L, res);
    }

    /* check attr */
    if (type == LUA_TSTRING) {
        NUIstring *s = test_string(get_default_state(L), L, 2);
        NUIvalue v;
        if (nui_getattr(n, s, &v))
            return push_value(L, v);
    }

    /* check uservalue */
    lua_getuservalue(L, 1);
    if (lua_isnil(L, -1)) return 0;
    lua_pushvalue(L, 2);
    lua_rawget(L, -2);
    return 1;
}

static int node_newindex(lua_State *L) {
    NUInode *n = lbind_check(L, 1, &lbT_Node);
    lua_CFunction f;
    int type;

    /* check node property */
    lua_pushvalue(L, 2);
    lua_rawget(L, lua_upvalueindex(1));
    f = lua_tocfunction(L, -1);
    if (f != NULL) {
        lua_settop(L, 3);
        return f(L);
    }

    /* check index */
    if ((type = lua_type(L, 2)) == LUA_TNUMBER) {
        NUInode *res, *newnode;
        int idx = lua_tointeger(L, 2);
        res = nui_indexnode(n, idx > 0 ? idx-1 : idx);
        newnode = lbind_check(L, 3, &lbT_Node);
        if (res != NULL) {
            nui_insert(res, newnode);
            nui_detach(res);
            retrieve_node(L, res);
            intern_node(L, -1, 0);
        }
        else {
            int count = nui_childcount(n);
            if (idx != count+1)
                lbind_argferror(L, 2, "index #%d out of bound (%d to %d)",
                        idx, -count, count+1);
            nui_setparent(newnode, n);
        }
        intern_node(L, 3, 1);
        return 0;
    }

    /* check attr */
    if (type == LUA_TSTRING) {
        NUIstring *s = test_string(get_default_state(L), L, 2);
        NUIvalue v;
        if (test_value(L, 3, &v) && nui_setattr(n, s, v))
            return 0;
    }

    /* check uservalue */
    lua_getuservalue(L, 1);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_createtable(L, 0, 1);
        lua_pushvalue(L, -1);
        lua_setuservalue(L, 1);
    }
    lua_pushvalue(L, 2);
    lua_pushvalue(L, 3);
    lua_rawset(L, -3);
    return 0;
}

static int Lnode_class(lua_State *L) {
    NUInode *n = lbind_check(L, 1, &lbT_Node);
    NUIclass *c;
    lbind_checkreadonly(L);
    c = nui_nodeclass(n);
    return retrieve_or_wrap(L, c, &lbT_Class);
}

static int Lnode_idx(lua_State *L) {
    NUInode *parent, *n = lbind_check(L, 1, &lbT_Node);
    int idx = 1;
    NUInode *i = NULL;
    lbind_checkreadonly(L);
    if ((parent = nui_parent(n)) == NULL) return 0;
    while ((i = nui_nextchild(parent, i)) != NULL && i != n)
        ++idx;
    if (i != n) return 0;
    lua_pushinteger(L, idx);
    return 1;
}

static int Lnode_parent(lua_State *L) {
    NUInode *n = lbind_check(L, 1, &lbT_Node);
    if (lua_isnone(L, 3))
        return retrieve_node(L, nui_parent(n));
    nui_setparent(n, lbind_check(L, 3, &lbT_Node));
    intern_node(L, 1, 3);
    return 0;
}

static int Lnode_children(lua_State *L) {
    NUInode *n = lbind_check(L, 1, &lbT_Node);
    NUInode *children;
    lbind_checkwriteonly(L);
    children = lbind_check(L, 3, &lbT_Node);
    nui_setchildren(n, children);
    return 0;
}

#define nui_firstchild(n)  (nui_nextchild)(n, NULL)
#define nui_lastchild(n)   (nui_prevchild)(n, NULL)
#define nui_nextsibling(n) (nui_nextsibling)(NULL, n)
#define nui_prevsibling(n) (nui_prevsibling)(NULL, n)
#define nui_firstleaf(n)   (nui_nextleaf)(n, NULL)
#define nui_lastleaf(n)    (nui_prevleaf)(n, NULL)
#define nui_nextleaf(n)    (nui_nextleaf)(NULL, n)
#define nui_prevleaf(n)    (nui_prevleaf)(NULL, n)
#define DEFINE_NODE_GETTER(name) \
    static int Lnode_##name(lua_State *L) {         \
        NUInode *n = lbind_check(L, 1, &lbT_Node);  \
        lbind_checkreadonly(L);                     \
        return retrieve_node(L, nui_##name(n));     \
    }

DEFINE_NODE_GETTER(firstchild)
DEFINE_NODE_GETTER(lastchild)
DEFINE_NODE_GETTER(prevsibling)
DEFINE_NODE_GETTER(nextsibling)
DEFINE_NODE_GETTER(root)
DEFINE_NODE_GETTER(firstleaf)
DEFINE_NODE_GETTER(lastleaf)
DEFINE_NODE_GETTER(prevleaf)
DEFINE_NODE_GETTER(nextleaf)

#undef nui_firstchild
#undef nui_lastchild
#undef nui_nextsibling
#undef nui_prevsibling
#undef nui_firstleaf
#undef nui_lastleaf
#undef nui_nextleaf
#undef nui_prevleaf
#undef DEFINE_NODE_GETTER

static int Lnode_handle(lua_State *L) {
    NUInode *n = lbind_check(L, 1, &lbT_Node);
    lbind_checkreadonly(L);
    lua_pushlightuserdata(L, nui_nativehandle(n));
    return 1;
}

static int Lnode_id(lua_State *L) {
    NUInode *n = lbind_check(L, 1, &lbT_Node);
    if (lua_isnone(L, 3))
        return push_string(L, n->id);
    n->id = check_string(get_default_state(L), L, 3);
    return 0;
}

static int Lnode_action(lua_State *L) {
    NUInode *n = lbind_check(L, 1, &lbT_Node);
    NUIaction *a;
    /* XXX */
    if (lua_isnone(L, 3)) {
        a = nui_getnodeaction(n);
        return retrieve_or_wrap(L, a, &lbT_Action);
    }
    a = lbind_check(L, 3, &lbT_Action);
    nui_setnodeaction(n, a);
    return 0;
}

static int Lnode_visible(lua_State *L) {
    NUInode *n = lbind_check(L, 1, &lbT_Node);
    lbind_checkreadonly(L);
    lua_pushboolean(L, nui_isvisible(n));
    return 1;
}

static void open_node(lua_State *L) {
    luaL_Reg libs[] = {
#define ENTRY(name) { #name, Lnode_##name }
        ENTRY(__len),
        ENTRY(new),
        ENTRY(delete),
        ENTRY(isinstance),
        ENTRY(map),
        ENTRY(unmap),
        ENTRY(walk),
        ENTRY(iter),
        ENTRY(append),
        ENTRY(insert),
        ENTRY(detach),
        ENTRY(frompos),
        ENTRY(abspos),
        ENTRY(position),
        ENTRY(usersize),
        ENTRY(naturalsize),
        ENTRY(move),
        ENTRY(resize),
        ENTRY(show),
        ENTRY(hide),
        { NULL, NULL }
    };
    luaL_Reg acc[] = {
        /* r  */ ENTRY(class),
        /* rw */ ENTRY(parent),
        /*  w */ ENTRY(children),
        /* r  */ ENTRY(firstchild),
        /* r  */ ENTRY(lastchild),
        /* r  */ ENTRY(prevsibling),
        /* r  */ ENTRY(nextsibling),
        /* r  */ ENTRY(root),
        /* r  */ ENTRY(firstleaf),
        /* r  */ ENTRY(lastleaf),
        /* r  */ ENTRY(prevleaf),
        /* r  */ ENTRY(nextleaf),
        /* r  */ ENTRY(handle),
        /* r  */ ENTRY(visible),
        /* rw */ ENTRY(id),
        /* r  */ ENTRY(idx),
        /* rw */ ENTRY(action),
        { NULL, NULL }
#undef  ENTRY
    };
    if (lbind_newmetatable(L, libs, &lbT_Node)) {
        luaL_newlib(L, acc); /* 1 */
        lua_pushvalue(L, -1); /* 2 */
        lua_pushcclosure(L, node_index, 1); /* 2->2 */
        lua_setfield(L, -3, "__index"); /* (2) */
        lua_pushcclosure(L, node_newindex, 1); /* 1->1 */
        lua_setfield(L, -2, "__newindex");
        lbind_setlibcall(L, "new");
    }
    lua_setfield(L, -2, "node");
}


/* nui class routines */

#define NUI_CLASS_CALLBACKS(X) \
    X(child_added) \
    X(child_removed) \
    X(delete_node) \
    X(get_handle) \
    X(get_parent) \
    X(getattr) \
    X(layout_naturalsize) \
    X(layout_update) \
    X(map) \
    X(new_node) \
    X(node_hide) \
    X(node_move) \
    X(node_resize) \
    X(node_show) \
    X(setattr) \
    X(unmap) \

typedef enum NUIclasscallbacks {
#define X(name) NUI_CCB_##name,
    NUI_CLASS_CALLBACKS(X)
#undef  X
    NUI_CCB_COUNT
} NUIclasscallbacks;

typedef struct LNUIclass {
    NUIclass base;
    lua_State *L;
    int refs[NUI_CCB_COUNT];
} LNUIclass;

static void lclass_deletor(NUIstate *S, NUIclass *c) {
    int i;
    LNUIclass *lc = (LNUIclass*)c;
    lua_State *L = lc->L;
    if (L != NULL) {
        for (i = 0; i < NUI_CCB_COUNT; ++i)
            luaL_unref(L, LUA_REGISTRYINDEX, lc->refs[i]);
        lc->L = NULL;
    }
}

static void init_lclass(lua_State *L, LNUIclass *c) {
    int i;
    c->L = L;
    for (i = 0; i < NUI_CCB_COUNT; ++i)
        c->refs[i] = LUA_REFNIL;
    c->base.deletor = lclass_deletor;
}

static NUIclass *test_class(lua_State *L, int idx) {
    NUIstate *S = get_default_state(L);
    NUIstring *name = test_string(S, L, idx);
    if (name != NULL) {
        LNUIclass *c = (LNUIclass*)nui_newclass(S, name, sizeof(LNUIclass));
        if (c == NULL) return NULL;
        if (!lbind_retrieve(L, c)) {
            init_lclass(L, c); /* XXX */
            lbind_wrap(L, c, &lbT_Class);
            lbind_untrack(L, -1);
        }
        return &c->base;
    }
    return lbind_test(L, idx, &lbT_Class);
}

static NUIclass *check_class(lua_State *L, int idx) {
    NUIclass *c;
    if ((c = test_class(L, idx)) == NULL)
        lbind_typeerror(L, idx, "class/classname");
    return c;
}

static int Lclass_new(lua_State *L) {
    NUIstate *S = get_default_state(L);
    NUIstring *name = check_string(S, L, 1);
    LNUIclass *c = (LNUIclass*)nui_class(S, name);
    if (c != NULL && c->base.deletor == lclass_deletor)
        return retrieve_or_wrap(L, c, &lbT_Class);
    c = (LNUIclass*)nui_newclass(S, name, sizeof(LNUIclass));
    if (c == NULL)
        lbind_argferror(L, 1,
                "class '%s' is exists and not Lua class", (char*)name);
    init_lclass(L, c);
    lbind_wrap(L, c, &lbT_Class);
    return 1;
}

static int Lclass_name(lua_State *L) {
    NUIclass *c = lbind_check(L, 1, &lbT_Class);
    lbind_checkreadonly(L);
    push_string(L, c->name);
    return 1;
}

static int Lclass_parent(lua_State *L) {
    NUIclass *c = lbind_check(L, 1, &lbT_Class);
    if (lua_isnone(L, 3))
        return retrieve_or_wrap(L, c->parent, &lbT_Class);
    c->parent = lbind_check(L, 2, &lbT_Class);
    return 0;
}

/* nui class attribute */

typedef struct LNUIattr {
    NUIattr base;
    lua_State *L;
    int setattr_ref;
    int getattr_ref;
} LNUIattr;

static void attr_deletor(NUIstate *S, NUIattr *attr) {
    LNUIattr *la = (LNUIattr*)attr;
    lua_State *L = la->L;
    (void)S;
    /*printf("!!!! attr_deletor: %p\n", la);*/
    luaL_unref(L, LUA_REGISTRYINDEX, la->getattr_ref);
    luaL_unref(L, LUA_REGISTRYINDEX, la->setattr_ref);
}

static int attr_getattr(NUIattr *attr, NUInode *n, NUIstring *key, NUIvalue *v) {
    LNUIattr *la = (LNUIattr*)attr;
    lua_State *L = la->L;
    int res;
    lua_rawgeti(L, LUA_REGISTRYINDEX, la->getattr_ref);
    retrieve_node(L, n);
    push_string(L, key);
    if (!push_value(L, la->base.value)) lua_pushnil(L);
    if (lbind_pcall(L, 3, 1) != LUA_OK)
        luaL_error(L, "getattr(Node: %p, %s): %s\n",
                n, (char*)key, lua_tostring(L, -1));
    res = test_value(L, -1, v);
    if (res) la->base.value = *v;
    lua_pop(L, 1);
    return res;
}

static int attr_setattr(NUIattr *attr, NUInode *n, NUIstring *key, NUIvalue *v) {
    LNUIattr *la = (LNUIattr*)attr;
    lua_State *L = la->L;
    int res = 1, top = lua_gettop(L);
    lua_rawgeti(L, LUA_REGISTRYINDEX, la->setattr_ref);
    retrieve_node(L, n);
    push_string(L, key);
    if (!push_value(L, *v)) lua_pushnil(L);
    if (!push_value(L, la->base.value)) lua_pushnil(L);
    if (lbind_pcall(L, 4, LUA_MULTRET) != LUA_OK)
        luaL_error(L, "setattr(Node: %p, %s): %s\n",
                n, (char*)key, lua_tostring(L, -1));
    if (lua_gettop(L) > top) {
        test_value(L, top+1, &la->base.value);
        res = lua_isnoneornil(L, top+2);
    }
    lua_settop(L, top);
    return res;
}

static int Lclass_attr(lua_State *L) {
    LNUIclass *c = lbind_check(L, 1, &lbT_Class);
    NUIstring *name = check_string(c->base.S, L, 2);
    LNUIattr *attr = (LNUIattr*)nui_attr(&c->base, name);
    if (attr != NULL && attr->base.deletor != attr_deletor)
        lbind_argferror(L, 1,
                "attr %s exists and not Lua attr", (char*)name);
    if (attr == NULL)
        attr = (LNUIattr*)nui_newattr(&c->base, name, sizeof(LNUIattr));
    if (attr == NULL)
        luaL_error(L, "create attr %s error", (char*)name);
    if (lua_gettop(L) == 2) { /* get attribute */
        lua_rawgeti(L, LUA_REGISTRYINDEX, attr->getattr_ref);
        lua_rawgeti(L, LUA_REGISTRYINDEX, attr->setattr_ref);
        return 3;
    }
    attr->base.deletor = attr_deletor;
    attr->L = L;
    attr->getattr_ref = LUA_REFNIL;
    attr->setattr_ref = LUA_REFNIL;
    if (!lua_isnoneornil(L, 3)) {
        lua_pushvalue(L, 3);
        attr->getattr_ref = luaL_ref(L, LUA_REGISTRYINDEX);
        attr->base.getattr = attr_getattr;
    }
    if (!lua_isnoneornil(L, 4)) {
        lua_pushvalue(L, 4);
        attr->setattr_ref = luaL_ref(L, LUA_REGISTRYINDEX);
        attr->base.setattr = attr_setattr;
    }
    return 0;
}

/* nui class callback */

static int ccb_new_node(NUIclass *c, NUInode *n) {
    LNUIclass *lc = (LNUIclass*)c;
    lua_State *L = lc->L;
    int res;
    if (lc->refs[NUI_CCB_new_node] == LUA_REFNIL)
        return 0;
    lua_rawgeti(L, LUA_REGISTRYINDEX, lc->refs[NUI_CCB_new_node]);
    retrieve_node(L, n);
    if (handle_error(L, lbind_pcall(L, 1, 1), NULL))
        return 0;
    res = lua_toboolean(L, -1);
    lua_pop(L, 1);
    return res;
}

static void ccb_delete_node(NUIclass *c, NUInode *n) {
    LNUIclass *lc = (LNUIclass*)c;
    lua_State *L = lc->L;
    if (lc->refs[NUI_CCB_delete_node] == LUA_REFNIL)
        return;
    lua_rawgeti(L, LUA_REGISTRYINDEX, lc->refs[NUI_CCB_delete_node]);
    retrieve_node(L, n);
    handle_error(L, lbind_pcall(L, 1, 0), NULL);
}

static int ccb_getattr(NUIclass *c, NUInode *n, NUIstring *key, NUIvalue *pv) {
    LNUIclass *lc = (LNUIclass*)c;
    lua_State *L = lc->L;
    int res;
    if (lc->refs[NUI_CCB_getattr] == LUA_REFNIL)
        return 0;
    lua_rawgeti(L, LUA_REGISTRYINDEX, lc->refs[NUI_CCB_getattr]);
    retrieve_node(L, n);
    if (handle_error(L, lbind_pcall(L, 1, 1), NULL))
        return 0;
    res = test_value(L, -1, pv);
    lua_pop(L, 1);
    return res;
}

static int ccb_setattr(NUIclass *c, NUInode *n, NUIstring *key, NUIvalue *pv) {
    LNUIclass *lc = (LNUIclass*)c;
    lua_State *L = lc->L;
    int res;
    if (lc->refs[NUI_CCB_getattr] == LUA_REFNIL)
        return 0;
    lua_rawgeti(L, LUA_REGISTRYINDEX, lc->refs[NUI_CCB_getattr]);
    retrieve_node(L, n);
    push_value(L, *pv);
    if (handle_error(L, lbind_pcall(L, 2, 1), NULL))
        return 0;
    res = lua_toboolean(L, -1);
    lua_pop(L, 1);
    return res;
}

static void *ccb_map(NUIclass *c, NUInode *n) {
    LNUIclass *lc = (LNUIclass*)c;
    lua_State *L = lc->L;
    void *res;
    if (lc->refs[NUI_CCB_map] == LUA_REFNIL)
        return 0;
    lua_rawgeti(L, LUA_REGISTRYINDEX, lc->refs[NUI_CCB_map]);
    retrieve_node(L, n);
    if (handle_error(L, lbind_pcall(L, 1, 1), NULL))
        return 0;
    res = lua_touserdata(L, -1);
    lua_pop(L, 1);
    return res;
}

static int ccb_unmap(NUIclass *c, NUInode *n, void *handle) {
    LNUIclass *lc = (LNUIclass*)c;
    lua_State *L = lc->L;
    int res;
    if (lc->refs[NUI_CCB_unmap] == LUA_REFNIL)
        return 0;
    lua_rawgeti(L, LUA_REGISTRYINDEX, lc->refs[NUI_CCB_unmap]);
    retrieve_node(L, n);
    lua_pushlightuserdata(L, handle);
    if (handle_error(L, lbind_pcall(L, 2, 1), NULL))
        return 0;
    res = lua_toboolean(L, -1);
    lua_pop(L, 1);
    return res;
}

static NUInode* ccb_get_parent(NUIclass *c, NUInode *n, NUInode *parent) {
    LNUIclass *lc = (LNUIclass*)c;
    lua_State *L = lc->L;
    NUInode *res;
    if (lc->refs[NUI_CCB_get_parent] == LUA_REFNIL)
        return 0;
    lua_rawgeti(L, LUA_REGISTRYINDEX, lc->refs[NUI_CCB_get_parent]);
    retrieve_node(L, n);
    retrieve_node(L, parent);
    if (handle_error(L, lbind_pcall(L, 2, 1), NULL))
        return 0;
    res = lbind_test(L, -1, &lbT_Node);
    lua_pop(L, 1);
    return res;
}

static void* ccb_get_handle(NUIclass *c, NUInode *n, void *handle) {
    LNUIclass *lc = (LNUIclass*)c;
    lua_State *L = lc->L;
    void *res;
    if (lc->refs[NUI_CCB_get_handle] == LUA_REFNIL)
        return 0;
    lua_rawgeti(L, LUA_REGISTRYINDEX, lc->refs[NUI_CCB_get_handle]);
    retrieve_node(L, n);
    lua_pushlightuserdata(L, handle);
    if (handle_error(L, lbind_pcall(L, 2, 1), NULL))
        return 0;
    res = lua_touserdata(L, -1);
    lua_pop(L, 1);
    return res;
}

static void ccb_child_added(NUIclass *c, NUInode *n, NUInode *child) {
    LNUIclass *lc = (LNUIclass*)c;
    lua_State *L = lc->L;
    if (lc->refs[NUI_CCB_child_added] == LUA_REFNIL)
        return;
    lua_rawgeti(L, LUA_REGISTRYINDEX, lc->refs[NUI_CCB_child_added]);
    retrieve_node(L, n);
    retrieve_node(L, child);
    handle_error(L, lbind_pcall(L, 2, 0), NULL);
}

static void ccb_child_removed(NUIclass *c, NUInode *n, NUInode *child) {
    LNUIclass *lc = (LNUIclass*)c;
    lua_State *L = lc->L;
    if (lc->refs[NUI_CCB_child_removed] == LUA_REFNIL)
        return;
    lua_rawgeti(L, LUA_REGISTRYINDEX, lc->refs[NUI_CCB_child_removed]);
    retrieve_node(L, n);
    retrieve_node(L, child);
    handle_error(L, lbind_pcall(L, 2, 0), NULL);
}

static int ccb_node_show(NUIclass *c, NUInode *n) {
    LNUIclass *lc = (LNUIclass*)c;
    lua_State *L = lc->L;
    int res;
    if (lc->refs[NUI_CCB_node_show] == LUA_REFNIL)
        return 0;
    lua_rawgeti(L, LUA_REGISTRYINDEX, lc->refs[NUI_CCB_node_show]);
    retrieve_node(L, n);
    if (handle_error(L, lbind_pcall(L, 1, 1), NULL))
        return 0;
    res = lua_toboolean(L, -1);
    lua_pop(L, 1);
    return res;
}

static int ccb_node_hide(NUIclass *c, NUInode *n) {
    LNUIclass *lc = (LNUIclass*)c;
    lua_State *L = lc->L;
    int res;
    if (lc->refs[NUI_CCB_node_hide] == LUA_REFNIL)
        return 0;
    lua_rawgeti(L, LUA_REGISTRYINDEX, lc->refs[NUI_CCB_node_hide]);
    retrieve_node(L, n);
    if (handle_error(L, lbind_pcall(L, 1, 1), NULL))
        return 0;
    res = lua_toboolean(L, -1);
    lua_pop(L, 1);
    return res;
}

static int ccb_node_move(NUIclass *c, NUInode *n, NUIpoint pos) {
    LNUIclass *lc = (LNUIclass*)c;
    lua_State *L = lc->L;
    int res;
    if (lc->refs[NUI_CCB_node_move] == LUA_REFNIL)
        return 0;
    lua_rawgeti(L, LUA_REGISTRYINDEX, lc->refs[NUI_CCB_node_move]);
    retrieve_node(L, n);
    lua_pushnumber(L, (lua_Number)pos.x);
    lua_pushnumber(L, (lua_Number)pos.y);
    if (handle_error(L, lbind_pcall(L, 3, 1), NULL))
        return 0;
    res = lua_toboolean(L, -1);
    lua_pop(L, 1);
    return res;
}

static int ccb_node_resize(NUIclass *c, NUInode *n, NUIsize size) {
    LNUIclass *lc = (LNUIclass*)c;
    lua_State *L = lc->L;
    int res;
    if (lc->refs[NUI_CCB_node_resize] == LUA_REFNIL)
        return 0;
    lua_rawgeti(L, LUA_REGISTRYINDEX, lc->refs[NUI_CCB_node_resize]);
    retrieve_node(L, n);
    lua_pushnumber(L, (lua_Number)size.width);
    lua_pushnumber(L, (lua_Number)size.height);
    if (handle_error(L, lbind_pcall(L, 3, 1), NULL))
        return 0;
    res = lua_toboolean(L, -1);
    lua_pop(L, 1);
    return res;
}

static int ccb_layout_update(NUIclass *c, NUInode *n) {
    LNUIclass *lc = (LNUIclass*)c;
    lua_State *L = lc->L;
    int res;
    if (lc->refs[NUI_CCB_layout_update] == LUA_REFNIL)
        return 0;
    lua_rawgeti(L, LUA_REGISTRYINDEX, lc->refs[NUI_CCB_layout_update]);
    retrieve_node(L, n);
    if (handle_error(L, lbind_pcall(L, 1, 1), NULL))
        return 0;
    res = lua_toboolean(L, -1);
    lua_pop(L, 1);
    return res;
}

static int ccb_layout_naturalsize(NUIclass *c, NUInode *n, NUIsize *psz) {
    LNUIclass *lc = (LNUIclass*)c;
    lua_State *L = lc->L;
    if (lc->refs[NUI_CCB_layout_update] == LUA_REFNIL)
        return 0;
    lua_rawgeti(L, LUA_REGISTRYINDEX, lc->refs[NUI_CCB_layout_update]);
    retrieve_node(L, n);
    if (handle_error(L, lbind_pcall(L, 1, 2), NULL))
        return 0;
    if (lua_isnil(L, -1))
        return 0;
    if (psz) {
        psz->width  = (float)lua_tonumber(L, -2);
        psz->height = (float)lua_tonumber(L, -1);
    }
    lua_pop(L, 2);
    return 1;
}

static int Lclass_callback(lua_State *L) {
    static lbind_EnumItem items[] = {
#define X(name) { #name, NUI_CCB_##name },
        NUI_CLASS_CALLBACKS(X)
#undef  X
        { NULL, 0 }
    };
    static lbind_Enum et = LBIND_INITENUM("callback", items);
    LNUIclass *c = lbind_check(L, 1, &lbT_Class);
    int idx = lbind_checkenum(L, 2, &et);
    if (lua_isnone(L, 3)) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, c->refs[idx]);
        return 1;
    }
    if (lua_isnil(L, 3))
        c->refs[idx] = LUA_REFNIL;
    else {
        lua_pushvalue(L, 3);
        c->refs[idx] = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    switch (idx) {
#define X(name) case NUI_CCB_##name: c->base.name = \
        (c->refs[idx] == LUA_REFNIL ? NULL : ccb_##name); break;
    NUI_CLASS_CALLBACKS(X)
#undef  X
    default: break;
    }
    return 0;
}

static void open_class(lua_State *L) {
    luaL_Reg libs[] = {
#define ENTRY(name) { #name, Lclass_##name }
        ENTRY(new),
        ENTRY(attr),
        ENTRY(callback),
        { NULL, NULL }
    };
    luaL_Reg acc[] = {
        /* r */  ENTRY(name),
        /* rw */ ENTRY(parent),
        { NULL, NULL }
    };
#undef  ENTRY
    if (lbind_newmetatable(L, libs, &lbT_Class)) {
        lbind_setmaptable(L, acc, LBIND_INDEX|LBIND_NEWINDEX);
        lbind_setlibcall(L, "new");
    }
    lua_setfield(L, -2, "class");
}


/* nui utils */

static int Lutils_point(lua_State *L) {
    NUIvalue v;
    if (lua_gettop(L) == 2) {
        float x = (float)luaL_checknumber(L, 1);
        float y = (float)luaL_checknumber(L, 2);
        v = nui_pointvalue(nui_point(x, y));
        push_value(L, v);
        return 1;
    }
    else if (test_value(L, 1, &v) && v.type == NUI_TPOINT) {
        lua_pushnumber(L, (lua_Number)v.point.x);
        lua_pushnumber(L, (lua_Number)v.point.y);
        return 2;
    }
    return 0;
}

static int Lutils_size(lua_State *L) {
    NUIvalue v;
    if (lua_gettop(L) == 2) {
        float width  = (float)luaL_checknumber(L, 1);
        float height = (float)luaL_checknumber(L, 2);
        v = nui_sizevalue(nui_size(width, height));
        push_value(L, v);
        return 1;
    }
    else if (test_value(L, 1, &v) && v.type == NUI_TSIZE) {
        lua_pushnumber(L, (lua_Number)v.size.width);
        lua_pushnumber(L, (lua_Number)v.size.height);
        return 2;
    }
    return 0;
}

static void open_utils(lua_State *L) {
    luaL_Reg libs[] = {
#define ENTRY(name) { #name, Lutils_##name }
        ENTRY(point),
        ENTRY(size),
        { NULL, NULL }
    };
#undef  ENTRY
    luaL_setfuncs(L, libs, 0);
}

LUALIB_API int luaopen_nui(lua_State *L) {
    open_state(L);
    open_node(L);
    open_class(L);
    open_action(L);
    open_utils(L);
    return 1;
}
/* cc: flags+='-s -O2 -shared -DLUA_BUILD_AS_DLL -DNUI_DLL'
 * cc: libs+='-llua53.dll' run='lua test.lua'
 * cc: input+='nui.c' output="nui.dll" */
