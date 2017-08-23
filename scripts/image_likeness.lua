require "vision"

a = vision.image.sub(vision.image.load(arg[1]), 71, 71, 96, 96)
b = vision.image.sub(vision.image.load(arg[2]), 71, 71, 96, 96)

c = vision.image.likeness(a, b)
d = vision.image.load("likeness.bmp")
print("Image likeness:", c)
d:show()
--a:show()
--b:show()


