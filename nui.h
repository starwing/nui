#ifndef nui_h
#define nui_h


#ifndef NUI_NS_BEGIN
# ifdef __cplusplus
#   define NUI_NS_BEGIN extern "C" {
#   define NUI_NS_END   }
# else
#   define NUI_NS_BEGIN
#   define NUI_NS_END
# endif
#endif /* NUI_NS_BEGIN */

#ifndef NUI_STATIC
# ifdef __GNUC__
#   define NUI_STATIC static __attribute((unused))
# else
#   define NUI_STATIC static
# endif
#endif

#ifdef NUI_STATIC_API
# ifndef NUI_IMPLEMENTATION
#  define NUI_IMPLEMENTATION
# endif
# define NUI_API NUI_STATIC
#endif

#if !defined(NUI_API) && defined(_WIN32)
# ifdef NUI_IMPLEMENTATION
#  define NUI_API __declspec(dllexport)
# else
#  define NUI_API __declspec(dllimport)
# endif
#endif

#ifndef NUI_API
# define NUI_API extern
#endif


#include <stddef.h>

NUI_NS_BEGIN


/* types */

typedef struct NUIstate  NUIstate;
typedef struct NUIparams NUIparams;
typedef struct NUItimer  NUItimer;

typedef struct NUInode  NUInode;
typedef struct NUIattr  NUIattr;
typedef struct NUItype  NUItype;
typedef struct NUIcomp  NUIcomp;
typedef struct NUIevent NUIevent;

typedef struct NUIpool   NUIpool;
typedef struct NUIdata   NUIdata;
typedef struct NUIkey    NUIkey;
typedef struct NUIentry  NUIentry;
typedef struct NUItable  NUItable;

#ifdef ZN_USE_64BIT_TIMER
typedef unsigned long long NUItime;
#else
typedef unsigned NUItime;
#endif


/* callbacks */

typedef NUItime NUItimerf   (void *ud, NUItimer *timer, NUItime elapsed);
typedef void    NUIhandlerf (void *ud, NUInode *node, const NUIevent *event);


/* nui global routines */

#define nui_rootnode(S) ((NUInode*)(S))

NUI_API NUIstate *nui_newstate (NUIparams *params);
NUI_API void      nui_close    (NUIstate *S);

NUI_API NUIparams *nui_getparams (NUIstate *S);

NUI_API int  nui_waitevents (NUIstate *S);
NUI_API int  nui_loop       (NUIstate *S);


/* nui node routines */

NUI_API NUInode  *nui_newnode (NUIstate *S);

NUI_API int nui_retain  (NUInode *n);
NUI_API int nui_release (NUInode *n);

NUI_API NUIstate *nui_state (const NUInode *n);

NUI_API NUInode *nui_parent      (const NUInode *n);
NUI_API NUInode *nui_prevchild   (const NUInode *n, const NUInode *curr);
NUI_API NUInode *nui_nextchild   (const NUInode *n, const NUInode *curr);
NUI_API NUInode *nui_prevsibling (const NUInode *n, const NUInode *curr);
NUI_API NUInode *nui_nextsibling (const NUInode *n, const NUInode *curr);

NUI_API NUInode *nui_root     (const NUInode *n);
NUI_API NUInode *nui_prevleaf (const NUInode *n, const NUInode *curr);
NUI_API NUInode *nui_nextleaf (const NUInode *n, const NUInode *curr);

NUI_API NUInode *nui_indexnode  (const NUInode *n, int idx);
NUI_API int      nui_nodeindex  (const NUInode *n);
NUI_API int      nui_childcount (const NUInode *n);

NUI_API void nui_setparent   (NUInode *n, NUInode *parent);
NUI_API void nui_setchildren (NUInode *n, NUInode *children);

NUI_API void nui_append (NUInode *n, NUInode *newnode);
NUI_API void nui_insert (NUInode *n, NUInode *newnode);

NUI_API int nui_detach (NUInode *n);


/* nui attribute routines */

NUI_API NUIattr *nui_setattr (NUInode *n, NUIkey *key, NUIattr *attr);
NUI_API NUIattr *nui_getattr (NUInode *n, NUIkey *key);
NUI_API NUIattr *nui_delattr (NUInode *n, NUIkey *key);

NUI_API NUIattr *nui_addattrhandler (NUInode *n, NUIattr *attr);
NUI_API NUIattr *nui_delattrhandler (NUInode *n, NUIattr *attr);

NUI_API int      nui_set (NUInode *n, NUIkey *key, const char *v);
NUI_API NUIdata *nui_get (NUInode *n, NUIkey *key);


/* nui event handler */

NUI_API NUIevent *nui_newevent (NUIstate *S, NUIkey *type, int bubbles, int cancelable);
NUI_API void      nui_delevent (NUIevent *evt);

NUI_API NUIkey   *nui_eventtype (const NUIevent *evt);
NUI_API NUItable *nui_eventdata (const NUIevent *evt);
NUI_API NUInode  *nui_eventnode (const NUIevent *evt);
NUI_API NUItime   nui_eventtime (const NUIevent *evt);
NUI_API NUIstate *nui_eventstate (const NUIevent *evt);

NUI_API int nui_eventstatus (const NUIevent *evt, int status);
enum NUIeventstatus { NUI_BUBBLES = 1, NUI_CANCELABLE, NUI_STOPPED, NUI_CANCELED, NUI_PHASE };
enum NUIeventphase  { NUI_CAPTURE = 1, NUI_TARGET, NUI_BUBBLE };

NUI_API void nui_stopevent   (NUIevent *evt, int stopnow);
NUI_API void nui_cancelevent (NUIevent *evt);

NUI_API int  nui_emitevent  (NUInode *n, NUIevent *evt);
NUI_API void nui_defhandler (NUInode *n, NUIkey *type, NUIhandlerf *h, void *ud);
NUI_API void nui_addhandler (NUInode *n, NUIkey *type, int capture, NUIhandlerf *h, void *ud);
NUI_API void nui_delhandler (NUInode *n, NUIkey *type, int capture, NUIhandlerf *h, void *ud);


/* nui componemt routines */

NUI_API NUItype *nui_newtype (NUIstate *S, NUIkey *name, size_t size, size_t csize);
NUI_API NUItype *nui_gettype (NUIstate *S, NUIkey *name);

NUI_API NUIcomp *nui_addcomp (NUInode *n, NUItype *t);
NUI_API NUIcomp *nui_getcomp (NUInode *n, NUItype *t);


/* nui timer routines */

NUI_API NUItime nui_time (NUIstate *S);

NUI_API NUItimer *nui_newtimer (NUIstate *S, NUItimerf *tiemrf, void *ud);
NUI_API void      nui_deltimer (NUItimer *timer);

NUI_API NUItimerf *nui_gettimerf (NUItimer *t, void **ud);

NUI_API int  nui_starttimer  (NUItimer *t, NUItime delayms);
NUI_API void nui_canceltimer (NUItimer *t);


/* nui memory routines */

#define NUI_KEY(S, s) (nui_newkey((S), #s, sizeof(#s)-1))
#define NUI_(s)     NUI_KEY(S, s)

NUI_API void  nui_initpool (NUIpool *pool, size_t objsize);
NUI_API void  nui_freepool (NUIstate *S, NUIpool *pool);
NUI_API void *nui_palloc   (NUIstate *S, NUIpool *pool);
NUI_API void  nui_pfree    (NUIpool *pool, void *obj);

NUI_API NUIdata *nui_newdata (NUIstate *S, const char *s, size_t len);
NUI_API NUIdata *nui_strdata (NUIstate *S, const char *s);
NUI_API void     nui_deldata (NUIstate *S, NUIdata *data);

NUI_API size_t  nui_len (NUIdata *data);

