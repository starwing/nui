#define NUI_CORE
#include "nui.h"
#include "nui_backend.h"


#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


/* === pre-defined string */

#define NUI_PREDEFINED_STRING(X) \
    X(default)     \
    X(position)    \
    X(abspos)      \
    X(size)        \
    X(nalturesize) \
    X(handle)      \
    X(id)          \
    X(parent)      \

typedef enum NUIpredefidx {
#define X(name) NUI_IDX_##name,
    NUI_PREDEFINED_STRING(X)
#undef  X
    NUI_PREDEF_COUNT
} NUIpredefidx;


/* === nui limits === */

/* minimum size for the string table (must be power of 2) */
#if !defined(NUI_MIN_STRTABLE_SIZE)
# define NUI_MIN_STRTABLE_SIZE         32
#endif

/* minimum log size for hash table */
#if !defined(NUI_MIN_TABLE_LSIZE)
# define NUI_MIN_TABLE_LSIZE           2
#endif

/* size of panic message buffer */
#if !defined(NUI_MAX_PANICBUFFER_SIZE)
# define NUI_MAX_PANICBUFFER_SIZE      512
#endif

/* size of action arguments array size */
#if !defined(NUI_MIN_STACK_SIZE)
# define NUI_MIN_STACK_SIZE              32
#endif

/* at most ~(2^NUI_HASHLIMIT) bytes from a string
 * to compute its hash */
#if !defined(NUI_HASHLIMIT)
# define NUI_HASHLIMIT           5
#endif

#define NUI_MAX_SIZET       ((size_t)(~(size_t)0)-2)

#define NUI_MAX_INT         (INT_MAX-2)  /* maximum value of an int (-2 for safety) */

#if !defined(UNUSED)
#define UNUSED(x)       ((void)(x))     /* to avoid warnings */
#endif

/* internal assertions for in-house debugging */
#if defined(nui_assert)
# define nui_checkexp(c,e)       (nui_assert(c), (e))
/* to avoid problems with conditions too long */
# define nui_longassert(c)       { if (!(c)) nui_assert(0); }
#elif 1 /* XXX */
# include <assert.h>
# define nui_assert(c)           assert(c)
# define nui_checkexp(c,e)       (assert(c), (e))
# define nui_longassert(c)       { if (!(c)) nui_assert(0); }
#else
# define nui_assert(c)           ((void)0)
# define nui_checkexp(c,e)       (e)
# define nui_longassert(c)       ((void)0)
#endif

/*
 * `module' operation for hashing (size is always a power of 2)
 */
#define nui_lmod(s,size) \
        (nui_checkexp((size&(size-1))==0, ((int)((s) & ((size)-1)))))


/* === nui list === */

#define nuiL_init(q)   ((void)((q)->NUI_PREV = (q), (q)->NUI_NEXT = (q)))
#define nuiL_empty(h)  ((h) == (h)->NUI_PREV)

#define nuiL_sentinel(h) (h)
#define nuiL_first(h)    ((h)->NUI_NEXT)
#define nuiL_last(h)     ((h)->NUI_PREV)
#define nuiL_next(q)     ((q)->NUI_NEXT)
#define nuiL_prev(q)     ((q)->NUI_PREV)

#define nuiL_insert(h, x) ((void)(                                            \
    (x)->NUI_PREV = (h)->NUI_PREV,                                            \
    (x)->NUI_PREV->NUI_NEXT = (x),                                            \
    (x)->NUI_NEXT = (h),                                                      \
    (h)->NUI_PREV = (x)))

#define nuiL_insert_pointer(p, x) ((void)(                                    \
    (p) != NULL ? nuiL_insert(p, x) :                                         \
    ( (p) = (x), nuiL_init(x) )))

#define nuiL_remove(x) ((void)(                                               \
    (x)->NUI_NEXT->NUI_PREV = (x)->NUI_PREV,                                  \
    (x)->NUI_PREV->NUI_NEXT = (x)->NUI_NEXT))

#define nuiL_remove_safe(x) ((void)(                                          \
    (x)->NUI_NEXT->NUI_PREV = (x)->NUI_PREV,                                  \
    (x)->NUI_PREV->NUI_NEXT = (x)->NUI_NEXT,                                  \
    (x)->NUI_PREV = (x),                                                      \
    (x)->NUI_NEXT = (x)))

#define nuiL_split(h, q, n) ((void)(                                          \
    (n)->NUI_PREV = (h)->NUI_PREV,                                            \
    (n)->NUI_PREV->NUI_NEXT = (n),                                            \
    (n)->NUI_NEXT = (q),                                                      \
    (h)->NUI_PREV = (q)->NUI_PREV,                                            \
    (h)->NUI_PREV->NUI_NEXT = (h),                                            \
    (q)->NUI_PREV = n))

#define nuiL_merge(h, n) ((void)(                                             \
    (h)->NUI_PREV->NUI_NEXT = (n)->NUI_NEXT,                                  \
    (n)->NUI_NEXT->NUI_PREV = (h)->NUI_PREV,                                  \
    (h)->NUI_PREV = (n)->NUI_PREV,                                            \
    (h)->NUI_PREV->NUI_NEXT = (h)))

#define nuiL_foreach(i, h)                                                    \
    for ((i) = (h)->NUI_NEXT; (i) != (h); (i) = (i)->NUI_NEXT)

#define nuiL_foreach_back(i, h)                                               \
    for ((i) = (h)->NUI_PREV; (i) != (h); (i) = (i)->NUI_PREV)

#define nuiL_foreach_safe(i, nexti, h)                                        \
    for ((i) = (h)->NUI_NEXT, (nexti) = (i)->NUI_NEXT;                        \
         (i) != (h);                                                          \
         (i) = (nexti), (nexti) = (nexti)->NUI_NEXT)


/* === nui half(hash) list === */

#define nuiHL_init(h, x) ((void)(                                             \
            (x)->pprev = &(h),                                                \
            (x)->next  = (h),                                                 \
            (void)((h) && ((h)->pprev = &(x)->next)),                         \
            (h) = (x)))

#define nuiHL_remove(x) ((void)(                                              \
            *(x)->pprev = (x)->next,                                          \
            (void)((x)->next && ((x)->next->pprev = (x)->pprev))))

#define nuiHL_insert(h, x) ((void)(nuiHL_remove(x), nuiHL_init(h, x)))


/* === nui structures === */

typedef struct NUIstringtable NUIstringtable;
typedef struct NUIactiondata NUIactiondata;
typedef struct NUInodedata NUInodedata;

struct NUIstringtable {
    NUIstring **hash;
    NUIstring *gclist;
    size_t nuse;
    size_t size;
};

struct NUIstring {
    NUIstring *next;
    NUIstring *nextgc;
    size_t len;
    unsigned hash;
    int refcount;
};

struct NUIactiondata {
    NUIactiondata *next;
    NUIactiondata **pprev; /* point to the `next` pointer of prev actions */
    NUIactiondata *next_linked;
    NUIactiondata *prev_linked;

    unsigned call_count;
    unsigned last_trigger_time;
    unsigned trigger_time;
    unsigned interval;

    NUIaction user;
};

struct NUInodedata {
    /* node structure */
    NUInodedata *next; /* next node in same class */
    NUInodedata *parent; /* parent of this node */
    NUInodedata *prev_sibling; /* previous sibling of this node */
    NUInodedata *next_sibling; /* next sibling of this node */
    NUInodedata *children; /* first child of this node */
    size_t childcount;

    /* natual attributes */
    NUIclass *klass;
    NUItable attrs; /* attributes table */
    NUIpoint pos;  /* upper-left corner relative to native parent. */
    NUIsize size; /* user defined-size for this node */
    int visible;
    int flags; /* flags of this node */
    void *handle; /* native handle for this node */

