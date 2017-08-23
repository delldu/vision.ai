require "vision"

a = vision.image.load(arg[1])
a:lee_filter(tonumber(arg[2]), tonumber(arg[3]))
a:show()
--os.exit(0)


