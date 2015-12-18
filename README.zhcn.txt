NUI 设计说明：Node Union Interface

    最早NUI是打算作为Native UI Framework的形式存在的，就是说它本身只提供一个框
架，底层可以使用原生的UI控件做GUI，但是NUI其实可以做更多的事情。比如说决定一个
节点相关的结构，然后对这个结构采用更多的方式进行渲染，比如OpenGL-ES渲染，这样
就类似于 cocos 框架了。

    采用节点层级的原因是：
        - 节点层级类似GUI的结构，可以很方便将每个层级映射到GUI上（map操作）
        - 可以对一整颗节点树进行处理，比如说可以统一移动一系列节点。
        - 很方便可以读取层级结构并产生对应的树，可以模拟DOM。
        - 很方便可以查询树中的某个节点，如果没有层级关系，那么查找一个特定节点
        完全由节点名字决定，会比较麻烦。

    现在节点本身的功能其实不重要。节点只是决定了层级关系，节点身上可以附加很多
其他的组件（componment），组件本身只能附加在一个节点上，但可以复制，而节点身上
可以附加很多组件，根据节点名决定。

    这样我们就需要一个类型系统，我们需要能够决定组件的类型，可以新建组件类型。
其实如果认为节点也是一种类型，那么我们就需要一套类型系统。之前kit做了这个事情
，但是 nui 的话，应该可以定制一整套类型系统。

    类型系统的一个问题是，你必须能够通过名字，查询到一个类型。同样的，根据名字
可以查询到组件、节点等等。这就意味着，名字本身是很重要的一种抽象。在Nui里面，
名字会被抽象成NUIstring，我想是需要更好的一个名字的，现在将其改名为NUIkey
，不仅仅是字符串，而且是关键字，是用于比较和辨认的。关键字可以添加、删除，也可
以查询是否有这个关键字，很多时候，如果查询一个字符串，那么先查询关键字是比较理
想的。通常查询一个对应对象的字符串的时候，可以先查询对应关键字在不在，如果在才
做实际的查询，这样提高效率，避免因为输入产生不必要的关键字——因为哈希表的key
肯定是关键字。

    Nui的第一个类型：NUIkey。并不希望引用计数，因为可能会反复赋值，很少需
要删除，除非你自己确定的确不必要。因此方法有这些：

    #define NUI_(s) (nui_newkey(S, #s, sizeof(#s)-1))
        常量key，用法NUI_(abc)，注意上下文需要有S。

    NUIkey *nui_newkey (NUIstate *S, const char *s, size_t len);
    NUIkey *nui_iskey  (NUIstate *S, const char *s, size_t len);
    void    nui_delkey (NUIstate *S, NUIkey *key);

    获取NUIkey的属性：
    size_t   nui_len   (NUIkey *key);
    unsigned nui_hash  (NUIkey *key);
    unsigned nui_calchash (NUIstate *S, const char *s, size_t len);

    然后是 Component 系统，Component 系统需要有自己的类型、自己的名字，还可以
被附加到节点上，并可以在节点身上进行查询。Conponment的查询实际上是调用对应的回
调函数，查询可以返回对应的 Component，也可以直接调用回调函数，实现复杂的功能。
比如说，对于渲染模块，可以注册多个回调函数，用于处理多个钩子，用来返回对应渲染
的信息等等。可以有位置模块，可以返回对应的坐标等等。对一个模块进行绑定之后，模
块本身可以自动给Node绑定更多的组件，并得到这些组件的引用。组件不能卸载，因此绑
定一定有效。Node销毁的时候，这个节点所有的组件销毁。

    组件可以有很多种，比如说：
        - 单纯提供信息的属性组件，存储某些特定信息。
        - 渲染组件，不止是附加在节点上，还在节点外有一个列表，利用这个列表进行
            渲染（这个列表可以排序）。这个列表可以通过节点的层级关系仅需维护。
            插入一个渲染组件之后，这个列表插入一项，可以进行DrawCall预合并等等
            优化。（这里需要实际考察才能决定是否这样设计，也可以没有这个列表，
            而仅仅是遍历所有组件）。有这个列表的话，遍历渲染组件会变得容易，这
            个列表只需要保证，自己的渲染出现在自己兄弟后面即可。对渲染层级发生
            改变，可以直接对这个列表发生改变，这个可以之后仔细研究一下。
        - Native组件，可以映射一个 Native Control，维护现实组件。
        - Action组件，也是有额外列表的，Action组件每帧都会调用Update，这样就不
            需要直接遍历整个列表，就可有执行有需要的节点的Update了。

    组件本身有类型，有名称，被分配后，有一个指针存在Node上，组件的生命期属于
