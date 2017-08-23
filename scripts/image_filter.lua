require "vision"

a = vision.image.load(arg[1])
a:bilate_filter(tonumber(arg[2]), tonumber(arg[3]))
--a:save("bilate_filter.jpg")
a:show()

