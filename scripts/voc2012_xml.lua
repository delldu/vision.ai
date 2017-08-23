require "LuaXML"
require "vision"

xmlname = "/home/dell/DataSet/VOCdevkit/VOC2012/Annotations/" .. arg[1] .. ".xml"
imgname = "/home/dell/DataSet/VOCdevkit/VOC2012/JPEGImages/" .. arg[1] .. ".jpg"

xfile = xml.load(xmlname)

function tag_value(o, tag)
	local x
	x = o:find(tag)
	if x ~= nil then
		return tonumber(x[1])
	end
	return 0
end


function box_parse(o)
	local b = {}
	x = o:find("name")
	if x ~= nil then
		b.name = x[1]
	end
	b.xmin = tag_value(o, "xmin");
	b.xmax = tag_value(o, "xmax");
	b.ymin = tag_value(o, "ymin");
	b.ymax = tag_value(o, "ymax");
	return b
end

function object_parse(xml)
	local boxs = {}

	local o
	for i = 1, #xml do
		o = xml[i]
		if o:tag() == "object" then
			boxs[#boxs + 1] = box_parse(o)
		end
	end
	return boxs
end

boxs = object_parse(xfile)
print(boxs)

img = vision.image.load(imgname)
print(imgname)

for i = 1, #boxs do
	img:draw_box(boxs[i].ymin, boxs[i].xmin, boxs[i].ymax, boxs[i].xmax, 0x00ffff)
	img:draw_text(boxs[i].ymin, boxs[i].xmin, string.format("%d", i), 0xff0000)
	img:draw_text(15*(i - 1) + 1, 1, string.format("%d.%s", i, boxs[i].name), 0xff0000)
	print("box:", i, "width:", boxs[i].xmax - boxs[i].xmin, "height:", boxs[i].ymax - boxs[i].ymin, boxs[i].name) 
end
img:show()

