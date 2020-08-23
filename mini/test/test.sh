#/************************************************************************************
#***
#***	Copyright 2020 Dell(18588220928g@163.com), All Rights Reserved.
#***
#***	File Author: Dell, 2020-08-23 17:50:52
#***
#************************************************************************************/
#
#! /bin/sh

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

