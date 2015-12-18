#ifndef nui_list_h
#define nui_list_h


#include "nui.h"

NUI_NS_BEGIN


typedef struct NUIlinked NUIlinked;

struct NUIlinked {
    NUIlinked *next;
    NUIlinked *prev;
};

NUI_STATIC void nuiL_init(NUIlinked *l)
{ l->next = l->prev = l; }

NUI_STATIC int nuiL_empty(NUIlinked *l)
{ return l->next == l; }

NUI_STATIC void nuiL_insert(NUIlinked *l, NUIlinked *n) {
    n->prev = l->prev;
    n->next = l;
    l->prev->next = n;
    l->prev = n;
}

NUI_STATIC void nuiL_detach(NUIlinked *l) {
    l->prev->next = l->next;
    l->next->prev = l->prev;
}

NUI_STATIC void nuiL_detach_init(NUIlinked *l)
{ nuiL_detach(l); nuiL_init(l); }

NUI_STATIC void nuiL_detach_insert(NUIlinked *l, NUIlinked *n)
{ nuiL_detach(n); nuiL_insert(l, n); }


NUI_NS_BEGIN

#endif /* nui_list_h */
