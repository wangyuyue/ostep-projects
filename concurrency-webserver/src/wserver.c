#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include "request.h"
#include "io_helper.h"

char default_root[] = ".";

pthread_t* thread_pool;

pthread_cond_t cv_nonempty;
pthread_cond_t cv_full;
pthread_cond_t cv_idle;

pthread_mutex_t mtx;

time_t now;
int listen_fd;
int* fd_buf;
int n_fd = 0;

int n_thread;
int n_idle_thr = 0;

int end_signal = 0;
void* process_routine(void* args) {
  int id = *((int*)args);
  printf("thread %d created\n", id);
  while(1) {
    pthread_mutex_lock(&mtx);    
    while (n_fd == 0) {
      printf("thread %d wait...\n", id);
      n_idle_thr++;
      if (n_idle_thr == n_thread) {
        pthread_cond_signal(&cv_idle);
      }
      pthread_cond_wait(&cv_nonempty, &mtx);
      if (end_signal) {
        printf("thread %d delete...\n", id);
        free(args);
        pthread_mutex_unlock(&mtx);
        return NULL;
      }
      n_idle_thr--;
    }    
    int conn_fd = fd_buf[0];
    for (int i = 0; i < n_fd - 1; i++) {
      fd_buf[i] = fd_buf[i + 1];
    }
    n_fd--;
    pthread_cond_signal(&cv_full);
    pthread_mutex_unlock(&mtx);

    //time(&now);
    //printf("%s thread %d process fd %d\n", ctime(&now), id, conn_fd);
		request_handle(conn_fd);
    close_or_die(conn_fd);
    //time(&now);
    //printf("%s thread %d finish processing fd %d\n", ctime(&now), id, conn_fd);

  }
} 

void end_routine() {
  printf("main thread receive end signal\n");
  pthread_mutex_lock(&mtx);
  if (n_idle_thr != n_thread)
    pthread_cond_wait(&cv_idle, &mtx);
  printf("wake up all thread to delete\n");
  pthread_cond_broadcast(&cv_nonempty);
  pthread_mutex_unlock(&mtx);
  for (int i = 0; i < n_thread; i++) {
    pthread_join(thread_pool[i], NULL);
    printf("thread %d deleted successfully\n", i);
  }
  free(thread_pool);
  free(fd_buf);
  close_or_die(listen_fd);
  exit(0);
}

void sigint_handler(int signum) {
  end_signal = 1; 
  if (pthread_mutex_trylock(&mtx) == 0) {
    printf("main thread doesn't have lock\n");
    pthread_mutex_unlock(&mtx);
    end_routine();
  }
}
//
// ./wserver [-d <basedir>] [-p <portnum>] 
// 
int main(int argc, char *argv[]) {
    int c;
    char *root_dir = default_root;
    int port = 10000;
    n_thread = 1;
    int sz_buf = 1;
    
    while ((c = getopt(argc, argv, "d:p:t:b:")) != -1)
			switch (c) {
			case 'd':
	    root_dir = optarg;
	    break;
			case 'p':
	    port = atoi(optarg);
	    break;
      case 't':
      n_thread = atoi(optarg);
      break;
      case 'b':
      sz_buf = atoi(optarg);
      break;
			default:
	    fprintf(stderr, "usage: wserver [-d basedir] [-p port]\n");
	    exit(1);
	}
   
  signal(SIGINT, sigint_handler); 

  assert(root_dir[0] != '/');
  assert(strstr(root_dir, "..") == NULL);
  chdir_or_die(root_dir);
  
  fd_buf = malloc(sizeof(int) * sz_buf); 

  pthread_mutex_init(&mtx, NULL);
  thread_pool = malloc(n_thread * sizeof(pthread_t));

  for (int i = 0; i < n_thread; i++) {
    int* id = malloc(sizeof(int));
    *id = i;
    pthread_create(&thread_pool[i], NULL, process_routine, (void*)id);
  }
  if (pthread_cond_init(&cv_nonempty, NULL) != 0) { exit(-1); }
  if (pthread_cond_init(&cv_full, NULL) != 0) { exit(-1); }
  if (pthread_cond_init(&cv_idle, NULL) != 0) { exit(-1); }

  // now, get to work
  listen_fd = open_listen_fd_or_die(port);
  while (1) {
    if (end_signal) {
      end_routine();
    }
	  struct sockaddr_in client_addr;
		int client_len = sizeof(client_addr);
		int conn_fd = accept_or_die(listen_fd, (sockaddr_t *) &client_addr, (socklen_t *) &client_len);
    pthread_mutex_lock(&mtx);
    while (n_fd == sz_buf) {
      printf("buffer full, wait for signal...\n");
      pthread_cond_wait(&cv_full, &mtx);
    }
    printf("add fd %d to buffer\n", conn_fd);
    fd_buf[n_fd] = conn_fd;
    n_fd++;
    pthread_cond_signal(&cv_nonempty);
    pthread_mutex_unlock(&mtx);
  }
}


    


 