NUI_API NUIkey *nui_newkey (NUIstate *S, const char *s, size_t len);
NUI_API NUIkey *nui_usekey (NUIkey *key);
NUI_API size_t  nui_keylen (NUIkey *key);
NUI_API void    nui_delkey (NUIstate *S, NUIkey *key);


/* nui table routines */

NUI_API void nui_inittable (NUItable *t);
NUI_API void nui_freetable (NUIstate *S, NUItable *t);

NUI_API size_t    nui_resizetable (NUIstate *S, NUItable *t, size_t len);
NUI_API NUIentry *nui_settable    (NUIstate *S, NUItable *t, NUIkey *key);

NUI_API const NUIentry *nui_gettable (const NUItable *t, NUIkey *key);

NUI_API int nui_nextentry (const NUItable *t, NUIentry **e);


/* struct fields */

struct NUIpool {
    void *pages;
    void *freed;
    size_t size;
};

struct NUIentry {
    ptrdiff_t next;
    NUIkey   *key;
    void     *value;
};

struct NUItable {
    size_t size;
    size_t lastfree;
    NUIentry *hash;
};

struct NUIparams {
    void *(*alloc) (NUIparams *params, void *p, size_t nsize, size_t osize);
    void *(*nomem) (NUIparams *params, void *p, size_t nsize, size_t osize);
    void  (*close) (NUIparams *params);

    NUItime (*time) (NUIparams *params);
    int     (*wait) (NUIparams *params, NUItime time);

    NUIstate *S;
};

struct NUIattr {
    NUIdata *(*get_attr) (NUIattr *attr, NUInode *node, NUIkey *key);
    int      (*set_attr) (NUIattr *attr, NUInode *node, NUIkey *key, const char *v);
    void     (*del_attr) (NUIattr *attr, NUInode *node);
};

struct NUItype {
    NUIkey *name;
    NUIpool comp_pool;
    size_t  type_size;
    size_t  comp_size;

    NUItype **(*depends) (NUItype *t, size_t *plen);

    int  (*new_comp) (NUItype *t, NUInode *n, NUIcomp *comp);
    void (*del_comp) (NUItype *t, NUInode *n, NUIcomp *comp);
    void (*close)    (NUItype *t, NUIstate *S);
};

struct NUIcomp {
    NUItype *type;
};


NUI_NS_END

#endif /* nui_h */


#if defined(NUI_IMPLEMENTATION) && !defined(nui_implemented)
#define nui_implemented

#include <assert.h>
#include <string.h>


#define NUI_POOLSIZE                  4096
#define NUI_MIN_TIMERHEAP             128
#define NUI_MIN_STRTABLE_SIZE         32
#define NUI_HASHLIMIT                 5
#define NUI_MIN_HASHSIZE              4
#define NUI_MAX_EVENTLEVEL            100

#define NUI_MIN_ESIZE                 (sizeof(NUIentry)*NUI_MIN_HASHSIZE)
#define NUI_SMALLSIZE                 (NUI_MIN_ESIZE > 64 ? NUI_MIN_ESIZE : 64)

#define NUI_TIMER_NOINDEX  (~(unsigned)0)
#define NUI_FOREVER        (~(NUItime)0)
#define NUI_MAX_SIZET      ((~(size_t)0)-100)

#define nui_lmod(s,size) \
        (assert((size&(size-1))==0), ((int)((s) & ((size)-1))))

#define nui_builtinkeys(X) \
    X(add_child) X(remove_child) X(delete_node) X(child)

enum NUIbuiltinkeys {
#define X(str) NUI_##str,
    nui_builtinkeys(X)
#undef  X
    NUI_MAX_BUILTINS
};

typedef struct NUItimerstate NUItimerstate;
typedef struct NUIkeyentry   NUIkeyentry;
typedef struct NUIkeytable   NUIkeytable;
typedef struct NUIhandlers   NUIhandlers;

struct NUItimer {
    union { NUItimer *next; void *ud; } u;
    NUItimerf *handler;
    NUIstate *S;
    size_t index;
    NUItime starttime;
    NUItime emittime;
};

struct NUItimerstate {
    NUIpool    pool;
    NUItimer **heap;
    NUItime    nexttime;
    size_t     heap_used;
    size_t     heap_size;
};

struct NUIkeyentry {
    NUIkeyentry *next;
    unsigned hash;
    unsigned ref;
};

struct NUIkeytable {
    NUIkeyentry **hash;
    size_t nuse;
    size_t size;
    unsigned seed;
};

struct NUIevent {
    NUIkey   *type;
    NUInode  *node;
    NUIstate *S;
    NUItime   emit_time;
    unsigned  phase      : 4;
    unsigned  bubbles    : 1;
    unsigned  cancelable : 1;
    unsigned  canceled   : 1;
    unsigned  stopnow    : 1;
    unsigned  stopped    : 1;
    NUItable  data;
};

struct NUIhandlers {
    NUIhandlers *next;
    union {
        NUIattr     *attr;
        NUIhandlerf *handler;
    } u;
    void *ud;
    unsigned level   : 16;
    unsigned capture : 1;
    unsigned running : 1;
    unsigned dead    : 1;
};

struct NUInode {
    NUInode  *parent;
    NUInode  *prev_sibling;
    NUInode  *next_sibling;
    NUInode  *children;
    NUIstate *S;
    int       child_count;
    int       ref;
    NUItable  comps;
    NUItable  attrs;
    NUItable  handlers;
    NUIhandlers *attrhandlers;
};

struct NUIstate {
    NUInode       base;
    NUIparams    *params;
    NUInode      *freenodes;
    NUIkeytable   strt;
    NUItable      types;
    NUItimerstate timers;
    NUIpool       eventpool;
    NUIpool       handlerpool;
    NUIpool       nodepool;
    NUIpool       smallpool;
    NUIkey       *builtins[NUI_MAX_BUILTINS];
};


/* memory */

NUI_API size_t nui_len(NUIdata *data)
{ return data ? ((unsigned*)data)[-1] : 0; }

NUI_API NUIdata *nui_strdata(NUIstate *S, const char *s)
{ return nui_newdata(S, s, strlen(s)); }

static void *nuiM_malloc(NUIstate *S, size_t sz) {
    void *ptr;
    if (sz <= NUI_SMALLSIZE)
        return nui_palloc(S, &S->smallpool);
    ptr = S->params->alloc(S->params, NULL, sz, 0);
    if (ptr == NULL) ptr = S->params->nomem(S->params, NULL, sz, 0);
    return ptr;
}

static void nuiM_free(NUIstate *S, void *ptr, size_t oz) {
    if (ptr == NULL || oz == 0) return;
    if (oz <= NUI_SMALLSIZE) {
        nui_pfree(&S->smallpool, ptr);
        return;
    }
    ptr = S->params->alloc(S->params, ptr, 0, oz);
    assert(ptr == NULL);
}

static void *nuiM_realloc(NUIstate *S, void *ptr, size_t nz, size_t oz) {
    void *newptr;
    if (oz <= NUI_SMALLSIZE || nz <= NUI_SMALLSIZE) {
        nuiM_free(S, ptr, oz);
        return nuiM_malloc(S, nz);
    }
    newptr = S->params->alloc(S->params, ptr, nz, oz);
    if (newptr == NULL) newptr = S->params->nomem(S->params, ptr, nz, oz);
    return newptr;
}

NUI_API void nui_initpool(NUIpool *pool, size_t objsize) {
    const size_t sp = sizeof(void*);
    pool->pages = NULL;
    pool->freed = NULL;
    pool->size = objsize;
    assert(((sp - 1) & sp) == 0);
    assert(objsize >= sp && objsize % sp == 0);
    assert(NUI_POOLSIZE / objsize > 2);
}

