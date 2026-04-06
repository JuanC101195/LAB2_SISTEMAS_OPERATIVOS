#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

static char **search_path = NULL;
static int path_count = 0;

void print_error(void) {
    char error_message[] = "An error has occurred\n";
    write(STDERR_FILENO, error_message, strlen(error_message));
}

void init_path(void) {
    search_path = malloc(2 * sizeof(char*));
    search_path[0] = strdup("/bin");
    search_path[1] = NULL;
    path_count = 1;
}

void free_path(void) {
    for (int i = 0; i < path_count; i++) free(search_path[i]);
    free(search_path);
    search_path = NULL;
    path_count = 0;
}

void set_path(char **new_dirs, int count) {
    free_path();
    search_path = malloc((count + 1) * sizeof(char*));
    for (int i = 0; i < count; i++) search_path[i] = strdup(new_dirs[i]);
    search_path[count] = NULL;
    path_count = count;
}

char *find_executable(const char *cmd) {
    if (strchr(cmd, '/')) {
        if (access(cmd, X_OK) == 0) return strdup(cmd);
        return NULL;
    }
    for (int i = 0; i < path_count; i++) {
        char *full = malloc(strlen(search_path[i]) + strlen(cmd) + 2);
        sprintf(full, "%s/%s", search_path[i], cmd);
        if (access(full, X_OK) == 0) return full;
        free(full);
    }
    return NULL;
}

// Tokenizador que trata '>' como token independiente
char **tokenize(char *input, int *count) {
    char **tokens = NULL;
    *count = 0;
    char *p = input;

    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == '\n') p++;
        if (*p == '\0') break;

        if (*p == '>') {
            tokens = realloc(tokens, (*count + 2) * sizeof(char*));
            tokens[(*count)++] = strdup(">");
            p++;
        } else {
            char *start = p;
            while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '>') p++;
            tokens = realloc(tokens, (*count + 2) * sizeof(char*));
            tokens[(*count)++] = strndup(start, p - start);
        }
    }
    if (tokens) tokens[*count] = NULL;
    else { tokens = malloc(sizeof(char*)); tokens[0] = NULL; }
    return tokens;
}

int parse_command(char *cmd_str, char ***argv_out, char **redir_file) {
    *redir_file = NULL;
    *argv_out = NULL;

    int tok_count = 0;
    char **tokens = tokenize(cmd_str, &tok_count);

    if (tok_count == 0) {
        *argv_out = calloc(1, sizeof(char*));
        free(tokens);
        return 0;
    }

    int redir_pos = -1;
    for (int i = 0; i < tok_count; i++) {
        if (strcmp(tokens[i], ">") == 0) {
            if (redir_pos != -1) goto parse_error; // doble redirección
            if (i + 1 >= tok_count) goto parse_error; // nada después del >
            redir_pos = i;
            *redir_file = strdup(tokens[i + 1]);
        }
    }

    // Verificar tokens extra después del archivo de redirección
    if (redir_pos != -1) {
        for (int i = redir_pos + 2; i < tok_count; i++) {
            if (strcmp(tokens[i], ">") != 0) goto parse_error;
        }
    }

    int argv_count = 0;
    char **argv = malloc(sizeof(char*));
    for (int i = 0; i < tok_count; i++) {
        if (redir_pos != -1 && (i == redir_pos || i == redir_pos + 1)) continue;
        if (strcmp(tokens[i], ">") == 0) goto parse_error; // > sin archivo
        argv = realloc(argv, (argv_count + 2) * sizeof(char*));
        argv[argv_count++] = strdup(tokens[i]);
    }
    argv[argv_count] = NULL;

    for (int i = 0; i < tok_count; i++) free(tokens[i]);
    free(tokens);
    *argv_out = argv;
    return 0;

parse_error:
    for (int i = 0; i < tok_count; i++) free(tokens[i]);
    free(tokens);
    if (*redir_file) { free(*redir_file); *redir_file = NULL; }
    return -1;
}

char **split_parallel_commands(char *line, int *out_count) {
    char **cmds = NULL;
    int count = 0;
    char *start = line;
    char *p = line;

    while (*p) {
        if (*p == '&') {
            *p = '\0';
            char *s = start;
            while (*s == ' ' || *s == '\t') s++;
            if (strlen(s) > 0) {
                cmds = realloc(cmds, (count + 2) * sizeof(char*));
                cmds[count++] = strdup(start);
            }
            start = p + 1;
        }
        p++;
    }
    // Último fragmento
    char *s = start;
    while (*s == ' ' || *s == '\t') s++;
    if (strlen(s) > 0) {
        cmds = realloc(cmds, (count + 2) * sizeof(char*));
        cmds[count++] = strdup(start);
    }
    if (!cmds) { cmds = malloc(sizeof(char*)); }
    cmds[count] = NULL;
    *out_count = count;
    return cmds;
}

static int is_builtin(const char *name) {
    return (strcmp(name,"exit")==0 || strcmp(name,"cd")==0 ||
            strcmp(name,"chd")==0 || strcmp(name,"path")==0 ||
            strcmp(name,"route")==0);
}

