#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main (int argc, char* argv[]) {
	char* seeds[13] = {"apple", "banana", "cucamber", "pineapple", "orange", "pear", "strawberry", "raspberry", "blueberry", "peach", "tomato", "tree", "bicycle"};
	int n_map = 10;
	int num_infile[10] = {50, 60, 30, 40, 50, 45, 90, 85, 75, 33};
	int num_word[13]; 
	memset(num_word, 0, 13 * sizeof(int));
	for (int i = 0; i < 13; i++) {
		printf("%s count: %d\n", seeds[i], num_word[i]); 
	}
	char file_prefix[20] = "input_";
	for (int i = 0; i < n_map; i++) {
		char file[20];
		char str_num[5];
		sprintf(str_num, "%d", i);
		strcpy(file, file_prefix);
		strcat(file, str_num);
		FILE* f = fopen(file, "w");

		for (int j = 0; j < num_infile[i]; j++) {
		 	int x = rand() % 13;
			fprintf(f, "%s ", seeds[x]);
			num_word[x]++;
		} 
		fclose(f); 
	}
	FILE* f = fopen("word_count", "w");
	for (int i = 0; i < 13; i++) {
		fprintf(f, "%s %d\n", seeds[i], num_word[i]);
	}
	fclose(f);
}
