require "vision"

a = vision.image.load(arg[1])
a = a: zoom(200, 256)
a:object_detect(tonumber(arg[2]))
a:show()