    NUInode user;
};

struct NUIstate {
    NUIparams *params;
    int is_closing;

    /* memory alloc */
    size_t totalmem;

    /* string inten table */
    unsigned seed;
    NUIstringtable strt;
    NUIstring *predefs[NUI_PREDEF_COUNT];

    /* actions */
    NUItable named_actions;
    NUIactiondata *actions;
    NUIactiondata *dead_actions;
    NUIactiondata *timed_actions;

    /* action callback stack */
    NUIvalue *stack;
    size_t stack_base;
    size_t stack_top;
    size_t stack_size;

    /* class/node */
    NUItable classes;
    NUIclass *default_class;
    NUInodedata *dead_nodes;
};


/* === nui memory === */

#define nuiM_reallocv(S,b,on,n,e) \
  ((void) \
     (((size_t)((n)+1) > NUI_MAX_SIZET/(e)) ? (nuiM_toobig(S), 0) : 0), \
   nuiM_realloc_(S, (b), (on)*(e), (n)*(e)))

#define nuiM_freemem(S, b, s)     nuiM_realloc_(S, (b), (s), 0)
#define nuiM_free(S, b)           nuiM_realloc_(S, (b), sizeof(*(b)), 0)
#define nuiM_freevector(S, b, n)  nuiM_reallocv(S, (b), n, 0, sizeof((b)[0]))

#define nuiM_malloc(S,s)        nuiM_realloc_(S, NULL, 0, (s))
#define nuiM_new(S,t)           ((t*)nuiM_malloc(S, sizeof(t)))
#define nuiM_newvector(S,n,t) \
                ((t*)nuiM_reallocv(S, NULL, 0, (n), sizeof(t)))

#define nuiM_reallocvector(S,v,oldn,n,t) \
   ((v)=((t*)nuiM_reallocv(S, v, oldn, n, sizeof(t))))

static n_noret nuiM_toobig(NUIstate *S) {
  nui_error(S, "memory allocation error: block too big");
}

static void *nuiM_realloc_ (NUIstate *S, void *block, size_t oldsize,
        size_t size) {
    void *newblock;
    nui_assert((oldsize == 0) == (block == NULL));
    if (size == 0) {
        free(block);
        newblock = NULL;
    }
    else
        newblock = realloc(block, size);
    if (newblock == NULL && size > 0) {
        if (size > oldsize)
            nui_error(S, 
                "realloc cannot fail when shrinking a block");
        if (newblock == NULL)
            nui_error(S, "out of memory");
    }
    nui_assert((size == 0) == (newblock == NULL));
#if 0 /* XXX */
    if (size > oldsize)
        printf("ALLOC: %d\n", size - oldsize);
    else if (size < oldsize)
        printf("FREE:  %d\n", oldsize - size);
#endif
    S->totalmem = (S->totalmem + size) - oldsize;
    return newblock;
}


/* 
 * the NUIstring, NUIaction and NUInode are opacity pointers. but all
 * of them can be used as user-defined buffer. you can cast then
 * directly to your type. such as (const char*)a_str_ptr, or
 * (MyNodeType*)a_node_ptr.
 *
 * In these types, the memory just after the structure is reserved to
 * user. nui doesn't use that memory.
 *
 * So, With a NUI API:
 * [nui-spec informations][user-defined memory buffer]
 * ^ NUI use this         ^ user use this.
 *
 * So, In a specfic API, before returned a such type to user, you
 * should call touser with the pointer, and before maintain the data
 * of such type, you should call todata.
 *
 * This is the implement details in the module itself. e.g. in
 * NUIstring codes, we call todata/touser, but a Node that using a
 * NUIstring needn't call that. as it's use NUI APIs to operate
 * NUIstring, but not access the data directly.
 */
#define offset_of(f,t)      ((ptrdiff_t) &((t*)0)->f)
#define container_of(p,f,t) ((t*)((char*)(p)-offset_of(f,t)))

#define string_touser(d) nui_checkexp(d != NULL, (d)+1)
#define string_todata(u) nui_checkexp(u != NULL, (u)-1)

#define action_touser(d) nui_checkexp(d != (NUIactiondata*)NULL, &((d)->user))
#define action_todata(u) nui_checkexp(u != (NUIaction*)NULL, \
        container_of(u, user, NUIactiondata))

#define node_touser(d) nui_checkexp(d != (NUInodedata*)NULL, &((d)->user))
#define node_todata(u) nui_checkexp(u != (NUInode*)NULL, \
        container_of(u, user, NUInodedata))


/* === nui string === */

static unsigned calc_hash (const char *s, size_t len, unsigned seed) {
    unsigned h = seed ^ (unsigned)len;
    size_t l1;
    size_t step = (len >> NUI_HASHLIMIT) + 1;
    for (l1 = len; l1 >= step; l1 -= step)
        h = h ^ ((h<<5) + (h>>2) + (unsigned char)(s[l1 - 1]));
    return h;
}

static void deletestring(NUIstate *S, NUIstring *s) {
    NUIstringtable *tb = &S->strt;
    NUIstring **list = &tb->hash[nui_lmod(s->hash, tb->size)];
    for (; *list != NULL && *list != s; list = &(*list)->next)
        ;
    if (*list == s) {
        *list = s->next;
        size_t totalsize = sizeof(NUIstring) +
            ((s->len + 1) * sizeof(char));
        nuiM_freemem(S, s, totalsize);
        --tb->nuse;
    }
}

static void nuiS_resize(NUIstate *S, size_t newsize) {
    NUIstringtable *tb = &S->strt;
    size_t i, realsize = NUI_MIN_STRTABLE_SIZE;
    while (realsize < NUI_MAX_SIZET/2 && realsize < newsize)
        realsize *= 2;
    if (realsize > tb->size) {
        nuiM_reallocvector(S, tb->hash, tb->size, realsize, NUIstring*);
        for (i = tb->size; i < realsize; ++i) tb->hash[i] = NULL;
    }
    /* rehash */
    for (i = 0; i < tb->size; ++i) {
        NUIstring *s = tb->hash[i];
        tb->hash[i] = NULL;
        while (s) { /* for each node in the list */
            NUIstring *next = s->next; /* save next */
            unsigned h = nui_lmod(s->hash, (realsize-1));
            s->next = tb->hash[h];
            tb->hash[h] = s;
            s = next;
        }
    }
    if (realsize < tb->size) {
        /* shrinking slice must be empty */
        nui_assert(tb->hash[realsize] == NULL && tb->hash[tb->size - 1] == NULL);
        nuiM_reallocvector(S, tb->hash, tb->size, realsize, NUIstring*);
    }
    tb->size = realsize;
}

static NUIstring *createstrobj (NUIstate *S, const char *s, size_t len,
                                unsigned int h, NUIstring **list) {
  NUIstring *o;
  size_t totalsize;  /* total size of NUIstring object */
  totalsize = sizeof(NUIstring) + ((len + 1) * sizeof(char));
  o = (NUIstring*)nuiM_malloc(S, totalsize);
  o->len = len;
  o->hash = h;
  o->refcount = 1;
  o->next = *list;
  o->nextgc = NULL;
  *list = o;
  memcpy(o+1, s, len*sizeof(char));
  ((char *)(o+1))[len] = '\0';  /* ending 0 */
  return o;
}

static NUIstring *newstring (NUIstate *S, const char *s, size_t len,
                                          unsigned int h) {
  NUIstring **list;  /* (pointer to) list where it will be inserted */
  NUIstringtable *tb = &S->strt;
  NUIstring *o;
  if (tb->nuse >= tb->size && tb->size <= NUI_MAX_INT/2)
    nuiS_resize(S, tb->size*2);  /* too crowded */
  list = &tb->hash[nui_lmod(h, tb->size)];
  o = createstrobj(S, s, len, h, list);
  tb->nuse++;
  return o;
}

