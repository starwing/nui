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

typedef enum NUIpredefstr {
#define X(name) NUI_IDX_##name,
    NUI_PREDEFINED_STRING(X)
#undef  X
    NUI_PREDEF_COUNT
} NUIpredefstr;


/* === nui limits === */

/* minimum size for the string table (must be power of 2) */
#if !defined(NUI_MINSTRTABSIZE)
# define NUI_MINSTRTABSIZE       32
#endif

/* mininum size for hash table (must be power of 2) */
#if !defined(NUI_MINTABLELSIZE)
# define NUI_MINTABLELSIZE        2
#endif

/* size of panic message buffer */
#if !defined(NUI_PANICBUFFSIZE)
# define NUI_PANICBUFFSIZE       512
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

#ifndef offsetof
# define offsetof(s, f)  ((size_t)&(((s*)0)->f))
#endif /* offsetof */

#define nuiL_data(q, type, field) \
    ((type*)((unsigned char*)(q) - offsetof(type, field)))

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

#define nuiL_foreach_safe(i, nexti, h)                                        \
    for ((i) = (h)->NUI_NEXT, (nexti) = (i)->NUI_NEXT;                        \
         (i) != (h);                                                          \
         (i) = (nexti), (nexti) = (nexti)->NUI_NEXT)


/* === nui structures === */

typedef void NUIdeletor(NUIstate *S, void *p);

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
    NUIaction *next_linked;
    NUIaction *prev_linked;

    NUIactionf *f;
    NUIactionf *deletor;
    NUInode *n;
    unsigned trigger_time;
    unsigned interval;
    size_t extra_size;
};

struct NUInode {
    /* node structure */
    NUInode *next; /* next node in same class */
    NUInode *parent; /* parent of this node */
    NUInode *prev_sibling; /* previous sibling of this node */
    NUInode *next_sibling; /* next sibling of this node */
    NUInode *children; /* first child of this node */

    /* node class & attributes */
    NUIclass *klass;
    NUItable table; /* attributes table */

    /* position and size of node */
    int visible;
    NUIpoint pos;  /* upper-left corner relative to native parent. */
    NUIsize size; /* user defined-size for this node */
    /*NUIlayoutparams *layout_params; [> layout params of this node <]*/

    /* natual attributes */
    int id; /* serial number used for controls that need a numeric id,
               initialized with -1 */
    int flags; /* flags of this node */
    void *handle; /* native handle for this node */
    NUIaction *action; /* default trigger action */
};

struct NUIstate {
    NUIparams *params;

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
    NUIaction *sched_actions;
    NUIaction *timed_actions;

    /* class/node */
    NUItable classes;
    NUIclass *default_class;
    NUInode *dead_nodes;
};


/* === nui memory === */

/* 
 * user can use NUIstring and NUIaction directly for a memory, so we
 * return (b)+1 (the actually memory pointer) to user, when user call
 * nui functions, we should get actually structure pointer from user's
 * pointer.
 */
#define nuiM_fromuser(b) ((b)-1)
#define nuiM_touser(b)   ((b)+1)

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
    size_t realsize = NUI_MINSTRTABSIZE;
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
                (memcmp(s, (char*)(o+1), (len+1)*sizeof(char)) == 0))
            return nuiM_touser(o);
    }
    return nuiM_touser(newstring(S, s, len, hash));
}

int nui_holdstring (NUIstate *S, NUIstring *s) {
    s = nuiM_fromuser(s);
    if (++s->refcount > 0 && s->nextgc != NULL)
        unmarkstring(S, s);
    return s->refcount;
}

int nui_dropstring (NUIstate *S, NUIstring *s) {
    s = nuiM_fromuser(s);
    if (--s->refcount <= 0 && s->nextgc == NULL)
        markstring(S, s);
    return s->refcount;
}

unsigned nui_calchash (NUIstate *S, const char *s, size_t len) {
    return calc_hash(s, len, S->seed);
}

size_t      nui_len  (NUIstring *s) { return nuiM_fromuser(s)->len;  }
unsigned    nui_hash (NUIstring *s) { return nuiM_fromuser(s)->hash; }