Node，不引用计数，组件本身有各种回调，回调放在组件类型上。因此还需要组件类型。

    NUIcomptype *nui_newcomptype (NUIstate *S, NUIkey *name);
    NUIcomptype *nui_iscomptype  (NUIstate *S, NUIkey *name);

    现在是节点（Node）本身了，节点本身的一个重要特点是，节点是可以引用计数的。
这是NUI里面唯一一种引用计数的类型。因此美誉节点删除函数，而是释放。节点是引用
计数的，那么没有引用，就会被回收了。

    NUInode *nui_newnode (NUIstate *S, NUIkey *name);

    size_t nui_retain      (NUInode *n);
    size_t nui_release     (NUInode *n);
    size_t nui_autorelease (NUInode *n);

    节点都有一个名字，可以为空：
    void        nui_setname (NUInode *n, NUIkey *name);

    可以通过节点一些信息：
    NUIstate    *nui_state (NUInode *n);
    NUIkey  *nui_name  (NUInode *n);

    前面提到过，节点可以添加组件：
    NUicomp *nui_newcomponment (NUInode *n, NUIcomptype *comptype);
    NUicomp *nui_getcomponment (NUInode *n, NUIcomptype *comptype);
    
    节点的层级操作：
    NUInode *nui_parent      (NUInode *n);
    NUInode *nui_prevchild   (NUInode *n, NUInode *curr);
    NUInode *nui_nextchild   (NUInode *n, NUInode *curr);
    NUInode *nui_prevsibling (NUInode *n, NUInode *curr);
    NUInode *nui_nextsibling (NUInode *n, NUInode *curr);

    NUInode *nui_root     (NUInode *n);
    NUInode *nui_prevleaf (NUInode *n, NUInode *curr);
    NUInode *nui_nextleaf (NUInode *n, NUInode *curr);

    NUInode *nui_indexnode  (NUInode *n, int idx);
    size_t   nui_childcount (NUInode *n);

    void nui_setparent   (NUInode *n, NUInode *parent);
    void nui_setchildren (NUInode *n, NUInode *children);

    void nui_append (NUInode *n, NUInode *newnode);
    void nui_insert (NUInode *n, NUInode *newnode);

    void nui_detach (NUInode *n);

    节点可以被设置属性监听：
    typedef int NUIattr (void *ud, NUInode *node, NUIkey *name, NUIvalue **v);

    NUIattr *nui_newattr (NUInode *n, NUIkey *name);
    NUIattr *nui_isattr  (NUInode *n, NUIkey *name);
    void     nui_delattr (NUInode *n, NUIkey *name);

    int nui_set (NUInode *n, NUIkey *key, NUIvalue *v);
    int nui_get (NUInode *n, NUIkey *key, NUIvalue **v);

    节点可以被设置监听：
    void nui_setlistener(NUInode *n, NUIlistener *listener);

    void (*delete_node)    (NUIlistener *l, NUInode *n);
    void (*new_componment) (NUIlistener *l, NUInode *n, NUIcomp *comp);
    void (*get_attr)       (NUIlistener *l, NUInode *n, NUIkey *key, NUIvalue *value);
    void (*set_attr)       (NUIlistener *l, NUInode *n, NUIkey *key, NUIvalue *value);
    void (*child_added)    (NUIlistener *l, NUInode *n, NUInode *child);
    void (*child_removed)  (NUIlistener *l, NUInode *n, NUInode *child);
    void (*added)          (NUIlistener *l, NUInode *parent, NUInode *n);
    void (*removed)        (NUIlistener *l, NUInode *parent, NUInode *n);
    NUInode *(*get_parent) (NUIlistener *l, NUInode *n);


    最后， NUIstate会提供一些全局函数用于做整体处理，比如计时器这种：

    NUIstate *nui_newstate (NUIparams *params);
    void      nui_close    (NUIstate *S);

    NUIparams *nui_getparams (NUIstate *S);

    typedef int NUIpoller (void *ud, NUIstate *S);
    int nui_addpoller (NUIstate *s, NUIpoller *f, void *ud);

    int  nui_pollevents (NUIstate *S);
    int  nui_waitevents (NUIstate *S);
    int  nui_loop       (NUIstate *S);
    void nui_breakloop  (NUIstate *S);

    typedef float NUItimer (void *ud, NUIstate *S, float elapsed);
    float nui_time        (NUIstate *S);
    void  nui_newtimer    (NUIstate *S, NUItimer *tiemrf, void *ud);
    void  nui_starttimer  (NUIstate *S, float delayed, float interval);
    void  nui_canceltimer (NUIstate *S);

