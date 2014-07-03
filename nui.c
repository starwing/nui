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
    (x)->NUI_NEXT = (h)->NUI_NEXT,                                            \
    (x)->NUI_NEXT->NUI_PREV = (x),                                            \
    (x)->NUI_PREV = (h),                                                      \
    (h)->NUI_NEXT = (x)))

#define nuiL_insert_pointer(p, x) ((void)(                                    \
    (p) != NULL ? nuiL_insert(p, x) :                                         \
    ( (p) = (x), nuiL_init(x) )))

#define nuiL_append(h, x) ((void)(                                            \
    (x)->NUI_PREV = (h)->NUI_PREV,                                            \
    (x)->NUI_PREV->NUI_NEXT = (x),                                            \
    (x)->NUI_NEXT = (h),                                                      \
    (h)->NUI_PREV = (x)))

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


/* === nui half list === */

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

typedef struct NUIstringtable {
    NUIstring **hash;
    NUIstring *gclist;
    size_t nuse;
    size_t size;
} NUIstringtable;

struct NUIstring {
    NUIstring *next;
    NUIstring *nextgc;
    size_t len;
    unsigned hash;
    int refcount;
};

struct NUIaction {
    NUIaction *next;
    NUIaction **pprev; /* point to the `next` pointer of prev actions */
    NUIaction *next_linked;
    NUIaction *prev_linked;

    size_t size;
    NUIactionf *f;
    NUIdeletor *deletor;

    NUIstate *S;
    NUInode *n;

    unsigned call_count;
    unsigned last_trigger_time;
    unsigned trigger_time;
    unsigned interval;
};

struct NUInode {
    /* node structure */
    NUInode *next; /* next node in same class */
    NUInode *parent; /* parent of this node */
    NUInode *prev_sibling; /* previous sibling of this node */
    NUInode *next_sibling; /* next sibling of this node */
    NUInode *children; /* first child of this node */
    size_t childcount;

    /* natual attributes */
    NUIclass *klass;
    NUItable attrs; /* attributes table */
    NUIpoint pos;  /* upper-left corner relative to native parent. */
    NUIsize size; /* user defined-size for this node */
    int visible;
    int flags; /* flags of this node */
    void *handle; /* native handle for this node */
    NUIstring *id;
    NUIaction *action; /* default trigger action */
    /*NUIlayoutparams *layout_params; [> layout params of this node <]*/
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
    NUIaction *actions;
    NUIaction *dead_actions;
    NUIaction *timed_actions;

    /* action callback stack */
    NUIvalue *stack;
    size_t stack_base;
    size_t stack_top;
    size_t stack_size;

    /* class/node */
    NUItable classes;
    NUIclass *default_class;
    NUInode *dead_nodes;
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
#define todata(b) nui_checkexp(b != NULL, (b)-1)
#define touser(b) nui_checkexp(b != NULL, (b)+1)

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
    int found = 0;
    NUIstringtable *tb = &S->strt;
    unsigned h = nui_lmod(s->hash, tb->size);
    NUIstring *list = tb->hash[h];
    if (list == s) {
        tb->hash[h] = s->next;
        found = 1;
    }
    else {
        for (; list != NULL; list = list->next) {
            if (list->next == s) {
                list->next = s->next;
                found = 1;
                break;
            }
        }
    }
    if (found) {
        size_t totalsize = sizeof(NUIstring) +
            ((s->len + 1) * sizeof(char));
        nuiM_freemem(S, s, totalsize);
    }
    --tb->nuse;
}

static void markstring(NUIstate *S, NUIstring *s) {
    NUIstringtable *tb = &S->strt;
    s->nextgc = tb->gclist;
    tb->gclist = s->nextgc;
}

