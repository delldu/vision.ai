require "vision"

a = vision.image.load(arg[1])
a:blend_multiband()
--a:show()

