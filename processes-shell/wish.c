#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <wait.h>
#include <signal.h>
#include <errno.h>
#include <string.h>


void clean_argument_list(int argc, char* argv[]) {
	//printf("clean argument list\n");
	//fflush(stdout);
	if (argv[0] != NULL) { free(argv[0]); }
	for (int i = 1; i < argc; i++)
		free(argv[i]);
	free(argv);
}

#define MAX_JOB_NUM 100
typedef struct job {
	int pid;
	int status; // 0: no process, 1: fg, 2:bg, 3:stopped
	int argc;
	char** argv;
} job;

job job_list[MAX_JOB_NUM];
int n_job;

void init_job_list() {
	memset(job_list, 0, MAX_JOB_NUM * sizeof(job));	
	n_job = 0;
}

void add_job(int pid, int status, int argc, char* argv[]) {
	job_list[n_job].pid = pid;
	job_list[n_job].status = status;
	job_list[n_job].argc = argc;
	job_list[n_job].argv = argv;

	n_job++;
}

void del_job(int pid) {
	//printf("del job pid %d\n", pid);
	int index = -1;
	for (int i = 0; i < n_job; i++) {
		if (job_list[i].pid == pid) {
			index = i;
			clean_argument_list(job_list[i].argc, job_list[i].argv);
		}
	}
	for (int i = index; i < n_job - 1; i++) {
		job_list[i] = job_list[i + 1];
	}
	job_list[n_job].pid = 0;
	job_list[n_job].status = 0;
	job_list[n_job].argc = 0;
	job_list[n_job].argv = NULL;
	n_job--;
}

void upd_job(int pid, int status) {
	for (int i = 0; i < n_job; i++) {
		if (job_list[i].pid == pid) {
			job_list[i].status = status;
		}
	}
}

int find_pid(int pid) {
	for (int i = 0; i < n_job; i++) {
		if (job_list[i].pid == pid)
			return i;
	}	
	return -1;
}

void print_job_list() {
	sigset_t prev_mask, chld_mask;
	sigemptyset(&chld_mask);
	sigaddset(&chld_mask, SIGCHLD);
	sigprocmask(SIG_BLOCK, &chld_mask, &prev_mask);
	if (n_job == 0)
		printf("there are no running jobs.\n");
	for (int i = 0; i < n_job; i++) {
		char status[4][15] = {"none", "foreground", "background", "stopped"};
		char cmd[100];
		strcpy(cmd, job_list[i].argv[0]);
		for (int j = 1; j < job_list[i].argc; j++) {
			strcat(cmd, " ");
			strcat(cmd, job_list[i].argv[j]);
		}
		printf("%d %s %s\n", job_list[i].pid, status[job_list[i].status], cmd);
	}
	sigprocmask(SIG_SETMASK, &prev_mask, NULL);
}

int has_fg = 0;

int find_fg() {
	for (int i = 0; i < n_job; i ++) {
		if (job_list[i].status == 1)
			return job_list[i].pid;
	}
	return 0;
}

int move_fg(int pid) {
	sigset_t prev_mask, chld_mask;
	sigemptyset(&chld_mask);
	sigaddset(&chld_mask, SIGCHLD);
	sigprocmask(SIG_BLOCK, &chld_mask, &prev_mask);

	int idx = find_pid(pid);
	if (idx == -1 || job_list[idx].status == 0 || job_list[idx].status == 1) {
		printf("error in move fg\n");
		sigprocmask(SIG_SETMASK, &prev_mask, NULL);
		return -1;
	}

	upd_job(pid, 1);
	kill(pid, SIGCONT);

	has_fg = 1;
	while(has_fg) { sigsuspend(&prev_mask); }

	sigprocmask(SIG_SETMASK, &prev_mask, NULL);

	return 0;
}

int move_bg(int pid) {
	sigset_t prev_mask, chld_mask;
	sigemptyset(&chld_mask);
	sigaddset(&chld_mask, SIGCHLD);
	sigprocmask(SIG_BLOCK, &chld_mask, &prev_mask);

	int idx = find_pid(pid);
	if (idx == -1 || job_list[idx].status != 3) {
		sigprocmask(SIG_SETMASK, &prev_mask, NULL);
		return -1;
	}

	upd_job(pid, 2);

	kill(pid, SIGCONT);	

	sigprocmask(SIG_SETMASK, &prev_mask, NULL);
	
	return 0;
}


