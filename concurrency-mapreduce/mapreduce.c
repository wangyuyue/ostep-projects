#include "mapreduce.h"
#include <stdlib.h>
#include <stdio.h> 
#include <string.h>
#include <pthread.h>

#define KEY_LEN 20
#define VAL_LEN 10
#define FILE_MAX_PAIR_NUMBER 10000
// Different function pointer types used by MR
typedef char *(*Getter)(char *key, int partition_number);
typedef void (*Mapper)(char *file_name);
typedef void (*Reducer)(char *key, Getter get_func, int partition_number);
typedef unsigned long (*Partitioner)(char *key, int num_partitions);

typedef struct mutex_vector {
	pthread_mutex_t mutex;	
	FILE* f;
	char tmp_val[VAL_LEN];
} mutex_vector;

typedef struct hash_table {
	mutex_vector* vectors;	
} hash_table;

void interm_file(char* fname, int num) {
	strcpy(fname, "interm_file_");
	char str_num[8];
	sprintf(str_num, "%d", num);
	strcat(fname, str_num);
}

void init_interm_files(int n_partition, hash_table* tbl) {
	tbl->vectors = malloc(n_partition * sizeof(mutex_vector));	
	for (int i = 0; i < n_partition; i++) {
		pthread_mutex_init(&tbl->vectors[i].mutex, NULL);
		char fname[20];
		interm_file(fname, i);
		tbl->vectors[i].f = fopen(fname, "w+");
	}
}

typedef struct mapreduce_context {
	Partitioner partition;
	int n_mapper;
	int n_reducer;
	hash_table tbl;
} mapreduce_context;

mapreduce_context cxt;


void init_mapreduce_context(Partitioner partition, int n_mapper, int n_reducer) {
	printf("init mapreduce context...\n");
	fflush(stdout);
	cxt.partition = partition;
	cxt.n_mapper = n_mapper;
	cxt.n_reducer = n_reducer;
	init_interm_files(n_reducer, &(cxt.tbl));
}

typedef struct kv_pair {
	char key[KEY_LEN];
	char val[VAL_LEN];
} kv_pair;

void MR_Emit(char *key, char *value) {
	unsigned long partition = cxt.partition(key, cxt.n_reducer);	
	pthread_mutex_lock(&cxt.tbl.vectors[partition].mutex);
	kv_pair pair;
	strcpy(pair.key, key);
	strcpy(pair.val, value);
	fwrite(&pair, sizeof(kv_pair), 1, cxt.tbl.vectors[partition].f);
	pthread_mutex_unlock(&cxt.tbl.vectors[partition].mutex);
}

unsigned long MR_DefaultHashPartition(char *key, int num_partitions) {
	unsigned long hash = 5381;
  int c;
  while ((c = *key++) != '\0')
    hash = hash * 33 + c;
  return hash % num_partitions;
} 

typedef struct map_arg {
	Mapper map_function; 
	char* file_name;	
} map_arg;

void* map_wrapper(void* arg) {
	map_arg* args = (map_arg*) arg;
	printf("map thread %s begins...\n", args->file_name);
	fflush(stdout);
	args->map_function(args->file_name);
	free(args);
	return NULL;
}

typedef struct reduce_arg {
	Reducer reduce_function;
	int partition_number; 		
	Getter get_next;
} reduce_arg;

char* getter(char *key, int partition_number) {
	FILE* f = cxt.tbl.vectors[partition_number].f;		
	kv_pair pair;
	int cnt = fread(&pair, sizeof(kv_pair), 1, f);
	if (cnt == 0)
			return NULL;
	if (strcmp(pair.key, key) == 0)  {
		strcpy(cxt.tbl.vectors[partition_number].tmp_val, pair.val);
		return cxt.tbl.vectors[partition_number].tmp_val;
	}
	fseek(f, -sizeof(kv_pair), SEEK_CUR);
	return NULL;
}

void* reduce_wrapper(void* arg) {
	reduce_arg* args = (reduce_arg*) arg;
	printf("begin reduce %d\n", args->partition_number);
	
	char fname[20];
	interm_file(fname, args->partition_number); 
	FILE* f = fopen(fname, "r");

	cxt.tbl.vectors[args->partition_number].f = f;		

	kv_pair pair;
	int cnt;
	while((cnt = fread(&pair, sizeof(kv_pair), 1, f)) != 0) {
		fseek(f, -sizeof(kv_pair), SEEK_CUR);
		args->reduce_function(pair.key, args->get_next, args->partition_number);
	}
	fclose(f);
	free(args);
	return NULL;
}

int cmp_func(const void* a, const void* b) {
	kv_pair* pa = (kv_pair*) a;
	kv_pair* pb = (kv_pair*) b;
	return strcmp(pa->key, pb->key);
}

void sort(char* fname) {
	printf("sort %s...\n", fname);
	kv_pair pairs[FILE_MAX_PAIR_NUMBER];

	FILE* f = fopen(fname, "r");
	int number = fread(pairs, sizeof(kv_pair), FILE_MAX_PAIR_NUMBER, f);
	fclose(f);

	qsort(pairs, number, sizeof(kv_pair), cmp_func);
	f = fopen(fname, "w");
	fwrite(pairs, sizeof(kv_pair), number, f);	
	fclose(f);
}

void MR_Run(int argc, char *argv[], 
	    Mapper map, int num_mappers, 
	    Reducer reduce, int num_reducers, 
	    Partitioner partition) {

  init_mapreduce_context(partition, num_mappers, num_reducers);

	pthread_t* mapper = malloc(num_mappers * sizeof(pthread_t));
	
	for (int i = 0; i < num_mappers; i++) {
		map_arg* args = malloc(sizeof(map_arg));
		args->map_function = map; 
		args->file_name = argv[i + 1];
		pthread_create(&mapper[i], NULL, map_wrapper, (void*)args);
	}

	for (int i = 0; i < num_mappers; i++) {
		pthread_join(mapper[i], NULL);
		printf("map thread %d join...\n", i);
	}

	free(mapper);
	
	for (int i = 0; i < num_reducers; i++) {
		fclose(cxt.tbl.vectors[i].f);
		char fname[20];
		interm_file(fname, i); 		
		sort(fname);	
	}
	pthread_t* reducer = malloc(num_reducers * sizeof(pthread_t));
	
	for (int i = 0; i < num_reducers; i++) {
		reduce_arg* args = malloc(sizeof(reduce_arg));
		args->reduce_function = reduce;
		args->partition_number = i; 		
		args->get_next = getter;
		pthread_create(&reducer[i], NULL, reduce_wrapper, (void*)args);	
	}
	
	for (int i = 0; i < num_reducers; i ++) {
		pthread_join(reducer[i], NULL);
	}
	free(reducer);
}