int process_command(char *cmd_str, int parallel) {
    // Trim
    while (*cmd_str == ' ' || *cmd_str == '\t') cmd_str++;
    char *end = cmd_str + strlen(cmd_str) - 1;
    while (end > cmd_str && (*end == ' ' || *end == '\t' || *end == '\n')) end--;
    *(end + 1) = '\0';
    if (strlen(cmd_str) == 0) return 0;

    char **argv = NULL;
    char *redir_file = NULL;
    if (parse_command(cmd_str, &argv, &redir_file) != 0) {
        print_error();
        return -1;
    }
    if (!argv || !argv[0]) {
        free(argv);
        if (redir_file) free(redir_file);
        return 0;
    }

    int ret = 0;

    if (strcmp(argv[0], "exit") == 0) {
        if (argv[1]) { print_error(); ret = -1; }
        else exit(0);
    }
    else if (strcmp(argv[0], "cd") == 0 || strcmp(argv[0], "chd") == 0) {
        if (!argv[1] || argv[2]) { print_error(); ret = -1; }
        else if (chdir(argv[1]) != 0) { print_error(); ret = -1; }
    }
    else if (strcmp(argv[0], "path") == 0 || strcmp(argv[0], "route") == 0) {
        int args = 0;
        while (argv[args]) args++;
        args--;
        set_path(argv + 1, args);
    }
    else {
        char *exec_path = find_executable(argv[0]);
        if (!exec_path) { print_error(); ret = -1; goto cleanup; }

        pid_t pid = fork();
        if (pid == 0) {
            if (redir_file) {
                int fd = open(redir_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) { print_error(); exit(1); }
                dup2(fd, STDOUT_FILENO);
                dup2(fd, STDERR_FILENO);
                close(fd);
            }
            execv(exec_path, argv);
            print_error();
            exit(1);
        } else if (pid < 0) {
            print_error(); ret = -1;
        } else {
            if (!parallel) waitpid(pid, NULL, 0);
        }
        free(exec_path);
    }

cleanup:
    for (int i = 0; argv[i]; i++) free(argv[i]);
    free(argv);
    if (redir_file) free(redir_file);
    return ret;
}

void run_parallel_commands(char *line) {
    int num_cmds = 0;
    char **cmds = split_parallel_commands(line, &num_cmds);

    if (num_cmds == 0) { free(cmds); return; }

    if (num_cmds == 1) {
        process_command(cmds[0], 0);
        free(cmds[0]); free(cmds);
        return;
    }

    // Verificar built-ins en paralelo (no permitido)
    for (int i = 0; i < num_cmds; i++) {
        char *tmp = strdup(cmds[i]);
        char **argv = NULL; char *redir = NULL;
        if (parse_command(tmp, &argv, &redir) == 0 && argv && argv[0]) {
            if (is_builtin(argv[0])) {
                print_error();
                if (argv) { for(int j=0;argv[j];j++) free(argv[j]); free(argv); }
                if (redir) free(redir);
                free(tmp);
                for (int k=0;k<num_cmds;k++) free(cmds[k]);
                free(cmds);
                return;
            }
        }
        if (argv) { for(int j=0;argv[j];j++) free(argv[j]); free(argv); }
        if (redir) free(redir);
        free(tmp);
    }

    // Lanzar todos en paralelo
    pid_t *pids = malloc(num_cmds * sizeof(pid_t));
    for (int i = 0; i < num_cmds; i++) {
        char **argv = NULL; char *redir = NULL;
        if (parse_command(cmds[i], &argv, &redir) != 0 || !argv || !argv[0]) {
            print_error();
            pids[i] = -1;
            if (argv) { for(int j=0;argv[j];j++) free(argv[j]); free(argv); }
            if (redir) free(redir);
            continue;
        }

        char *exec_path = find_executable(argv[0]);
        if (!exec_path) { print_error(); pids[i] = -1; }
        else {
            pid_t pid = fork();
            if (pid == 0) {
                if (redir) {
                    int fd = open(redir, O_WRONLY|O_CREAT|O_TRUNC, 0644);
                    if (fd >= 0) { dup2(fd,STDOUT_FILENO); dup2(fd,STDERR_FILENO); close(fd); }
                }
                execv(exec_path, argv);
                exit(1);
            }
            pids[i] = pid;
            free(exec_path);
        }
        for(int j=0;argv[j];j++) free(argv[j]); free(argv);
        if (redir) free(redir);
    }

    for (int i = 0; i < num_cmds; i++) {
        if (pids[i] > 0) waitpid(pids[i], NULL, 0);
    }

    free(pids);
    for (int i = 0; i < num_cmds; i++) free(cmds[i]);
    free(cmds);
}

void shell_loop(FILE *input, int interactive) {
    char *line = NULL;
    size_t len = 0;
    ssize_t nread;
    while (1) {
        if (interactive) { printf("wish> "); fflush(stdout); }
        nread = getline(&line, &len, input);
        if (nread == -1) { free(line); exit(0); }
        if (line[nread-1] == '\n') line[nread-1] = '\0';
        if (strlen(line) == 0) continue;
        run_parallel_commands(line);
    }
    free(line);
}

int main(int argc, char *argv[]) {
    init_path();
    if (argc == 1) {
        shell_loop(stdin, 1);
    } else if (argc == 2) {
        FILE *batch = fopen(argv[1], "r");
        if (!batch) { print_error(); exit(1); }
        shell_loop(batch, 0);
        fclose(batch);
    } else {
        print_error();
        exit(1);
    }
    free_path();
    return 0;
}