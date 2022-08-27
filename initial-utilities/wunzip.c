#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "wzip.h"

int main(int argc, char* argv[]) {

	if (argc == 1) {
		fprintf(stdout, "wunzip: file1 [file2 ...]\n");
		exit(1);
	}

	for (int idx_arg = 1; idx_arg < argc; idx_arg++) {
		FILE* f = fopen(argv[idx_arg], "r");

		if (f == NULL) {
			fprintf(stdout, "wunzip: cannot open file\n");
			exit(1);
		}

		zip_unit units[100];
		size_t n_read;
		while((n_read = fread(units, sizeof(zip_unit), 100, f)) != 0) {
			for (int idx_unit = 0; idx_unit < n_read; idx_unit++) {
				 for (int cnt = 0; cnt < units[idx_unit].num; cnt++) {
					fprintf(stdout, "%c", units[idx_unit].c);
				}
			}	
		}
		fclose(f);
	}
}