NUI_API void nui_freepool(NUIstate *S, NUIpool *pool) {
    const size_t offset = NUI_POOLSIZE - sizeof(void*);
    while (pool->pages != NULL) {
        void *next = *(void**)((char*)pool->pages + offset);
        nuiM_free(S, pool->pages, NUI_POOLSIZE);
        pool->pages = next;
    }
    nui_initpool(pool, pool->size);
}

NUI_API void *nui_palloc(NUIstate *S, NUIpool *pool) {
    void *obj = pool->freed;
    if (obj == NULL) {
        const size_t offset = NUI_POOLSIZE - sizeof(void*);
        void *end, *newpage = nuiM_malloc(S, NUI_POOLSIZE);
        if (newpage == NULL) return NULL;
        *(void**)((char*)newpage + offset) = pool->pages;
        pool->pages = newpage;
        end = (char*)newpage + (offset/pool->size-1)*pool->size;
        while (end != newpage) {
            *(void**)end = pool->freed;
            pool->freed = (void**)end;
            end = (char*)end - pool->size;
        }
        return end;
    }
    pool->freed = *(void**)obj;
    return obj;
}

NUI_API void nui_pfree(NUIpool *pool, void *obj) {
    *(void**)obj = pool->freed;
    pool->freed = obj;
}

NUI_API NUIdata *nui_newdata(NUIstate *S, const char *s, size_t len) {
    size_t size = sizeof(unsigned) + len + 1;
    char *buff;
    NUIdata *data = (NUIdata*)nuiM_malloc(S, size);
    *(unsigned*)data = (unsigned)len;
    buff = (char*)((unsigned*)data+1);
    if (s != NULL) { memcpy(buff, s, len); buff[len] = '\0'; }
    return (NUIdata*)buff;
}

NUI_API void nui_deldata(NUIstate *S, NUIdata *data) {
    void *header = (unsigned*)data-1;
    size_t len;
    if (data == NULL) return;
    len = *(unsigned*)header;
    nuiM_free(S, header, sizeof(unsigned) + len + 1);
}


/* string table */

#define nuiS_data(key)   ((NUIkey*)((key) + 1))
#define nuiS_header(key) ((NUIkeyentry*)(key) - 1)
#define nuiS_hash(key)   (nuiS_header(key)->hash)

NUI_API NUIkey *nui_usekey(NUIkey *key)
{ if (key) ++nuiS_header(key)->ref; return key; }

NUI_API size_t nui_keylen(NUIkey *key)
{ return !key ? 0 : nui_len((NUIdata*)nuiS_header(key)) - sizeof(NUIkeyentry); }

static void nuiS_resize(NUIstate *S, size_t newsize) {
    NUIkeytable  oldstrt = S->strt;
    NUIkeytable *newstrt = &S->strt;
    size_t i, realsize = NUI_MIN_STRTABLE_SIZE;
    while (realsize < NUI_MAX_SIZET/2/sizeof(NUIkey*) && realsize < newsize)
        realsize *= 2;
    newstrt->hash = (NUIkeyentry**)nuiM_malloc(S, realsize*sizeof(NUIkeyentry*));
    newstrt->size = realsize;
    for (i = 0; i < newstrt->size; ++i) newstrt->hash[i] = NULL;
    for (i = 0; i < oldstrt.size; ++i) {
        NUIkeyentry *s = oldstrt.hash[i];
        while (s) { /* for each node in the list */
            NUIkeyentry *next = s->next; /* save next */
            unsigned h = nui_lmod(s->hash, realsize);
            s->next = newstrt->hash[h];
            newstrt->hash[h] = s;
            s = next;
        }
    }
    nuiM_free(S, oldstrt.hash, oldstrt.size*sizeof(NUIkeyentry*));
}

static unsigned nui_calchash(NUIstate *S, const char *s, size_t len) {
    unsigned h = S->strt.seed ^ (unsigned)len;
    size_t l1;
    size_t step = (len >> NUI_HASHLIMIT) + 1;
    for (l1 = len; l1 >= step; l1 -= step)
        h = h ^ ((h<<5) + (h>>2) + (unsigned char)(s[l1 - 1]));
    return h;
}

static NUIkey *nuiS_new(NUIstate *S, const char *s, size_t len, unsigned h) {
    NUIkeyentry **list;  /* (pointer to) list where it will be inserted */
    NUIkeytable *kp = &S->strt;
    NUIkeyentry *o;
    if (s == NULL || len == 0) return NULL;
    if (kp->nuse >= kp->size && kp->size <= NUI_MAX_SIZET/2)
        nuiS_resize(S, kp->size*2);  /* too crowded */
    list = &kp->hash[nui_lmod(h, kp->size)];
    o = (NUIkeyentry*)nui_newdata(S, NULL, sizeof(NUIkeyentry)+len);
    o->hash = nui_calchash(S, s, len);
    o->ref  = 0;
    o->next = *list;
    *list = o;
    memcpy(o+1, s, len);
    ((char *)(o+1))[len] = '\0';  /* ending 0 */
    kp->nuse++;
    return nuiS_data(o);
}

static NUIkey *nuiS_get(NUIstate *S, const char *s, size_t len, unsigned h) {
    NUIkeyentry *o;
    if (s == NULL || len == 0 || S->strt.hash == NULL) return NULL;
    for (o = S->strt.hash[nui_lmod(h, S->strt.size)];
         o != NULL;
         o = o->next) {
        size_t olen = nui_len((NUIdata*)o) - sizeof(NUIkeyentry);
        if (h == o->hash && len == olen &&
                (memcmp(s, (char*)(o + 1), len+1) == 0))
            return nuiS_data(o);
    }
    return NULL;
}

NUI_API NUIkey *nui_newkey(NUIstate *S, const char *s, size_t len) {
    unsigned hash = nui_calchash(S, s, len);
    NUIkey *key = nuiS_get(S, s, len, hash);
    return key ? key : nuiS_new(S, s, len, hash);
}

NUI_API void nui_delkey(NUIstate *S, NUIkey *key) {
    NUIkeytable  *kp = &S->strt;
    NUIkeyentry **list, *h = nuiS_header(key);
    if (key == NULL || --h->ref > 0) return;
    for (list = &kp->hash[nui_lmod(h->hash, kp->size)];
            *list != NULL && *list != h; list = &(*list)->next)
        ;
    if (*list == h) {
        *list = h->next;
        nui_deldata(S, (NUIdata*)h);
        --kp->nuse;
    }
}

static void nuiS_close(NUIstate *S) {
    size_t i;
    NUIkeytable *kp = &S->strt;
    for (i = 0; i < kp->size; ++i) {
        NUIkeyentry *h = kp->hash[i];
        kp->hash[i] = NULL;
        while (h) {
            NUIkeyentry *next = h->next;
            nui_deldata(S, (NUIdata*)h);
            h = next;
        }
    }
    nuiM_free(S, kp->hash, kp->size * sizeof(NUIkey*));
}


/* hash table */

NUI_API void nui_inittable(NUItable *t)
{ t->size = 0, t->lastfree = 0, t->hash = NULL; }

static size_t nuiH_hashsize(size_t len) {
    size_t newsize = NUI_MIN_HASHSIZE;
    while (newsize < NUI_MAX_SIZET/2/sizeof(NUIentry) && newsize < len)
        newsize <<= 1;
    return newsize;
}

static size_t nuiH_countsize(NUItable *t) {
    size_t i, count = 0;
    for (i = 0; i < t->size; ++i) {
        NUIentry *e = &t->hash[i];
        if (e->key != NULL && e->value != NULL)
            ++count;
    }
    return count;
}

static NUIentry *nuiH_mainposition(NUItable *t, NUIkey *key) {
    assert((t->size & (t->size - 1)) == 0);
    return &t->hash[nuiS_hash(key) & (t->size - 1)];
}