static void unmarkstring(NUIstate *S, NUIstring *s) {
    NUIstringtable *tb = &S->strt;
    NUIstring *o = tb->gclist;
    if (o == s) {
        tb->gclist = s->nextgc;
        s->nextgc = NULL;
    }
    for (; o != NULL; o = o->nextgc) {
        if (o->nextgc == s) {
            o->nextgc = s->nextgc;
            s->nextgc = NULL;
            break;
        }
    }
    if (s->nextgc == NULL) {
        NUIstring **list = &tb->hash[nui_lmod(s->hash, tb->size)];
        s->next = *list;
        *list = s->next;
    }
}

static void nuiS_resize(NUIstate *S, size_t newsize) {
    int i;
    NUIstringtable *tb = &S->strt;
    size_t realsize = NUI_MIN_STRTABLE_SIZE;
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
                (memcmp(s, (char*)touser(o), (len+1)*sizeof(char)) == 0))
            return touser(o);
    }
    return touser(newstring(S, s, len, hash));
}

int nui_holdstring (NUIstate *S, NUIstring *s) {
    s = todata(s);
    if (++s->refcount > 0 && s->nextgc != NULL)
        unmarkstring(S, s);
    return s->refcount;
}

int nui_dropstring (NUIstate *S, NUIstring *s) {
    s = todata(s);
    if (--s->refcount <= 0 && s->nextgc == NULL)
        markstring(S, s);
    return s->refcount;
}

unsigned nui_calchash (NUIstate *S, const char *s, size_t len) {
    return calc_hash(s, len, S->seed);
}

size_t      nui_strlen  (NUIstring *s) { return todata(s)->len;  }
unsigned    nui_strhash (NUIstring *s) { return todata(s)->hash; }

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
    int i;
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

    while ((1<<lsize) < NUI_MAX_SIZET/2 && (1<<lsize) < size)
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
            nui_deltable(S, &t->node[i]);
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

void nui_deltable(NUIstate *S, NUIentry *n) {
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
    if (S->stack_top - S->stack_base < nargs)
        nargs = S->stack_top - S->stack_base;
    S->stack_base = S->stack_top - nargs;
    S->stack_top  = S->stack_base + nargs;
    S->stack_base = S->stack_top;
    return curr_base;
}