NUIstring *nui_string  (NUIstate *S, const char *s) {
    return nui_newlstr(S,s,strlen(s));
}

NUIstring *nui_newlstr (NUIstate *S, const char *s, size_t len) {
    NUIstring *o;
    unsigned hash = calc_hash(s, len, S->seed);
    for (o = S->strt.hash[nui_lmod(hash, S->strt.size)];
         o != NULL;
         o = o->next) {
        if (   hash == o->hash &&
                len == o->len  &&
                (memcmp(s, (char*)string_touser(o),
                        (len+1)*sizeof(char)) == 0))
            return string_touser(o);
    }
    return string_touser(newstring(S, s, len, hash));
}

int nui_holdstring (NUIstate *S, NUIstring *s) {
    s = string_todata(s);
    if (++s->refcount > 0 && s->nextgc != NULL) {
        NUIstring **list = &S->strt.gclist;
        for (; *list != NULL && *list != s; list = &(*list)->nextgc)
            ;
        if (*list == s) {
            *list = s->nextgc;
            s->nextgc = NULL;
        }
    }
    return s->refcount;
}

int nui_dropstring (NUIstate *S, NUIstring *s) {
    s = string_todata(s);
    if (--s->refcount <= 0 && s->nextgc == NULL) {
        NUIstringtable *tb = &S->strt;
        s->nextgc = tb->gclist;
        tb->gclist = s->nextgc;
    }
    return s->refcount;
}

unsigned nui_calchash (NUIstate *S, const char *s, size_t len) {
    return calc_hash(s, len, S->seed);
}

size_t   nui_strlen  (NUIstring *s) { return string_todata(s)->len;  }
unsigned nui_strhash (NUIstring *s) { return string_todata(s)->hash; }

static void nuiS_open(NUIstate *S) {
    NUIstringtable *tb = &S->strt;
    tb->gclist = NULL;
    tb->hash = NULL;
    tb->nuse = 0;
    tb->size = 0;
    nuiS_resize(S, NUI_MIN_STRTABLE_SIZE);
    S->seed = 0;
#define X(name) S->predefs[NUI_IDX_##name] = nui_string(S, #name);
    NUI_PREDEFINED_STRING(X)
#undef  X
}

static void nuiS_close(NUIstate *S) {
    size_t i;
    NUIstringtable *tb = &S->strt;
    for (i = 0; i < tb->size; ++i) {
        NUIstring *s = tb->hash[i];
        tb->hash[i] = NULL;
        while (s) {
            NUIstring *next = s->next;
            size_t totalsize = sizeof(NUIstring) +
                ((s->len + 1) * sizeof(char));
            nuiM_freemem(S, s, totalsize);
            s = next;
        }
    }
    nuiM_freevector(S, tb->hash, tb->size);
}

static void nuiS_sweep(NUIstate *S) {
    NUIstring **list = &S->strt.gclist;
    while (*list != NULL) {
        NUIstring *next = (*list)->nextgc;
        deletestring(S, *list);
        *list = next;
    }
}


/* === nui table === */

#define nui_tsize(t) (1<<((t)->lsize))

static int getnodeoffset(NUItable *t, NUIentry *n, size_t *psz) {
    ptrdiff_t off = n - t->node;
    if (off < 0 || off >= nui_tsize(t))
        return 0;
    *psz = (size_t)off;
    return 1;
}

static NUIentry *getfreepos(NUItable *t) {
    while (t->lastfree > t->node) {
        t->lastfree--;
        if (t->lastfree->key == NULL && t->lastfree->next == NULL)
            return t->lastfree;
    }
    return NULL;  /* could not find a free place */
}

static NUIentry *getnewkeynode(NUIstate *S, NUItable *t, NUIstring *key) {
    NUIentry *mp;
    if (key == NULL) return NULL;
    mp = &t->node[nui_lmod(nui_strhash(key), nui_tsize(t))];
    if (mp == NULL || mp->key != NULL) { /* main position is taken? */
        NUIentry *othern;
        NUIentry *n = getfreepos(t); /* get a free place */
        if (n == NULL) { /* can not find a free place? */
            nui_resizetable(S, t, nui_counttable(t)+1); /* grow table */
            return nui_settable(S, t, key);
        }
        othern = &t->node[nui_lmod(nui_strhash(mp->key), nui_tsize(t))];
        if (othern != mp) { /* is colliding node out of its main position? */
            /* yes; move colliding node into free position */
            while (othern->next != mp) othern = othern->next; /* find previous */
            othern->next = n; /* redo the chain whth `n' in place of `mp' */
            *n = *mp; /* copy colliding node into free pos (mp->next also goes) */
            mp->next = NULL; /* now `mp' is free */
        }
        else { /* colliding node is in its own main position */
            /* new node will go into free position */
            n->next = mp->next;
            mp->next = n;
            mp = n;
        }
    }
    nui_holdstring(S, key);
    mp->key = key;
    mp->value = NULL;
    return mp;
}

void nui_inittable(NUIstate *S, NUItable *t, size_t size) {
    size_t i, lsize = NUI_MIN_TABLE_LSIZE;
    if (size == 0) {
        t->node = NULL;
        t->lastfree = NULL;
        t->lsize = 0;
        return;
    }

    while ((size_t)(1<<lsize) < NUI_MAX_SIZET/2
	    && (size_t)(1<<lsize) < size)
        ++lsize;
    if ((1<<lsize) >= NUI_MAX_SIZET/2)
        nuiM_toobig(S);
    size = 1<<lsize;
    t->node = nuiM_newvector(S, size, NUIentry);
    for (i = 0; i < size; ++i) {
        NUIentry *n = &t->node[i];
        n->next = NULL;
        n->key = NULL;
        n->value = NULL;
        n->deletor = NULL;
    }
    t->lastfree = &t->node[size];
    t->lsize = lsize;
}

void nui_freetable(NUIstate *S, NUItable *t) {
    nui_resettable(S, t);
    if (t->node != NULL)
        nuiM_freevector(S, t->node, nui_tsize(t));
    nui_inittable(S, t, 0);
}

void nui_resizetable(NUIstate *S, NUItable *t, size_t newsize) {
    size_t oldsize = 1<<t->lsize;
    NUIentry *node = t->node;
    size_t i;
    nui_inittable(S, t, newsize);
    if (node == NULL)
        return;
    for (i = 0; i < oldsize; ++i) {
        NUIentry *old = &node[i];
        if (old->key != NULL) {
            NUIentry *new = nui_settable(S, t, old->key);
            new->value = old->value;
            new->deletor = old->deletor;
        }
    }
    nuiM_freevector(S, node, oldsize);
}

void nui_resettable(NUIstate *S, NUItable *t) {
    if (t->node != NULL) {
        size_t i, size = nui_tsize(t);
        for (i = 0; i < size; ++i)
            nui_dropentry(S, &t->node[i]);
    }
}

NUIentry *nui_gettable(NUItable *t, NUIstring *key) {
    NUIentry *n;
    if (key == NULL) return NULL;
    n = &t->node[nui_lmod(nui_strhash(key), nui_tsize(t))];
    for (; n != NULL; n = n->next)
        if (n->key == key)
            return n;
    return NULL;
}

NUIentry *nui_settable(NUIstate *S, NUItable *t, NUIstring *key) {
    NUIentry *n = nui_gettable(t, key);
    if (n != NULL) return n;
    if (key == NULL) return NULL;
    return getnewkeynode(S, t, key);
}

