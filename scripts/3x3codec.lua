require "vision"

a = vision.image.load(arg[1])
a:enc3x3()
--a:show()
a:dec3x3()
--a:show()

