require "vision"
require 'sys'

b = vision.image.load(arg[1])
a = b:zoom(400, 512)
levs = 16 -- tonumber(arg[2])
--a:gray()
time = sys.clock()
a:face_detect(levs, 1)
--a:skin_detect()
time = sys.clock() - time

print("time:", time)
a:show()

