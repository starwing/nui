#ifndef nui_backend_h
#define nui_backend_h


#include "nui.h"

/* non-return type */
#ifndef n_noret
# if defined(__GNUC__)
#  define n_noret        void __attribute__((noreturn))
# elif defined(_MSC_VER)
#  define n_noret        void __declspec(noreturn)
# else
#  define n_noret        void
# endif
#endif


typedef struct NUIentry NUIentry;
typedef struct NUItable NUItable;
typedef struct NUIattrib NUIattrib;
typedef struct NUIclass NUIclass;
typedef struct NUIparams NUIparams;

typedef void NUIdeletor(NUIstate *S, void *p);


struct NUIparams {
    void (*deletor)(NUIparams *params);
    void (*error)(NUIparams *params, const char *msg);
    unsigned (*time)(NUIparams *params);
    int  (*poll)(NUIparams *params);
    int  (*wait)(NUIparams *params, unsigned waittime);
    void (*quit)(NUIparams *params);
};

struct NUIentry {
    NUIentry *next;
    NUIstring *key;
    void *value;
    NUIdeletor *deletor;
};

struct NUItable {
    size_t lsize;
    NUIentry *node;
    NUIentry *lastfree;
};

struct NUIattrib {
    size_t extra_size;
    int (*getattr) (NUIattrib *attrib, NUInode *n, NUIstring *key, NUIvalue *v);
    int (*setattr) (NUIattrib *attrib, NUInode *n, NUIstring *key, NUIvalue *v);
    int (*delattr) (NUIattrib *attrib, NUInode *n, NUIstring *key);
};

struct NUIclass {
    NUIstate *S;
    NUIstring *name;
    NUIclass *parent;
    NUInode *all_nodes;
    size_t class_extra_size;
    size_t node_extra_size;
    NUItable attrib_table;

    /* creation/destroy */
    void (*deletor) (NUIstate *S, NUIclass *c);

    int  (*new_node)    (NUIclass *c, NUInode *n, NUIvalue *v);
    void (*delete_node) (NUIclass *c, NUInode *n);

    /* default accessor */
    int (*getattr) (NUIclass *c, NUInode *n, NUIstring *key, NUIvalue *pv);
    int (*setattr) (NUIclass *c, NUInode *n, NUIstring *key, NUIvalue *pv);
    int (*delattr) (NUIclass *c, NUInode *n, NUIstring *key);

    /* for map/unmap */
    int (*map)   (NUIclass *c, NUInode *n);
    int (*unmap) (NUIclass *c, NUInode *n);

    NUInode* (*get_parent) (NUIclass *c, NUInode *n, NUInode *parent);
    void*    (*get_handle) (NUIclass *c, NUInode *n, void *handle);

    /* events */
    void (*child_added)    (NUIclass *c, NUInode *n, NUInode *child);
    void (*child_removed)  (NUIclass *c, NUInode *n, NUInode *child);

    /* node operations */
    int (*node_show)   (NUIclass *c, NUInode *n);
    int (*node_hide)   (NUIclass *c, NUInode *n);
    int (*node_move)   (NUIclass *c, NUInode *n, NUIpoint pos);
    int (*node_resize) (NUIclass *c, NUInode *n, NUIsize size);

    /* layout */
    int (*layout_update)      (NUIclass *c, NUInode *n);
    int (*layout_naturalsize) (NUIclass *c, NUInode *n, NUIsize *psz);
};

NUI_API n_noret nui_error(NUIstate *S, const char *fmt, ...);

NUI_API NUIstate  *nui_newstate(NUIparams *params);
NUI_API NUIclass  *nui_newclass(NUIstate *S, NUIstring *class_name, size_t sz);
NUI_API NUIattrib *nui_newattrib(NUIstate *S, NUIclass *klass, NUIstring *key, size_t sz);
NUI_API NUIattrib *nui_newnodeattrib(NUIstate *S, NUInode *n, NUIstring *key, size_t sz);
NUI_API NUIaction *nui_newnamedaction(NUIstate *S, NUIstring *name, NUIactionf *f, size_t sz);

NUI_API NUIclass *nui_nodeclass(NUInode *n);


/* === table API === */

NUI_API void nui_inittable(NUIstate *S, NUItable *t, size_t size);
NUI_API void nui_freetable(NUIstate *S, NUItable *t);
NUI_API void nui_resettable(NUIstate *S, NUItable *t);
NUI_API void nui_resizetable(NUIstate *S, NUItable *t, size_t size);

NUI_API NUIentry *nui_gettable(NUItable *t, NUIstring *key);
NUI_API NUIentry *nui_settable(NUIstate *S, NUItable *t, NUIstring *key);
NUI_API void      nui_deltable(NUIstate *S, NUIentry *e);

NUI_API size_t    nui_counttable(NUItable *t);
NUI_API NUIentry *nui_nextentry(NUItable *t, NUIentry *curr);


#endif /* nui_backend_h */
