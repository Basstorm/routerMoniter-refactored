#include "utils.h"

#include <cstdio>

double bytes_to_human_size(double bytes, char **human_size) {
	static const char *suffix[] = {"K/s", "M/s", "G/", "T/s"};
	char length = sizeof(suffix) / sizeof(suffix[0]);

	int i = 0;
	double result = bytes;
	while (result > 1024.0 && i < length - 1) {
		result /= 1024;
		i++;
	}

	*human_size = (char *)suffix[i];
	return result;
}