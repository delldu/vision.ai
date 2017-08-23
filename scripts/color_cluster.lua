require "vision"

a = vision.image.load(arg[1])
a = a:zoom(600, 640)
num = tonumber(arg[2])
for i = 1, 1 do
	a:color_cluster(num)
	--a:gauss_filter()
	--a:bilate_filter()
end
a:show()
--os.exit(0)