static NUIentry *nuiH_newkey(NUIstate *S, NUItable *t, NUIkey *key) {
    NUIentry *mp;
    if (t->size == 0 && nui_resizetable(S, t, NUI_MIN_HASHSIZE) == 0) 
        return NULL;
redo:
    mp = nuiH_mainposition(t, key);
    if (mp->key != 0) {
        NUIentry *f = NULL, *othern;
        while (t->lastfree > 0) {
            NUIentry *e = &t->hash[--t->lastfree];
            if (e->key == 0)  { f = e; break; }
        }
        if (f == NULL) {
            if (nui_resizetable(S, t, nuiH_countsize(t)*2) == 0)
                return NULL;
            goto redo; /* return nuiH_newkey(t, entry); */
        }
        assert(f->key == NULL);
        othern = nuiH_mainposition(t, mp->key);
        if (othern != mp) {
            while (othern + othern->next != mp)
                othern += othern->next;
            othern->next = f - othern;
            *f = *mp;
            if (mp->next != 0)
            { f->next += mp - f; mp->next = 0; }
        }
        else {
            if (mp->next != 0)
                f->next = (mp + mp->next) - f;
            else assert(f->next == 0);
            mp->next = f - mp; mp = f;
        }
    }
    mp->key = key;
    mp->value = NULL;
    nui_usekey(key);
    return mp;
}

NUI_API void nui_freetable(NUIstate *S, NUItable *t) {
    size_t i;
    for (i = 0; i < t->size; ++i) {
        NUIentry *e = &t->hash[i];
        if (e->key != NULL) nui_delkey(S, e->key);
    }
    if (t->hash != NULL)
        nuiM_free(S, t->hash, t->size*sizeof(NUIentry));
    nui_inittable(t);
}

NUI_API size_t nui_resizetable(NUIstate *S, NUItable *t, size_t len) {
    size_t i;
    NUItable nt;
    nt.size = nuiH_hashsize(len);
    nt.hash = (NUIentry*)nuiM_malloc(S, nt.size*sizeof(NUIentry));
    nt.lastfree = nt.size;
    memset(nt.hash, 0, sizeof(NUIentry)*nt.size);
    for (i = 0; i < t->size; ++i) {
        NUIentry *e = &t->hash[i];
        if (e->key != NULL && e->value != NULL)
            nuiH_newkey(S, &nt, e->key);
    }
    nuiM_free(S, t->hash, t->size*sizeof(NUIentry));
    *t = nt;
    return t->size;
}

NUI_API const NUIentry *nui_gettable(const NUItable *t, NUIkey *key) {
    const NUIentry *e;
    if (t->size == 0 || key == NULL) return NULL;
    assert((t->size & (t->size - 1)) == 0);
    e = &t->hash[nui_lmod(nuiS_hash(key), t->size)];
    while (1) {
        ptrdiff_t next = e->next;
        if (e->key == key) return e;
        if (next == 0) return NULL;
        e += next;
    }
    return NULL;
}

NUI_API NUIentry *nui_settable(NUIstate *S, NUItable *t, NUIkey *key) {
    NUIentry *ret;
    if (key == NULL) return NULL;
    if ((ret = (NUIentry*)nui_gettable(t, key)) != NULL)
        return ret;
    return nuiH_newkey(S, t, key);
}

NUI_API int nui_nextentry(const NUItable *t, NUIentry **pentry) {
    size_t i = *pentry ? *pentry - &t->hash[0] + 1 : 0;
    for (; i < t->size; ++i) {
        NUIentry *e = &t->hash[i];
        if (e->key != NULL && e->value != NULL)
        { *pentry = e; return 1; }
    }
    *pentry = NULL;
    return 0;
}


/* nui event handlers */

NUI_API NUIkey *nui_eventtype(const NUIevent *evt) { return evt->type; }
NUI_API NUItable *nui_eventdata(const NUIevent *evt) { return (NUItable*)&evt->data; }
NUI_API NUInode *nui_eventnode(const NUIevent *evt) { return evt->node; }
NUI_API NUItime nui_eventtime(const NUIevent *evt) { return evt->emit_time; }
NUI_API NUIstate *nui_eventstate(const NUIevent *evt) { return evt->S; }

NUI_API void nui_cancelevent(NUIevent *evt)
{ if (evt->cancelable) evt->canceled = 1; }

NUI_API void nui_stopevent(NUIevent *evt, int stopnow)
{ evt->stopnow = stopnow ? 1 : 0; evt->stopped = 1; }

static void nuiE_dodefault(NUInode *n, NUIevent *evt) {
    const NUIentry *e = nui_gettable(&n->handlers, evt->type);
    NUIhandlers *hs;
    if (e == NULL || (hs = (NUIhandlers*)e->value) == NULL)
        return;
    if (hs->u.handler && hs->level < NUI_MAX_EVENTLEVEL) {
        ++hs->level;
        hs->u.handler(hs->ud, n, evt);
        --hs->level;
    }
}

static void nuiE_sweepdead(NUIstate *S, NUIhandlers *hs) {
    NUIhandlers **pp = &hs->next;
    while (*pp != NULL) {
        if (!(*pp)->dead)
            pp = &(*pp)->next;
        else {
            NUIhandlers *next = (*pp)->next;
            nui_pfree(&S->handlerpool, *pp);
            *pp = next;
        }
    }
}

static void nuiE_doevent(NUInode *n, NUIevent *evt, int capture) {
    const NUIentry *e = nui_gettable(&n->handlers, evt->type);
    NUIhandlers *hs, *p;
    int havedead = 0;
    if (e == NULL ||
            (hs = (NUIhandlers*)e->value) == NULL ||
            (p = hs->next) == NULL)
        return;
    assert(capture == 0 || capture == 1);
    hs->running = 1;
    while (p != NULL) {
        NUIhandlers *next = p->next;
        if (p->dead) havedead = 1;
        else if (p->u.handler &&
                p->capture == capture &&
                p->level < NUI_MAX_EVENTLEVEL)
        {
            ++p->level;
            p->u.handler(p->ud, n, evt);
            --p->level;
        }
        if (evt->stopnow) break;
        p = next;
    }
    hs->running = 0;
    if (havedead) nuiE_sweepdead(n->S, hs->next);
}

static void nuiE_capture(NUInode *n, NUIevent *evt) {
    NUInode *parent = n->parent;
    if (parent && !evt->stopped) {
        nui_retain(parent);
        nuiE_capture(parent, evt);
        nuiE_doevent(parent, evt, 1);
        nui_release(parent);
    }
}

static void nuiE_bubble(NUInode *n, NUIevent *evt) {
    NUInode *parent = n->parent;
    if (parent && !evt->stopped) {
        nui_retain(parent);
        nuiE_doevent(parent, evt, 0);
        nuiE_bubble(parent, evt);
        nui_release(parent);
    }
}

NUI_API int nui_emitevent(NUInode *n, NUIevent *evt) {
    if (!n || !evt) return 1;
    evt->node = n;
    evt->emit_time = nui_time(n->S);
    evt->phase = NUI_CAPTURE;
    evt->canceled = 0;
    evt->stopnow = 0;
    evt->stopped = 0;
    nuiE_capture(n, evt);
    nui_retain(n);
    if (!evt->stopped) {
        evt->phase = NUI_TARGET;
        nuiE_doevent(n, evt, 0);
        if (!evt->stopped && evt->bubbles) {
            evt->phase = NUI_BUBBLE;
            nuiE_bubble(n, evt);
        }
    }
    evt->phase = 0;
    if (!evt->canceled)
        nuiE_dodefault(n, evt);
    nui_release(n);
    return !evt->canceled;
}

NUI_API NUIevent *nui_newevent(NUIstate *S, NUIkey *type, int bubbles, int cancelable) {
    NUIevent *evt = (NUIevent*)nui_palloc(S, &S->eventpool);
    memset(evt, 0, sizeof(*evt));
    evt->type = type;
    evt->S = S;
    evt->bubbles    = bubbles    ? 1 : 0;
    evt->cancelable = cancelable ? 1 : 0;
    nui_usekey(type);
    return evt;
}

