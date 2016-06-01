local nui = require "nui"

function test_mem()
   local S = nui.new()
   local t = S:timer(function() end)
   local n = S:node() {}
   local a = S:attr()
   local e = S:event "foo"
   local t = S:type "foo" {}
end

test_mem()

