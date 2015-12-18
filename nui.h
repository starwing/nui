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
typedef struct NUIkey    NUIkey;
typedef struct NUItimer  NUItimer;
typedef struct NUIvalue  NUIvalue;
typedef struct NUInode   NUInode;
typedef struct NUItype   NUItype;
typedef struct NUIcomp   NUIcomp;

#ifdef ZN_USE_64BIT_TIMER
typedef unsigned long long NUItime;
#else
typedef unsigned NUItime;
#endif

/* callbacks */

typedef struct NUIlistener NUIlistener;
typedef struct NUIpoller   NUIpoller;
typedef struct NUIattr     NUIattr;
typedef NUItime NUItimerf (void *ud, NUItimer *timer, NUItime elapsed);


/* nui global routines */

NUI_API NUIstate *nui_newstate (NUIparams *params);
NUI_API void      nui_close    (NUIstate *S);

NUI_API NUIparams *nui_getparams (NUIstate *S);

NUI_API void nui_addpoller (NUIstate *S, NUIpoller *p);
NUI_API void nui_delpoller (NUIstate *S, NUIpoller *p);

NUI_API int  nui_pollevents (NUIstate *S);
NUI_API int  nui_waitevents (NUIstate *S);
NUI_API int  nui_loop       (NUIstate *S);
NUI_API void nui_breakloop  (NUIstate *S);


/* nui timer routines */

NUI_API NUItime nui_time (NUIstate *S);

NUI_API NUItimer *nui_newtimer (NUIstate *S, NUItimerf *tiemrf, void *ud);
NUI_API void      nui_deltimer (NUItimer *timer);

NUI_API NUItimerf *nui_gettimerf (NUItimer *t, void **ud);

NUI_API int  nui_starttimer  (NUItimer *t, NUItime delayms);
NUI_API void nui_canceltimer (NUItimer *t);


/* nui key symbol routines */

#define NUI_(s) (nui_newkey(S, #s, sizeof(#s)-1))

NUI_API NUIkey *nui_newkey (NUIstate *S, const char *s, size_t len);
NUI_API NUIkey *nui_getkey (NUIstate *S, const char *s, size_t len);

NUI_API size_t   nui_len      (NUIkey *key);
NUI_API unsigned nui_hash     (NUIkey *key);
NUI_API unsigned nui_calchash (NUIstate *S, const char *s, size_t len);


/* nui node routines */

NUI_API NUInode  *nui_newnode (NUIstate *S, NUItype **types, size_t count);
NUI_API NUIstate *nui_state   (NUInode *n);

NUI_API int nui_retain  (NUInode *n);
NUI_API int nui_release (NUInode *n);

NUI_API void nui_addlistener (NUInode *n, NUIlistener *li);
NUI_API void nui_dellistener (NUInode *n, NUIlistener *li);

NUI_API NUInode *nui_parent      (NUInode *n);
NUI_API NUInode *nui_prevchild   (NUInode *n, NUInode *curr);
NUI_API NUInode *nui_nextchild   (NUInode *n, NUInode *curr);
NUI_API NUInode *nui_prevsibling (NUInode *n, NUInode *curr);
NUI_API NUInode *nui_nextsibling (NUInode *n, NUInode *curr);

NUI_API NUInode *nui_root     (NUInode *n);
NUI_API NUInode *nui_prevleaf (NUInode *n, NUInode *curr);
NUI_API NUInode *nui_nextleaf (NUInode *n, NUInode *curr);

NUI_API NUInode *nui_indexnode  (NUInode *n, int idx);
NUI_API int      nui_nodeindex  (NUInode *n);
NUI_API int      nui_childcount (NUInode *n);

NUI_API void nui_setparent   (NUInode *n, NUInode *parent);
NUI_API void nui_setchildren (NUInode *n, NUInode *children);

NUI_API void nui_append (NUInode *n, NUInode *newnode);
NUI_API void nui_insert (NUInode *n, NUInode *newnode);

NUI_API int nui_detach (NUInode *n);


/* nui attribute routines */

NUI_API NUIattr *nui_addattr (NUInode *n, NUIkey *key, NUIattr *attr);
NUI_API NUIattr *nui_getattr (NUInode *n, NUIkey *key);
NUI_API void     nui_delattr (NUInode *n, NUIkey *key);

NUI_API void nui_addattrfallback (NUInode *n, NUIattr *attr);
NUI_API void nui_delattrfallback (NUInode *n, NUIattr *attr);

NUI_API int nui_set (NUInode *n, NUIkey *key, const NUIvalue *pv);
NUI_API int nui_get (NUInode *n, NUIkey *key, NUIvalue *pv);


/* nui componemt routines */

NUI_API NUItype *nui_newtype (NUIstate *S, NUIkey *name, size_t size, size_t csize);
NUI_API NUItype *nui_gettype (NUIstate *S, NUIkey *name);

NUI_API NUIcomp *nui_addcomp (NUInode *n, NUItype *t);
NUI_API NUIcomp *nui_getcomp (NUInode *n, NUItype *t);


/* value types */

typedef enum NUIvaluetype {
    NUI_TNIL, NUI_TBOOL, NUI_TINT, NUI_TNUM,
    NUI_TSTR, NUI_TPTR,  NUI_TFUN,
} NUIvaluetype;

typedef enum NUIvptype {
    NUI_TPTR_ANY,  NUI_TPTR_STATE, NUI_TPTR_PARAMS,
    NUI_TPTR_KEY,  NUI_TPTR_TIMER, NUI_TPTR_VALUE,
    NUI_TPTR_NODE, NUI_TPTR_TYPE,  NUI_TPTR_COMP
} NUIvptype;

struct NUIvalue {
    unsigned type    : 4;
    unsigned subtype : 28;
    union {
        long long ll;
        double d;
        const char *s;   /* subtype == length */
        void *p;         /* subtype == NUIvptype */
        void (*f)(void);
    } u;
};

