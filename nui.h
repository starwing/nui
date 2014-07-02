/* NUI - a native, node-based UI library */
#ifndef nui_h
#define nui_h


#include <stddef.h>


#if defined(__WIN32__) && defined(NUI_DLL)
# ifdef NUI_CORE
#  define NUI_API __declspec(dllexport)
# else
#  define NUI_API __declspec(dllimport)
# endif /* NUI_CORE */
#endif /* __WIN32__ */

#ifndef NUI_API
# define NUI_API extern
#endif


#define NUI_VALUE_TYPES(X) \
    X(POINTER,   void*,       pointer  ) \
    X(NUMBER,    double,      number   ) \
    X(INTEGER,   long,        integer  ) \
    X(FUNCTION,  NUIfuncptr*, function ) \
    X(POINT,     NUIpoint,    point    ) \
    X(SIZE,      NUIsize,     size     ) \
    X(ACTION,    NUIaction*,  action   ) \
    X(NODE,      NUInode*,    node     ) \
    X(STRING,    NUIstring*,  string   ) \


typedef struct NUIstate  NUIstate;
typedef struct NUInode   NUInode;
typedef struct NUIaction NUIaction;
typedef struct NUIstring NUIstring;

typedef void (*NUIfuncptr) (void);

typedef struct NUIpoint {
    int x, y;
} NUIpoint;

typedef struct NUIsize {
    int width, height;
} NUIsize;

typedef enum NUItype {
    NUI_TNIL,
#define X(name, type, var) NUI_T##name,
    NUI_VALUE_TYPES(X)
#undef  X
    NUI_TARGSEND,
    NUI_TYPE_COUNT
} NUItype;

typedef struct NUIvalue {
    NUItype type;
    union {
#define X(name, type, var) type var;
        NUI_VALUE_TYPES(X)
#undef  X
    };
} NUIvalue;

typedef void NUIactionf(NUIstate *S, NUIaction *a, NUInode *n);


/* destroy */
NUI_API void nui_close(NUIstate *S);


/* string */
NUI_API NUIstring *nui_string  (NUIstate *S, const char *s);
NUI_API NUIstring *nui_newlstr (NUIstate *S, const char *s, size_t len);
NUI_API int nui_holdstring (NUIstate *S, NUIstring *s);
NUI_API int nui_dropstring (NUIstate *S, NUIstring *s);

NUI_API size_t   nui_strlen   (NUIstring *s);
NUI_API unsigned nui_strhash  (NUIstring *s);
NUI_API unsigned nui_calchash (NUIstate *S, const char *s, size_t len);


/* action arguments stack */

NUI_API int  nui_gettop(NUIstate *S);
NUI_API void nui_settop(NUIstate *S, int idx);
NUI_API int  nui_pushvalue(NUIstate *S, NUIvalue v);
NUI_API int  nui_setvalue(NUIstate *S, int idx, NUIvalue v);
NUI_API int  nui_getvalue(NUIstate *S, int idx, NUIvalue *pv);
NUI_API void nui_rotate(NUIstate *S, int idx, int n);
NUI_API int  nui_copyvalues(NUIstate *S, int idx);


/* loop & action */
NUI_API int  nui_loop(NUIstate *S);
NUI_API int  nui_pollevents(NUIstate *S);
NUI_API int  nui_waitevents(NUIstate *S);
NUI_API void nui_breakloop(NUIstate *S);

NUI_API unsigned nui_time(NUIstate *S);

NUI_API NUIaction *nui_action(NUIstate *S, NUIactionf *f, size_t sz);
NUI_API NUIaction *nui_namedaction(NUIstate *S, NUIstring *name);
NUI_API void nui_dropaction(NUIaction *a);

NUI_API NUIaction *nui_copyaction(NUIaction *a);
NUI_API size_t nui_actionsize(NUIaction *a);

NUI_API void nui_setactionf(NUIaction *a, NUIactionf *f);
NUI_API NUIactionf *nui_getactionf(NUIaction *a);
NUI_API void nui_setactionnode(NUIaction *a, NUInode *n);
NUI_API NUInode *nui_getactionnode(NUIaction *a);

