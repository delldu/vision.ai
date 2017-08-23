require "vision"

a = vision.image.load(arg[1])
b = a:copy()
b:gray()
--a:guided_filter(b, tonumber(arg[2]), tonumber(arg[3]))
a:guided_filter(b, tonumber(arg[2]), tonumber(arg[3]), tonumber(arg[4]))
a:save("guided.jpg")
a:show()
--os.exit(0)