void nui_dropentry(NUIstate *S, NUIentry *n) {
    if (n == NULL)
        return;
    if (n->key != NULL) {
        nui_dropstring(S, n->key);
        n->key = NULL;
    }
    if (n->deletor) {
        n->deletor(S, n->value);
        n->deletor = NULL;
    }
    n->value = NULL;
}

size_t nui_counttable(NUItable *t) {
    size_t i, count = 0;
    if (t->node != NULL) {
        size_t size = nui_tsize(t);
        for (i = 0; i < size; ++i)
            if (t->node[i].key != NULL)
                ++count;
    }
    return count;
}

NUIentry *nui_nextentry(NUItable *t, NUIentry *n) {
    size_t i, size = nui_tsize(t);
    if (n == NULL)
        i = 0;
    else if (!getnodeoffset(t, n, &i))
        return NULL;
    for (; i < size; ++i) {
        NUIentry *n = &t->node[i];
        if (n->key != NULL)
            return n;
    }
    return NULL;
}


/* === nui action arguments stack === */

static NUIvalue *stack_index(NUIstate *S, int idx) {
    if (idx > 0 && S->stack_base+idx-1 <= S->stack_top)
        return &S->stack[S->stack_base+idx-1];
    if (idx < 0 && S->stack_top+idx >= S->stack_base)
        return &S->stack[S->stack_top+idx];
    return NULL;
}

static int stack_resize(NUIstate *S, size_t size) {
    size_t realsize = NUI_MIN_STACK_SIZE;
    while (realsize < NUI_MAX_SIZET/2 && realsize < size)
        realsize *= 2;
    if (realsize >= NUI_MAX_SIZET/2)
        return 0;
    nuiM_reallocvector(S, S->stack, S->stack_size, realsize, NUIvalue);
    S->stack_size = realsize;
    if (S->stack_base > S->stack_size)
        S->stack_base = S->stack_size;
    if (S->stack_top > S->stack_size)
        S->stack_top = S->stack_size;
    return 1;
}

static size_t stack_newframe(NUIstate *S, int nargs) {
    size_t curr_base = S->stack_base;
    if (S->stack_top - S->stack_base < (size_t)nargs)
        nargs = S->stack_top - S->stack_base;
    S->stack_base = S->stack_top - nargs;
    S->stack_top  = S->stack_base + nargs;
    /*printf("stack_newframe(%d):\n", nargs);*/
    /*printf("  curr_base=%d\n", curr_base);*/
    /*printf("  new_base=%d\n", S->stack_base);*/
    /*printf("  new_top=%d\n", S->stack_top);*/
    return curr_base;
}

static int stack_popframe(NUIstate *S, size_t base, int nrets) {
    if ((size_t)nrets < S->stack_top-S->stack_base)
        nui_settop(S, nrets);
    if (S->stack_top - nrets > S->stack_base) {
        int i;
        for (i = 0; i < nrets; ++i)
            S->stack[S->stack_base+i] = S->stack[S->stack_top-nrets+i];
    }
    /*printf("stack_popframe(%d):\n", nrets);*/
    /*printf("  curr_base=%d\n", S->stack_base);*/
    S->stack_top = S->stack_base + nrets;
    S->stack_base = base;
    /*printf("  new_base=%d\n", S->stack_base);*/
    /*printf("  new_top=%d\n", S->stack_top);*/
    return nrets;
}

int nui_gettop(NUIstate *S) {
    return S->stack_top - S->stack_base;
}

void nui_settop(NUIstate *S, int idx) {
    if (idx >= 0) {
        size_t newtop = S->stack_base+idx;
        if (newtop > S->stack_size)
            stack_resize(S, newtop);
        while (S->stack_top < newtop)
            S->stack[S->stack_top++] = nui_nilvalue();
        S->stack_top = newtop;
    }
    else if (S->stack_top+idx >= S->stack_base)
        S->stack_top += idx;
}

int nui_pushvalue(NUIstate *S, NUIvalue v) {
    if (S->stack_top == S->stack_size
            && !stack_resize(S, S->stack_size*2))
        return 0;
    S->stack[S->stack_top++] = v;
    return 1;
}

int nui_setvalue(NUIstate *S, int idx, NUIvalue v) {
    NUIvalue *dst = stack_index(S, idx);
    if (dst) *dst = v;
    return dst != NULL;
}

int nui_getvalue(NUIstate *S, int idx, NUIvalue *pv) {
    NUIvalue *dst = stack_index(S, idx);
    if (dst && pv) *pv = *dst;
    return dst != NULL;
}

static void reverse(NUIvalue *from, NUIvalue *to) {
    for (; from < to; ++from, --to) {
        NUIvalue v = *from;
        *from = *to;
        *to = v;
    }
}

void nui_rotate(NUIstate *S, int idx, int n) {
    int top = S->stack_top-S->stack_base;
    NUIvalue *begin = stack_index(S, idx);
    NUIvalue *end = stack_index(S, -1);
    NUIvalue *middle;
    if (n == 0 || begin == NULL || end == NULL)
        return;
    middle = n > 0 ? end-(n%top) : begin+(-n)%top-1;
    reverse(begin, middle);
    reverse(middle+1, end);
    reverse(begin, end);
}

int nui_copyvalues(NUIstate *S, int idx) {
    int copyn = idx<0 ? -idx : nui_gettop(S)+idx+1;
    if (S->stack_top+copyn >= S->stack_size
            && !stack_resize(S, S->stack_top+copyn))
        return 0;
    memcpy(&S->stack[S->stack_top],
           &S->stack[S->stack_top-copyn],
           copyn * sizeof(NUIvalue));
    return 1;
}


/* === nui action === */

#define NUI_PREV prev_linked
#define NUI_NEXT next_linked

#define action_size(size) (offset_of(user, NUIactiondata)+(size))

NUIaction *nui_action(NUIstate *S, size_t sz) {
    NUIactiondata *D;
    if (S->is_closing)
        return NULL;
    if (sz == 0) sz = sizeof(NUIaction);
    D = (NUIactiondata*)nuiM_malloc(S, action_size(sz));
    nuiHL_init(S->actions, D);
    nuiL_init(D);
    D->call_count = 0;
    D->last_trigger_time = 0;
    D->trigger_time = 0;
    D->interval = 0;

    D->user.size = sz;
    D->user.emit = NULL;
    D->user.deletor = NULL;
    D->user.S = S;
    D->user.n = NULL;
    return action_touser(D);
}

NUIaction *nui_copyaction(NUIaction *a) {
    NUIactiondata *D = action_todata(a);
    NUIactiondata *copied = action_todata(nui_action(a->S, a->size));
    if (copied == NULL) return NULL;
    copied->user = D->user;
    if (a->copy != NULL
            && !a->copy(a->S, a, action_touser(copied))) {
        nuiM_freemem(a->S, copied, action_size(a->size));
        return NULL;
    }
    return action_touser(copied);
}

NUI_API NUIaction *nui_namedaction(NUIstate *S, NUIstring *name) {
    NUIentry *entry = nui_gettable(&S->named_actions, name);
    if (entry != NULL)
        return nui_copyaction((NUIaction*)entry->value);
    return NULL;
}

NUIaction *nui_newnamedaction(NUIstate *S, NUIstring *name, size_t sz) {
    NUIentry *entry = nui_gettable(&S->named_actions, name);
    if (entry == NULL) {
        NUIaction *a = nui_action(S, sz);
        entry->value = a;
        entry->deletor = (NUIdeletor*)nui_dropaction;
    }
    return (NUIaction*)entry->value;
}

