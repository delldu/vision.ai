require "vision"

a = vision.image.load(arg[1])
a:dehaze_filter(tonumber(arg[2]))
a:show()
--os.exit(0)


