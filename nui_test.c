#include <stdio.h>
#define NUI_IMPLEMENTATION
#include "nui.h"

static size_t allmem;

static void *debug_alloc(NUIparams *params, void *ptr, size_t nsize, size_t osize) {
    if (params->S == NULL) {
        allmem = 0;
        printf("[M] *** state init ***\n");
    }
    if (nsize == 0) {
        allmem -= osize;
        printf("[M] free:    %d (%d)\n", (int)allmem, (int)osize);
        if (ptr == params->S) {
            assert(allmem == 0);
            printf("[M] *** state free ***\n");
        }
        free(ptr);
        return NULL;
    }
    if (ptr == NULL) {
        assert(osize == 0);
        allmem += nsize;
        printf("[M] malloc:  %d (%d)\n", (int)allmem, (int)nsize);
        return malloc(nsize);
    }
    allmem += nsize;
    allmem -= osize;
    printf("[M] realloc: %d (%d->%d)\n", (int)allmem, (int)osize, (int)nsize);
    return realloc(ptr, nsize);
}

typedef struct NUIcomp_name {
    NUIcomp base;
    const char *name;
} NUIcomp_name;

static void putname(NUInode *n) {
    NUIstate *S = nui_state(n);
    NUItype *t;
    NUIcomp_name *comp = NULL;
    const char *name = NULL;
    if (S == NULL) { printf("NULL"); return; }
    t = nui_gettype(S, NUI_(name));
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

static NUInode *new_named_node(NUIstate *S, const char *name) {
    NUInode *n = nui_newnode(S);
    NUIcomp_name *comp = (NUIcomp_name*)nui_addcomp(n, nui_gettype(S, NUI_(name)));
    comp->name = name;
    nui_setparent(n, nui_rootnode(S));
    return n;
}

static void add_child(void *ud, NUInode *n, const NUIevent *evt) {
    NUIstate *S = nui_state(n);
    const NUIentry *e = nui_gettable(nui_eventdata(evt), NUI_(child));
    printf("add_child:\t");
    putname(nui_eventnode(evt));
    printf(" <- ");
    putname((NUInode*)e->value);
    printf("\n");
}

static void remove_child(void *ud, NUInode *n, const NUIevent *evt) {
    NUIstate *S = nui_state(n);
    const NUIentry *e = nui_gettable(nui_eventdata(evt), NUI_(child));
    printf("remove_child:\t");
    putname(nui_eventnode(evt));
    printf(" <- ");
    putname((NUInode*)e->value);
    printf("\n");
}

static void delete_node(void *ud, NUInode *n, const NUIevent *evt) {
    printf("delete:\t\t"); putname(nui_eventnode(evt)); printf("\n");
}

static int add_listener(NUIstate *S) {
    nui_addhandler(nui_rootnode(S), NUI_(add_child), 1, add_child, NULL);
    nui_addhandler(nui_rootnode(S), NUI_(remove_child), 1, remove_child, NULL);
    nui_addhandler(nui_rootnode(S), NUI_(delete_node), 1, delete_node, NULL);
    nui_addhandler(nui_rootnode(S), NUI_(add_child), 0, add_child, NULL);
    nui_addhandler(nui_rootnode(S), NUI_(remove_child), 0, remove_child, NULL);
    nui_addhandler(nui_rootnode(S), NUI_(delete_node), 0, delete_node, NULL);
    return 1;
}

static int tracked_node = 0;

static void track_node_delete(void *ud, NUInode *n, const NUIevent *evt) {
    printf("node: %p deleted\n", n);
    --tracked_node;
}

static NUInode *new_track_node(NUIstate *S) {
    NUInode *n = nui_newnode(S);
    nui_addhandler(n, NUI_(delete_node), 0, track_node_delete, NULL);
    printf("new node: %p\n", n);
    ++tracked_node;
    return n;
}

static void test_mem(void) {
    NUIparams params = { debug_alloc };
    NUIstate *S = nui_newstate(&params);
    printf("new state: %p\n", S);

    NUItimer *timer = nui_newtimer(S, NULL, NULL);
    printf("new timer: %p\n", timer);

    NUIkey *name = NUI_(observer);
    printf("new key: %p\n", name);
    printf("new key: %p\n", NUI_(observer));

    NUInode *n = new_track_node(S); /* a pending node */
    n = new_track_node(S);

    NUIevent evt; nui_initevent(&evt, NUI_(test), 1, 1);
    nui_settable(S, nui_eventdata(&evt), NUI_(foo))->value = nui_strdata(S, "bar");
    nui_emitevent(n, &evt);
    printf("new event: %p\n", &evt);
    nui_deldata(S, (NUIdata*)nui_gettable(nui_eventdata(&evt), NUI_(foo))->value);
    nui_freeevent(S, &evt);

    NUIattr attr = { NULL };
    nui_setattr(n, NUI_(attr), &attr);
    nui_delattr(n, NUI_(attr));
    printf("new attr: %p\n", &attr);

    nui_release(n);
    printf("release node\n");

    NUItype *t = nui_newtype(S, NUI_(observer), 0, sizeof(NUIcomp_name));
    printf("new type: %p\n", t);

    NUInode *n1 = new_track_node(S);
    nui_setparent(n1, nui_rootnode(S));
    NUIcomp *comp1 = nui_addcomp(n1, t);
    printf("new node with comp: %p\n", comp1);

    NUIcomp *comp = nui_getcomp(n1, t);
    assert(comp == comp1);
    assert(comp == nui_addcomp(n1, t));

    NUInode *n2 = new_track_node(S);
    nui_setparent(n2, n1);
    comp = nui_addcomp(n2, t);
    assert(comp == nui_getcomp(n2, t));

    nui_close(S);
    assert(tracked_node == 0);
}

static void test_node(void) {
    NUIparams params = { debug_alloc };
    NUIstate *S = nui_newstate(&params);
    NUItype *t = open_name_type(S);
    ((NUIcomp_name*)nui_addcomp(nui_rootnode(S), t))->name = "root";
    add_listener(S);
    printf("-------------\n");
    NUInode *n = new_named_node(S, "n");
    printf("-------------\n");                                  
    NUInode *n1 = new_named_node(S, "n1");
    printf("-------------\n");
    NUInode *n2 = new_named_node(S, "n2");
    printf("-------------\n");
    NUInode *n3 = new_named_node(S, "n3");
    printf("-------------\n");
    NUInode *n4 = new_named_node(S, "n4");
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
    nui_setparent(n4, NULL);
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

static void on_foo(void *ud, NUInode *n, const NUIevent *evt) {
    NUIstate *S = nui_state(n);
    NUIevent new_evt; nui_initevent(&new_evt, NUI_(foo), 0, 0);
    nui_emitevent(n, &new_evt); /* should loop */
    nui_freeevent(S, &new_evt);
}

static void on_remove_child(void *ud, NUInode *n, const NUIevent *evt) {
    NUIstate *S = nui_state(n);
    NUInode *child = (NUInode*)nui_gettable(nui_eventdata(evt), NUI_(child))->value;
    nui_detach(child); /* trigger new event, should loop */
}

static void test_event(void) {
    NUIparams params = { debug_alloc };
    NUIstate *S = nui_newstate(&params);
    nui_addhandler(nui_rootnode(S), NUI_(foo), 0, on_foo, NULL);
    printf("trigger loop event\n");
    on_foo(NULL, nui_rootnode(S), NULL); /* emit first event */

    nui_addhandler(nui_rootnode(S), NUI_(remove_child), 0, on_remove_child, NULL);

    NUInode *n1 = nui_newnode(S);
    nui_setparent(n1, nui_rootnode(S));

    NUInode *n2 = nui_newnode(S);
    nui_setparent(n2, nui_rootnode(S));

    /* trigger event
     * event should loop maximum (100), and doesn't do anything */
    printf("trigger loop event by changing node\n");
    nui_setparent(n1, n2);
    assert(nui_parent(n1) == n2); /* event handler should have no effect */

    nui_close(S);
}

static NUItime on_timer(void *ud, NUItimer *t, NUItime elapsed) {
    printf("on_timer: %p: %u\n", t, elapsed);
    return ud ? 1000 : 0;
}

static void test_timer(void) {
    NUIparams params = { debug_alloc };
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
    test_event();
    test_timer();
    return 0;
}
/* cc: flags+='-ggdb' */
