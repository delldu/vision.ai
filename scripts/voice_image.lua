require "vision"

a = vision.image.load(arg[1])
levs = tonumber(arg[2])
num = tonumber(arg[3])
a:voice_show(a:rows()/2, a:cols()/2, levs, num)
os.exit(0)