NUI_API void nui_delevent(NUIevent *evt) {
    NUIstate *S = evt->S;
    nui_freetable(S, &evt->data);
    nui_pfree(&S->eventpool, evt);
}

NUI_API int nui_eventstatus(const NUIevent *evt, int status) {
    switch (status) {
    case NUI_BUBBLES: return evt->bubbles;
    case NUI_CANCELABLE: return evt->cancelable;
    case NUI_STOPPED: return evt->stopped;
    case NUI_CANCELED: return evt->canceled;
    case NUI_PHASE: return evt->phase;
    default: return -1;
    }
}

NUI_API void nui_defhandler(NUInode *n, NUIkey *type, NUIhandlerf *h, void *ud) {
    NUIentry *e = nui_settable(n->S, &n->handlers, type);
    NUIhandlers *hs = (NUIhandlers*)e->value;
    if (hs == NULL) {
        hs = (NUIhandlers*)nui_palloc(n->S, &n->S->handlerpool);
        memset(hs, 0, sizeof(*hs));
        e->value = hs;
    }
    hs->u.handler = h;
    hs->ud        = ud;
}

NUI_API void nui_addhandler(NUInode *n, NUIkey *type, int capture, NUIhandlerf *h, void *ud) {
    NUIentry *e = nui_settable(n->S, &n->handlers, type);
    NUIhandlers **pp, *hs = (NUIhandlers*)e->value;
    if (h == NULL) return;
    if (hs == NULL) {
        hs = (NUIhandlers*)nui_palloc(n->S, &n->S->handlerpool);
        memset(hs, 0, sizeof(*hs));
        e->value = hs;
    }
    pp = &hs->next;
    while (*pp != NULL) pp = &(*pp)->next;
    hs = *pp = (NUIhandlers*)nui_palloc(n->S, &n->S->handlerpool);
    memset(hs, 0, sizeof(*hs));
    hs->u.handler = h;
    hs->ud        = ud;
    hs->capture   = capture;
}

NUI_API void nui_delhandler(NUInode *n, NUIkey *type, int capture, NUIhandlerf *h, void *ud) {
    const NUIentry *e = nui_gettable(&n->handlers, type);
    NUIhandlers **pp, *hs = (NUIhandlers*)e->value;
    pp = &hs->next; /* skip default handler */
    while (*pp != NULL) {
        NUIhandlers *cur = *pp, **next = &(*pp)->next;
        if (cur->u.handler == h &&
                cur->ud == ud &&
                (cur->capture == 0) == (capture == 0))
            break;
        pp = next;
    }
    if (*pp == NULL) return;
    if (hs->running)
        (*pp)->dead = 1;
    else {
        hs = *pp;
        *pp = (*pp)->next;
        nui_pfree(&n->S->handlerpool, hs);
    }
}

void nuiE_clear(NUInode *n) {
    NUIentry *e = NULL;
    while (nui_nextentry(&n->handlers, &e)) {
        NUIhandlers *hs = (NUIhandlers*)e->value;
        while (hs) {
            NUIhandlers *next = hs->next;
            nui_pfree(&n->S->handlerpool, hs);
            hs = next;
        }
    }
    nui_freetable(n->S, &n->handlers);
}


/* nui attribute */

NUI_API NUIattr *nui_setattr(NUInode *n, NUIkey *key, NUIattr *attr) {
    NUIentry *e = nui_settable(n->S, &n->attrs, key);
    if (!attr) {  nui_delattr(n, key); return NULL; }
    return (NUIattr*)(e->value ? NULL : (e->value = attr));
}

NUI_API NUIattr *nui_getattr(NUInode *n, NUIkey *key) {
    const NUIentry *e = nui_gettable(&n->attrs, key);
    return e ? (NUIattr*)e->value : NULL;
}

NUI_API NUIattr *nui_delattr(NUInode *n, NUIkey *name) {
    const NUIentry *e = nui_gettable(&n->attrs, name);
    NUIattr *attr;
    if (e == NULL) return NULL;
    attr = (NUIattr*)e->value;
    if (attr->del_attr != NULL)
        attr->del_attr(attr, n);
    ((NUIentry*)e)->value = NULL;
    return attr;
}

NUI_API NUIattr *nui_addattrhandler(NUInode *n, NUIattr *attr) {
    NUIhandlers *hs = (NUIhandlers*)nui_palloc(n->S, &n->S->handlerpool);
    memset(hs, 0, sizeof(*hs));
    hs->next = n->attrhandlers;
    hs->u.attr = attr;
    return attr;
}

NUI_API NUIattr *nui_delattrhandler(NUInode *n, NUIattr *attr) {
    NUIhandlers **pp = &n->attrhandlers;
    while (*pp != NULL) {
        if ((*pp)->u.attr != attr)
            pp = &(*pp)->next;
        else {
            NUIhandlers *pnext = (*pp)->next;
            nui_pfree(&n->S->handlerpool, *pp);
            *pp = pnext;
        }
    }
    return attr;
}

NUI_API int nui_set(NUInode *n, NUIkey *key, const char *v) {
    NUIattr *attr = nui_getattr(n, key);
    NUIhandlers *hs = n->attrhandlers;
    if (attr && attr->set_attr && attr->set_attr(attr, n, key, v))
        return 1;
    while (hs != NULL) {
        attr = hs->u.attr;
        if (attr->set_attr && attr->set_attr(attr, n, key, v))
            return 1;
        hs = hs->next;
    }
    return 0;
}

NUI_API NUIdata *nui_get(NUInode *n, NUIkey *key) {
    NUIattr *attr = nui_getattr(n, key);
    NUIdata *ret = NULL;
    NUIhandlers *hs = n->attrhandlers;
    if (attr && attr->get_attr &&
            (ret = attr->get_attr(attr, n, key)) != NULL)
        return ret;
    while (hs != NULL) {
        attr = hs->u.attr;
        if (attr->get_attr &&
                (ret = attr->get_attr(attr, n, key)) != NULL)
            return ret;
        hs = hs->next;
    }
    return 0;
}

static void nuiA_clear(NUInode *n) {
    NUIentry *e = NULL;
    NUIhandlers *hs;
    while (nui_nextentry(&n->attrs, &e)) {
        NUIattr *attr = (NUIattr*)e->value;
        if (attr->del_attr)
            attr->del_attr(attr, n);
    }
    nui_freetable(n->S, &n->attrs);
    hs = n->attrhandlers;
    while (hs) {
        NUIhandlers *next = hs->next;
        NUIattr *attr = hs->u.attr;
        if (attr->del_attr)
            attr->del_attr(attr, n);
        nui_pfree(&n->S->handlerpool, n->attrhandlers);
        hs = next;
    }
    n->attrhandlers = NULL;
}


/* nui componemt */

NUI_API NUItype *nui_newtype(NUIstate *S, NUIkey *name, size_t size, size_t csize) {
    NUIentry *e = nui_settable(S, &S->types, name);
    NUItype *t;
    if (e->value != NULL) return NULL;
    if (size  < sizeof(NUItype))  size = sizeof(NUItype);
    if (csize < sizeof(NUIcomp)) csize = sizeof(NUIcomp);
    t = (NUItype*)nuiM_malloc(S, size);
    memset(t, 0, size);
    t->name = name;
    t->type_size =  size;
    t->comp_size = csize;
    if (csize < NUI_POOLSIZE/4)
        nui_initpool(&t->comp_pool, csize);
    e->value = t;
    return t;
}

NUI_API NUItype *nui_gettype(NUIstate *S, NUIkey *name) {
    const NUIentry *e = nui_gettable(&S->types, name);
    return e ? (NUItype*)e->value : NULL;
}

