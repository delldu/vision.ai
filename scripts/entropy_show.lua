require "vision"

b = vision.image.load(arg[1])
a = b:zoom(768, 1024)
a:entropy(16, 16, 32, 1)
a:show()


