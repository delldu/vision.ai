require "vision"

a = vision.image.load(arg[1])
a:retinex(tonumber(arg[2]))
a:show()
