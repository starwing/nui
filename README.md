nui - the Native cell-structured UI framework
=============================================

nui is a abstract cell-structured UI library, it's written in purely ANSI C,
without any platform-spec codes. It's only have a consist interface for really
GUI construct codes. 

nui has several main types: 

  - `NUIstate`: the nui state handle.

  - `NUIstring`: ref-counted interned string. `NUIstring` is immutable, after
    creation, all same string share a single `NUIstring` pointer. So compare
    `NUIstring` pointer is the same as compare the string value in
    `NUIstring`.

  - `NUIaction`: action is the only callback mechanism in nui. A action can
    saved in a node (see below) and called when some event happens. Action
    also has built-in timer support. You can specify a `NUIaction` called
    after sometime, instead immediately.

  - `NUInode`: `NUInode` is the center structure in nui. All widget in nui is
    a `NUInode`. `NUInode` is tree-structured, So it has parent, children,
    brothers. A `NUInode` has nothing to do with actually underlying OS
    widgets/controls, But it support a "map" operation, when mapping a
    `NUInode`, the underlying OS controls is created on the fly. The mapping
    is done automatic when you show the `NUInode`.

    `NUInode` also has a attribute system. You can query any attribute of
    `NUInode`. A back-end (OS controls mapper or actually widget implementor)
    can register some attribute to `NUInode`, when you query/setting/delete a
    attribute in `NUInode`, a function in `NUInode` will called. Attribute is
    association with a `NUIstring`.

  - `NUIclass`: `NUIclass` is the class of `NUInode`, all `NUInode` has a
    class.  The actually underlying work is done by class. A back-end
    implementor should implement a series of `NUIclass`. When you
    show/hide/resize/move a `NUInode`, the corresponding class method is
    called, allow back-end to map your operations to underlying OS APIs.


So, nui can be used as a interface to actually OS GUI APIs. It allows reduce
the amount of API used when you manipulate GUI.

nui has not `NUIstate` creation API, A backend should offer one. To create a
`NUIstate` in a backend, you should call `nui_newstate` with a backend
`NUIparams` structure, With the implement of several OS-relative routines.
e.g.  wait function, event poller, error handling, etc. When you worked done
with nui, a `nui_close` should called.

A typical backend should offer a header file to specify the creation function
for NUIstate, this function construct a `NUIparams` structure, and call
`nui_newstate` to get a `NUIstate` structure. After that, backend should using
`nui_newclass` to create the concrete GUI widget class, such as button,
textedit, label, group, etc. Then backend can return `NUIstate` to user. user
can call `nui_node` on this state to get the real NUInode.

backend can also implement named actions. named actions are default actions
that can be done. e.g. "quit" action can quit the main loop.

A user's nui code should looked like this:

```C
#include "nui.h"
#include "nui_win32.h"

#define STR(str) nui_string(S, #str)
#define STRING_VALUE(str) nui_vstring(STR(str))
#define ACTION_VALUE(str) nui_vaction(nui_namedaction(S, STR(str)))

int main(void) {
    NUIstate *S = nui_openwin32();
    NUInode *win = nui_node(S, NULL, NULL);
    NUInode *lable = nui_node(S, STR(label), &win);
    NUInode *btn = nui_node(S, STR(button), &win);

    nui_setattr(win,   STR(layout), STRING_VALUE(vertical));
    nui_setattr(win,   STR(layout_margin), nui_vnumber(10.0));
    nui_setattr(label, STR(text),   STRING_VALUE(Hello World!));
    nui_setattr(btn,   STR(text),   STRING_VALUE(OK));

    nui_setaction(btn, ACTION_VALUE(quit));

    nui_show(win);
    return nui_loop(S);
}
```

It looks not very good. The main purpose of nui is create a easy use GUI
toolkit in scripting language, such as Lua. in Lua, a nui code may looks like
this:

```Lua
local nui = require "nui.win32"
local win = nui.node {
    layout = "vertical";
    layout_margin = "10px";

    nui.label {
	text = "Hello world!";
    };

    nui.button {
	text = "OK";
	action = "quit";
    };
}

win:show()
nui.loop()
```

Looks really nice, isn't? :)


Building
--------

nui has only one source file: nui.c. So just compile it with your project.
after some backend implemented, the Makefile will produce to build nui with
backends.


License
-------

The license is the same as Lua programming.


Things to do
------------

- A sample back-end. (Windows, or OpenGL-based)
- the default layout algorithm to place `NUInode` in window.
- the Lua binding for nui.