void child_handler(int signum) {
	sigset_t prev_mask, chld_mask;
	sigemptyset(&chld_mask);
	sigaddset(&chld_mask, SIGCHLD);
	sigprocmask(SIG_BLOCK, &chld_mask, &prev_mask);

	//printf("enter SIGCHLD handler\n");
	int status;
	int pid;
	while ((pid = waitpid(-1, &status, WUNTRACED | WNOHANG)) > 0) {
		if (WIFSTOPPED(status)) {
			printf("process %d stop\n", pid);
			upd_job(pid, 3);
			has_fg = 0;
		} else {
			printf("process %d terminate\n", pid);
			if (has_fg && pid == find_fg())
				has_fg = 0;
			del_job(pid);	
		} 
	}
	sigprocmask(SIG_SETMASK, &prev_mask, NULL);

}

void int_handler(int signum) {
	sigset_t prev_mask, chld_mask;
	sigemptyset(&chld_mask);
	sigaddset(&chld_mask, SIGCHLD);
	sigprocmask(SIG_BLOCK, &chld_mask, &prev_mask);

	//printf("enter SIGINT handler\n");
	if (has_fg) {
		int fg_pid = find_fg();
		printf("interrupt foreground process %d\n", fg_pid);
		kill(fg_pid, SIGTERM);
	}
	sigprocmask(SIG_SETMASK, &prev_mask, NULL);
}

void stop_handler(int signum) {
	sigset_t prev_mask, chld_mask;
	sigemptyset(&chld_mask);
	sigaddset(&chld_mask, SIGCHLD);
	sigprocmask(SIG_BLOCK, &chld_mask, &prev_mask);

	//printf("enter SIGTSTP handler\n");
	if (has_fg) {
		int fg_pid = find_fg();
		printf("stop fg process %d\n", fg_pid);
		kill(fg_pid, SIGTSTP);
	}	
	sigprocmask(SIG_SETMASK, &prev_mask, NULL);
}

char error_message[30] = "An error has occurred\n";
#define print_error write(STDERR_FILENO, error_message, strlen(error_message))

char var_path[100] = "/bin:/usr/bin:/home/gali/Desktop/ostep-projects/processes-shell";

char* multi_strsep(char** line_ptr, char* delimit) {
	char* tok = NULL;
	while((tok = strsep(line_ptr, delimit)) != NULL) {
		if (strcmp(tok, "") != 0) {
			break;	
		}
	}
	return tok;
}

int set_path(char paths[]) {
	if (strlen(paths) > 99) {
//	print_error;
		printf("len too great\n");
		return -1;
	}
	char* tok = strsep(&paths, " ");
	if (tok == NULL) {
		strcpy(var_path, "");
	} else {
		strcpy(var_path, tok);	
		printf("path :%s<end>\n", tok);
		while((tok = multi_strsep(&paths, " ")) != NULL) {
			strcat(var_path, ":");
			strcat(var_path, tok);
			printf("path :%s<end>\n", tok);
		}
	}
	return 0;
}


char* abs_cmd_path(char* cmd) {
	char* path = NULL; 
	char* tmp_paths, *iter_paths;
	tmp_paths = iter_paths = malloc((strlen(var_path) + 1) * sizeof(char));
	strcpy(iter_paths, var_path);
	while((path = strsep(&iter_paths, ":")) != NULL) {
		char* cmd_path = malloc((strlen(path) + strlen(cmd) + 2) * sizeof(char));
		strcpy(cmd_path, path);
		strcat(cmd_path, "/");
		strcat(cmd_path, cmd);
		if (access(cmd_path, X_OK) != -1) {
			free(tmp_paths);
			return cmd_path;
		}
		free(cmd_path);
	}
	free(tmp_paths);
	return NULL;
}

int extract_arg_out(char* line, char** args[], int* out_fd) {
	char* redir_tok = strchr(line, (int)'>');	
	if (redir_tok != NULL) {
		*redir_tok  = '\0';
		char* remains;
		int append = 0;
		if (redir_tok[1] == '>') {
			remains = redir_tok + 2;
			append = 1;
		} else {
			remains = redir_tok + 1;
		}
		if (strchr(remains, (int)'>') != NULL) {
			printf("more than one > found!!\n");
			print_error;
			return -1;
		}
		char* output = multi_strsep(&remains, " ");
		if (multi_strsep(&remains, " ") != NULL) {
			printf("more than one output found!!\n");
			print_error;
			return -1;
		}
		/*if (append)
		printf("try append file %s<end>\n", output);
		else printf("try write file %s<end>\n", output);*/
		if ((*out_fd = (append == 0) ? open(output, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR) : \
														open(output, O_WRONLY | O_CREAT | O_APPEND , S_IRUSR | S_IWUSR)) == -1) {
			*out_fd = -1;
			printf("failed to open file %s\n", output);
			print_error;
			return -1;
		}
	}
	int len_args = strlen(line);
	int cnt_args = 0;
	char prev = ' ';
	for (int i = 0; i < len_args; i++) {
		if (prev == ' ' && line[i] != ' ') {
			cnt_args++;	
		}
		prev = line[i];
	}

	*args= malloc((cnt_args + 2) * sizeof(char*));
	if (*args == NULL) {
		print_error;
		return -1;
	}

	char* tok = NULL;
	(*args)[0] = NULL;
	int idx_args = 0;

	while((tok = multi_strsep(&line, " ")) != NULL) {
		++idx_args;
		(*args)[idx_args] = malloc((strlen(tok) + 1) * sizeof(char));	
		strcpy((*args)[idx_args], tok); 
		//printf("arg[%d] = %s<end>\n", idx_args, (*args)[idx_args]);
	}
	(*args)[idx_args + 1] = NULL;

	return 1 + cnt_args;
}

