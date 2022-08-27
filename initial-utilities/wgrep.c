#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
int* KMP(const char buf[], int len) {
  // KMP algorithm
	int* kmp = malloc(len * sizeof(int));

	if (kmp == NULL) { exit(-1); }

	kmp[0] = -1;	
	kmp[1] = 0;
	for (int i = 2; i <= len; i++) {
		int t = i  - 1;
		while(buf[i - 1] != buf[kmp[t]]) {
			t = kmp[t];
			if (t == -1)
				break;
		}
		kmp[i] = (t == -1) ? 0 : kmp[t] + 1;
	}	
	return kmp;
}

int match(char buf[], int kmp[], int pattern_length, char line[], int n_read) {
	int p_idx = 0;
	int l_idx = 0;
	while(l_idx < n_read && p_idx < pattern_length) {	
		while(p_idx != -1 && buf[p_idx] != line[l_idx]) {
			p_idx = kmp[p_idx];
		}
		p_idx++;
		l_idx++;
	}
	return p_idx == pattern_length;
}

int main (int argc, char* argv[]) {
	if (argc == 1) {
		fprintf(stdout, "wgrep: searchterm [file ...]\n");
		exit(1);
	}	
	int pattern_length = strlen(argv[1]);

	char* buf = malloc((pattern_length + 1) * sizeof(char));
	if (buf == NULL) { exit(-1); }
	strcpy(buf, argv[1]);

	int* kmp = KMP(buf, pattern_length);
	/*
	for (int i = 0; i < pattern_length; i ++) {
			printf("%d\n", kmp[i]);
	}*/
	
	for (int i = 2; i < argc; i++) {

		FILE* f = fopen(argv[i], "r");
		if (f == NULL) {
				fprintf(stdout, "wgrep: cannot open file\n");
				free(buf);
				free(kmp);
				exit(1);
		}
		
		char* line = NULL;
		size_t len = 0;
		size_t n_read;
		while((n_read = getline(&line, &len, f)) != - 1) {
			if (match(buf, kmp, pattern_length, line, n_read)) {
				fprintf(stdout, "%s", line);
			} 
		}
		if (line != NULL) { free(line); }
		fclose(f);
	}
	if (argc == 2) {
		char line[100];
		while(fgets(line, 100, stdin) != NULL) {
			if (match(buf, kmp, pattern_length, line, strlen(line))) {
				fprintf(stdout, "%s", line);
			}
		}
	}
	free(buf);
	free(kmp);
	return 0;
		
}