#define nui_ivalue(v) ((v)->u.i)
#define nui_uvalue(v) ((v)->u.u)
#define nui_dvalue(v) ((v)->u.d)
#define nui_pvalue(v) ((v)->u.p)
#define nui_fvalue(v) ((v)->u.f)
#define nui_svalue(v) ((v)->u.s)


/* struct fields */

struct NUIparams {
    void *(*alloc) (NUIstate *S, void *p, size_t nsize, size_t osize);
    void *(*nomem) (NUIstate *S, void *p, size_t nsize, size_t osize);
    void  (*close) (NUIstate *S);

    NUItime (*time) (NUIstate *S);
    int     (*wait) (NUIstate *S, NUItime time);

    int (*break_wait) (NUIstate *S);
};

struct NUIpoller {
    void (*close) (NUIpoller *p, NUIstate *S);
    int  (*poll)  (NUIpoller *p, NUIstate *S);
};

struct NUIattr {
    int  (*get_attr) (NUIattr *attr, NUInode *node, NUIkey *key, NUIvalue *v);
    int  (*set_attr) (NUIattr *attr, NUInode *node, NUIkey *key, const NUIvalue *v);
    void (*del_attr) (NUIattr *attr, NUInode *node);
};

struct NUItype {
    NUIkey *name;
    size_t  type_size;
    size_t  comp_size;
    size_t  comp_align; /* should be 2^n, 0 means native align, sizeof(void*) */

    NUItype **(*depends) (NUItype *t, size_t *plen);

    int  (*new_componment) (NUItype *t, NUInode *n, NUIcomp *comp);
    void (*delete_node)    (NUItype *t, NUInode *n, NUIcomp *comp);
    void (*close)          (NUItype *t, NUIstate *S);
};

struct NUIcomp {
    NUItype *type;
};

struct NUIlistener {
    void (*delete_node)  (NUIlistener *li, NUInode *n);
    void (*add_child)    (NUIlistener *li, NUInode *self, NUInode *child);
    void (*remove_child) (NUIlistener *li, NUInode *self, NUInode *child);
    void (*added)        (NUIlistener *li, NUInode *parent, NUInode *self);
    void (*removed)      (NUIlistener *li, NUInode *parent, NUInode *self);
};


NUI_NS_END

#endif /* nui_h */


#if defined(NUI_IMPLEMENTATION) && !defined(nui_implemented)
#define nui_implemented

#define NUI_POLLSIZE                  4096
#define NUI_MAX_POLLELEMS(type)       ((NUI_POLLSIZE-sizeof(void*))/sizeof(type))
#define NUI_MIN_TIMERHEAP             128
#define NUI_MIN_STRTABLE_SIZE         32
#define NUI_HASHLIMIT                 5
#define NUI_MIN_HASHSIZE              4

#define NUI_TIMER_NOINDEX  (~(unsigned)0)
#define NUI_FOREVER        (~(NUItime)0)
#define NUI_MAX_SIZET      ((~(size_t)0)-100)

#define nui_lmod(s,size) \
        (assert((size&(size-1))==0), ((int)((s) & ((size)-1))))

#include <assert.h>
#include <string.h>

typedef struct NUItimerpool  NUItimerpool;
typedef struct NUItimers     NUItimers;
typedef struct NUIkeypool    NUIkeypool;
typedef struct NUIkeyheader  NUIkeyheader;
typedef struct NUIlist       NUIlist;
typedef struct NUIlistpool   NUIlistpool;
typedef struct NUIlists      NUIlists;
typedef struct NUItable      NUItable;
typedef struct NUIentry      NUIentry;

struct NUItimer {
    union { NUItimer *next; void *ud; } u;
    NUItimerf *handler;
    NUIstate *S;
    size_t index;
    NUItime starttime;
    NUItime emittime;
};

struct NUItimerpool {
    struct NUItimerpool *next;
    NUItimer timers[NUI_MAX_POLLELEMS(NUItimer)];
};

struct NUItimers {
    NUItimerpool *pool;
    NUItimer *freed;
    NUItimer **heap;
    NUItime nexttime;
    size_t pool_free;
    size_t heap_used;
    size_t heap_size;
};

struct NUIkeypool {
    NUIkeyheader **hash;
    size_t nuse;
    size_t size;
    unsigned seed;
};

struct NUIentry {
    ptrdiff_t next;
    NUIkey   *key;
    void     *value;
};

struct NUItable {
    size_t size;
    size_t lastfree;
    NUIentry *node;
};

struct NUIlist {
    NUIlist *next;
    void *value;
};

struct NUIlistpool {
    struct NUIlistpool *next;
    NUIlist lists[NUI_MAX_POLLELEMS(NUIlist)];
};

struct NUIlists {
    size_t pool_free;
    NUIlistpool *pool;
    NUIlist *freed;
};

struct NUInode {
    NUInode *parent;
    NUInode *prev_sibling;
    NUInode *next_sibling;
    NUInode *children;
    int child_count;
    int ref_count;
    size_t node_size;
    NUItable  attrs;
    NUItable  comps;
    NUIstate *S;
    NUIlist  *node_listeners;
    NUIlist  *attr_listeners;
};

struct NUIstate {
    NUIparams *params;
    NUIkeypool strt;
    NUItable types;
    NUItimers timers;
    NUIlists  lists;
    NUIlist *pollers;
    NUInode *nodes;
#ifdef NUI_DEBUG_MEM
    size_t allmem;
#endif
};

struct NUIkeyheader {
    NUIkeyheader *next;
    size_t len;
    unsigned hash;
};


/* memory */

static void *nuiM_malloc(NUIstate *S, size_t sz) {
    void *ptr = S->params->alloc(S, NULL, sz, 0);
    if (ptr == NULL) ptr = S->params->nomem(S, NULL, sz, 0);
#ifdef NUI_DEBUG_MEM
    S->allmem += sz;
    NUI_DEBUG_MEM("malloc: %d (%d)\n", S->allmem, sz);
#endif
    return ptr;
}