void nui_dropnamedaction(NUIstate *S, NUIstring *name)
{ nui_dropentry(S, nui_gettable(&S->named_actions, name)); }

void nui_dropaction(NUIaction *a) {
    NUIactiondata *D = action_todata(a);
    nuiHL_insert(a->S->dead_actions, D);
}

void nui_actiondelayed(NUIaction *a, unsigned delayed) {
    NUIactiondata *D = action_todata(a);
    D->trigger_time = nui_time(a->S) + delayed;
}

void nui_linkaction(NUIaction *a, NUIaction *na) {
    NUIactiondata *D = action_todata(a);
    NUIactiondata *Dnew = action_todata(na);
    nuiL_remove(D);
    nuiL_insert(D, Dnew);
}

void nui_unlinkaction(NUIaction *a) {
    nuiL_remove_safe(action_todata(a));
}

NUIaction *nui_prevaction(NUIaction *a, NUIaction *curr) {
    if (curr == a) return NULL;
    if (curr == NULL) curr = a;
    return action_touser(nuiL_prev(action_todata(curr)));
}

NUIaction *nui_nextaction(NUIaction *a, NUIaction *curr) {
    if (curr == NULL) return a;
    curr = action_touser(nuiL_next(action_todata(curr)));
    return curr == a ? NULL : curr;
}

void nui_emitaction(NUIaction *a, int nargs) {
    NUIactiondata *D = action_todata(a), *Di, *next;
    NUIstate *S = a->S;
    NUInode *n = a->n;
    int res = 0, is_single = nuiL_empty(D);
    size_t base;

    if (!is_single)
        nui_copyvalues(S, nargs);
    if (a->emit != NULL) {
        base = stack_newframe(S, nargs);
        res = a->emit(S, a, n);
        stack_popframe(S, base, 0);
    }
    if (is_single || res)
        return;
    nuiL_foreach_safe(Di, next, D) {
        NUIaction *i = action_touser(Di);
        if (i->emit != NULL) {
            nui_copyvalues(S, nargs);
            base = stack_newframe(S, nargs);
            res = i->emit(S, i, n);
            stack_popframe(S, base, 0);
            if (res) break;
        }
    }
    /* remove copied arguments */
    nui_settop(S, -nargs-1);
}

void nui_starttimer(NUIaction *a, unsigned delayed, unsigned interval) {
    NUIactiondata *D = action_todata(a), **list;
    if (a->S->is_closing) return;
    D->trigger_time = nui_time(a->S) + delayed;
    D->interval = interval;
    list = &a->S->timed_actions;
    /* insert D into S->timed_actions */
    for (list = &a->S->timed_actions;
         *list != NULL && D->trigger_time >= (*list)->trigger_time;
         list = &(*list)->next)
        ;
    nuiHL_insert(*list, D);
}

void nui_stoptimer(NUIaction *a) {
    NUIactiondata *D = action_todata(a);
    if (!a->S->is_closing)
        nuiHL_insert(a->S->actions, D);
    D->interval = 0;
    D->trigger_time = 0;
}

static void action_sweeplist(NUIstate *S, NUIactiondata **list) {
    while (*list != NULL) {
        NUIactiondata *D = *list;
        NUIaction *a = action_touser(D);
        if (a->deletor)
            a->deletor(S, a);
        nuiL_remove(D);
        nuiHL_remove(D);
        nuiM_freemem(S, a, action_size(a->size));
    }
}

static void nuiA_open(NUIstate *S) {
    nui_inittable(S, &S->named_actions, 0);
    S->actions = NULL;
    S->timed_actions = NULL;
    S->dead_actions = NULL;
    S->stack = NULL;
    S->stack_base = 0;
    S->stack_top = 0;
    S->stack_size = 0;
}

static void nuiA_sweep(NUIstate *S) {
    action_sweeplist(S, &S->dead_actions);
}

static void nuiA_close(NUIstate *S) {
    nui_freetable(S, &S->named_actions);
    action_sweeplist(S, &S->actions);
    action_sweeplist(S, &S->timed_actions);
    action_sweeplist(S, &S->dead_actions);
    nuiM_freevector(S, S->stack, S->stack_size);
}

unsigned nuiA_remaintime(NUIstate *S) {
    int delta;
    if (S->timed_actions == NULL)
        return 0;
    delta = (int)S->timed_actions->trigger_time - nui_time(S);
    return delta > 0 ? delta : 1;
}

void nuiA_updateactions(NUIstate *S) {
    NUIactiondata *D;
    if ((D = S->timed_actions) != NULL) {
        unsigned time = nui_time(S);
        S->timed_actions = NULL;
        while (D != NULL && D->trigger_time <= time) {
            NUIactiondata *next = D->next;
            NUIaction *a = action_touser(D);
            nui_pushvalue(S, nui_integervalue(time - D->last_trigger_time));
            nui_pushvalue(S, nui_integervalue(D->call_count));
            nui_emitaction(a, 2);
            if (D->next == next) { /* a is still in list? */
                if (D->interval != 0)
                    nui_starttimer(a, D->interval, D->interval);
                else
                    nui_stoptimer(a);
            }
            D->last_trigger_time = D->trigger_time;
            ++D->call_count;
            D = next;
        }
    }
}

#undef NUI_PREV
#undef NUI_NEXT


/* === nui node === */

#define NUI_PREV prev_sibling
#define NUI_NEXT next_sibling

#define node_size(size) (offset_of(user, \
            NUInodedata)+((size)!=0?(size):sizeof(NUInode)))

static void node_setparent(NUInodedata *Dn, NUInodedata *Dparent) {
    NUInode *n = node_touser(Dn);
    if (Dn->parent && Dn->parent != Dparent) {
        NUInodedata *Dp = Dn->parent;
        if (Dp->klass->child_removed)
            Dp->klass->child_removed(Dp->klass, node_touser(Dp), n);
        Dn->parent = NULL;
    }
    if (Dparent) {
        NUInode *parent = node_touser(Dparent);
        if (Dparent->klass->child_added)
            Dparent->klass->child_added(Dparent->klass, parent, n);
        Dn->parent = Dparent;
    }
}

static void node_delete(NUIstate *S, NUInodedata *Dn) {
    NUInode *n = node_touser(Dn);
    nui_detach(n);
    if (Dn->klass->delete_node)
        Dn->klass->delete_node(Dn->klass, n);
    nui_freetable(Dn->klass->S, &Dn->attrs);
    nuiM_freemem(S, Dn, node_size(Dn->klass->node_size));
}

NUInode *nui_node(NUIstate *S, NUIstring *classname) {
    NUInodedata *D;
    NUIentry *entry = nui_gettable(&S->classes, classname);
    NUIclass *klass = entry ?
        (NUIclass*)entry->value : S->default_class;
    D = (NUInodedata*)nuiM_malloc(S, node_size(klass->node_size));
    if (D == NULL) return NULL;
    D->next = klass->all_nodes;
    klass->all_nodes = D;
    D->parent = NULL;
    nuiL_init(D);
    D->children = NULL;
    D->childcount = 0;

    D->klass = klass;
    nui_inittable(S, &D->attrs, 0);
    D->pos = nui_point(0, 0);
    D->size = nui_size(0, 0);
    D->visible = 0;
    D->flags = 0;
    D->handle = NULL;
    D->user.id = NULL;
    D->user.action = NULL;
    /*n->user.layout_params = NULL;*/

    if (D->klass->new_node
            && !D->klass->new_node(D->klass, node_touser(D))) {
        nuiM_freemem(S, D, node_size(klass->node_size));
        return NULL;
    }
    return node_touser(D);
}

