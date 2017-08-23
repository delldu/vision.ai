require "vision"

a = vision.image.load(arg[1])
a:beeps_filter(tonumber(arg[2]), tonumber(arg[3]))
--a:save("beeps.jpg")
a:show()
--os.exit(0)


