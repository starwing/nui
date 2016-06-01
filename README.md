nui - the Native cell-structured UI framework
=============================================

nui is a DOM like abstract cell-structured UI interface. It's written
in purely ANSI C, without any platform-spec codes. It's only have a
consist interface for really GUI construct codes. 

nui has several main types: 

  - `NUIstate`: the nui state handle.

  - `NUInode`: `NUInode` is the center structure in nui. All widget in
    nui is a `NUInode`. `NUInode` is tree-structured, So it has
    parent, children, brothers. A `NUInode` has nothing to do with
    actually underlying OS widgets/controls, But you can "map" real
    widgets to a node. "mapping" can be done with type/comp mechanisim.

  - `NUIattr`: `NUInode` have a attribute system. You can query any
    attribute of `NUInode`. A back-end (OS controls mapper or actually
    widget implementor) can register some attribute to `NUInode`, when
    you query/setting/delete a attribute in `NUInode`, a function in
    `NUInode` will called. Attribute is association with a `NUIkey`.

  - `NUItype`, `NUIcomp`: type/comp mechanisim is same with Unity's
    Component system. can you create a new type with `nui_newtype()`
    after that, you can create a comp to any node. a comp can contain
    information of the sematics of node. e.g. you can make a "button"
    type, and add this type to a node makes it behaves like a button.

  - `NUIevent`: `NUInode` is also a `EventTarget`. nui implements the
    DOM Event Model, you can use full functionality of DOM Event in
    nui, including capture, bubbles, default behavior and stop/cancel
    event.

  - `NUItimer`: although event model give nui powerful ability to
    maintain UI change, nui still include a soft-implement of timer.
    timer APIs only need host implement `time()` and `wait()`
    function. so you can merge timer into OS event loop. it's very
    fast, and if you don't use nui with any OS, nui itself offer the
    default implement of timer.

  - `NUIkey`: ref-counted interned string. `NUIkey` is immutable after
    creation, all same key share a single `NUIkey` pointer. So compare
    `NUIkey` pointer is the same as compare the value in `NUIkey`.
    `NUIkey` used as the hashtable key, so find a keyword in table
    needn't compare string content itself.

  - `NUItable`/`NUIentry`: the hashtable implement. it use a `NUIkey`
    as key so it's very fast. and it contains a non-sematics pointer
    as value.

  - `NUIpool`: memory pool, it makes nui run very fast. because mostly
    time nui needn't memory allocation. the pool get one memory page
    when needed, so nui avoid a large amount of small memory piece.

nui can be used as a interface to actually OS GUI APIs. It allows
reduce the amount of API used when you manipulate GUI.

nui has not `NUIstate` creation API, A backend should offer one. To
create a `NUIstate` in a backend, you should call `nui_newstate` with
a backend `NUIparams` structure, With the implement of several
OS-relative routines.  e.g.  wait function, timing function, error
handling, etc. When your work was done with nui, a `nui_close` should
called.

A typical backend should offer a header file to specify the creation
function for NUIstate, this function construct a `NUIparams`
structure, and call `nui_newstate` to get a `NUIstate` structure.
After that, backend should using `nui_newtype` to create the concrete
GUI widget class, such as button, textedit, label, group, etc. Then
backend can return `NUIstate` to user. user can call `nui_node` on
this state to get the real NUInode.

backend can also add some event processor to root node. when a event
emits out, default event processor can do it's work. e.g. "quit"
action can quit the main loop.

A `NUIstate` is also a `NUInode`, use macro `nui_rootnode()` to get
it, or type casting `NUIstate` to `NUInode` yourself. `NUIstate` is
the root node of all nodes, except of detached nodes. a backend can
use root node's children as the top-level window frame.

A user's nui code should looked like this:

```C
#include "nui.h"
#include "nui_win32.h"

int main(void) {
    NUIstate *S = nui_openwin32();
    NUInode *win = nui_node(S);
    nui_adcomp(win, nui_gettype(S, NUI_(Window)));
    nui_setparent(win, nui_rootnode(S));

    NUInode *lable = nui_node(S);
    nui_adcomp(label, nui_gettype(S, NUI_(Label)));
    nui_setparent(label, win);

    NUInode *btn = nui_node(S);
    nui_adcomp(btn, nui_gettype(S, NUI_(Button)));
    nui_setparent(btn, win);

    nui_setattr(win,   NUI_(style.margin), "10.0");
    nui_setattr(label, NUI_(text),  "Hello World!");
    nui_setattr(label, NUI_(style.display),  "block");
    nui_setattr(btn,   NUI_(text),   "OK");
    nui_setattr(btn, NUI_(style.float),  "right");

    nui_addhandler(btn, NUI_("onaction"), 0, do_quit, NULL);

    NUIevent *ev = nui_newevent(S, "show", 0, 0);
    nui_emitevent(win, ev);
    nui_delevent(ev);

    return nui_loop(S);
}
```

You may notice that above code using the DOM/CSS sematics. it's
recommended to implement DOM/CSS sematics in underlying backend. it's
easy to understand and easy to learn.

The code looks not very good. The main purpose of nui is create a easy
use GUI toolkit in scripting language, such as Lua. in Lua, a nui code
may looks like this:

```Lua
local nui = require "nui.win32" ()
local N = nui.node

local win = N"Window" {
    style = {
        margin = "10px";
    }

    N"Label" {
        style = { display = "block" };
        text = "Hello world!";
    };

    N"Button" {
        style = { float = "right" };
        text = "OK";
        action = function() nui.emitevent "quit" end;
    };
}

win:show()
nui.loop()
```

Looks really nice, isn't? :)


Building
--------

nui is a header-only project. only contain `nui.h` into your project
and implement your own backend to use it!

define macro `NUI_IMPLEMENTATION` in ONLY ONE source file to contain
the implements of nui. if you want ot use it privately in only one
source, define `NUI_STATIC_API`. nui APIs will not exported after
that.


License
-------

The license is the same as Lua programming.


Things to do
------------

- A sample back-end. (Windows, or OpenGL-based)
- the default layout algorithm to place `NUInode` in window.
- the Lua binding for nui.


<!-- vim: set ft=markdown nu et sw=4 : -->
