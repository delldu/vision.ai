require "vision"
start = tonumber(arg[2])
model = vision.image.load(arg[3])
vision.video.match(arg[1], start, model, tonumber(arg[4]))