NUI_API NUIcomp *nui_addcomp(NUInode *n, NUItype *t) {
    NUIentry *e = nui_settable(n->S, &n->comps, t->name);
    NUItype **depends;
    NUIcomp *comp;
    size_t i, dlen;
    if (e->value) return (NUIcomp*)e->value;
    if (t->depends && (depends = t->depends(t, &dlen)) != NULL)
        for (i = 0; i < dlen; ++i)
            nui_addcomp(n, depends[i]);
    if (t->comp_pool.size == 0)
        comp = (NUIcomp*)nuiM_malloc(n->S, t->comp_size);
    else
        comp = (NUIcomp*)nui_palloc(n->S, &t->comp_pool);
    memset(comp, 0, t->comp_size);
    if (t->new_comp && !t->new_comp(t, n, comp)) {
        nui_pfree(&t->comp_pool, comp);
        return NULL;
    }
    e->value = comp;
    comp->type = t;
    return comp;
}

NUI_API NUIcomp *nui_getcomp(NUInode *n, NUItype *t) {
    const NUIentry *e = nui_gettable(&n->comps, t->name);
    return e ? (NUIcomp*)e->value : NULL;
}

static void nuiC_close(NUIstate *S) {
    NUIentry *e = NULL;
    while (nui_nextentry(&S->types, &e)) {
        NUItype *t = (NUItype*)e->value;
        if (t->close != NULL)
            t->close(t, S);
        nui_freepool(S, &t->comp_pool);
        nuiM_free(S, t, t->type_size);
    }
    nui_freetable(S, &S->types);
}

static void nuiC_clear(NUInode *n) {
    NUIentry *e = NULL;
    while (nui_nextentry(&n->comps, &e)) {
        NUIcomp *comp = (NUIcomp*)e->value;
        NUItype *type = comp->type;
        if (type->del_comp)
            type->del_comp(comp->type, n, comp);
        if (type->comp_pool.size == 0)
            nuiM_free(n->S, comp, type->comp_size);
        else
            nui_pfree(&type->comp_pool, comp);
    }
    nui_freetable(n->S, &n->comps);
}


/* nui node */

NUI_API NUIstate *nui_state(const NUInode *n) { return n ? n->S : NULL; }

NUI_API int nui_retain(NUInode *n) { return ++n->ref; } 
NUI_API int nui_childcount(const NUInode *n) { return n->child_count; }

NUI_API NUInode* nui_parent(const NUInode *n)
{ return n && n->parent ? n->parent : NULL; }

static void nuiN_insert(NUInode *h, NUInode *n) {
    n->prev_sibling = h->prev_sibling;
    n->prev_sibling->next_sibling = n;
    n->next_sibling = h;
    h->prev_sibling = n;
}

static void nuiN_append(NUInode **p, NUInode *n) {
    if (*p != NULL)
        nuiN_insert(*p, n);
    else {
        *p = n;
        n->next_sibling = n->prev_sibling = n;
    }
}

static void nuiN_merge(NUInode *h, NUInode *n) {
    h->next_sibling->prev_sibling = n->prev_sibling;
    n->prev_sibling->next_sibling = h->next_sibling;
    h->next_sibling = n;
    n->prev_sibling = h;
}

static void nuiN_detach(NUInode *n) {
    NUInode *next = nui_nextsibling(n, n);
    NUInode *parent = n->parent;
    if (next != NULL) {
        n->next_sibling->prev_sibling = n->prev_sibling;
        n->prev_sibling->next_sibling = n->next_sibling;
        n->prev_sibling = n->next_sibling = n;
    }
    if (parent != NULL) {
        if (parent->children == n)
            parent->children = next;
        --parent->child_count;
    }
    else if (n->S->freenodes == n)
        n->S->freenodes = NULL;
}

static int nuiN_emitevent(int id, int cancelable, NUInode *n, NUInode *parent) {
    NUIstate *S = n->S;
    NUIevent *evt = nui_newevent(S, S->builtins[id], 0, cancelable);
    int ret = 0;
    if (id != NUI_add_child && id != NUI_remove_child)
        ret = nui_emitevent(n, evt);
    else if (parent) {
        NUIkey *child = S->builtins[NUI_child];
        nui_settable(S, &evt->data, child)->value = n;
        ret = nui_emitevent(parent, evt);
    }
    nui_delevent(evt);
    return ret;
}

static void nuiN_childrenevents(NUIstate *S, int id, NUInode *parent) {
    NUIkey *child = S->builtins[NUI_child];
    NUInode *i, *next;
    NUIevent *evt;
    assert(id == NUI_add_child || id == NUI_remove_child);
    if (parent == NULL || parent->children == NULL) return;
    evt = nui_newevent(S, S->builtins[id], 0, 0);
    for (i = nui_nextchild(parent, NULL); i != NULL; i = next) {
        next = nui_nextchild(parent, i);
        nui_settable(S, &evt->data, child)->value = i;
        nui_emitevent(parent, evt);
    }
    nui_delevent(evt);
}

static void nuiN_cleanchildren(NUInode *n) {
    NUInode *i, *next;
    if (n == NULL || n->children == NULL)
        return;
    for (i = nui_nextchild(n, NULL); i != NULL; i = next) {
        next = nui_nextchild(n, i);
        i->parent = NULL;
    }
    if (n->S->freenodes == NULL)
        n->S->freenodes = n->children;
    else if (n->children != NULL)
        nuiN_merge(n->S->freenodes, n->children);
    n->children = NULL, n->child_count = 0;
}

static void nuiN_delete(NUInode *n, int freeself) {
    if (!nuiN_emitevent(NUI_delete_node, 1, n, NULL)) return;
    nuiN_detach(n);
    nuiN_cleanchildren(n);
    n->parent = NULL;
    nuiA_clear(n);
    nuiC_clear(n);
    nuiE_clear(n);
    if (freeself) nui_pfree(&n->S->nodepool, n);
}

static void nuiN_releaseall(NUInode *n) {
    while (n->children)
        nuiN_releaseall(nui_prevchild(n, NULL));
    nuiN_delete(n, 1);
}

static NUInode *nuiN_sweepdead(NUInode *n) {
    NUInode *i = nui_prevsibling(n, NULL);
    while (i != n) {
        NUInode *next = nui_prevsibling(n, i);
        if (i->ref <= 0) nuiN_delete(i, 1);
        i = next;
    }
    if (n && n->ref <= 0) { nuiN_delete(n, 1); return NULL; }
    return n;
}

NUI_API NUInode *nui_newnode(NUIstate *S) {
    NUInode *n;
    n = (NUInode*)nui_palloc(S, &S->nodepool);
    memset(n, 0, sizeof(NUInode));
    n->ref = 0;
    n->S = S;
    nuiN_append(&S->freenodes, n);
    return n;
}

NUI_API int nui_release(NUInode *n) {
    NUInode *root = &n->S->base;
    if (n == root || (n->parent && n->parent != root)) return 1;
    return --n->ref; /* delete after pollevents */
}

NUI_API void nui_setparent(NUInode *n, NUInode *parent) {
    if (n == NULL || parent == n->parent) return;
    nuiN_emitevent(NUI_remove_child, 0, n, n->parent);
    nuiN_detach(n);
    n->parent = parent;
    if (parent == NULL) { nuiN_append(&n->S->freenodes, n); return; }
    if (parent == n) { n->parent = NULL; return; }
    nuiN_append(&parent->children, n);
    ++parent->child_count;
    nuiN_emitevent(NUI_add_child, 0, n, parent);
}