void nui_dropnode(NUInode *n) {
    NUInodedata *D = node_todata(n), **list;
    for (list = &D->klass->all_nodes;
            *list != NULL && *list != D;
            list = &(*list)->next)
        ;
    if (*list == D) {
        *list = D->next;
        D->next = D->klass->S->dead_nodes;
        D->klass->S->dead_nodes = D;
    }
}

void nui_setchildren(NUInode *n, NUInode *newnode) {
    NUInodedata *D = node_todata(n);
    NUInodedata *i;
    size_t childcount = 0;
    if (D->children != NULL) {
        node_setparent(D->children, NULL);
        nuiL_foreach (i, D->children)
            node_setparent(i, NULL);
        D->children = NULL;
    }
    if (newnode != NULL) {
        NUInodedata *Dnewnode = node_todata(newnode);
        childcount = 1;
        node_setparent(Dnewnode, D);
        nuiL_foreach (i, Dnewnode) {
            node_setparent(i, D);
            ++childcount;
        }
        if (Dnewnode->parent) {
            Dnewnode->parent->children = NULL;
            Dnewnode->parent->childcount = 0;
        }
        D->children = Dnewnode;
    }
    D->childcount = childcount;
}

void nui_append(NUInode *n, NUInode *newnode) {
    NUInodedata *D = node_todata(n);
    if (newnode != NULL) {
        NUInodedata *Dnewnode = node_todata(newnode);
        NUInodedata *next;
        if (Dnewnode->parent != NULL)
            nui_detach(newnode);
        node_setparent(Dnewnode, D->parent);
        next = nuiL_next(D);
        nuiL_insert(next, Dnewnode);
        if (D->parent)
            ++D->parent->childcount;
    }
}

void nui_insert(NUInode *n, NUInode *newnode) {
    NUInodedata *D = node_todata(n);
    if (newnode != NULL) {
        NUInodedata *Dnewnode = node_todata(newnode);
        if (Dnewnode->parent != NULL)
            nui_detach(newnode);
        node_setparent(Dnewnode, D->parent);
        nuiL_insert(D, Dnewnode);
        if (D->parent && D->parent->children == D)
            D->parent->children = Dnewnode;
        if (D->parent)
            ++D->parent->childcount;
    }
}

void nui_detach(NUInode *n) {
    NUInodedata *D = node_todata(n);
    NUInodedata *next = nuiL_empty(D) ? NULL : nuiL_next(D);
    NUInodedata *parent = D->parent;
    node_setparent(D, NULL);
    nuiL_remove_safe(D);
    if (parent) {
        if (parent->children == D)
            parent->children = next;
        --parent->childcount;
    }
}

NUInode* nui_parent(NUInode *n) {
    NUInodedata *D = node_todata(n);
    NUInode *parent = (D->parent == NULL ? NULL : node_touser(D->parent));
    if (D->klass->get_parent)
        return D->klass->get_parent(D->klass, n, parent);
    return parent;
}

void nui_setparent(NUInode *n, NUInode *parent) {
    NUInodedata *D = node_todata(n);
    if (D->parent != NULL) nui_detach(n);
    if (parent != NULL) {
        NUInodedata *Dparent = node_todata(parent);
        node_setparent(D, Dparent);
        if (Dparent->children)
            nuiL_insert(Dparent->children, D);
        else {
            nuiL_init(D);
            Dparent->children = D;
        }
        ++Dparent->childcount;
    }
}

NUInode *nui_prevchild(NUInode *n, NUInode *curr) {
    NUInodedata *D;
    if (n == NULL) return NULL;
    D = node_todata(n);
    if (D->children == NULL) return NULL;
    return nui_prevsibling(node_touser(D->children), curr);
}

NUInode *nui_nextchild(NUInode *n, NUInode *curr) {
    NUInodedata *D;
    if (n == NULL) return NULL;
    D = node_todata(n);
    if (D->children == NULL) return NULL;
    return nui_nextsibling(node_touser(D->children), curr);
}

NUInode* nui_prevsibling(NUInode *n, NUInode *curr) {
    if (curr == n) return NULL;
    if (curr == NULL) curr = n;
    return node_touser(nuiL_prev(node_todata(curr)));
}

NUInode* nui_nextsibling(NUInode *n, NUInode *curr) {
    if (curr == NULL) return n;
    curr = node_touser(nuiL_next(node_todata(curr)));
    return curr == n ? NULL : curr;
}

NUInode* nui_root(NUInode *n) {
    NUInodedata *D, *parent;
    if (n == NULL) return NULL;
    D = node_todata(n);
    while ((parent = D->parent) != NULL)
        D = parent;
    return node_touser(D);
}

NUInode* nui_prevleaf(NUInode *n, NUInode *curr) {
    NUInodedata *D, *parent, *firstchild;
    if (curr == n) return NULL; /* end of iteration */
    if (curr == NULL) curr = n; /* first of iteration */
    D = node_todata(curr);
    if (curr != n) {
        /* return parent if D is the first child of parent. */
        if ((parent = D->parent) != NULL && parent->children == D)
            return node_touser(parent);
        /* set D to the preious sibling */
        D = nuiL_prev(D);
    }
    /* and get it's last leaf */
    while ((firstchild = D->children) != NULL)
        D = nuiL_prev(firstchild);
    return node_touser(D);
}

NUInode* nui_nextleaf(NUInode *n, NUInode *curr) {
    NUInodedata *D, *parent;
    if (curr == NULL) return n;
    /* if D has children, return the first one */
    D = node_todata(curr);
    if (D->children != NULL)
        return node_touser(D->children);
    /* otherwise, get the first parent that not the last child */
    while (node_touser(D) != n
            && (parent = D->parent) != NULL
            && parent->children == nuiL_next(D))
        D = parent;
    if (node_touser(D) == n) return NULL;
    /* return the next sibling of that node */
    return node_touser(nuiL_next(D));
}

size_t nui_childcount(NUInode *n) {
    return node_todata(n)->childcount;
#if 0
    size_t count = 1;
    NUInode *i;
    n = todata(n);
    if (n->children == NULL)
        return 0;
    nuiL_foreach(i, n->children)
        ++count;
    return count;
#endif
}

NUInode *nui_index(NUInode *n, int idx) {
    NUInodedata *D = node_todata(n);
    NUInodedata *i;
    if (D->children == NULL
            || (idx >= 0 &&  (size_t)idx > D->childcount)
            || (idx <  0 && (size_t)-idx > D->childcount ))
        return NULL;
    if (idx >= 0) {
        if (idx == 0)
            return node_touser(D->children);
        nuiL_foreach(i, D->children)
            if (--idx == 0)
                return node_touser(i);
    }
    else {
        nuiL_foreach_back(i, D->children)
            if (++idx == 0)
                return node_touser(i);
        if (idx == -1)
            return node_touser(D->children);
    }
    return NULL;
}

int nui_matchclass(NUInode *n, NUIstring *classname) {
    NUIclass *c = node_todata(n)->klass;
    while (c != NULL) {
        if (c->name == classname)
            return 1;
        c = c->parent;
    }
    return 0;
}

NUIclass *nui_nodeclass(NUInode *n)  { return node_todata(n)->klass; }
NUIstring *nui_classname(NUInode *n) { return node_todata(n)->klass->name; }

NUIpoint nui_position(NUInode *n) { return node_todata(n)->pos; }
NUIsize nui_usersize(NUInode *n)  { return node_todata(n)->size; }

int nui_isvisible(NUInode *n) { return node_todata(n)->visible; }

void nui_setnodeaction(NUInode *n, NUIaction *a)
{ n->action = a; if (a != NULL) a->n = n; }

NUIaction *nui_getnodeaction(NUInode *n)
{ return n->action; }

