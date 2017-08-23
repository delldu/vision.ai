require "vision"

a = vision.image.load(arg[1])
strength = tonumber(arg[2])
zoom = tonumber(arg[3])
b = a:defisheye(strength, zoom)
--b:save("/home/dell/Pictures/defisheye.jpg")
b:show()
--os.exit(0)


