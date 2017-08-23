require "vision"

dst = vision.image.load(arg[1])
src = vision.image.load(arg[2])

dst:blend_cluster(src)
dst : show()