NUInode *nui_nodefrompos(NUInode *n, NUIpoint pos) {
    NUInode *i;
    for (i = n; i != NULL; i = nui_nextleaf(i, n)) {
        NUInodedata *Di = node_todata(i);
        if (Di->pos.x >= pos.x && Di->pos.y >= pos.y
                && Di->pos.x+Di->size.width < pos.x
                && Di->pos.y+Di->size.height < pos.y)
            break;
    }
    return i;
}

NUIpoint nui_abspos(NUInode *n) {
    NUInodedata *D = node_todata(n);
    NUIpoint pos = { 0, 0 };
    while (D != NULL) {
        pos.x += D->pos.x;
        pos.y += D->pos.y;
        D = D->parent;
    }
    return pos;
}

NUIsize nui_naturalsize(NUInode *n) {
    NUInodedata *D = node_todata(n);
    NUIsize size;
    if (D->klass->layout_naturalsize
            && D->klass->layout_naturalsize(D->klass, n, &size))
        return size;
    return D->size;
}

void nui_move(NUInode *n, NUIpoint pos) {
    NUInodedata *D = node_todata(n);
    if (D->klass->node_move
            && D->klass->node_move(D->klass, n, pos))
        D->pos = pos;
}

void nui_resize(NUInode *n, NUIsize size) {
    NUInodedata *D = node_todata(n);
    if (D->klass->node_resize
            && D->klass->node_resize(D->klass, n, size))
        D->size = size;
}

int nui_show(NUInode *n) {
    NUInodedata *D = node_todata(n);
    if (D->handle == NULL && !nui_mapnode(n)) {
        nui_assert(D->visible == 0);
        return 0;
    }
    if (!D->visible && D->klass->node_show
            && D->klass->node_show(D->klass, n))
        D->visible = 1;
    return D->visible;
}

int nui_hide(NUInode *n) {
    NUInodedata *D = node_todata(n);
    if (D->visible && D->klass->node_hide
            && D->klass->node_hide(D->klass, n))
        D->visible = 0;
    return D->visible;
}

int nui_mapnode(NUInode *n) {
    NUInodedata *D = node_todata(n);
    void *handle;
    if (D->handle == NULL
            && (handle = D->klass->map(D->klass, n)) != NULL)
        D->handle = handle;
    return D->handle != NULL;
}

int nui_unmapnode(NUInode *n) {
    NUInodedata *D = node_todata(n);
    if (D->handle != NULL
            && D->klass->unmap(D->klass, n, D->handle))
        D->handle = NULL;
    return D->handle == NULL;
}

void *nui_nativehandle(NUInode *n) {
    NUInodedata *D = node_todata(n);
    if (D->klass->get_handle)
        return D->klass->get_handle(D->klass, n, D->handle);
    return D->handle;
}

static int call_getattr(NUIstate *S, NUInodedata *n, NUItable *t, NUIstring *key, NUIvalue *pv) {
    NUIentry *entry = nui_gettable(t, key);
    if (entry != NULL) {
        NUIattr *attr = (NUIattr*)entry->value;
        if (attr->getattr != NULL)
            return attr->getattr(attr, node_touser(n), key, pv);
    }
    return 0;
}

int nui_getattr(NUInode *n, NUIstring *key, NUIvalue *pv) {
    NUInodedata *D = node_todata(n);
    NUIclass *klass = D->klass;
    NUIstate *S = klass->S;
    NUIclass *defklass = S->default_class;
    int nrets = 0;
    if ((nrets = call_getattr(S, D, &D->attrs, key, pv)) != 0)
        return nrets;
    while (klass != NULL) {
        if ((nrets = call_getattr(S, D, &klass->attrs, key, pv)) != 0)
            return nrets;
        if (klass->getattr != NULL)
            return klass->getattr(D->klass, n, key, pv);
        klass = klass->parent;
    }
    if (klass != defklass)
        return call_getattr(S, D, &defklass->attrs, key, pv);
    return 0;
}

static int call_setattr(NUIstate *S, NUInodedata *n, NUItable *t, NUIstring *key, NUIvalue *pv) {
    NUIentry *entry = nui_gettable(t, key);
    if (entry != NULL) {
        NUIattr *attr = (NUIattr*)entry->value;
        if (attr->setattr != NULL) {
            return attr->setattr(attr, node_touser(n), key, pv);
        }
    }
    return 0;
}

int nui_setattr(NUInode *n, NUIstring *key, NUIvalue v) {
    NUInodedata *D = node_todata(n);
    NUIclass *klass = D->klass;
    NUIstate *S = klass->S;
    NUIclass *defklass = S->default_class;
    int nrets = 0;
    if ((nrets = call_setattr(S, D, &D->attrs, key, &v)) != 0)
        return nrets;
    while (klass != NULL) {
        if ((nrets = call_setattr(S, D, &klass->attrs, key, &v)) != 0)
            return nrets;
        if (klass->getattr != NULL)
            return klass->setattr(D->klass, n, key, &v);
        klass = klass->parent;
    }
    if (klass != defklass)
        return call_setattr(S, D, &defklass->attrs, key, &v);
    return 0;
}

#undef NUI_PREV
#undef NUI_NEXT


/* === default class === */

static void init_default_class(NUIstate *S) {
    NUIclass *klass = S->default_class;
    (void)klass; /* XXX */
}


/* === nui class === */

static void nuiC_open(NUIstate *S) {
    S->dead_nodes = NULL;
    nui_inittable(S, &S->classes, 0);
    S->default_class = nui_newclass(S, S->predefs[NUI_IDX_default], 0);
    init_default_class(S);
}

static void nuiC_sweep(NUIstate *S) {
    while (S->dead_nodes != NULL) {
        NUInodedata *next = S->dead_nodes->next;
        node_delete(S, S->dead_nodes);
        S->dead_nodes = next;
    }
}

static void nuiC_close(NUIstate *S) {
    nuiC_sweep(S);
    nui_freetable(S, &S->classes);
    S->default_class = NULL;
}

static void class_deletor(NUIstate *S, void *p) {
    NUIclass *c = (NUIclass*)p;
    /* free all nodes */
    while (c->all_nodes != NULL) {
        NUInodedata *next = c->all_nodes->next;
        node_delete(S, c->all_nodes);
        c->all_nodes = next;
    }
    /* call deletor */
    if (c->deletor != NULL)
        c->deletor(S, c);
    /* clear resources */
    nui_freetable(S, &c->attrs);
    nuiM_freemem(S, c, c->class_size);
}

NUIclass *nui_class(NUIstate *S, NUIstring *classname) {
    NUIentry *entry;
    if (classname == NULL)
        classname = S->predefs[NUI_IDX_default];
    entry = nui_gettable(&S->classes, classname);
    return entry == NULL ? NULL : (NUIclass*)entry->value;
}

NUIclass *nui_newclass(NUIstate *S, NUIstring *classname, size_t sz) {
    NUIclass *klass;
    NUIentry *entry;
    if (classname == NULL)
        classname = S->predefs[NUI_IDX_default];
    entry = nui_gettable(&S->classes, classname);
    if (entry != NULL) return NULL;
    if (sz == 0) sz = sizeof(NUIclass);
    nui_assert(sz >= sizeof(NUIclass));

    klass = nuiM_malloc(S, sz);
    if (klass == NULL) return NULL;
    entry = nui_settable(S, &S->classes, classname);
    entry->value = (void*)klass;
    entry->deletor = class_deletor;
    klass->name = classname;
    klass->parent = NULL;
    klass->S = S;
    nui_inittable(S, &klass->attrs, 0);
    klass->all_nodes = NULL;
    klass->node_size = 0;
    klass->class_size = sz;
    klass->deletor = NULL;
    klass->new_node = NULL;
    klass->delete_node = NULL;
    klass->getattr = NULL;
    klass->setattr = NULL;
    klass->map = NULL;
    klass->unmap = NULL;
    klass->get_parent = NULL;
    klass->get_handle = NULL;
    klass->child_added = NULL;
    klass->child_removed = NULL;
    klass->node_move = NULL;
    klass->node_resize = NULL;
    klass->node_show = NULL;
    klass->node_hide = NULL;
    klass->layout_update = NULL;
    klass->layout_naturalsize = NULL;
    return klass;
}

