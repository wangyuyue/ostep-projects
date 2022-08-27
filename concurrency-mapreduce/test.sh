gcc -Wall -o input_gen input_gen.c
./input_gen

gcc -Wall -o testbench main.c mapreduce.c -lpthread
./test_bench