NUI_API void nui_setchildren(NUInode *n, NUInode *newnode) {
    NUInode *i, *next;
    if (n == NULL || (newnode && newnode->parent == n)) return;
    nuiN_childrenevents(n->S, NUI_remove_child, n);
    if (newnode && newnode->parent)
        nuiN_childrenevents(n->S, NUI_remove_child, newnode->parent);
    nuiN_cleanchildren(n);
    if (newnode == NULL) return;
    if (newnode->parent == NULL)
        nuiN_detach(newnode);
    else {
        newnode->parent->children = NULL;
        newnode->parent->child_count = 0;
    }
    n->children = newnode;
    for (i = nui_nextsibling(newnode, NULL); i != NULL; i = next) {
        next = nui_nextsibling(newnode, i);
        i->parent = n;
        ++n->child_count;
    }
    nuiN_childrenevents(n->S, NUI_add_child, n);
}

NUI_API void nui_append(NUInode *n, NUInode *newnode) {
    NUInode *parent;
    if (n == NULL || newnode == NULL) return;
    parent = newnode->parent;
    nuiN_emitevent(NUI_remove_child, 1, newnode, parent);
    nuiN_detach(newnode);
    newnode->parent = n->parent;
    if (n->parent == NULL)
        nuiN_append(&n->S->freenodes, newnode);
    else {
        nuiN_insert(n->next_sibling, newnode);
        ++n->parent->child_count;
        nuiN_emitevent(NUI_add_child, 0, newnode, n->parent);
    }
}

NUI_API void nui_insert(NUInode *n, NUInode *newnode) {
    NUInode *parent;
    if (n == NULL || newnode == NULL) return;
    parent = newnode->parent;
    nuiN_emitevent(NUI_remove_child, 1, newnode, parent);
    nuiN_detach(newnode);
    newnode->parent = n->parent;
    if (n->parent == NULL)
        nuiN_append(&n->S->freenodes, newnode);
    else {
        nuiN_insert(n, newnode);
        if (n->parent->children == n)
            n->parent->children = newnode;
        ++n->parent->child_count;
        nuiN_emitevent(NUI_add_child, 0, newnode, n->parent);
    }
}

NUI_API int nui_detach(NUInode *n) {
    if (n->parent == NULL) return 1;
    nuiN_emitevent(NUI_remove_child, 1, n, n->parent);
    nuiN_detach(n);
    nuiN_append(&n->S->freenodes, n);
    n->parent = NULL;
    return n->ref;
}

NUI_API NUInode *nui_prevchild(const NUInode *n, const NUInode *curr) {
    if (n == NULL || n->children == NULL)
        return NULL;
    return nui_prevsibling(n->children, curr);
}

NUI_API NUInode *nui_nextchild(const NUInode *n, const NUInode *curr) {
    if (n == NULL || n->children == NULL)
        return NULL;
    return nui_nextsibling(n->children, curr);
}

NUI_API NUInode* nui_prevsibling(const NUInode *n, const NUInode *curr) {
    if (curr == n) return NULL;
    if (curr == NULL) curr = n;
    return curr->prev_sibling;
}

NUI_API NUInode* nui_nextsibling(const NUInode *n, const NUInode *curr) {
    if (curr == NULL) return (NUInode*)n;
    curr = curr->next_sibling;
    return curr == n ? NULL : (NUInode*)curr;
}

NUI_API NUInode* nui_root(const NUInode *n) {
    NUInode *parent;
    if (n == NULL) return NULL;
    while ((parent = n->parent) != NULL)
        n = parent;
    return (NUInode*)n;
}

NUI_API NUInode* nui_prevleaf(const NUInode *n, const NUInode *curr) {
    const NUInode *parent, *firstchild;
    if (curr == n) return NULL; /* end of iteration */
    if (curr == NULL) curr = n; /* first of iteration */
    if (curr != n) {
        /* return parent if curr is the first child of parent. */
        if ((parent = curr->parent) != NULL && parent->children == curr)
            return (NUInode*)parent;
        /* if curr is not top, set curr to the preious sibling */
        if (parent) curr = curr->prev_sibling;
    }
    /* and get it's last leaf */
    while ((firstchild = curr->children) != NULL)
        curr = firstchild->prev_sibling;
    return (NUInode*)curr;
}

NUI_API NUInode* nui_nextleaf(const NUInode *n, const NUInode *curr) {
    NUInode *parent = NULL;
    if (curr == NULL) return (NUInode*)n;
    /* if curr has children, return the first one */
    if (curr->children != NULL)
        return curr->children;
    /* otherwise, get the first parent that not the last child */
    while (curr != n
            && (parent = curr->parent) != NULL
            && parent->children == curr->next_sibling)
        curr = parent;
    if (parent == NULL || curr == n) return NULL;
    /* return the next sibling of that node */
    return curr->next_sibling;
}

NUI_API NUInode *nui_indexnode(const NUInode *n, int idx) {
    NUInode *i, *children = n->children;
    if (children == NULL
            || (idx >= 0 &&  idx > n->child_count)
            || (idx <  0 && -idx > n->child_count ))
        return NULL;
    if (idx >= 0) {
        if (idx == 0)
            return children;
        for (i = children->next_sibling; i != children; i = i->next_sibling)
            if (--idx == 0) return i;
    }
    else {
        for (i = children->prev_sibling; i != children; i = i->prev_sibling)
            if (++idx == 0)
                return i;
        if (idx == -1)
            return n->children;
    }
    return NULL;
}

NUI_API int nui_nodeindex(const NUInode *n) {
    int idx = 0;
    NUInode *i, *children;
    if (n == NULL || n->parent == NULL)
        return -1;
    children = n->parent->children;
    if (children == n)
        return 0;
    for (i = children->next_sibling; i != children; i = i->next_sibling) {
        ++idx; if (i == n) return idx;
    }
    return -1;
}


/* timer */

static int nuiT_hastimers(NUIstate *S)
{ return S->timers.heap_used != 0; }

NUI_API NUItimerf *nui_gettimerf(NUItimer *timer, void **ud)
{ if (ud) *ud = timer->u.ud; return timer->handler; }

NUI_API NUItime nui_time(NUIstate *S)
{ return S->params->time(S->params); }

static int nuiT_resizeheap(NUIstate *S, size_t size) {
    NUItimer **heap;
    size_t realsize = NUI_MIN_TIMERHEAP;
    while (realsize < size && realsize < NUI_MAX_SIZET/sizeof(NUItimer*)/2)
        realsize <<= 1;
    if (realsize < size) return 0;
    heap = (NUItimer**)nuiM_realloc(S, S->timers.heap,
            realsize*sizeof(NUItimer*),
            S->timers.heap_size*sizeof(NUItimer*));
    if (heap == NULL) return 0;
    S->timers.heap = heap;
    S->timers.heap_size = realsize;
    return 1;
}

NUI_API NUItimer *nui_newtimer(NUIstate *S, NUItimerf *cb, void *ud) {
    NUItimer *timer = (NUItimer*)nui_palloc(S, &S->timers.pool);
    timer->u.ud = ud;
    timer->handler = cb;
    timer->S = S;
    timer->index = NUI_TIMER_NOINDEX;
    return timer;
}

NUI_API void nui_deltimer(NUItimer *timer) {
    nui_canceltimer(timer);
    nui_pfree(&timer->S->timers.pool, timer);
}

NUI_API int nui_starttimer(NUItimer *timer, NUItime delayms) {
    unsigned index;
    NUItimerstate *ts = &timer->S->timers;
    if (timer->index != NUI_TIMER_NOINDEX)
        nui_canceltimer(timer);
    if (ts->heap_size == ts->heap_used
            && !nuiT_resizeheap(timer->S, ts->heap_size * 2))
        return 0;
    index = (unsigned)ts->heap_used++;
    timer->starttime = nui_time(timer->S);
    timer->emittime = timer->starttime + delayms;
    while (index) {
        unsigned parent = (index-1)>>1;
        if (ts->heap[parent]->emittime <= timer->emittime)
            break;
        ts->heap[index] = ts->heap[parent];
        ts->heap[index]->index = index;
        index = parent;
    }
    ts->heap[index] = timer;
    timer->index = index;
    if (index == 0) ts->nexttime = timer->emittime;
    return 1;
}

