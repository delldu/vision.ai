#/************************************************************************************#***
#***	Copyright Dell 2020, All Rights Reserved.
#***
#***	File Author: Dell, 2020年 08月 23日 星期日 22:39:11 CST
#***
#************************************************************************************/
#
#! /bin/sh

func_test()
{
	# load bmp
	./test -i images/src.bmp -o images/dst_bmp_bmp.bmp
	./test -i images/src.bmp -o images/dst_bmp_jpg.bmp
	./test -i images/src.bmp -o images/dst_bmp_jpeg.bmp

	# load jpg
	./test -i images/src.jpg -o images/dst_jpg_bmp.bmp
	./test -i images/src.jpg -o images/dst_jpg_jpg.bmp
	./test -i images/src.jpg -o images/dst_jpg_jpeg.bmp

	# load jpeg
	./test -i images/src.jpeg -o images/dst_jpeg_bmp.bmp
	./test -i images/src.jpeg -o images/dst_jpeg_jpg.bmp
	./test -i images/src.jpeg -o images/dst_jpeg_jpeg.bmp

	# load pbm
	./test -i images/src.pbm -o images/dst_pbm_bmp.bmp
	./test -i images/src.pbm -o images/dst_pbm_jpg.bmp
	./test -i images/src.pbm -o images/dst_pbm_jpeg.bmp

	# load pgm
	./test -i images/src.pgm -o images/dst_pgm_bmp.bmp
	./test -i images/src.pgm -o images/dst_pgm_jpg.bmp
	./test -i images/src.pgm -o images/dst_pgm_jpeg.bmp

	# load ppm
	./test -i images/src.ppm -o images/dst_ppm_bmp.bmp
	./test -i images/src.ppm -o images/dst_ppm_jpg.bmp
	./test -i images/src.ppm -o images/dst_ppm_jpeg.bmp
}

perf_test()
{
	time ./test -r /tmp/speed.jpg
}


help()
{
	echo "Help Test"
	echo "./test.sh func_test -- Test functions"
	echo "./test.sh perf_test -- Test performance"
}


if [ "$*" == "" ] ;
then
	help
else
	eval "$*"
fi
