case $1 in
	01)
	top=120
	left=120
	;;
	02)
	top=220
	left=40
	;;
	03)
	top=0
	left=80
	;;
	04)
	top=10
	left=10
	;;
	*)
	echo "Please choice 01-04"
	exit
	;;
esac


qlua image_blending.lua document/blend/src_img$1.jpg document/blend/mask_img$1.jpg document/blend/tar_img$1.jpg $top $left

