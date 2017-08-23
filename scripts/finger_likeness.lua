require "vision"

a = vision.image.load(arg[1])
b = vision.image.load(arg[2])
c = vision.image.finger_likeness(a, b, 1)
print("Image finger likeness:", c)


