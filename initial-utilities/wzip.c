#include <stdio.h>
#include <stdlib.h>
#include "wzip.h"

int main(int argc, char* argv[]) {

	if (argc == 1) {
		fprintf(stdout, "wzip: file1 [file2 ...]\n");
		exit(1);
	}

	for (int idx_arg = 1; idx_arg < argc; idx_arg++) {
		FILE* f = fopen(argv[idx_arg], "r");

		if (f == NULL) {
			fprintf(stdout, "wzip: cannot open file\n");
			exit(1);
		}

		char* line = NULL;
		size_t len = 0;
		size_t n_read;
		while((n_read = getline(&line, &len, f)) != -1) {
			/*
			fprintf(stderr, "%s", line);
			fprintf(stderr, "%ld\n", n_read);	
			*/
			zip_unit units[100];
			int num_units = 0;
			units[num_units].num = 1;
			units[num_units].c = line[0];
			for(size_t idx_line = 1; idx_line < n_read; idx_line++) {
				if (units[num_units].c == line[idx_line]) {
					units[num_units].num++;
				} else {
					num_units++;
					if (num_units == 100) {
					/*	
						for (int k = 0; k < num_units; k++) {
							fprintf(stderr, "%d%c", units[k].num, units[k].c);
							fflush(stderr);
						}
					*/
						fwrite(units, sizeof(zip_unit), num_units, stdout);
						num_units = 0;
					}
					units[num_units].num = 1;
					units[num_units].c = line[idx_line];
				}
			}
			/*	
			for (int k = 0; k <= num_units; k++) {
				fprintf(stderr, "%d%c", units[k].num, units[k].c);
				fflush(stderr);
			}
			*/
			fwrite(units, sizeof(zip_unit), num_units + 1, stdout);
		} 
		free(line);
		fclose(f);
	}
	return 0;
}