static void nuiS_open(NUIstate *S) {
    NUIstringtable *tb = &S->strt;
    tb->gclist = NULL;
    tb->hash = NULL;
    tb->nuse = 0;
    tb->size = 0;
    nuiS_resize(S, NUI_MINSTRTABSIZE);
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
    mp = &t->node[nui_lmod(key->hash, nui_tsize(t))];
    if (mp == NULL || mp->key != NULL) { /* main position is taken? */
        NUIentry *othern;
        NUIentry *n = getfreepos(t); /* get a free place */
        if (n == NULL) { /* can not find a free place? */
            nui_resizetable(S, t, nui_counttable(t)+1); /* grow table */
            return nui_settable(S, t, nuiM_touser(key));
        }
        othern = &t->node[nui_lmod(mp->key->hash, nui_tsize(t))];
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
    mp->key = nuiM_touser(key);
    mp->value = NULL;
    return mp;
}

void nui_inittable(NUIstate *S, NUItable *t, size_t size) {
    size_t i, lsize = NUI_MINTABLELSIZE;
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
    unsigned hash;
    if (key == NULL) return NULL;
    hash = nuiM_fromuser(key)->hash;
    n = &t->node[nui_lmod(hash, nui_tsize(t))];
    for (; n != NULL; n = n->next)
        if (n->key == key)
            return n;
    return NULL;
}

NUIentry *nui_settable(NUIstate *S, NUItable *t, NUIstring *key) {
    NUIentry *n = nui_gettable(t, key);
    if (n != NULL) return n;
    if (key == NULL) return NULL;
    return getnewkeynode(S, t, nuiM_fromuser(key));
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


/* === nui action === */

#define NUI_PREV prev_linked
#define NUI_NEXT next_linked

NUIaction *nui_action(NUIstate *S, NUIactionf *f, size_t sz) {
    NUIaction *a = (NUIaction*)nuiM_malloc(S, sizeof(NUIaction) + sz);
    nuiL_insert_pointer(S->actions, a);
    a->f = f;
    a->deletor = NULL;
    a->n = NULL;
    a->trigger_time = 0;
    a->interval = 0;
    a->extra_size = sz;
    return nuiM_touser(a);
}

NUIaction *nui_copyaction(NUIstate *S, NUIaction *a) {
    NUIaction *copied;
    a = nuiM_fromuser(a);
    copied = nui_action(S, a->f, a->extra_size);
    copied->deletor = a->deletor;
    return nuiM_touser(copied);
}

NUI_API NUIaction *nui_namedaction(NUIstate *S, NUIstring *name) {
    NUIentry *entry = nui_gettable(&S->named_actions, name);
    if (entry != NULL)
        return nui_copyaction(S, nuiM_touser((NUIaction*)entry->value));
    return NULL;
}

NUIaction *nui_newnamedaction(NUIstate *S, NUIstring *name, NUIactionf *f, size_t sz) {
    NUIentry *entry = nui_gettable(&S->named_actions, name);
    if (entry == NULL) {
        NUIaction *a = nui_action(S, f, sz);
        entry->value = nuiM_fromuser(a);
        entry->deletor = (NUIdeletor*)nui_dropaction;
    }
    return nuiM_touser((NUIaction*)entry->value);
}

void nui_dropaction(NUIstate *S, NUIaction *a) {
    a = nuiM_fromuser(a);
    nuiL_remove(a);
    nuiL_insert_pointer(S->dead_actions, a);
}

size_t nui_actionbuffsize(NUIaction *a) {
    a = nuiM_fromuser(a);
    return a->extra_size;
}

void nui_linkaction(NUIaction *a, NUIaction *na) {
    a = nuiM_fromuser(a);
    na = nuiM_fromuser(na);
    nuiL_insert(a, na);
}

void nui_unlinkaction(NUIaction *a) {
    a = nuiM_fromuser(a);
    nuiL_remove_safe(a);
}

NUIaction *nui_nextaction(NUIaction *a, NUIaction *curr) {
    a = nuiM_fromuser(a);
    if (curr == NULL)
        curr = a;
    else
        curr = nuiM_fromuser(curr);
    if (nuiL_next(curr) == a)
        return NULL;
    return nuiM_touser(nuiL_next(curr));
}

void nui_setactionf(NUIaction *a, NUIactionf *f) {
    nuiM_fromuser(a)->f = f;
}

void nui_emitaction(NUIstate *S, NUIaction *a, NUInode *n, void **params) {
    a = nuiM_fromuser(a);
    if (a->f != NULL)
        a->f(S, a, n, params);
}

void nui_schedaction(NUIstate *S, NUIaction *a, NUInode *n) {
    a = nuiM_fromuser(a);
    nuiL_remove(a);
    nuiL_insert_pointer(S->sched_actions, a);
    a->n = n;
}

void nui_starttimer(NUIstate *S, NUIaction *a, NUInode *n, unsigned delayed, unsigned interval) {
    NUIaction *curr = NULL;

    a = nuiM_fromuser(a);
    a->trigger_time = nui_time(S) + delayed;
    a->interval = interval;
    a->n = n;

    /* insert a into S->timed_actions */
    nuiL_remove(a);
    if (S->timed_actions == NULL) {
        S->timed_actions = a;
        nuiL_init(a);
        return;
    }

    if (a->trigger_time < S->timed_actions->trigger_time) {
        nuiL_insert(S->timed_actions, a);
        S->timed_actions = a;
        return;
    }

    if (a->trigger_time > nuiL_prev(S->timed_actions)->trigger_time) {
        nuiL_insert(S->timed_actions, a);
        return;
    }

    while ((curr = nui_nextaction(S->timed_actions, curr)) != NULL) {
        if (a->trigger_time < curr->trigger_time) {
            nuiL_insert(curr, a);
            return;
        }
    }

    /* can not reach here */
    nui_canceltimer(S, nuiM_touser(a));
    nui_assert(0);
}

void nui_canceltimer(NUIstate *S, NUIaction *a) {
    a = nuiM_fromuser(a);
    nuiL_remove_safe(a);
    if (S->timed_actions == a)
        S->timed_actions = NULL;
    a->interval = 0;
    a->trigger_time = 0;
}

static void nuiA_sweep(NUIstate *S) {
    while (S->dead_actions)
        nui_dropaction(S, S->dead_actions);
}

static void nuiA_open(NUIstate *S) {
    nui_inittable(S, &S->named_actions, 0);
    S->actions = NULL;
    S->timed_actions = NULL;
    S->sched_actions = NULL;
    S->dead_actions = NULL;
}

static void nuiA_close(NUIstate *S) {
    nui_freetable(S, &S->named_actions);
    nuiA_sweep(S);
    while (S->actions)
        nui_dropaction(S, S->actions);
    while (S->sched_actions)
        nui_dropaction(S, S->sched_actions);
    while (S->timed_actions)
        nui_dropaction(S, S->timed_actions);
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
            NUIaction *next = nuiL_empty(a) ? NULL : nuiL_next(a);
            nuiL_remove(a);
            nuiL_insert_pointer(S->actions, a);
            if (a->f != NULL)
                a->f(S, a, a->n, NULL);
            a = next;
        }
    }
    if ((a = S->sched_actions) != NULL) {
        S->sched_actions = NULL;
        while (a != NULL) {
            NUIaction *next = nuiL_empty(a) ? NULL : nuiL_next(a);
            nuiL_remove(a);
            nuiL_insert_pointer(S->actions, a);
            if (a->f != NULL)
                a->f(S, a, a->n, NULL);
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
            n->klass->child_removed(n->parent, n);
        n->parent = NULL;
    }
    if (parent) {
        if (n->klass->child_added)
            n->klass->child_added(parent, n);
        n->parent = parent;
    }
}

static void node_delete(NUIstate *S, NUInode *n) {
    size_t totalsize = sizeof(NUInode) + n->klass->node_extra_size;
    node_setparent(n, NULL);
    nuiL_remove_safe(n);
    if (n->klass->delete_node)
        n->klass->delete_node(n);
    nui_freetable(n->klass->state, &n->table);
    nuiM_freemem(S, n, totalsize);
}

NUInode *nui_node(NUIstate *S, NUIstring *classname, void **params) {
    NUInode *n;
    NUIentry *klass_entry = nui_gettable(&S->classes, classname);
    NUIclass *klass = klass_entry ?
        (NUIclass*)klass_entry->value : S->default_class;
    size_t totalsize = sizeof(NUInode) + klass->node_extra_size;

    n = (NUInode*)nuiM_malloc(S, totalsize);
    nuiL_init(n);
    n->klass = klass;
    n->next = klass->all_nodes;
    klass->all_nodes = n;

    n->parent = NULL;
    n->children = NULL;
    nui_inittable(S, &n->table, 0);
    n->pos.x = n->pos.y = 0;
    n->size.width = n->size.height = 0;
    /*n->layout_params = NULL;*/
    n->id = 0;
    n->flags = 0;
    n->handle = NULL;
    if (n->klass->new_node && !n->klass->new_node(n, params)) {
        nuiM_freemem(S, n, totalsize);
        return NULL;
    }
    return n;
}

int nui_dropnode(NUInode *n) {
    NUIstate *S = n->klass->state;
    NUInode **list = &n->klass->all_nodes;
    while (*list != NULL) {
        if (*list == n) {
            *list = n->next;
            break;
        }
        list = &n->next;
    }
    n->next = S->dead_nodes;
    S->dead_nodes = n;
    return 1;
}

void nui_setchildren(NUInode *n, NUInode *newnode) {
    NUInode *i;
    if (n->children != NULL) {
        node_setparent(n->children, NULL);
        nuiL_foreach (i, n->children)
            node_setparent(i, NULL);
    }
    if (newnode) {
        node_setparent(newnode, n);
        nuiL_foreach (i, newnode)
            node_setparent(i, n);
        if (newnode->parent)
            newnode->parent->children = NULL;
    }
    n->children = newnode;
}

void nui_append(NUInode *n, NUInode *newnode) {
    node_setparent(newnode, n->parent);
    nuiL_remove(newnode);
    nuiL_append(n, newnode);
}

void nui_insert(NUInode *n, NUInode *newnode) {
    node_setparent(newnode, n->parent);
    nuiL_remove(newnode);
    nuiL_insert(n, newnode);
    if (n->parent && n->parent->children == n)
        n->parent->children = newnode;
}

void nui_detach(NUInode *n) {
    NUInode *next;
    node_setparent(n, NULL);
    next = nuiL_empty(n) ? NULL : nuiL_next(n);
    nuiL_remove_safe(n);
    if (n->parent && n->parent->children == n)
        n->parent->children = next;
}

NUInode* nui_parent(NUInode *n) {
    if (n->klass->get_parent)
        return n->klass->get_parent(n);
    return n->parent;
}

NUInode* nui_firstchild  (NUInode *n) { return n->children; }
NUInode* nui_lastchild   (NUInode *n) { return n->children ? nuiL_prev(n->children) : NULL; }
NUInode* nui_prevsibling (NUInode *n) { return nuiL_empty(n) ? NULL : nuiL_prev(n); }
NUInode* nui_nextsibling (NUInode *n) { return nuiL_empty(n) ? NULL : nuiL_next(n); }

NUInode* nui_root(NUInode *n) {
    NUInode *parent;
    while ((parent = n->parent) != NULL)
        n = parent;
    return n;
}

NUInode* nui_firstleaf(NUInode *root) {
    return root;
}

NUInode* nui_lastleaf(NUInode *root) {
    NUInode *firstchild;
    while ((firstchild = root->children) != NULL)
        root = nuiL_prev(firstchild);
    return root;
}

NUInode* nui_prevleaf(NUInode *n) {
    NUInode *parent, *firstchild;
    if ((parent = n->parent) && parent->children == n) /* first child */
        return parent;
    n = nuiL_prev(n);
    while ((firstchild = n->children))
        n = nuiL_prev(firstchild);
    return n;
}

NUInode* nui_nextleaf(NUInode *n) {
    NUInode *parent;
    if (n->children)
        return n->children;
    while ((parent = n->parent)
            && parent->children == nuiL_next(n))
        n = parent;
    return nuiL_next(n);
}

NUIpoint nui_position (NUInode *n) { return n->pos;  }
NUIsize  nui_size     (NUInode *n) { return n->size; }

void nui_setid(NUInode *n, int id) { n->id = id; }
int  nui_getid(NUInode *n)         { return n->id; }

void       nui_setaction(NUInode *n, NUIaction *a) { n->action = a;    }
NUIaction *nui_getaction(NUInode *n)               { return n->action; }

NUIpoint nui_abspos(NUInode *n) {
    NUIpoint pos = { 0, 0 };
    while (n != NULL) {
        pos.x += n->pos.x;
        pos.y += n->pos.y;
        n = n->parent;
    }
    return pos;
}

NUIsize nui_naturalsize(NUInode *n) {
    NUIsize size;
    if (n->klass->layout_naturalsize
            && n->klass->layout_naturalsize(n, &size))
        return size;
    return n->size;
}

void nui_move(NUInode *n, NUIpoint pos) {
    if (n->klass->node_move
            && n->klass->node_move(n, pos))
        n->pos = pos;
}

void nui_resize(NUInode *n, NUIsize size) {
    if (n->klass->node_resize
            && n->klass->node_resize(n, size))
        n->size = size;
}

void nui_show(NUInode *n) {
    if (n->klass->node_show
            && n->klass->node_show(n))
        n->visible = 1;
}

void nui_hide(NUInode *n) {
    if (n->klass->node_hide
            && n->klass->node_hide(n))
        n->visible = 0;
}

void *nui_gethandle(NUInode *n) {
    if (n->klass->get_handle)
        return n->klass->get_handle(n);
    return n->handle;
}

static int call_getattr(NUInode *n, NUItable *t, NUIstring *key, NUIvalue *pv) {
    NUIentry *attrib_entry = nui_gettable(t, key);
    if (attrib_entry != NULL) {
        NUIattrib *attrib = (NUIattrib*)attrib_entry->value;
        if (attrib->getattr != NULL) {
            attrib->key = key;
            attrib->node = n;
            return attrib->getattr(attrib, pv);
        }
    }
    return 0;
}

static int call_setattr(NUInode *n, NUItable *t, NUIstring *key, NUIvalue *pv) {
    NUIentry *attrib_entry = nui_gettable(t, key);
    if (attrib_entry != NULL) {
        NUIattrib *attrib = (NUIattrib*)attrib_entry->value;
        if (attrib->setattr != NULL) {
            attrib->key = key;
            attrib->node = n;
            return attrib->setattr(attrib, pv);
        }
    }
    return 0;
}

static int call_delattr(NUInode *n, NUItable *t, NUIstring *key) {
    NUIentry *attrib_entry = nui_gettable(t, key);
    if (attrib_entry != NULL) {
        NUIattrib *attrib = (NUIattrib*)attrib_entry->value;
        if (attrib->delattr != NULL) {
            attrib->key = key;
            attrib->node = n;
            return attrib->delattr(attrib);
        }
    }
    return 0;
}

int nui_getattr(NUInode *n, NUIstring *key, NUIvalue *pv) {
    int res;
    NUIclass *klass = n->klass;
    NUIclass *defklass;
    if ((res = call_getattr(n, &n->table, key, pv)) != 0)
        return res;
    while (klass != NULL) {
        if ((res = call_getattr(n, &klass->attrib_table, key, pv)) != 0)
            return res;
        if (klass->getattr != NULL)
            return klass->getattr(n, key, pv);
        klass = klass->parent;
    }
    defklass = n->klass->state->default_class;
    if (n->klass != defklass &&
            (res = call_getattr(n, &defklass->attrib_table, key, pv)) != 0)
        return res;
    if (pv) {
        pv->type = NUI_TPOINTER;
        pv->u.pointer = NULL;
    }
    return 0;
}

int nui_setattr(NUInode *n, NUIstring *key, NUIvalue v) {
    int res;
    NUIclass *klass = n->klass;
    NUIclass *defklass;
    if ((res = call_setattr(n, &n->table, key, &v)) != 0)
        return res;
    while (klass != NULL) {
        if ((res = call_setattr(n, &klass->attrib_table, key, &v)) != 0)
            return res;
        if (klass->setattr != NULL)
            return klass->setattr(n, key, &v);
        klass = klass->parent;
    }
    defklass = n->klass->state->default_class;
    if (n->klass != defklass &&
            (res = call_setattr(n, &defklass->attrib_table, key, &v)) != 0)
        return res;
    return 0;
}

int nui_delattr(NUInode *n, NUIstring *key) {
    int res;
    NUIclass *klass = n->klass;
    NUIclass *defklass;
    if ((res = call_delattr(n, &n->table, key)) != 0)
        return res;
    while (klass != NULL) {
        if ((res = call_delattr(n, &klass->attrib_table, key)) != 0)
            return res;
        if (klass->setattr != NULL)
            return klass->delattr(n, key);
        klass = klass->parent;
    }
    defklass = n->klass->state->default_class;
    if (n->klass != defklass &&
            (res = call_delattr(n, &defklass->attrib_table, key)) != 0)
        return res;
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
    S->default_class = nuiM_fromuser(nui_newclass(S, S->predefs[NUI_IDX_default], 0));
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
    nui_freetable(S, &c->attrib_table);
    nuiM_freemem(S, c, sizeof(NUIclass) + c->class_extra_size);
}

static void attrib_deletor(NUIstate *S, void *p) {
    nuiM_free(S, (NUIattrib*)p);
}

NUIclass *nui_newclass(NUIstate *S, NUIstring *classname, size_t sz) {
    NUIentry *klass_entry;
    NUIclass *klass;
    if (classname == NULL)
        classname = S->predefs[NUI_IDX_default];
    klass_entry = nui_gettable(&S->classes, classname);
    if (klass_entry != NULL)
        return nuiM_touser((NUIclass*)klass_entry->value);
    klass = nuiM_malloc(S, sizeof(NUIclass) + sz);
    if (klass == NULL) return NULL;
    klass_entry = nui_settable(S, &S->classes, classname);
    klass_entry->value = (void*)klass;
    klass_entry->deletor = class_deletor;
    klass->name = classname;
    klass->parent = NULL;
    klass->state = S;
    nui_inittable(S, &klass->attrib_table, 0);
    klass->all_nodes = NULL;
    klass->node_extra_size = 0;
    klass->class_extra_size = sz;
    klass->deletor = NULL;
    klass->new_node = NULL;
    klass->delete_node = NULL;
    klass->getattr = NULL;
    klass->setattr = NULL;
    klass->delattr = NULL;
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
    return nuiM_touser(klass);
}

NUIattrib *nui_newattrib(NUIstate *S, NUIclass *klass, NUIstring *key) {
    NUIattrib *attrib;
    NUIentry *attrib_entry;
    klass = nuiM_fromuser(klass);
    attrib_entry = nui_gettable(&klass->attrib_table, key);
    if (attrib_entry != NULL)
        return attrib_entry->value;
    attrib = nuiM_new(S, NUIattrib);
    attrib_entry = nui_settable(S, &klass->attrib_table, key);
    attrib_entry->deletor = attrib_deletor;
    attrib_entry->value = attrib;
    attrib->key = key;
    attrib->node = NULL;
    attrib->getattr = NULL;
    attrib->setattr = NULL;
    attrib->delattr = NULL;
    return attrib;
}


/* === nui state === */

n_noret nui_error(NUIstate *S, const char *fmt, ...) {
    char buff[NUI_PANICBUFFSIZE];
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
    nuiS_open(S); /* open string pool support */
    nuiA_open(S); /* open action support */
    nuiC_open(S); /* open class support */
    return S;
}

void nui_close(NUIstate *S) {
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

    if (res == 0 && (S->timed_actions != NULL || S->sched_actions != NULL))
        res = 1;

    nuiC_sweep(S); /* sweep dead nodes */
    nuiA_sweep(S); /* sweep dead actions */
    nuiS_sweep(S); /* sweep dead strings */ 
    return res;
}

int nui_loop(NUIstate *S) {
    int res;
    while ((res = nui_waitevents(S)) > 0)
        ;
    return res;
}


/* cc: flags+='-ggdb -O3 -mdll -DNUI_DLL -Wl,--output-def,nui.def'
 * cc: output='nui.dll' run='nui_test' */
