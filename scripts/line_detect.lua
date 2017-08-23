require "vision"

a = vision.image.load(arg[1])
--a:gray()
a:line_detect()
a:show()

