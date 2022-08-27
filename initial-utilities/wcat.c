#include <stdio.h>
#include <stdlib.h>
int main(int argc, char* argv[]){
	if ( argc == 1 ) {
		exit(0);
	}
	char buf[100];
	int SIZE = 100;
	for (int i = 1; i < argc; i++) {
		FILE* f = fopen(argv[i], "r");
		if (f == NULL) {
		fprintf(stdout, "wcat: cannot open file\n");
		exit(1);
		}
		while (fgets(buf, SIZE, f) != NULL) {
			fprintf(stdout, "%s", buf);	
		}
		fclose(f);
	}
	return 0;
}
