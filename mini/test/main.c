/************************************************************************************
***
***	Copyright 2020 Dell(18588220928g@163.com), All Rights Reserved.
***
***	File Author: Dell, 2020-08-23 17:47:32
***
************************************************************************************/


#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>

#include "image.h"

void help(char *cmd)
{
	printf("Usage: %s [option]\n", cmd);
	printf("Convert input image to output.\n");
	printf("    -h, --help                   Display this help.\n");
	printf("    -i, --input                  Input image file name.\n");
	printf("    -o, --output                 Output image file name.\n");
	printf("    -r, --repeat                 Repeat image file 1000 times.\n");

	exit(1);
}

int main(int argc, char **argv)
{
	int optc, i;
	int option_index = 0;
	char *input_filename = NULL;
	char *output_filename = NULL;
	char *repeat_filename = NULL;

	IMAGE *image;

	struct option long_opts[] = {
		{ "help", 0, 0, 'h'},
		{ "input", required_argument, 0, 'i'},
		{ "output", required_argument, 0, 'o'},
		{ "repeat", required_argument, 0, 'r'},
		{ 0, 0, 0, 0}

	};

	if (argc <= 1)
		help(argv[0]);
	
	while ((optc = getopt_long(argc, argv, "h i: o: r:", long_opts, &option_index)) != EOF) {
		switch (optc) {
		case 'i':
			input_filename = optarg;
			break;
		case 'o':
			output_filename = optarg;
			break;
		case 'r':
			repeat_filename = optarg;
			break;
		case 'h':	// help
		default:
			help(argv[0]);
			break;
	    	}
	}

	if (input_filename && output_filename) {
		image = image_load(input_filename);
		if (image_valid(image)) {
			image_save(image, output_filename);
			image_destroy(image);
		}
	} else if (repeat_filename) {
		for (i = 0; i < 1000; i++) {
			image = image_load(repeat_filename);
			image_destroy(image);
		}
	} else {
		help(argv[0]);
	}

	return 0;
}
