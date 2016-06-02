local nui = require "nui"

function test_mem()
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
end

test_mem()

