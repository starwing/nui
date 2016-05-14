#include <stdio.h>
#define NUI_DEBUG_MEM printf
#define NUI_IMPLEMENTATION
#include "nui.h"

typedef struct NUIcomp_name {
    NUIcomp base;
    const char *name;
} NUIcomp_name;

typedef struct NUIcomp_observer {
    NUIcomp base;
    NUIlistener li;
} NUIcomp_observer;

static void setname(NUInode *n, const char *name) {
    NUIstate *S = nui_state(n);
    NUItype *t = nui_gettype(S, NUI_(name));
    NUIcomp_name *comp = NULL;
    if (t) comp = (NUIcomp_name*)nui_getcomp(n, t);
    if (comp) comp->name = name;
}

static void putname(NUInode *n) {
    NUIstate *S = nui_state(n);
    NUItype *t = nui_gettype(S, NUI_(name));
    NUIcomp_name *comp = NULL;
    const char *name = NULL;
    if (t) comp = (NUIcomp_name*)nui_getcomp(n, t);
    if (comp) name = comp->name;
    if (name != NULL)
        printf("%s", (char*)name);
    else
        printf("node: %p", n);
}

static NUItype *open_name_type(NUIstate *S) {
    return nui_newtype(S, NUI_(name), 0, sizeof(NUIcomp_name));
}

static void add_child(NUIlistener *li, NUInode *self, NUInode *child)
{ printf("add_child:\t"); putname(self); printf(" -> "); putname(child); printf("\n"); }

static void remove_child(NUIlistener *li, NUInode *self, NUInode *child)
{ printf("remove_child:\t"); putname(self); printf(" -> "); putname(child); printf("\n"); }

static void added(NUIlistener *li, NUInode *parent, NUInode *self)
{ printf("added:\t\t"); putname(parent); printf(" -> "); putname(self); printf("\n"); }

static void removed(NUIlistener *li, NUInode *parent, NUInode *self)
{ printf("removed:\t"); putname(parent); printf(" -> "); putname(self); printf("\n"); }

static int new_componment(NUItype *t, NUInode *n, NUIcomp *comp) {
    NUIcomp_observer *self = (NUIcomp_observer*)comp;
    self->li.add_child = add_child;
    self->li.del_child = remove_child;
    self->li.added = added;
    self->li.removed = removed;
    nui_addlistener(n, &self->li);
    return 1;
}

static void delete_node(NUItype *t, NUInode *n, NUIcomp *comp) {
    NUIcomp_observer *self = (NUIcomp_observer*)comp;
    nui_dellistener(n, &self->li);
}

static NUItype *open_observer_type(NUIstate *S) {
    NUItype *t = nui_newtype(S, NUI_(observer), 0, sizeof(NUIcomp_observer));
    t->new_comp = new_componment;
    t->del_comp = delete_node;
    return t;
}

static void test_mem(void) {
    NUIparams params = { NULL };
    NUIstate *S = nui_newstate(&params);
    printf("new state: %p\n", S);

    NUIpoller p = { NULL };
    nui_addpoller(S, &p);
    nui_delpoller(S, &p);
    printf("new poller: %p\n", &p);

    NUItimer *timer = nui_newtimer(S, NULL, NULL);
    printf("new timer: %p\n", timer);

    NUIkey *name = NUI_(observer);
    printf("new key: %p\n", name);
    printf("new key: %p\n", NUI_(observer));

    NUInode *n = nui_newnode(S, NULL, 0);
    printf("new node: %p\n", n);

    NUIlistener li = { NULL };
    nui_addlistener(n, &li);
    nui_dellistener(n, &li);
    printf("new listener: %p\n", &li);

    NUIattr attr = { NULL };
    nui_addattr(n, NUI_(attr), &attr);
    nui_delattr(n, NUI_(attr));
    printf("new attr: %p\n", &attr);

    nui_release(n);
    printf("release node\n");

    NUItype *t = nui_newtype(S, NUI_(observer), 0, sizeof(NUIcomp_observer));
    printf("new type: %p\n", t);

    n = nui_newnode(S, &t, 1);
    printf("new node with type: %p\n", n);

    NUIcomp *comp = nui_getcomp(n, t);
    assert(comp == nui_addcomp(n, t));

    n = nui_newnode(S, NULL, 0);
    comp = nui_addcomp(n, t);
    assert(comp == nui_getcomp(n, t));

    nui_close(S);
}

static void test_node(void) {
    NUIparams params = { NULL };
    NUIstate *S = nui_newstate(&params);
    NUItype *t1 = open_observer_type(S);
    NUItype *t2 = open_name_type(S);
    NUItype *ts[] = { t1, t2 };
    printf("-------------\n");
    NUInode *n = nui_newnode(S, ts, 2);  setname(n, "n");   
    printf("-------------\n");                                  
    NUInode *n1 = nui_newnode(S, ts, 2); setname(n1, "n1"); 
    printf("-------------\n");                                  
    NUInode *n2 = nui_newnode(S, ts, 2); setname(n2, "n2"); 
    printf("-------------\n");                                  
    NUInode *n3 = nui_newnode(S, ts, 2); setname(n3, "n3"); 
    printf("-------------\n");                                  
    NUInode *n4 = nui_newnode(S, ts, 2); setname(n4, "n4"); 
    printf("-------------\n");

    nui_setparent(n1, n);
    nui_setparent(n2, n);
    nui_setparent(n3, n);
    assert(nui_childcount(n) == 3);
    assert(nui_nodeindex(n1) == 0);
    assert(nui_nodeindex(n2) == 1);
    assert(nui_nodeindex(n3) == 2);
    assert(nui_nodeindex(nui_indexnode(n, 0)) == 0);
    assert(nui_nodeindex(nui_indexnode(n, 1)) == 1);
    assert(nui_nodeindex(nui_indexnode(n, 2)) == 2);

    nui_setchildren(n4, n1);
    assert(nui_childcount(n) == 0);
    assert(nui_nodeindex(n4) == -1);

    nui_setparent(n1, n);
    assert(nui_childcount(n) == 1);
    assert(nui_nodeindex(n1) == 0);

    nui_insert(n1, n2);
    assert(nui_childcount(n) == 2);
    assert(nui_nodeindex(n2) == 0);
    assert(nui_nodeindex(n1) == 1);

    nui_append(n1, n3);
    assert(nui_childcount(n) == 3);
    assert(nui_nodeindex(n3) == 2);

    nui_close(S);
}

static NUItime on_timer(void *ud, NUItimer *t, NUItime elapsed) {
    printf("on_timer: %p: %u\n", t, elapsed);
    return ud ? 1000 : 0;
}

static void test_timer(void) {
    NUIparams params = { NULL };
    NUIstate *S = nui_newstate(&params);
    nui_starttimer(nui_newtimer(S, on_timer, NULL), 1000);
    nui_starttimer(nui_newtimer(S, on_timer, NULL), 2000);
    nui_starttimer(nui_newtimer(S, on_timer, NULL), 3000);
    nui_starttimer(nui_newtimer(S, on_timer, NULL), 4000);
    nui_starttimer(nui_newtimer(S, on_timer, NULL), 5000);
    nui_starttimer(nui_newtimer(S, on_timer, NULL), 4000);
    nui_loop(S);
    nui_close(S);
}

int main(void) {
    test_mem();
    test_node();
    test_timer();
    return 0;
}
/* cc: flags+='-ggdb' */