void nui_dropclass(NUIstate *S, NUIstring *classname)
{ nui_dropentry(S, nui_gettable(&S->classes, classname)); }

static void attr_deletor(NUIstate *S, void *p) {
    NUIattr *attr = (NUIattr*)p;
    if (attr->deletor)
        attr->deletor(S, attr);
    nuiM_freemem(S, p, attr->size);
}

static void default_attrdeletor(NUIstate *S, NUIattr *attr) {
    switch (attr->value.type) {
    case NUI_TACTION: nui_dropaction(attr->value.action); return;
    case NUI_TSTRING: nui_dropstring(S, attr->value.string); return;
    case NUI_TNODE:   nui_dropnode(attr->value.node); return;
    default: return;
    }
}

static int default_getattr(NUIattr *attr, NUInode *n, NUIstring *key, NUIvalue *v) {
    *v = attr->value;
    return 1;
}

static int default_setattr(NUIattr *attr, NUInode *n, NUIstring *key, NUIvalue *v) {
    attr->value = *v;
    return 1;
}

static NUIattr *new_attr(NUIstate *S, NUItable *t, NUIstring *key, size_t sz) {
    NUIattr *attr = nuiM_malloc(S, sz);
    NUIentry *entry = nui_settable(S, t, key);
    entry->deletor = attr_deletor;
    entry->value = attr;
    attr->size = sz;
    attr->value = nui_nilvalue();
    attr->deletor = default_attrdeletor;
    attr->getattr = default_getattr;
    attr->setattr = default_setattr;
    return attr;
}

NUIattr *nui_attr(NUIclass *klass, NUIstring *key) {
    NUIentry *entry = nui_gettable(&klass->attrs, key);
    return entry == NULL ? NULL : entry->value;
}

NUIattr *nui_newattr(NUIclass *klass, NUIstring *key, size_t sz) {
    if (nui_attr(klass, key)) return NULL;
    if (sz == 0) sz = sizeof(NUIattr);
    nui_assert(sz >= sizeof(NUIattr));
    return new_attr(klass->S, &klass->attrs, key, sz);
}

void nui_dropattr(NUIclass *c, NUIstring *key)
{ nui_dropentry(c->S, nui_gettable(&c->attrs, key)); }

NUIattr *nui_nodeattr(NUInode *n, NUIstring *key) {
    NUInodedata *D = node_todata(n);
    NUIentry *entry;
    entry = nui_gettable(&D->attrs, key);
    return entry == NULL ? NULL : entry->value;
}

NUIattr *nui_newnodeattr(NUInode *n, NUIstring *key, size_t sz) {
    NUInodedata *D = node_todata(n);
    if (nui_nodeattr(n, key)) return NULL;
    if (sz == 0) sz = sizeof(NUIattr);
    nui_assert(sz >= sizeof(NUIattr));
    return new_attr(D->klass->S, &D->attrs, key, sz);
}

void nui_dropnodeattr(NUInode *n, NUIstring *key) {
    NUInodedata *D = node_todata(n);
    nui_dropentry(D->klass->S, nui_gettable(&D->attrs, key));
}


/* === nui state === */

n_noret nui_error(NUIstate *S, const char *fmt, ...) {
    char buff[NUI_MAX_PANICBUFFER_SIZE];
    va_list list;
    va_start(list, fmt);
    vsprintf(buff, fmt, list);
    va_end(list);
    if (S->params->error == NULL)
        fprintf(stderr, "nui: %s\n", buff);
    else
        S->params->error(S->params, buff);
    abort();
}

void nui_breakloop(NUIstate *S) {
    if (S->params->quit)
        S->params->quit(S->params);
}

unsigned nui_time(NUIstate *S) {
    if (S->params->time != NULL)
        return S->params->time(S->params);
    return (unsigned)clock();
}

static void params_deletor(NUIparams *params) {
    free(params);
}

NUIstate *nui_newstate(NUIparams *params) {
    NUIstate *S = (NUIstate*)malloc(sizeof(NUIstate));
    if (S == NULL) return NULL;
    S->totalmem = 0;
    if (params == NULL) {
        params = malloc(sizeof(NUIparams));
        memset(params, 0, sizeof(NUIparams));
        params->deletor = params_deletor;
    }
    S->params = params;
    S->is_closing = 0;
    nuiS_open(S); /* open string pool support */
    nuiA_open(S); /* open action support */
    nuiC_open(S); /* open class support */
    return S;
}

NUIparams *nui_stateparams(NUIstate *S) {
    return S->params;
}

void nui_close(NUIstate *S) {
    S->is_closing = 1;
    nuiC_close(S); /* close class support */
    nuiA_close(S); /* close action support */
    nuiS_close(S); /* close string pool */
    nui_assert(S->totalmem == 0);
    free(S);
}

int nui_pollevents(NUIstate *S) {
    int res = 0;
    if (S->timed_actions != NULL) {
        unsigned first_trigger_time = S->timed_actions->trigger_time;
        unsigned current_time = nui_time(S);
        if (first_trigger_time <= current_time)
            nuiA_updateactions(S);
    }

    if (S->params->poll)
        res = S->params->poll(S->params);

    nuiC_sweep(S); /* sweep dead nodes */
    nuiA_sweep(S); /* sweep dead actions */
    nuiS_sweep(S); /* sweep dead strings */ 
    nui_assert(S->stack_base == 0 && S->stack_top == 0);
    return res;
}

int nui_waitevents(NUIstate *S) {
    int res = 0;
    unsigned first_trigger_time = 0;
    unsigned current_time = 0;
    unsigned wait_time = 0;

    if (S->timed_actions != NULL)
        first_trigger_time = S->timed_actions->trigger_time;

    current_time = nui_time(S);
    if (first_trigger_time <= current_time) {
        nuiA_updateactions(S);
        if (S->timed_actions != NULL) {
            first_trigger_time = S->timed_actions->trigger_time;
            nui_assert(first_trigger_time > current_time);
            wait_time = first_trigger_time - current_time;
        }
    }

    if (S->params->wait) {
        while (wait_time > 0) {
            if ((res = S->params->wait(S->params, wait_time)) != 0)
                break;
            current_time = nui_time(S);
            if (first_trigger_time < current_time) {
                wait_time = 0;
                break;
            }
            wait_time = first_trigger_time - current_time;
        }
        if (wait_time == 0)
            nuiA_updateactions(S);
    }

    if (res == 0 && S->timed_actions != NULL)
        res = 1;

    nuiC_sweep(S); /* sweep dead nodes */
    nuiA_sweep(S); /* sweep dead actions */
    nuiS_sweep(S); /* sweep dead strings */ 
    nui_assert(S->stack_base == 0 && S->stack_top == 0);
    return res;
}

int nui_loop(NUIstate *S) {
    int res;
    while ((res = nui_waitevents(S)) > 0)
        ;
    return res;
}


/* cc: flags+='-g -shared -DLUA_BUILD_AS_DLL -DNUI_DLL'
 * cc: libs+='-llua52.dll' run='lua test.lua'
 * cc: input+='nui_lua.c' output="nui.dll" */
