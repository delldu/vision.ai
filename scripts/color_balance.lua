require "vision"

a = vision.image.load(arg[1])
--a = a:zoom(600, 800)
--a:show()
method = tonumber(arg[2])
a:color_balance(method)
a:save("color_balance.jpg")
a:show()
--os.exit(0)