static int stack_popframe(NUIstate *S, size_t base, int nrets) {
    if (nrets < S->stack_top-S->stack_base)
        nui_settop(S, nrets);
    if (S->stack_top - nrets > S->stack_base) {
        int i;
        for (i = 0; i < nrets; ++i)
            S->stack[S->stack_base+i] = S->stack[S->stack_top-nrets+i];
    }
    S->stack_top = S->stack_base + nrets;
    S->stack_base = base;
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

NUIaction *nui_action(NUIstate *S, NUIactionf *f, size_t sz) {
    NUIaction *a;
    if (S->is_closing)
        return NULL;
    a = (NUIaction*)nuiM_malloc(S, sizeof(NUIaction) + sz);
    nuiHL_init(S->actions, a);
    nuiL_init(a);
    a->size = sz;
    a->f = f;
    a->deletor = NULL;
    a->S = S;
    a->n = NULL;
    a->call_count = 0;
    a->last_trigger_time = 0;
    a->trigger_time = 0;
    a->interval = 0;
    return touser(a);
}

NUIaction *nui_copyaction(NUIaction *a) {
    NUIaction *copied;
    a = todata(a);
    copied = nui_action(a->S, a->f, a->size);
    if (copied == NULL) return NULL;
    copied->deletor = a->deletor;
    return touser(copied);
}

NUI_API NUIaction *nui_namedaction(NUIstate *S, NUIstring *name) {
    NUIentry *entry = nui_gettable(&S->named_actions, name);
    if (entry != NULL)
        return nui_copyaction((NUIaction*)entry->value);
    return NULL;
}

NUIaction *nui_newnamedaction(NUIstate *S, NUIstring *name, NUIactionf *f, size_t sz) {
    NUIentry *entry = nui_gettable(&S->named_actions, name);
    if (entry == NULL) {
        NUIaction *a = nui_action(S, f, sz);
        entry->value = a;
        entry->deletor = (NUIdeletor*)nui_dropaction;
    }
    return (NUIaction*)entry->value;
}

void nui_dropaction(NUIaction *a) {
    a = todata(a);
    nuiHL_insert(a->S->dead_actions, a);
}

size_t nui_actionsize(NUIaction *a) {
    return todata(a)->size;
}

void nui_setactionf(NUIaction *a, NUIactionf *f) { todata(a)->f = f; }
NUIactionf *nui_getactionf(NUIaction *a) { return todata(a)->f; }
void nui_setactionnode(NUIaction *a, NUInode *n) { todata(a)->n = n; }
NUInode *nui_getactionnode(NUIaction *a) { return todata(a)->n; }
void nui_setactiondeletor(NUIaction *a, NUIdeletor *f) { todata(a)->deletor = f; }
NUIdeletor *nui_getactiondeletor(NUIaction *a) { return todata(a)->deletor; }

void nui_actiondelayed(NUIaction *a, unsigned delayed) {
    a = todata(a);
    a->trigger_time = nui_time(a->S) + delayed;
}

void nui_linkaction(NUIaction *a, NUIaction *na) {
    a = todata(a);
    na = todata(na);
    nuiL_remove(a);
    nuiL_insert(a, na);
}

void nui_unlinkaction(NUIaction *a) {
    a = todata(a);
    nuiL_remove_safe(a);
}

NUIaction *nui_nextaction(NUIaction *a, NUIaction *curr) {
    a = todata(a);
    curr = (curr == NULL) ? a : todata(curr);
    if (nuiL_next(curr) == a)
        return NULL;
    return touser(nuiL_next(curr));
}

void nui_emitaction(NUIaction *a, int nargs) {
    NUIstate *S;
    NUIaction *i, *next;
    NUInode *n;
    size_t base;
    a = todata(a);
    S = a->S;
    n = a->n;

    if (a->f != NULL) {
        if (!nuiL_empty(a))
            nui_copyvalues(S, nargs);
        base = stack_newframe(S, nargs);
        a->f(S, touser(a), n);
        stack_popframe(S, base, 0);
    }
    nuiL_foreach_safe(i, next, a) {
        if (i->f != NULL) {
            nui_copyvalues(S, nargs);
            base = stack_newframe(S, nargs);
            i->f(S, touser(i), n);
            stack_popframe(S, base, 0);
        }
    }
}

void nui_starttimer(NUIaction *a, unsigned delayed, unsigned interval) {
    NUIaction **list;
    a = todata(a);
    if (a->S->is_closing) return;
    a->trigger_time = nui_time(a->S) + delayed;
    a->interval = interval;
    list = &a->S->timed_actions;
    /* insert a into S->timed_actions */
    while (*list != NULL) {
        if (a->trigger_time < (*list)->trigger_time)
            break;
        list = &(*list)->next;
    }
    nuiHL_insert(*list, a);
}

void nui_stoptimer(NUIaction *a) {
    a = todata(a);
    if (!a->S->is_closing)
        nuiHL_insert(a->S->actions, a);
    a->interval = 0;
    a->trigger_time = 0;
}

static void action_sweeplist(NUIstate *S, NUIaction **list) {
    while (*list != NULL) {
        NUIaction *a = *list;
        size_t totalsize = sizeof(NUIaction) + a->size;
        if (a->deletor)
            a->deletor(S, touser(a));
        nuiL_remove(a);
        nuiHL_remove(a);
        nuiM_freemem(S, a, totalsize);
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
    NUIaction *a;
    if ((a = S->timed_actions) != NULL) {
        unsigned time = nui_time(S);
        S->timed_actions = NULL;
        while (a != NULL && a->trigger_time <= time) {
            NUIaction *next = a->next;
            nui_pushvalue(S, nui_integervalue(time - a->last_trigger_time));
            nui_pushvalue(S, nui_integervalue(a->call_count));
            nui_emitaction(touser(a), 2);
            if (a->next == next) { /* a is still in list? */
                if (a->interval != 0)
                    nui_starttimer(touser(a), a->interval, a->interval);
                else
                    nui_stoptimer(touser(a));
            }
            a->last_trigger_time = a->trigger_time;
            ++a->call_count;
            a = next;
        }
    }
}

#undef NUI_PREV
#undef NUI_NEXT


/* === nui node === */

#define NUI_PREV prev_sibling
#define NUI_NEXT next_sibling

static void node_setparent(NUInode *n, NUInode *parent) {
    if (n->parent && n->parent != parent) {
        if (n->klass->child_removed)
            n->klass->child_removed(n->klass, touser(n->parent), touser(n));
        n->parent = NULL;
    }
    if (parent) {
        if (n->klass->child_added)
            n->klass->child_added(n->klass, touser(parent), touser(n));
        n->parent = parent;
    }
}

static void node_delete(NUIstate *S, NUInode *n) {
    size_t totalsize = sizeof(NUInode) + n->klass->node_size;
    node_setparent(n, NULL);
    nuiL_remove_safe(n);
    if (n->klass->delete_node)
        n->klass->delete_node(n->klass, touser(n));
    nui_freetable(n->klass->S, &n->attrs);
    nuiM_freemem(S, n, totalsize);
}

NUInode *nui_node(NUIstate *S, NUIstring *classname) {
    NUInode *n;
    NUIentry *entry = nui_gettable(&S->classes, classname);
    NUIclass *klass = entry ?
        (NUIclass*)entry->value : S->default_class;
    size_t totalsize = sizeof(NUInode) + klass->node_size;

    n = (NUInode*)nuiM_malloc(S, totalsize);
    n->next = klass->all_nodes;
    klass->all_nodes = n;
    n->parent = NULL;
    nuiL_init(n);
    n->children = NULL;
    n->childcount = 0;

    n->klass = klass;
    nui_inittable(S, &n->attrs, 0);
    n->pos.x = n->pos.y = 0;
    n->size.width = n->size.height = 0;
    n->visible = 0;
    n->flags = 0;
    n->handle = NULL;
    n->id = NULL;
    n->action = NULL;
    /*n->layout_params = NULL;*/

    if (n->klass->new_node
            && !n->klass->new_node(n->klass, touser(n))) {
        nuiM_freemem(S, n, totalsize);
        return NULL;
    }
    return touser(n);
}

void nui_dropnode(NUInode *n) {
    NUInode **list;
    n = todata(n);
    list = &n->klass->all_nodes;
    while (*list != NULL) {
        if (*list == n) {
            *list = n->next;
            break;
        }
        list = &n->next;
    }
    n->next = n->klass->S->dead_nodes;
    n->klass->S->dead_nodes = n;
}

void nui_setchildren(NUInode *n, NUInode *newnode) {
    NUInode *i;
    size_t childcount = 0;
    n = todata(n);
    newnode = todata(newnode);
    if (n->children != NULL) {
        node_setparent(n->children, NULL);
        nuiL_foreach (i, n->children)
            node_setparent(i, NULL);
    }
    if (newnode != NULL) {
        childcount = 1;
        node_setparent(newnode, n);
        nuiL_foreach (i, newnode) {
            node_setparent(i, n);
            ++childcount;
        }
        if (newnode->parent) {
            newnode->parent->children = NULL;
            newnode->parent->childcount = 0;
        }
    }
    n->children = newnode;
    n->childcount = childcount;
}

void nui_append(NUInode *n, NUInode *newnode) {
    n = todata(n);
    newnode = todata(newnode);
    node_setparent(newnode, n->parent);
    nuiL_remove(newnode);
    nuiL_append(n, newnode);
    if (n->parent)
        ++n->parent->childcount;
}

void nui_insert(NUInode *n, NUInode *newnode) {
    n = todata(n);
    newnode = todata(newnode);
    node_setparent(newnode, n->parent);
    nuiL_remove(newnode);
    nuiL_insert(n, newnode);
    if (n->parent && n->parent->children == n)
        n->parent->children = newnode;
    if (n->parent)
        ++n->parent->childcount;
}

void nui_detach(NUInode *n) {
    NUInode *next;
    n = todata(n);
    node_setparent(n, NULL);
    next = nuiL_empty(n) ? NULL : nuiL_next(n);
    nuiL_remove_safe(n);
    if (n->parent) {
        if (n->parent->children == n)
            n->parent->children = next;
        --n->parent->childcount;
    }
}

NUInode* nui_parent(NUInode *n) {
    n = todata(n);
    if (n->klass->get_parent)
        return n->klass->get_parent(n->klass, touser(n), touser(n->parent));
    return touser(n->parent);
}

void nui_setparent(NUInode *n, NUInode *parent) {
    if (parent == NULL) return nui_detach(n);
    n = todata(n);
    parent = todata(parent);
    node_setparent(n, parent);
    nuiL_remove(n);
    if (parent->children)
        nuiL_insert(parent->children, n);
    else {
        nuiL_init(n);
        parent->children = n;
    }
    ++parent->childcount;
}

NUInode* nui_firstchild(NUInode *n) {
    return touser(todata(n)->children);
}

NUInode* nui_lastchild(NUInode *n) {
    n = todata(n);
    return !n->children ? NULL : touser(nuiL_prev(n->children));
}

NUInode* nui_prevsibling(NUInode *n) {
    n = todata(n);
    if (n->parent != NULL && n == n->parent->children)
        return NULL;
    return nuiL_empty(n) ? NULL : touser(nuiL_prev(n));
}

NUInode* nui_nextsibling(NUInode *n) {
    n = todata(n);
    if (n->parent != NULL && n == nuiL_prev(n->parent->children))
        return NULL;
    return nuiL_empty(n) ? NULL : touser(nuiL_next(n));
}

NUInode* nui_root(NUInode *n) {
    NUInode *parent;
    n = todata(n);
    while ((parent = n->parent) != NULL)
        n = parent;
    return touser(n);
}

NUInode* nui_firstleaf(NUInode *root) {
    return root;
}

NUInode* nui_lastleaf(NUInode *root) {
    NUInode *firstchild;
    root = todata(root);
    while ((firstchild = root->children) != NULL)
        root = nuiL_prev(firstchild);
    return touser(root);
}

NUInode* nui_prevleaf(NUInode *n) {
    NUInode *parent, *firstchild;
    n = todata(n);
    if ((parent = n->parent) != NULL
            && parent->children == n) /* first child */
        return parent;
    if (nuiL_empty(n))
        return NULL;
    n = nuiL_prev(n);
    while ((firstchild = n->children) != NULL)
        n = nuiL_prev(firstchild);
    return touser(n);
}

NUInode* nui_nextleaf(NUInode *n) {
    NUInode *parent;
    n = todata(n);
    if (n->children)
        return n->children;
    while ((parent = n->parent) != NULL
            && parent->children == nuiL_next(n))
        n = parent;
    return nuiL_empty(n) ? NULL : touser(nuiL_next(n));
}

size_t nui_childcount(NUInode *n) {
    return todata(n)->childcount;
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
    NUInode *i;
    n = todata(n);
    if (n->children == NULL
            || (idx >= 0 &&  idx > n->childcount)
            || (idx <  0 && -idx > n->childcount ))
        return NULL;
    if (idx >= 0) {
        if (idx == 0)
            return touser(n->children);
        nuiL_foreach(i, n->children)
            if (--idx == 0)
                return touser(i);
    }
    else {
        nuiL_foreach_back(i, n->children)
            if (++idx == 0)
                return touser(i);
        if (idx == -1)
            return touser(n->children);
    }
    return NULL;
}

int nui_matchclass(NUInode *n, NUIstring *classname) {
    NUIclass *c = n->klass;
    while (c != NULL) {
        if (c->name == classname)
            return 1;
        c = c->parent;
    }
    return 0;
}

NUIclass *nui_nodeclass(NUInode *n)
{ return todata(n)->klass; }

NUIstring *nui_classname(NUInode *n)
{ return todata(n)->klass->name; }

NUIpoint nui_position(NUInode *n)
{ return todata(n)->pos; }

NUIsize nui_size(NUInode *n)
{ return todata(n)->size; }

int nui_isvisible(NUInode *n)
{ return todata(n)->visible; }

void nui_setid(NUInode *n, NUIstring *id)
{ todata(n)->id = id; }

NUIstring *nui_getid(NUInode *n)
{ return todata(n)->id; }

void nui_setaction(NUInode *n, NUIaction *a)
{ todata(n)->action = a; }

NUIaction *nui_getaction(NUInode *n)
{ return todata(n)->action; }

NUInode *nui_nodefrompos(NUInode *n, NUIpoint pos) {
    NUInode *parent;
    NUInode *i;
    n = todata(n);
    parent = n->parent;
    n->parent = NULL; /* nui_nextleaf will check whole tree. */
    for (i = n; i != NULL; i = nui_nextleaf(touser(i))) {
        if (i->pos.x >= pos.x && i->pos.y >= pos.y
                && i->pos.x+i->size.width < pos.x
                && i->pos.y+i->size.height < pos.y)
            break;
    }
    n->parent = parent;
    return i == NULL ? NULL : touser(i);
}

NUIpoint nui_abspos(NUInode *n) {
    NUIpoint pos = { 0, 0 };
    n = todata(n);
    while (n != NULL) {
        pos.x += n->pos.x;
        pos.y += n->pos.y;
        n = n->parent;
    }
    return pos;
}

NUIsize nui_naturalsize(NUInode *n) {
    NUIsize size;
    n = todata(n);
    if (n->klass->layout_naturalsize
            && n->klass->layout_naturalsize(n->klass, touser(n), &size))
        return size;
    return n->size;
}

void nui_move(NUInode *n, NUIpoint pos) {
    n = todata(n);
    if (n->klass->node_move
            && n->klass->node_move(n->klass, touser(n), pos))
        n->pos = pos;
}

void nui_resize(NUInode *n, NUIsize size) {
    n = todata(n);
    if (n->klass->node_resize
            && n->klass->node_resize(n->klass, touser(n), size))
        n->size = size;
}

int nui_show(NUInode *n) {
    n = todata(n);
    if (n->handle == NULL && !nui_mapnode(touser(n))) {
        nui_assert(n->visible == 0);
        return 0;
    }
    if (!n->visible && n->klass->node_show
            && n->klass->node_show(n->klass, touser(n)))
        n->visible = 1;
    return n->visible;
}

int nui_hide(NUInode *n) {
    n = todata(n);
    if (n->visible && n->klass->node_hide
            && n->klass->node_hide(n->klass, touser(n)))
        n->visible = 0;
    return n->visible;
}

int nui_mapnode(NUInode *n) {
    void *handle;
    n = todata(n);
    if (n->handle == NULL
            && (handle = n->klass->map(n->klass, touser(n))) != NULL)
        n->handle = handle;
    return n->handle != NULL;
}

int nui_unmapnode(NUInode *n) {
    n = todata(n);
    if (n->handle != NULL
            && n->klass->unmap(n->klass, touser(n), n->handle))
        n->handle = NULL;
    return n->handle == NULL;
}

void *nui_gethandle(NUInode *n) {
    n = todata(n);
    if (n->klass->get_handle)
        return n->klass->get_handle(n->klass, touser(n), n->handle);
    return n->handle;
}

static int call_getattr(NUIstate *S, NUInode *n, NUItable *t, NUIstring *key, NUIvalue *pv) {
    NUIentry *entry = nui_gettable(t, key);
    if (entry != NULL) {
        NUIattr *attr = (NUIattr*)entry->value;
        if (attr->getattr != NULL)
            return attr->getattr(attr, touser(n), key, pv);
    }
    return 0;
}

int nui_getattr(NUInode *n, NUIstring *key, NUIvalue *pv) {
    NUIstate *S = n->klass->S;
    NUIclass *klass = n->klass;
    NUIclass *defklass = klass->S->default_class;
    int nrets = 0;
    n = todata(n);
    if ((nrets = call_getattr(S, n, &n->attrs, key, pv)) != 0)
        return nrets;
    while (klass != NULL) {
        if ((nrets = call_getattr(S, n, &klass->attrs, key, pv)) != 0)
            return nrets;
        if (klass->getattr != NULL)
            return klass->getattr(n->klass, touser(n), key, pv);
        klass = klass->parent;
    }
    if (klass != defklass)
        return call_getattr(S, n, &defklass->attrs, key, pv);
    return 0;
}

static int call_setattr(NUIstate *S, NUInode *n, NUItable *t, NUIstring *key, NUIvalue *pv) {
    NUIentry *entry = nui_gettable(t, key);
    if (entry != NULL) {
        NUIattr *attr = (NUIattr*)entry->value;
        if (attr->setattr != NULL) {
            return attr->setattr(attr, touser(n), key, pv);
        }
    }
    return 0;
}

int nui_setattr(NUInode *n, NUIstring *key, NUIvalue v) {
    NUIstate *S = n->klass->S;
    NUIclass *klass = n->klass;
    NUIclass *defklass = klass->S->default_class;
    int nrets = 0;
    n = todata(n);
    if ((nrets = call_setattr(S, n, &n->attrs, key, &v)) != 0)
        return nrets;
    while (klass != NULL) {
        if ((nrets = call_setattr(S, n, &klass->attrs, key, &v)) != 0)
            return nrets;
        if (klass->getattr != NULL)
            return klass->setattr(n->klass, touser(n), key, &v);
        klass = klass->parent;
    }
    if (klass != defklass)
        return call_setattr(S, n, &defklass->attrs, key, &v);
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
        NUInode *next = S->dead_nodes->next;
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
        NUInode *next = c->all_nodes->next;
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

static void attrib_deletor(NUIstate *S, void *p) {
    NUIattr *attr = (NUIattr*)p;
    if (attr->deletor)
        attr->deletor(attr);
    nuiM_freemem(S, p, attr->size);
}

NUIclass *nui_newclass(NUIstate *S, NUIstring *classname, size_t sz) {
    NUIentry *entry;
    NUIclass *klass;
    if (classname == NULL)
        classname = S->predefs[NUI_IDX_default];
    entry = nui_gettable(&S->classes, classname);
    if (entry != NULL)
        return (NUIclass*)entry->value;
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

NUIattr *nui_newattr(NUIclass *klass, NUIstring *key, size_t sz) {
    NUIattr *attr;
    NUIentry *entry;
    entry = nui_gettable(&klass->attrs, key);
    if (entry != NULL)
        return entry->value;
    if (sz == 0)
        sz = sizeof(NUIattr);
    nui_assert(sz >= sizeof(NUIattr));
    attr = nuiM_malloc(klass->S, sz);
    entry = nui_settable(klass->S, &klass->attrs, key);
    entry->deletor = attrib_deletor;
    entry->value = attr;
    attr->size = sz;
    attr->deletor = NULL;
    attr->getattr = NULL;
    attr->setattr = NULL;
    return attr;
}

void nui_delattr(NUIclass *c, NUIstring *key) {
    NUIentry *entry = nui_gettable(&c->attrs, key);
    if (entry != NULL)
        nui_deltable(c->S, entry);
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


/* cc: flags+='-s -O3 -mdll -DNUI_DLL -Wl,--output-def,nui.def'
 * cc: output='nui.dll' run='nui_test' */