static void nuiM_free(NUIstate *S, void *ptr, size_t oz) {
    if (ptr == NULL || oz == 0) return;
    ptr = S->params->alloc(S, ptr, 0, oz);
    assert(ptr == NULL);
#ifdef NUI_DEBUG_MEM
    S->allmem -= oz;
    NUI_DEBUG_MEM("free: %d (%d)\n", S->allmem, oz);
#endif
}

static void *nuiM_realloc(NUIstate *S, void *ptr, size_t nz, size_t oz) {
    void *newptr = S->params->alloc(S, ptr, nz, oz);
    if (newptr == NULL) newptr = S->params->nomem(S, ptr, nz, oz);
#ifdef NUI_DEBUG_MEM
    S->allmem += nz;
    S->allmem -= oz;
    NUI_DEBUG_MEM("realloc: %d (%d->%d)\n", S->allmem, oz, nz);
#endif
    return newptr;
}

static NUIlist *nuiM_getlist(NUIstate *S) {
    NUIlist *li = S->lists.freed;
    if (li != NULL)
        S->lists.freed = li->next;
    else {
        if (S->lists.pool_free == 0) {
            NUIlistpool *pool = (NUIlistpool*)nuiM_malloc(S, sizeof(NUIlistpool));
            if (pool == NULL) return NULL;
            pool->next = S->lists.pool;
            S->lists.pool = pool;
            S->lists.pool_free = NUI_MAX_POLLELEMS(NUIlist);
        }
        li = &S->lists.pool->lists[NUI_MAX_POLLELEMS(NUIlist) - S->lists.pool_free--];
    }
    return li;
}

static void nuiM_putlist(NUIstate *S, NUIlist *li) {
    li->next = S->lists.freed;
    S->lists.freed = li;
}

static void nuiM_addlist(NUIstate *S, NUIlist **list, void *li) {
    NUIlist *newl = nuiM_getlist(S);
    assert(li != NULL);
    newl->next = *list;
    newl->value = li;
    *list = newl;
}

static void nuiM_dellist(NUIstate *S, NUIlist **list, void *li) {
    while ((*list) != NULL && (*list)->value != li)
        list = &(*list)->next;
    if ((*list) != NULL) {
        NUIlist *next = (*list)->next;
        nuiM_putlist(S, *list);
        (*list) = next;
    }
}


/* timer */

static int nuiT_hastimers(NUIstate *S)
{ return S->timers.heap_used != 0; }

NUI_API NUItimerf *nui_gettimerf(NUItimer *timer, void **ud)
{ if (ud) *ud = timer->u.ud; return timer->handler; }

NUI_API NUItime nui_time(NUIstate *S)
{ return S->params->time(S); }

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
    NUItimers *ts = &S->timers;
    NUItimer *timer = ts->freed;
    if (timer != NULL)
        ts->freed = timer->u.next;
    else {
        if (ts->pool_free == 0) {
            NUItimerpool *pool = (NUItimerpool*)nuiM_malloc(S, sizeof(NUItimerpool));
            if (pool == NULL) return NULL;
            pool->next = ts->pool;
            ts->pool = pool;
            ts->pool_free = NUI_MAX_POLLELEMS(NUItimer);
        }
        timer = &ts->pool->timers[NUI_MAX_POLLELEMS(NUItimer) - ts->pool_free--];
    }
    timer->u.ud = ud;
    timer->handler = cb;
    timer->S = S;
    timer->index = NUI_TIMER_NOINDEX;
    return timer;
}

NUI_API void nui_deltimer(NUItimer *timer) {
    nui_canceltimer(timer);
    timer->handler = NULL;
    timer->u.next = timer->S->timers.freed;
    timer->S->timers.freed = timer;
}

NUI_API int nui_starttimer(NUItimer *timer, NUItime delayms) {
    unsigned index;
    NUItimers *ts = &timer->S->timers;
    if (timer->index != NUI_TIMER_NOINDEX)
        nui_canceltimer(timer);
    if (ts->heap_size == ts->heap_used
            && !nuiT_resizeheap(timer->S, ts->heap_size * 2))
        return 0;
    index = ts->heap_used++;
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
    NUItimers *ts = &timer->S->timers;
    unsigned index = timer->index;
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
    NUItimers *ts = &S->timers;
    while (ts->pool != NULL) {
        NUItimerpool *next = ts->pool->next;
        nuiM_free(S, ts->pool, sizeof(NUItimerpool));
        ts->pool = next;
    }
    nuiM_free(S, ts->heap, ts->heap_size*sizeof(NUItimer*));
    memset(ts, 0, sizeof(NUItimers));
    ts->nexttime = NUI_FOREVER;
}

