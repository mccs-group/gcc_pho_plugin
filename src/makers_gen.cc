//! This program generates pass_makers.cc from file with "pass_class pass_name" pairs

#include "cstdlib"
#include "stdio.h"
#include "string.h"

int main(int argc, char* argv[]) {
	if (argc != 2) {
		fprintf(stderr, "Incorrect number of arguments\n");
		return -1;
	}
	
	FILE* passes_file = fopen(argv[1], "r");
	if (passes_file == NULL) {
		fprintf(stderr, "Could not open file with passes to generate makers for\n");
		return -1;
	}

	FILE* makers_file = fopen("pass_makers.cc", "w");
	if (passes_file == NULL) {
		fprintf(stderr, "Could not create file for makers\n");
		fclose(passes_file);
		return -1;
	}

	fprintf(makers_file, "opt_pass* pass_by_name(const char* name) {\n");
	
	char* line = NULL;
	size_t len = 0;
	while (getline(&line, &len, passes_file) != -1) {
		char* space_ptr = strchr(line, ' ');
		if (*space_ptr == 0) {
			fprintf(stderr, "Passes file is corrupted (missing whitespace) on line [%s]\n", line);
			free(line);
			fclose(passes_file);
			fclose(makers_file);
			return -1;
		}
		*space_ptr = 0;
		space_ptr += 1;

		int namelen = strlen(space_ptr);

		fprintf(makers_file, "\tif (!strcmp(name, \"%.*s\")) {\n\t\treturn make_%s(g);\n\t}\n", namelen - 1, space_ptr, line);
	}
	fprintf(makers_file, "\treturn NULL;\n}");

	free(line);
	fclose(passes_file);
	fclose(makers_file);
}