NUI_API void nui_canceltimer(NUItimer *timer) {
    NUItimerstate *ts = &timer->S->timers;
    unsigned index = (unsigned)timer->index;
    if (index == NUI_TIMER_NOINDEX) return;
    timer->index = NUI_TIMER_NOINDEX;
    if (ts->heap_used == 0 || timer == ts->heap[--ts->heap_used])
        return;
    timer = ts->heap[ts->heap_used];
    while (1) {
        unsigned left = (index<<1)|1, right = (index+1)<<1;
        unsigned newindex = right;
        if (left >= ts->heap_used) break;
        if (timer->emittime >= ts->heap[left]->emittime) {
            if (right >= ts->heap_used
                    || ts->heap[left]->emittime < ts->heap[right]->emittime)
                newindex = left;
        }
        else if (right >= ts->heap_used
                || timer->emittime <= ts->heap[right]->emittime)
            break;
        ts->heap[index] = ts->heap[newindex];
        ts->heap[index]->index = index;
        index = newindex;
    }
    ts->heap[index] = timer;
    timer->index = index;
}

static void nuiT_cleartimers(NUIstate *S) {
    NUItimerstate *ts = &S->timers;
    nui_freepool(S, &ts->pool);
    nuiM_free(S, ts->heap, ts->heap_size*sizeof(NUItimer*));
    memset(ts, 0, sizeof(NUItimerstate));
    ts->nexttime = NUI_FOREVER;
}

static void nuiT_updatetimers(NUIstate *S, NUItime current) {
    NUItimerstate *ts = &S->timers;
    if (ts->nexttime > current) return;
    while (ts->heap_used && ts->heap[0]->emittime <= current) {
        NUItimer *timer = ts->heap[0];
        nui_canceltimer(timer);
        if (timer->handler) {
            int ret = timer->handler(timer->u.ud,
                    timer, current - timer->starttime);
            if (ret > 0) nui_starttimer(timer, ret);
        }
    }
    ts->nexttime = ts->heap_used == 0 ? NUI_FOREVER : ts->heap[0]->emittime;
}

static NUItime nuiT_gettimeout(NUIstate *S, NUItime current) {
    NUItime emittime = S->timers.nexttime;
    if (emittime < current) return 0;
    return emittime - current;
}


/* nui state */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#ifdef _WIN32
# define WIN32_LEAN_AND_MEAN
# include <Windows.h>
#else
# ifdef __APPLE__ /* apple don't have spinlock :( */
#   include <mach/mach.h>
#   include <mach/mach_time.h>
# endif
# include <sys/select.h>
#endif

NUI_API NUIparams *nui_getparams(NUIstate *S)
{ return S->params; }

static void *nuiD_alloc(NUIparams *params, void *p, size_t nsize, size_t osize) {
    (void)params, (void)osize;
    if (nsize == 0) {
        free(p);
        return NULL;
    }
    return realloc(p, nsize);
}

static void *nuiD_nomem(NUIparams *params, void *p, size_t nsize, size_t osize) {
    (void)params, (void)p, (void)nsize, (void)osize;
    fprintf(stderr, "nui: out of memory\n");
    abort();
    return NULL;
}

static NUItime nuiD_time(NUIparams *params) {
    (void)params;
    {
#ifdef _WIN32
        static LARGE_INTEGER counterFreq;
        static LARGE_INTEGER startTime;
        LARGE_INTEGER current;
        if (counterFreq.QuadPart == 0) {
            QueryPerformanceFrequency(&counterFreq);
            QueryPerformanceCounter(&startTime);
            assert(counterFreq.HighPart == 0);
        }
        QueryPerformanceCounter(&current);
        return (NUItime)((current.QuadPart - startTime.QuadPart) * 1000
                / counterFreq.LowPart);
#elif __APPLE__
        static mach_timebase_info_data_t time_info;
        static uint64_t start;
        uint64_t now = mach_absolute_time();
        if (!time_info.numer) {
            start = now;
            (void)mach_timebase_info(&time_info);
            return 0;
        }
        return (NUItime)((now-start)*time_info.numer/time_info.denom/1000000);
#else
        struct timespec ts;
        if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0)
            return 0;
        return (NUItime)(ts.tv_sec*1000+ts.tv_nsec/1000000);
#endif
    }
}

static int nuiD_wait(NUIparams *params, NUItime time) {
#ifdef _WIN32
    Sleep(time);
#else
    struct timeval timeout;
    timeout.tv_sec = time/1000;
    timeout.tv_usec = time%1000;
    select(0, NULL, NULL, NULL, &timeout);
#endif
    (void)params;
    return 0;
}

NUI_API NUIstate *nui_newstate(NUIparams *params) {
    NUIstate *S;
    if (!params->alloc) params->alloc = nuiD_alloc;
    if (!params->nomem) params->nomem = nuiD_nomem;
    if (!params->time)  params->time  = nuiD_time;
    if (!params->wait)  params->wait  = nuiD_wait;
    params->S = NULL;
    S = (NUIstate*)params->alloc(params, NULL, sizeof(NUIstate), 0);
    params->S = S;
    if (S == NULL) return NULL;
    memset(S, 0, sizeof(NUIstate));
    S->params = params;
    S->base.S = S;
    S->base.next_sibling = S->base.prev_sibling = &S->base;
    nui_initpool(&S->timers.pool, sizeof(NUItimer));
    nui_initpool(&S->eventpool, sizeof(NUIevent));
    nui_initpool(&S->handlerpool, sizeof(NUIhandlers));
    nui_initpool(&S->nodepool, sizeof(NUInode));
    nui_initpool(&S->smallpool, NUI_SMALLSIZE);
#define X(str) S->builtins[NUI_##str] = nui_usekey(NUI_(str));
    nui_builtinkeys(X)
#undef  X
    return S;
}

NUI_API void nui_close(NUIstate *S) {
    NUIparams *params = S->params;
    NUInode *n = &S->base;
    nuiN_delete(n, 0);
    n = S->freenodes;
    while (n) {
        NUInode *next = nui_nextsibling(n, n);
        nuiN_releaseall(n);
        n = next;
    }
    S->freenodes = NULL;
    if (S->params->close)
        S->params->close(S->params);
    nuiC_close(S);
    nuiT_cleartimers(S);
    nuiS_close(S);
    nui_freepool(S, &S->eventpool);
    nui_freepool(S, &S->handlerpool);
    nui_freepool(S, &S->nodepool);
    nui_freepool(S, &S->smallpool);
    params->alloc(S->params, S, 0, sizeof(NUIstate));
    params->S = NULL;
}

NUI_API int nui_pollevents(NUIstate *S) {
    int ret = 0;
    if (nuiT_hastimers(S)) {
        NUItime current = nui_time(S);
        nuiT_updatetimers(S, current);
    }
    S->freenodes = nuiN_sweepdead(S->freenodes);
    return !(ret || nuiT_hastimers(S) || S->base.child_count != 0);
}

NUI_API int nui_waitevents(NUIstate *S) {
    int ret;
    if (nuiT_hastimers(S)) {
        NUItime current = nui_time(S);
        nuiT_updatetimers(S, current);
        if ((ret = S->params->wait(S->params, nuiT_gettimeout(S, current))) < 0)
            return ret;
    }
    return nui_pollevents(S);
}

NUI_API int nui_loop(NUIstate *S) {
    int ret;
    while ((ret = nui_waitevents(S)) == 0)
        ;
    return ret;
}


#endif /* NUI_IMPLEMENTATION */

/* win32cc: flags+='-s -O3 -mdll -DNUI_IMPLEMENTATION -xc' output='nui.dll'
 * unixcc: flags+='-O2 -shared -DNUI_IMPLEMENTATION -xc' output='nui.so' */

