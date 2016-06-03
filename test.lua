local nui = require "nui"

function test_mem()
   io.write("[TEST] mem ... ")
   local S = nui.new()
   local t = S:timer(function() end)
   local n = S:node() {}
   local a = S:attr()
   local e = S:event "foo"
   local nt = S:type "foo" {}
   S:delete()
   assert(tostring(t):match "%[N%]")
   assert(tostring(n):match "%[N%]")
   assert(tostring(a):match "%[N%]")
   assert(tostring(e):match "%[N%]")
   assert(tostring(nt):match "%[N%]")
   assert(tostring(S):match "%[N%]")
   io.write("OK\n")
end

function test_node()
   io.write("[TEST] node ...\n")
   local S = nui.new()
   local nameT = S:type "name" {
      newcomp = function(t, n)
         return { name = n.name }
      end
   }
   local function new_named_node(name)
      local n = S:node()
      n.name = name
      n:addcomp(nameT)
      assert(n:getcomp(nameT))
      assert(n:getcomp(nameT) == n:getcomp "name")
      assert(n:getcomp(nameT).name == name)
      n.parent = S.rootnode
      return n
   end
   local function add_child(n, e)
      local parent = e.node and e.node.name or e.node
      local child = e.child and e.child.name or e.child
      print("add_child:", parent, "<-", child)
   end
   local function remove_child(n, e)
      local parent = e.node and e.node.name or e.node
      local child = e.child and e.child.name or e.child
      print("remove_child:", parent, "<-", child)
   end
   local function delete_node(n, e)
      local target = e.node and e.node.name or e.node
      print("delete_node:", target)
   end
   S.rootnode.name = "rootnode"
   S.rootnode:addhandler("add_child",  add_child, true)
   S.rootnode:addhandler("remove_child", remove_child, true)
   S.rootnode:addhandler("delete_node", delete_node, true)
   S.rootnode:addhandler("add_child", add_child)
   S.rootnode:addhandler("remove_child", remove_child)
   S.rootnode:addhandler("delete_node", delete_node)
   local n = new_named_node "n"
   local n1 = new_named_node "n1"
   local n2 = new_named_node "n2"
   local n3 = new_named_node "n3"
   local n4 = new_named_node "n4"
   n1.parent = n
   n2.parent = n
   n3.parent = n
   assert(#n == 3)
   assert(n1.index == 1)
   assert(n2.index == 2)
   assert(n3.index == 3)
   assert(n[1].index == 1)
   assert(n[2].index == 2)
   assert(n[3].index == 3)

   n4.children = n1
   n4.parent = nil
   assert(#n == 0)
   assert(not n4.index)

   n1.parent = n
   assert(#n == 1)
   assert(n1.index == 1)

   n1:insert(n2)
   assert(#n == 2)
   assert(n2.index == 1)
   assert(n1.index == 2)
   
   n1:append(n3)
   assert(#n == 3)
   assert(n3.index == 3)

   S:delete()
   io.write("OK\n")
end

test_mem()
test_node()