static void nuiT_updatetimers(NUIstate *S, NUItime current) {
    NUItimers *ts = &S->timers;
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


/* string table */

#define nuiS_data(key)   ((NUIkey*)((key) + 1))
#define nuiS_header(key) ((NUIkeyheader*)(key) - 1)

NUI_API size_t   nui_len  (NUIkey *key) { return nuiS_header(key)->len;  }
NUI_API unsigned nui_hash (NUIkey *key) { return nuiS_header(key)->hash; }

static void nuiS_resize(NUIstate *S, size_t newsize) {
    NUIkeypool *kp = &S->strt;
    size_t i, realsize = NUI_MIN_STRTABLE_SIZE;
    while (realsize < NUI_MAX_SIZET/2/sizeof(NUIkey*) && realsize < newsize)
        realsize *= 2;
    if (realsize > kp->size) {
        kp->hash = (NUIkeyheader**)nuiM_realloc(S, kp->hash,
                realsize * sizeof(NUIkeyheader*),
                kp->size * sizeof(NUIkeyheader*));
        for (i = kp->size; i < realsize; ++i) kp->hash[i] = NULL;
    }
    /* rehash */
    for (i = 0; i < kp->size; ++i) {
        NUIkeyheader *s = kp->hash[i];
        kp->hash[i] = NULL;
        while (s) { /* for each node in the list */
            NUIkeyheader *next = s->next; /* save next */
            unsigned h = nui_lmod(s->hash, (realsize-1));
            s->next = kp->hash[h];
            kp->hash[h] = s;
            s = next;
        }
    }
    if (realsize < kp->size) {
        /* shrinking slice must be empty */
        assert(kp->hash[realsize] == NULL && kp->hash[kp->size - 1] == NULL);
        kp->hash = (NUIkeyheader**)nuiM_realloc(S, kp->hash,
                kp->size * sizeof(NUIkeyheader*),
                realsize * sizeof(NUIkeyheader*));
    }
    kp->size = realsize;
}

static NUIkey *nuiS_new(NUIstate *S, const char *key, size_t len, unsigned h) {
    NUIkeyheader **list;  /* (pointer to) list where it will be inserted */
    NUIkeypool *kp = &S->strt;
    NUIkeyheader *o;
    size_t totalsize;  /* total size of NUIkey object */
    if (kp->nuse >= kp->size && kp->size <= NUI_MAX_SIZET/2)
        nuiS_resize(S, kp->size*2);  /* too crowded */
    list = &kp->hash[nui_lmod(h, kp->size)];
    totalsize = sizeof(NUIkeyheader) + ((len + 1) * sizeof(char));
    o = (NUIkeyheader*)nuiM_malloc(S, totalsize);
    o->len = len;
    o->hash = nui_calchash(S, key, len);
    o->next = *list;
    *list = o;
    memcpy(o+1, key, len*sizeof(char));
    ((char *)(o+1))[len] = '\0';  /* ending 0 */
    kp->nuse++;
    return nuiS_data(o);
}

static NUIkey *nuiS_get(NUIstate *S, const char *key, size_t len, unsigned h) {
    NUIkeyheader *o;
    if (S->strt.hash == NULL) return NULL;
    for (o = S->strt.hash[nui_lmod(h, S->strt.size)];
         o != NULL;
         o = o->next) {
        if (h == o->hash && len == o->len &&
                (memcmp(key, (char*)(o + 1), (len+1)*sizeof(char)) == 0))
            return nuiS_data(o);
    }
    return NULL;
}

NUI_API NUIkey *nui_newkey (NUIstate *S, const char *s, size_t len) {
    unsigned hash = nui_calchash(S, s, len);
    NUIkey *key = nuiS_get(S, s, len, hash);
    return key ? key : nuiS_new(S, s, len, hash);
}

NUI_API NUIkey *nui_getkey (NUIstate *S, const char *s, size_t len) {
    return nuiS_get(S, s, len, nui_calchash(S, s, len));
}

NUI_API void nui_delkey(NUIstate *S, NUIkey *key) {
    NUIkeypool    *kp = &S->strt;
    NUIkeyheader  *h = nuiS_header(key);
    NUIkeyheader **list = &kp->hash[nui_lmod(h->hash, kp->size)];
    for (; *list != NULL && *list != h; list = &(*list)->next)
        ;
    if (*list == h) {
        *list = h->next;
        size_t totalsize = sizeof(NUIkeyheader) + ((h->len+1) * sizeof(char));
        nuiM_free(S, key, totalsize);
        --kp->nuse;
    }
}

NUI_API unsigned nui_calchash(NUIstate *S, const char *s, size_t len) {
    unsigned h = S->strt.seed ^ (unsigned)len;
    size_t l1;
    size_t step = (len >> NUI_HASHLIMIT) + 1;
    for (l1 = len; l1 >= step; l1 -= step)
        h = h ^ ((h<<5) + (h>>2) + (unsigned char)(s[l1 - 1]));
    return h;
}

static void nuiS_close(NUIstate *S) {
    size_t i;
    NUIkeypool *kp = &S->strt;
    for (i = 0; i < kp->size; ++i) {
        NUIkeyheader *h = kp->hash[i];
        kp->hash[i] = NULL;
        while (h) {
            NUIkeyheader *next = h->next;
            size_t totalsize = sizeof(NUIkeyheader)
                + ((h->len+1) * sizeof(char));
            nuiM_free(S, h, totalsize);
            h = next;
        }
    }
    nuiM_free(S, kp->hash, kp->size * sizeof(NUIkey*));
}


/* hash table */

static size_t nuiH_resize(NUIstate *S, NUItable *t, size_t len);

static size_t nuiH_hashsize(size_t len) {
    size_t newsize = NUI_MIN_HASHSIZE;
    while (newsize < NUI_MAX_SIZET/2/sizeof(NUIentry) && newsize < len)
        newsize <<= 1;
    return newsize;
}

static size_t nuiH_countsize(NUItable *t) {
    size_t i, count = 0;
    for (i = 0; i < t->size; ++i) {
        NUIentry *e = &t->node[i];
        if (e->key != NULL && e->value != NULL)
            ++count;
    }
    return count;
}

static NUIentry *nuiH_mainposition(NUItable *t, NUIkey *key) {
    assert((t->size & (t->size - 1)) == 0);
    return &t->node[nui_hash(key) & (t->size - 1)];
}

static NUIentry *nuiH_newkey(NUIstate *S, NUItable *t, NUIkey *key) {
    NUIentry *mp;
    if (t->size == 0 && nuiH_resize(S, t, NUI_MIN_HASHSIZE) == 0) 
        return NULL;
redo:
    mp = nuiH_mainposition(t, key);
    if (mp->key != 0) {
        NUIentry *f = NULL, *othern;
        while (t->lastfree > 0) {
            NUIentry *e = &t->node[--t->lastfree];
            if (e->key == 0)  { f = e; break; }
        }
        if (f == NULL) {
            if (nuiH_resize(S, t, nuiH_countsize(t)*2) == 0)
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
            if (mp->next != 0) {
                f->next += mp - f;
                mp->next = 0;
            }
        }
        else {
            if (mp->next != 0)
                f->next = (mp + mp->next) - f;
            else assert(f->next == 0);
            mp->next = f - mp;
            mp = f;
        }
    }
    mp->key = key;
    mp->value = NULL;
    return mp;
}

static void nuiH_init(NUItable *t) {
    t->size = 0;
    t->lastfree = 0;
    t->node = NULL;
}

static void nuiH_free(NUIstate *S, NUItable *t) {
    if (t->node != NULL)
        nuiM_free(S, t->node, t->size*sizeof(NUIentry));
    nuiH_init(t);
}

static size_t nuiH_resize(NUIstate *S, NUItable *t, size_t len) {
    size_t i;
    NUItable nt;
    nt.size = nuiH_hashsize(len);
    nt.node = (NUIentry*)nuiM_malloc(S, nt.size*sizeof(NUIentry));
    nt.lastfree = nt.size;
    memset(nt.node, 0, sizeof(NUIentry)*nt.size);
    for (i = 0; i < t->size; ++i) {
        NUIentry *e = &t->node[i];
        if (e->key != NULL && e->value != NULL)
            nuiH_newkey(S, &nt, e->key);
    }
    nuiM_free(S, t->node, t->size*sizeof(NUIentry));
    *t = nt;
    return t->size;
}

static NUIentry *nuiH_get(NUItable *t, NUIkey *key) {
    NUIentry *e;
    if (t->size == 0 || key == NULL) return NULL;
    assert((t->size & (t->size - 1)) == 0);
    e = &t->node[nui_lmod(nui_hash(key), t->size)];
    while (1) {
        ptrdiff_t next = e->next;
        if (e->key == key) return e;
        if (next == 0) return NULL;
        e += next;
    }
    return NULL;
}

static NUIentry *nuiH_set(NUIstate *S, NUItable *t, NUIkey *key) {
    NUIentry *ret;
    if (key == NULL) return NULL;
    if ((ret = nuiH_get(t, key)) != NULL)
        return ret;
    return nuiH_newkey(S, t, key);
}


/* nui attribute */

NUI_API void nui_addattrfallback(NUInode *n, NUIattr *attr)
{ nuiM_addlist(n->S, &n->attr_listeners, attr); }

NUI_API void nui_delattrfallback(NUInode *n, NUIattr *attr)
{ nuiM_dellist(n->S, &n->attr_listeners, attr); }

NUI_API NUIattr *nui_addattr(NUInode *n, NUIkey *key, NUIattr *attr) {
    NUIentry *e = nuiH_set(n->S, &n->attrs, key);
    return (NUIattr*)(e->value ? NULL : (e->value = attr));
}

NUI_API NUIattr *nui_getattr(NUInode *n, NUIkey *key) {
    NUIentry *e = nuiH_get(&n->attrs, key);
    return e ? (NUIattr*)e->value : NULL;
}

NUI_API void nui_delattr(NUInode *n, NUIkey *name) {
    NUIentry *e = nuiH_get(&n->attrs, name);
    NUIattr *attr;
    if (e == NULL) return;
    attr = (NUIattr*)e->value;
    if (attr->del_attr != NULL)
        attr->del_attr(attr, n);
    e->value = NULL;
}

NUI_API int nui_set(NUInode *n, NUIkey *key, const NUIvalue *pv) {
    NUIattr *attr = nui_getattr(n, key);
    NUIlist *li = n->attr_listeners;
    int ret;
    if (attr->set_attr &&
            (ret = attr->set_attr(attr, n, key, pv)) != NUI_TNIL)
        return ret;
    while (li != NULL) {
        attr = (NUIattr*)li->value;
        if (attr->set_attr &&
                (ret = attr->set_attr(attr, n, key, pv)) != NUI_TNIL)
            return ret;
        li = li->next;
    }
    return 0;
}

NUI_API int nui_get(NUInode *n, NUIkey *key, NUIvalue *pv) {
    NUIattr *attr = nui_getattr(n, key);
    NUIlist *li = n->attr_listeners;
    int ret;
    if (attr->get_attr &&
            (ret = attr->get_attr(attr, n, key, pv)) != NUI_TNIL)
        return ret;
    while (li != NULL) {
        attr = (NUIattr*)li->value;
        if (attr->get_attr &&
                (ret = attr->get_attr(attr, n, key, pv)) != NUI_TNIL)
            return ret;
        li = li->next;
    }
    return 0;
}

static void nuiA_clear(NUInode *n) {
    size_t i;
    for (i = 0; i < n->attrs.size; ++i) {
        NUIentry *e = &n->attrs.node[i];
        if (e->value) {
            NUIattr *attr = (NUIattr*)e->value;
            if (attr->del_attr)
                attr->del_attr(attr, n);
        }
    }
    nuiH_free(n->S, &n->attrs);
    while (n->attr_listeners) {
        NUIlist *next = n->attr_listeners->next;
        NUIattr *attr = (NUIattr*)n->attr_listeners->value;
        if (attr->del_attr)
            attr->del_attr(attr, n);
        nuiM_putlist(n->S, n->attr_listeners);
        n->attr_listeners = next;
    }
}


/* nui componemt */

static int nuiC_ptrinrange(void *p, void *begin, void *end)
{ return (ptrdiff_t)p >= (ptrdiff_t)begin && (ptrdiff_t)p < (ptrdiff_t)end; }

NUI_API NUItype *nui_newtype(NUIstate *S, NUIkey *name, size_t size, size_t csize) {
    NUIentry *e = nuiH_set(S, &S->types, name);
    NUItype *t;
    if (e->value != NULL) return NULL;
    if (size < sizeof(NUItype))  size = sizeof(NUItype);
    if (csize < sizeof(NUIcomp)) csize = sizeof(NUIcomp);
    t = (NUItype*)nuiM_malloc(S, size);
    memset(t, 0, size);
    t->name = name;
    t->type_size = size;
    t->comp_size = csize;
    e->value = t;
    return t;
}

NUI_API NUItype *nui_gettype(NUIstate *S, NUIkey *name) {
    NUIentry *e = nuiH_get(&S->types, name);
    return e ? (NUItype*)e->value : NULL;
}

NUI_API NUIcomp *nui_addcomp(NUInode *n, NUItype *t) {
    NUIentry *e = nuiH_set(n->S, &n->comps, t->name);
    NUIcomp *comp;
    if (e->value) return (NUIcomp*)e->value;
    assert(t->comp_size >= sizeof(NUIcomp));
    comp = (NUIcomp*)nuiM_malloc(n->S, t->comp_size);
    memset(comp, 0, t->comp_size);
    comp->type = t;
    e->value = comp;
    if (t->new_componment && !t->new_componment(t, n, comp)) {
        nuiM_free(n->S, comp, t->comp_size);
        return NULL;
    }
    return comp;
}

NUI_API NUIcomp *nui_getcomp(NUInode *n, NUItype *t) {
    NUIentry *e = nuiH_get(&n->comps, t->name);
    return e ? (NUIcomp*)e->value : NULL;
}

static void nuiC_close(NUIstate *S) {
    size_t i;
    for (i = 0; i < S->types.size; ++i) {
        NUItype *t = (NUItype*)S->types.node[i].value;
        if (t != NULL) {
            if (t->close != NULL)
                t->close(t, S);
            nuiM_free(S, t, t->type_size);
        }
    }
    nuiH_free(S, &S->types);
}

static void nuiC_clear(NUInode *n) {
    size_t i;
    void *endn = (void*)((char*)n + n->node_size);
    for (i = 0; i < n->comps.size; ++i) {
        NUIentry *e = &n->comps.node[i];
        if (e->value) {
            NUIcomp *comp = (NUIcomp*)e->value;
            if (comp->type->delete_node)
                comp->type->delete_node(comp->type, n, comp);
            if (!nuiC_ptrinrange((void*)comp, (void*)n, endn))
                nuiM_free(n->S, comp, comp->type->comp_size);
        }
    }
    nuiH_free(n->S, &n->comps);
    while (n->node_listeners) {
        NUIlist *next = n->node_listeners->next;
        NUIlistener *li = (NUIlistener*)n->node_listeners->value;
        if (li->delete_node)
            li->delete_node(li, n);
        nuiM_putlist(n->S, n->node_listeners);
        n->node_listeners = next;
    }
}

static size_t nuiC_calcalign(size_t base, NUItype *t) {
    size_t align = t->comp_align == 0 ? sizeof(void*) : t->comp_align;
    size_t diff = base % align;
    if (diff == 0) return base;
    return base + align - diff;
}

static size_t nuiC_calcsize(NUIstate *S, size_t base, NUItable *b, NUItype **types, size_t count) {
    size_t i, node_size = base;
    for (i = 0; i < count; ++i) {
        size_t dlen;
        NUItype *t = types[i], **depends;
        NUIentry *e;
        if (t->depends && (depends = t->depends(t, &dlen)) != NULL)
            node_size += nuiC_calcsize(S, node_size, b, depends, dlen);
        if ((e = nuiH_set(S, b, t->name))->value == NULL) {
            node_size = nuiC_calcalign(node_size, t);
            e->value = (void*)((char*)NULL + node_size);
            node_size += t->comp_size;
        }
    }
    return node_size;
}

static void nuiC_initcomps(NUInode *n, NUItype **types, size_t count) {
    size_t i;
    void *endn = (void*)((char*)0 + n->node_size);
    assert((void*)n > endn);
    for (i = 0; i < count; ++i) {
        size_t dlen;
        NUItype *t = types[i], **depends;
        NUIentry *e;
        if (t->depends && (depends = t->depends(t, &dlen)) != NULL)
            nuiC_initcomps(n, depends, dlen);
        e = nuiH_get(&n->comps, t->name);
        if (nuiC_ptrinrange(e->value, NULL, endn)) {
            NUIcomp *c = (NUIcomp*)((char*)n + ((char*)e->value - (char*)0));
            e->value = (void*)c;
            c->type = t;
            if (t->new_componment)
                t->new_componment(t, n, c);
        }
    }
}


/* nui node */

NUI_API NUIstate *nui_state(NUInode *n) { return n ? n->S : NULL; }

NUI_API int nui_retain(NUInode *n) { return ++n->ref_count; } 
NUI_API int nui_childcount(NUInode *n) { return n->child_count; }

NUI_API NUInode* nui_parent(NUInode *n)
{ return n && n->parent ? n->parent : NULL; }

NUI_API void nui_addlistener(NUInode *n, NUIlistener *li)
{ nuiM_addlist(n->S, &n->node_listeners, li); }

NUI_API void nui_dellistener(NUInode *n, NUIlistener *li)
{ nuiM_dellist(n->S, &n->node_listeners, li); }

static void nuiN_setparent(NUInode *n, NUInode *parent) {
    NUIlist *li;
    if (n->parent == parent) return;
    if (n->parent) {
        /* removed from previous parent */
        for (li = n->node_listeners; li != NULL; li = li->next) {
            NUIlistener *listener = (NUIlistener*)li->value;
            if (listener->removed)
                listener->removed(listener, n, n->parent);
        }
        for (li = n->parent->node_listeners; li != NULL; li = li->next) {
            NUIlistener *listener = (NUIlistener*)li->value;
            if (listener->remove_child)
                listener->remove_child(listener, n, n->parent);
        }
        n->parent = NULL;
    }
    if (parent) {
        /* insert child to new parent */
        for (li = n->node_listeners; li != NULL; li = li->next) {
            NUIlistener *listener = (NUIlistener*)li->value;
            if (listener->added)
                listener->added(listener, n, parent);
        }
        for (li = parent->node_listeners; li != NULL; li = li->next) {
            NUIlistener *listener = (NUIlistener*)li->value;
            if (listener->add_child)
                listener->add_child(listener, n, parent);
        }
        n->parent = parent;
    }
}

static void nuiN_insert(NUInode *h, NUInode *n) {
    n->prev_sibling = h->prev_sibling;
    n->prev_sibling->next_sibling = n;
    n->next_sibling = h;
    h->prev_sibling = n;
}

static void nuiN_merge(NUInode *h, NUInode *n) {
    h->next_sibling->prev_sibling = n->prev_sibling;
    n->prev_sibling->next_sibling = h->next_sibling;
    h->next_sibling = n;
    n->prev_sibling = h;
}

static void nuiN_settop(NUIstate *S, NUInode *n) {
    if (S->nodes != NULL)
        nuiN_insert(S->nodes, n);
    else {
        S->nodes = n;
        n->next_sibling = n->prev_sibling = n;
    }
}

static void nuiN_detach(NUInode *n) {
    NUInode *next = n == n->prev_sibling ? NULL : n->next_sibling;
    NUInode *parent = n->parent;
    nuiN_setparent(n, NULL);
    n->next_sibling->prev_sibling = n->prev_sibling;
    n->prev_sibling->next_sibling = n->next_sibling;
    n->prev_sibling = n->next_sibling = n;
    if (parent) {
        if (parent->children == n)
            parent->children = next;
        --parent->child_count;
    }
    else if (n->S->nodes == n)
        n->S->nodes = NULL;
}

static void nuiN_delete(NUInode *n) {
    assert(n->parent == NULL);
    nuiN_detach(n);
    nui_setchildren(n, NULL);
    while (n->node_listeners) {
        NUIlist *next = n->node_listeners->next;
        NUIlistener *li = (NUIlistener*)n->node_listeners->value;
        if (li->delete_node)
            li->delete_node(li, n);
        nuiM_putlist(n->S, n->node_listeners);
        n->node_listeners = next;
    }
    nuiA_clear(n);
    nuiC_clear(n);
    nuiM_free(n->S, n, n->node_size);
}

static NUInode *nuiN_sweepdead(NUInode *n) {
    NUInode *i, *next;
    while (n && n->ref_count <= 0) {
        next = n == n->next_sibling ? NULL : n->next_sibling;
        nuiN_delete(n);
        n = next;
    }
    if (n == NULL) return NULL;
    nuiN_setparent(n, NULL);
    for (i = n->next_sibling; i != n; i = next) {
        next = i->next_sibling;
        if (i->ref_count <= 0)
            nuiN_delete(i);
        else
            nuiN_setparent(i, NULL);
    }
    return n;
}

NUI_API NUInode *nui_newnode(NUIstate *S, NUItype **types, size_t count) {
    NUInode *n;
    NUItable bases;
    size_t node_size;
    nuiH_init(&bases);
    node_size = nuiC_calcsize(S, sizeof(NUInode), &bases, types, count);
    n = (NUInode*)nuiM_malloc(S, node_size);
    memset(n, 0, node_size);
    n->ref_count = 1;
    n->node_size = node_size;
    nuiH_init(&n->attrs);
    n->comps = bases;
    n->S = S;
    nuiC_initcomps(n, types, count);
    nuiN_settop(S, n);
    return n;
}

NUI_API int nui_release(NUInode *n) {
    int cur = --n->ref_count;
    if (n->parent) return 1;
    if (cur == 0) nuiN_delete(n);
    return cur;
}

NUI_API void nui_setparent(NUInode *n, NUInode *parent) {
    if (parent == n->parent) return;
    nuiN_detach(n);
    if (parent == NULL)
        nuiN_settop(n->S, n);
    else {
        nuiN_setparent(n, parent);
        if (parent->children)
            nuiN_insert(parent->children, n);
        else {
            n->next_sibling = n->prev_sibling = n;
            parent->children = n;
        }
        ++parent->child_count;
    }
}

NUI_API void nui_setchildren(NUInode *n, NUInode *newnode) {
    NUInode *i, *children = n->children;
    int child_count = 0;
    if (children != NULL) {
        children = nuiN_sweepdead(children);
        n->children = NULL;
    }
    if (newnode != NULL) {
        if (newnode->parent == NULL)
            nuiN_detach(newnode);
        else {
            NUInode *parent = newnode->parent;
            newnode = nuiN_sweepdead(newnode);
            parent->children = NULL;
            parent->child_count = 0;
        }
        child_count = 1;
        nuiN_setparent(newnode, n);
        for (i = newnode->next_sibling; i != newnode; i = i->next_sibling) {
            nuiN_setparent(i, n);
            ++child_count;
        }
        n->children = newnode;
    }
    else if (n->S->nodes == NULL)
        n->S->nodes = children;
    else if (children != NULL)
        nuiN_merge(n->S->nodes, children);
    n->child_count = child_count;
}

NUI_API void nui_append(NUInode *n, NUInode *newnode) {
    if (newnode != NULL) {
        NUInode *next;
        nuiN_detach(newnode);
        nuiN_setparent(newnode, n->parent);
        next = n->next_sibling;
        if (n->parent == NULL)
            nuiN_settop(n->S, newnode);
        else {
            nuiN_insert(next, newnode);
            ++n->parent->child_count;
        }
    }
}

NUI_API void nui_insert(NUInode *n, NUInode *newnode) {
    if (newnode != NULL) {
        nuiN_detach(newnode);
        nuiN_setparent(newnode, n->parent);
        if (n->parent == NULL)
            nuiN_settop(n->S, newnode);
        else {
            nuiN_insert(n, newnode);
            if (n->parent->children == n)
                n->parent->children = newnode;
            ++n->parent->child_count;
        }
    }
}

NUI_API int nui_detach(NUInode *n) {
    if (n->parent != NULL) {
        if (n->ref_count <= 0) {
            nuiN_delete(n);
            return 0;
        }
        nuiN_detach(n);
        nuiN_settop(n->S, n);
    }
    return n->ref_count;
}

NUI_API NUInode *nui_prevchild(NUInode *n, NUInode *curr) {
    if (n == NULL || n->children == NULL)
        return NULL;
    return nui_prevsibling(n->children, curr);
}

NUI_API NUInode *nui_nextchild(NUInode *n, NUInode *curr) {
    if (n == NULL || n->children == NULL)
        return NULL;
    return nui_nextsibling(n->children, curr);
}

NUI_API NUInode* nui_prevsibling(NUInode *n, NUInode *curr) {
    if (curr == n) return NULL;
    if (curr == NULL) curr = n;
    return curr->prev_sibling;
}

NUI_API NUInode* nui_nextsibling(NUInode *n, NUInode *curr) {
    if (curr == NULL) return n;
    curr = curr->next_sibling;
    return curr == n ? NULL : curr;
}

NUI_API NUInode* nui_root(NUInode *n) {
    NUInode *parent;
    if (n == NULL) return NULL;
    while ((parent = n->parent) != NULL)
        n = parent;
    return n;
}

NUI_API NUInode* nui_prevleaf(NUInode *n, NUInode *curr) {
    NUInode *parent, *firstchild;
    if (curr == n) return NULL; /* end of iteration */
    if (curr == NULL) curr = n; /* first of iteration */
    if (curr != n) {
        /* return parent if curr is the first child of parent. */
        if ((parent = curr->parent) != NULL && parent->children == curr)
            return parent;
        /* if curr is not top, set curr to the preious sibling */
        if (parent) curr = curr->prev_sibling;
    }
    /* and get it's last leaf */
    while ((firstchild = curr->children) != NULL)
        curr = firstchild->prev_sibling;
    return curr;
}

NUI_API NUInode* nui_nextleaf(NUInode *n, NUInode *curr) {
    NUInode *parent;
    if (curr == NULL) return n;
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

NUI_API NUInode *nui_indexnode(NUInode *n, int idx) {
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

NUI_API int nui_nodeindex(NUInode *n) {
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


/* nui state */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#ifdef _WIN32
# define WIN32_LEAN_AND_MEAN
# include <Windows.h>
#else
# include <sys/select.h>
#endif

NUI_API NUIparams *nui_getparams(NUIstate *S)
{ return S->params; }

NUI_API void nui_addpoller(NUIstate *S, NUIpoller *p)
{ nuiM_addlist(S, &S->pollers, p); }

NUI_API void nui_delpoller(NUIstate *S, NUIpoller *p)
{ nuiM_dellist(S, &S->pollers, p); }

static void *def_alloc(NUIstate *S, void *p, size_t nsize, size_t osize) {
    if (nsize == 0) {
        free(p);
        return NULL;
    }
    return realloc(p, nsize);
}

static void *def_nomem(NUIstate *S, void *p, size_t nsize, size_t osize) {
    fprintf(stderr, "nui: out of memory\n");
    abort();
    return NULL;
}

static NUItime def_time(NUIstate *S) {
    return (NUItime)clock();
}

static int def_wait(NUIstate *S, NUItime time) {
#ifdef _WIN32
    Sleep(time);
#else
    struct timeval timeout;
    timeout.tv_sec = time/1000;
    timeout.tv_usec = time%1000;
    select(0, NULL, NULL, NULL, &timeout);
#endif
    return 0;
}

NUI_API NUIstate *nui_newstate(NUIparams *params) {
    NUIstate *S;
    if (!params->alloc) params->alloc = def_alloc;
    if (!params->nomem) params->nomem = def_nomem;
    if (!params->time)  params->time  = def_time;
    if (!params->wait)  params->wait  = def_wait;
    S = (NUIstate*)params->alloc(NULL, NULL, sizeof(NUIstate), 0);
    if (S == NULL) return NULL;
    memset(S, 0, sizeof(NUIstate));
    S->params = params;
    nuiH_init(&S->types);
    return S;
}

NUI_API void nui_close(NUIstate *S) {
    while (S->nodes) {
        NUInode *current = S->nodes;
        S->nodes = S->nodes->next_sibling == S->nodes ?
            NULL : S->nodes->next_sibling;
        nuiN_delete(current);
    }
    while (S->pollers) {
        NUIlist *next = S->pollers->next;
        NUIpoller *p = (NUIpoller*)S->pollers->value;
        if (p->close)
            p->close(p, S);
        nuiM_putlist(S, S->pollers);
        S->pollers = next;
    }
    if (S->params->close)
        S->params->close(S);
    while (S->lists.pool) {
        NUIlistpool *next = S->lists.pool->next;
        nuiM_free(S, S->lists.pool, sizeof(NUIlistpool));
        S->lists.pool = next;
    }
    nuiC_close(S);
    nuiT_cleartimers(S);
    nuiS_close(S);
#ifdef NUI_DEBUG_MEM
    NUI_DEBUG_MEM("allmem: %d\n", S->allmem);
    assert(S->allmem == 0);
#endif
    S->params->alloc(NULL, S, 0, sizeof(NUIstate));
}

NUI_API int nui_pollevents(NUIstate *S) {
    NUIlist *li, *next;
    if (nuiT_hastimers(S)) {
        NUItime current = nui_time(S);
        nuiT_updatetimers(S, current);
    }
    for (li = S->pollers; li != NULL; li = next) {
        int ret;
        NUIpoller *p = (NUIpoller*)li->value;
        next = li->next;
        if ((ret = p->poll(p, S)) != 0)
            return ret;
    }
    return nuiT_hastimers(S) || S->nodes != NULL ? 0 : 1;
}

NUI_API int nui_waitevents(NUIstate *S) {
    int ret;
    if (nuiT_hastimers(S)) {
        NUItime current = nui_time(S);
        nuiT_updatetimers(S, current);
        if ((ret = S->params->wait(S, nuiT_gettimeout(S, current))) != 0)
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

NUI_API void nui_breakloop(NUIstate *S) {
    if (S->params->break_wait)
        S->params->break_wait(S);
}


#endif /* NUI_IMPLEMENTATION */
/* cc: flags+='-s -O3 -mdll -DNUI_IMPLEMENTATION -xc' output='nui.dll' */