int execute(char* cmd, int argc, char* argv[], int out_fd, int fg) {
	char* abs_path = abs_cmd_path(cmd);
	argv[0] = abs_path;
	if (abs_path == NULL) {
		print_error;
		return -1;
	}

	sigset_t prev_mask, int_stp_mask, chld_mask;

	sigemptyset(&int_stp_mask);
  sigaddset(&int_stp_mask, SIGINT);
	sigaddset(&int_stp_mask, SIGTSTP);

	sigemptyset(&chld_mask);
	sigaddset(&chld_mask, SIGCHLD);
	sigprocmask(SIG_BLOCK, &int_stp_mask, &prev_mask);
	sigprocmask(SIG_BLOCK, &chld_mask, NULL);

	pid_t pid = fork(); 
	if (pid == 0) {
		printf("new process %d \n", getpid());
		if (out_fd != -1) {
			dup2(out_fd, STDOUT_FILENO);
		}
		setpgid(0, 0);
		sigprocmask(SIG_SETMASK, &prev_mask, NULL);
		execv(abs_path, argv);
	}
	if (pid != -1) {
		add_job(pid, 2 - fg, argc, argv);
		if (fg) {
			has_fg = 1;
			while(has_fg) { sigsuspend(&prev_mask); }
		}
	} else {
		print_error;
		clean_argument_list(argc, argv);
	}
	sigprocmask(SIG_SETMASK, &prev_mask, NULL);
	return pid;
}

void interactive_mode() {
	char* line = NULL;
	size_t len = 0;
	size_t n_read = 0;
	do {
		if (line && line[n_read - 1] == '\n') { line[n_read - 1] = ' ';	}
		char* parse = line;
		char* cmd = multi_strsep(&parse, " \n");
		if (cmd == NULL) {
			
		} else if (strcmp(cmd, "exit") == 0) {
			if (line != NULL) { free(line); }
			exit(0);
		} else if (strcmp(cmd, "cd") == 0) {
			char* dir = multi_strsep(&parse, " \n");
			if (!dir || chdir(dir) == -1) {
				print_error;
			}
		} else if (strcmp(cmd, "path") == 0) {
			printf("set path...\n");
			set_path(parse);	
		} else if (strcmp(cmd, "$path") == 0) {
			printf("PATH Variable: %s\n", var_path);	
		} else if (strcmp(cmd, "jobs") == 0) {
			print_job_list();
		} else if (strcmp(cmd, "fg") == 0) {
			int pid = atoi(multi_strsep(&parse, " "));
			move_fg(pid);
		} else if (strcmp(cmd, "bg") == 0) {
			int pid = atoi(multi_strsep(&parse, " "));
			move_bg(pid);
		} else {
			char** args = NULL;
			int out_fd = -1;
			int n_args = extract_arg_out(parse, &args, &out_fd);			
			if (n_args == -1) {
				continue;	
			}
			/*printf("arguments:\n");
			for (int i = 0; i <= n_args; i++) {
				if (args[i] == NULL) {
					printf("null<end>\n");
				} else {
					printf("%s<end>\n", args[i]);
				}
			}
			printf("file descriptor: %d\n", out_fd);*/
			execute(cmd, n_args, args, out_fd, 1);
			//printf("close fd %d if not -1\n", out_fd);
			if (out_fd != -1) { close(out_fd); }
		}
		printf("wish> ");
	} while((n_read = getline(&line, &len, stdin)) != -1);	

	if (line != NULL) { free(line); }
}

void batch_mode() {
}

int main (int argc, char* argv[]) {
	signal(SIGCHLD, child_handler);
	signal(SIGINT, int_handler);
	signal(SIGTSTP, stop_handler); 
	if (argc == 1) {
		interactive_mode();	
	} else if (argc == 2) {
		batch_mode(argv[1]);
	} else {
		print_error;
		exit(1);
	}
}
