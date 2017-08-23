require "vision"

a = vision.image.load(arg[1])
a:gray()
--a = a:zoom(600, 800)
--a:shape_midedge()
--a:bilate_filter()
--a:gauss_filter()
a:shape_bestedge()
--a:shape_midedge()
a:show()