NUI_API void nui_linkaction(NUIaction *a, NUIaction *newa);
NUI_API void nui_unlinkaction(NUIaction *a);

NUI_API NUIaction *nui_nextaction(NUIaction *a, NUIaction *curr);

NUI_API void nui_emitaction(NUIaction *a, int nargs);

NUI_API void nui_starttimer(NUIaction *a, unsigned delayed, unsigned interval);
NUI_API void nui_stoptimer(NUIaction *a);


/* node */
NUI_API NUInode *nui_node(NUIstate *S, NUIstring *class_name);
NUI_API void nui_dropnode(NUInode *n);

NUI_API NUIstring *nui_classname(NUInode *n);
NUI_API int nui_matchclass(NUInode *n, NUIstring *class_name);

NUI_API int nui_mapnode   (NUInode *n);
NUI_API int nui_unmapnode (NUInode *n);

NUI_API NUInode *nui_parent(NUInode *n);
NUI_API NUInode *nui_firstchild(NUInode *n);
NUI_API NUInode *nui_lastchild(NUInode *n);
NUI_API NUInode *nui_prevsibling(NUInode *n);
NUI_API NUInode *nui_nextsibling(NUInode *n);

NUI_API NUInode *nui_root(NUInode *n);
NUI_API NUInode *nui_firstleaf(NUInode *n);
NUI_API NUInode *nui_lastleaf(NUInode *n);
NUI_API NUInode *nui_prevleaf(NUInode *n);
NUI_API NUInode *nui_nextleaf(NUInode *n);

NUI_API NUInode *nui_index(NUInode *n, int idx);
NUI_API size_t   nui_childcount(NUInode *n);

NUI_API void nui_setparent(NUInode *n, NUInode *parent);
NUI_API void nui_setchildren(NUInode *n, NUInode *children);

NUI_API void nui_append(NUInode *n, NUInode *newnode);
NUI_API void nui_insert(NUInode *n, NUInode *newnode);

NUI_API void nui_detach(NUInode *n);


/* node attributes */
NUI_API NUInode *nui_nodefrompos(NUInode *n, NUIpoint pos);
NUI_API NUIpoint nui_abspos(NUInode *n);
NUI_API NUIpoint nui_position(NUInode *n);
NUI_API NUIsize  nui_size(NUInode *n);
NUI_API NUIsize  nui_naturalsize(NUInode *n);

NUI_API void nui_move(NUInode *n, NUIpoint pos);
NUI_API void nui_resize(NUInode *n, NUIsize size);
NUI_API int  nui_show(NUInode *n);
NUI_API int  nui_hide(NUInode *n);
NUI_API int  nui_isvisible(NUInode *n);

NUI_API void *nui_gethandle(NUInode *n);

NUI_API int nui_getattr(NUInode *n, NUIstring *key, NUIvalue *pv);
NUI_API int nui_setattr(NUInode *n, NUIstring *key, NUIvalue v);

NUI_API void nui_setid(NUInode *n, NUIstring *id);
NUI_API NUIstring *nui_getid(NUInode *n);

NUI_API NUIaction *nui_getaction(NUInode *n);
NUI_API void       nui_setaction(NUInode *n, NUIaction *a);


/* value helpers */
#ifndef NUI_INLINE
# ifdef NUI_STATIC_INLINE
#  define NUI_INLINE static 
# else
#  define NUI_INLINE inline static
# endif
#endif /* NUI_INLINE */

NUI_INLINE NUIvalue nui_nilvalue(void) {
    NUIvalue v;
    v.type = NUI_TNIL;
    return v;
}

NUI_INLINE NUIvalue nui_argsend(void) {
    NUIvalue v;
    v.type = NUI_TARGSEND;
    return v;
}

#define X(name, vtype, var) \
    NUI_INLINE NUIvalue nui_##var##value(vtype var) { \
        NUIvalue v;                                   \
        v.type = NUI_T##name;                         \
        v.var = var;                                  \
        return v;                                     \
    }
NUI_VALUE_TYPES(X)
#undef X

#endif /* nui_h */
