#include "wsh.h"
#include "dynamic_array.h"
#include "utils.h"
#include "hash_map.h"

#include <stdio.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/* ===== Global state ===== */
int rc = EXIT_SUCCESS;
HashMap *alias_hm = NULL;
static DynamicArray *history_da = NULL;

#define RC_EXIT_REQUEST 2 /* internal: user asked to exit */

void wsh_free(void)
{
  if (alias_hm)
  {
    hm_free(alias_hm);
    alias_hm = NULL;
  }
  if (history_da)
  {
    da_free(history_da);
    history_da = NULL;
  }
}

void clean_exit(int return_code)
{
  wsh_free();
  exit(return_code);
}

void wsh_warn(const char *msg, ...)
{
  va_list args;
  va_start(args, msg);
  vfprintf(stderr, msg, args);
  va_end(args);
  rc = EXIT_FAILURE;
}

void parseline_no_subst(const char *cmdline, char **argv, int *argc)
{
  if (!cmdline)
  {
    *argc = 0;
    argv[0] = NULL;
    return;
  }

  char *buf = strdup(cmdline);
  if (!buf)
  {
    perror("strdup");
    clean_exit(EXIT_FAILURE);
  }

  size_t len = strlen(buf);
  if (len > 0 && buf[len - 1] == '\n')
  {
    buf[len - 1] = ' ';
  }
  else
  {
    char *new_buf = realloc(buf, len + 2);
    if (!new_buf)
    {
      perror("realloc");
      free(buf);
      clean_exit(EXIT_FAILURE);
    }
    buf = new_buf;
    strcat(buf, " ");
  }

  int count = 0;
  char *p = buf;

  while (*p && *p == ' ')
  {
    p++;
  }

  while (*p)
  {
    char *token_start = p;
    char *token = NULL;

    if (*p == '\'')
    {
      token_start = ++p;
      token = strchr(p, '\'');
      if (!token)
      {
        wsh_warn(MISSING_CLOSING_QUOTE);

        free(buf);
        for (int i = 0; i < count; i++)
          free(argv[i]);
        *argc = 0;
        argv[0] = NULL;
        return;
      }

      *token = '\0';
      p = token + 1;
    }
    else
    {
      token = strchr(p, ' ');
      if (!token)
        break;
      *token = '\0';
      p = token + 1;
    }

    argv[count] = strdup(token_start);
    if (!argv[count])
    {
      perror("strdup");
      for (int i = 0; i < count; i++)
        free(argv[i]);
      free(buf);
      clean_exit(EXIT_FAILURE);
    }
    count++;

    while (*p && *p == ' ')
    {
      p++;
    }
  }

  argv[count] = NULL;
  *argc = count;

  free(buf);
}

static int is_abs_or_rel(const char *s)
{
  if (!s)
    return 0;
  return (s[0] == '/' || s[0] == '.') ? 1 : 0;
}

/* Returns malloc'ed path or NULL. Prints EMPTY_PATH if PATH is empty/unset. */
static char *find_in_path(const char *cmd)
{
  char *path_env = getenv("PATH");
  if (!path_env || path_env[0] == '\0')
  {
    fprintf(stderr, EMPTY_PATH);
    return NULL;
  }

  char *copy = strdup(path_env);
  if (!copy)
  {
    perror("strdup");
    return NULL;
  }

  char *save = NULL;
  char *tok = strtok_r(copy, ":", &save);
  while (tok)
  {
    if (tok[0] != '\0')
    {
      size_t a = strlen(tok), b = strlen(cmd);
      char *full = (char *)malloc(a + 1 + b + 1);
      if (!full)
      {
        free(copy);
        return NULL;
      }
      snprintf(full, a + 1 + b + 1, "%s/%s", tok, cmd);

      if (access(full, X_OK) == 0)
      {
        free(copy);
        return full;
      }
      free(full);
    }
    tok = strtok_r(NULL, ":", &save);
  }

  free(copy);
  return NULL;
}

static int execute_one(char **argv)
{
  if (!argv || !argv[0])
    return EXIT_SUCCESS;

  char *exec_path = NULL;
  if (is_abs_or_rel(argv[0]))
  {
    exec_path = strdup(argv[0]);
    if (!exec_path)
    {
      perror("strdup");
      return EXIT_FAILURE;
    }
  }
  else
  {
    exec_path = find_in_path(argv[0]);
    if (!exec_path)
    {
      if (getenv("PATH") && getenv("PATH")[0] != '\0')
      {
        fprintf(stderr, CMD_NOT_FOUND, argv[0]);
      }
      return EXIT_FAILURE;
    }
  }

  pid_t pid = fork();
  if (pid < 0)
  {
    perror("fork");
    free(exec_path);
    return EXIT_FAILURE;
  }

  if (pid == 0)
  {
    execv(exec_path, argv);
    fprintf(stderr, CMD_NOT_FOUND, argv[0]);
    _exit(1);
  }

  free(exec_path);

  int status = 0;
  if (waitpid(pid, &status, 0) < 0)
  {
    perror("waitpid");
    return EXIT_FAILURE;
  }

  if (WIFEXITED(status))
  {
    return (WEXITSTATUS(status) == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
  }
  return EXIT_FAILURE;
}

static void history_init(void)
{
  history_da = da_create(16);
}

static void history_add_raw_line(const char *line)
{
  if (!history_da || !line)
    return;

  char *tmp = strdup(line);
  if (!tmp)
  {
    perror("strdup");
    return;
  }

  size_t len = strlen(tmp);
  if (len > 0 && tmp[len - 1] == '\n')
    tmp[len - 1] = '\0';

  da_put(history_da, tmp);
  free(tmp);
}

static const char *history_get_line(size_t idx)
{
  if (!history_da)
    return NULL;
  return da_get(history_da, idx);
}

static size_t history_count(void)
{
  if (!history_da)
    return 0;
  return history_da->size;
}

static int is_builtin_name(const char *name)
{
  if (!name)
    return 0;
  return strcmp(name, "exit") == 0 ||
         strcmp(name, "path") == 0 ||
         strcmp(name, "cd") == 0 ||
         strcmp(name, "which") == 0 ||
         strcmp(name, "history") == 0 ||
         strcmp(name, "alias") == 0 ||
         strcmp(name, "unalias") == 0;
}

static int builtin_exit(int argc, char **argv)
{
  (void)argv;
  if (argc != 1)
  {
    fprintf(stderr, INVALID_EXIT_USE);
    return EXIT_FAILURE;
  }
  return RC_EXIT_REQUEST; /* signal caller to exit cleanly */
}

static int builtin_path(int argc, char **argv)
{
  if (argc == 1)
  {
    char *val = getenv("PATH");
    if (!val)
      val = (char *)"";
    printf("%s\n", val);
    fflush(stdout);
    return EXIT_SUCCESS;
  }

  if (argc == 2)
  {
    if (setenv("PATH", argv[1], 1) != 0)
    {
      perror("setenv");
      return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
  }

  fprintf(stderr, INVALID_PATH_USE);
  return EXIT_FAILURE;
}

static int builtin_cd(int argc, char **argv)
{
  if (argc > 2)
  {
    fprintf(stderr, INVALID_CD_USE);
    return EXIT_FAILURE;
  }

  const char *target = NULL;
  if (argc == 1)
  {
    char *home = getenv("HOME");
    if (!home)
    {
      fprintf(stderr, CD_NO_HOME);
      return EXIT_FAILURE;
    }
    target = home;
  }
  else
  {
    target = argv[1];
  }

  if (chdir(target) != 0)
  {
    perror("cd");
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

static int builtin_which(int argc, char **argv)
{
  if (argc != 2)
  {
    fprintf(stderr, INVALID_WHICH_USE);
    return EXIT_FAILURE;
  }

  const char *name = argv[1];

  const char *aliased_cmd = hm_get(alias_hm, name);
  if (aliased_cmd)
  {
    printf(WHICH_ALIAS, name, aliased_cmd);
    fflush(stdout);
    return EXIT_SUCCESS;
  }

  if (is_builtin_name(name))
  {
    printf(WHICH_BUILTIN, name);
    fflush(stdout);
    return EXIT_SUCCESS;
  }

  if (is_abs_or_rel(name))
  {
    if (access(name, X_OK) == 0)
    {
      printf(WHICH_EXTERNAL, name, name);
      fflush(stdout);
      return EXIT_SUCCESS;
    }
    else
    {
      printf(WHICH_NOT_FOUND, name);
      fflush(stdout);
      return EXIT_FAILURE;
    }
  }

  char *resolved = find_in_path(name);
  if (!resolved)
  {
    printf(WHICH_NOT_FOUND, name);
    fflush(stdout);
    return EXIT_FAILURE;
  }
  printf(WHICH_EXTERNAL, name, resolved);
  fflush(stdout);
  free(resolved);
  return EXIT_SUCCESS;
}

static int builtin_history(int argc, char **argv)
{
  if (argc == 1)
  {
    size_t n = history_count();
    for (size_t i = 0; i < n; i++)
    {
      const char *line = history_get_line(i);
      if (line)
      {
        printf("%s\n", line);
        fflush(stdout);
      }
    }
    return EXIT_SUCCESS;
  }

  if (argc == 2)
  {
    char *endp = NULL;
    long idx = strtol(argv[1], &endp, 10);
    if (*argv[1] == '\0' || *endp != '\0' || idx < 1)
    {
      fprintf(stderr, HISTORY_INVALID_ARG);
      return EXIT_FAILURE;
    }
    size_t n = history_count();
    if ((long)n < idx)
    {
      fprintf(stderr, HISTORY_INVALID_ARG);
      return EXIT_FAILURE;
    }
    const char *line = history_get_line((size_t)(idx - 1));
    if (!line)
    {
      fprintf(stderr, HISTORY_INVALID_ARG);
      return EXIT_FAILURE;
    }
    printf("%s\n", line);
    fflush(stdout);
    return EXIT_SUCCESS;
  }

  fprintf(stderr, INVALID_HISTORY_USE);
  return EXIT_FAILURE;
}

static int builtin_alias(int argc, char **argv)
{
  if (argc == 1)
  {
    hm_print_sorted(alias_hm);
    fflush(stdout);
    return EXIT_SUCCESS;
  }

  if (argc != 3 && argc != 4)
  {
    fprintf(stderr, INVALID_ALIAS_USE);
    return EXIT_FAILURE;
  }

  if (strcmp(argv[2], "=") != 0)
  {
    fprintf(stderr, INVALID_ALIAS_USE);
    return EXIT_FAILURE;
  }

  if (!argv[1] || argv[1][0] == '\0')
  {
    fprintf(stderr, INVALID_ALIAS_USE);
    return EXIT_FAILURE;
  }

  const char *value = (argc == 3) ? "" : (argv[3] ? argv[3] : "");
  hm_put(alias_hm, argv[1], value);
  return EXIT_SUCCESS;
}

static int builtin_unalias(int argc, char **argv)
{
  if (argc != 2 || !argv[1] || argv[1][0] == '\0')
  {
    fprintf(stderr, INVALID_UNALIAS_USE);
    return EXIT_FAILURE;
  }
  hm_delete(alias_hm, argv[1]);
  return EXIT_SUCCESS;
}

static int maybe_expand_leading_alias(char **in_argv, int in_argc,
                                      char ***out_argv, int *out_argc)
{
  *out_argv = NULL;
  *out_argc = 0;

  if (in_argc == 0 || !in_argv[0])
    return 0;

  const char *val = hm_get(alias_hm, in_argv[0]);
  if (!val)
    return 0;

  char *tmp_argv[MAX_ARGS];
  int tmp_argc = 0;
  parseline_no_subst(val, tmp_argv, &tmp_argc);

  char **new_argv = (char **)calloc(MAX_ARGS, sizeof(char *));
  if (!new_argv)
  {
    perror("calloc");
    for (int i = 0; i < tmp_argc; i++)
      free(tmp_argv[i]);
    return 0;
  }

  int new_argc = 0;
  for (int i = 0; i < tmp_argc && new_argc < MAX_ARGS - 1; i++)
  {
    new_argv[new_argc] = strdup(tmp_argv[i]);
    if (!new_argv[new_argc])
      perror("strdup");
    new_argc++;
  }
  for (int i = 1; i < in_argc && new_argc < MAX_ARGS - 1; i++)
  {
    new_argv[new_argc] = strdup(in_argv[i]);
    if (!new_argv[new_argc])
      perror("strdup");
    new_argc++;
  }
  new_argv[new_argc] = NULL;

  for (int i = 0; i < tmp_argc; i++)
    free(tmp_argv[i]);

  *out_argv = new_argv;
  *out_argc = new_argc;
  return 1;
}

static void free_heap_argv(char **argv, int argc)
{
  if (!argv)
    return;
  for (int i = 0; i < argc; i++)
    free(argv[i]);
  free(argv);
}

static int run_pipeline(char **argv, int argc)
{
  int segs = 1;
  for (int i = 0; i < argc; i++)
    if (strcmp(argv[i], "|") == 0)
      segs++;
  if (segs > 128)
    segs = 128;

  char **seg_argvs[128];
  int seg_argcs[128];
  char **exp_argvs[128];
  int exp_argcs[128];
  char *exec_paths[128];

  int start = 0;
  int seg_index = 0;

  for (int i = 0; i <= argc; i++)
  {
    int is_pipe = (i < argc && strcmp(argv[i], "|") == 0);
    if (is_pipe || i == argc)
    {
      int end = i;
      int n = end - start;

      char **out = (char **)calloc(MAX_ARGS, sizeof(char *));
      int c = 0;
      if (!out)
      {
        perror("calloc");
        return EXIT_FAILURE;
      }
      for (int j = start; j < end && c < MAX_ARGS - 1; j++)
      {
        out[c] = strdup(argv[j]);
        if (!out[c])
          perror("strdup");
        c++;
      }
      out[c] = NULL;

      seg_argvs[seg_index] = out;
      seg_argcs[seg_index] = c;
      exp_argvs[seg_index] = NULL;
      exp_argcs[seg_index] = 0;
      exec_paths[seg_index] = NULL;

      if (n == 0)
      {
        fprintf(stderr, EMPTY_PIPE_SEGMENT);
        for (int z = 0; z <= seg_index; z++)
        {
          if (exp_argvs[z])
            free_heap_argv(exp_argvs[z], exp_argcs[z]);
          if (seg_argvs[z])
            free_heap_argv(seg_argvs[z], seg_argcs[z]);
          if (exec_paths[z])
            free(exec_paths[z]);
        }
        return EXIT_FAILURE;
      }

      int expanded = maybe_expand_leading_alias(seg_argvs[seg_index],
                                                seg_argcs[seg_index],
                                                &exp_argvs[seg_index],
                                                &exp_argcs[seg_index]);

      char **use_argv = expanded ? exp_argvs[seg_index] : seg_argvs[seg_index];

      if (!is_builtin_name(use_argv[0]))
      {
        if (is_abs_or_rel(use_argv[0]))
        {
          if (access(use_argv[0], X_OK) != 0)
          {
            fprintf(stderr, CMD_NOT_FOUND, use_argv[0]);
            for (int z = 0; z <= seg_index; z++)
            {
              if (exp_argvs[z])
                free_heap_argv(exp_argvs[z], exp_argcs[z]);
              if (seg_argvs[z])
                free_heap_argv(seg_argvs[z], seg_argcs[z]);
              if (exec_paths[z])
                free(exec_paths[z]);
            }
            return EXIT_FAILURE;
          }
        }
        else
        {
          char *p = find_in_path(use_argv[0]);
          if (!p)
          {
            if (getenv("PATH") && getenv("PATH")[0] != '\0')
            {
              fprintf(stderr, CMD_NOT_FOUND, use_argv[0]);
            }
            for (int z = 0; z <= seg_index; z++)
            {
              if (exp_argvs[z])
                free_heap_argv(exp_argvs[z], exp_argcs[z]);
              if (seg_argvs[z])
                free_heap_argv(seg_argvs[z], seg_argcs[z]);
              if (exec_paths[z])
                free(exec_paths[z]);
            }
            return EXIT_FAILURE;
          }
          exec_paths[seg_index] = p;
        }
      }

      seg_index++;
      start = i + 1;
    }
  }

  int segs_total = seg_index;

  int pipes[127][2];
  for (int i = 0; i < segs_total - 1; i++)
  {
    if (pipe(pipes[i]) < 0)
    {
      perror("pipe");
      for (int z = 0; z < segs_total; z++)
      {
        if (exp_argvs[z])
          free_heap_argv(exp_argvs[z], exp_argcs[z]);
        if (seg_argvs[z])
          free_heap_argv(seg_argvs[z], seg_argcs[z]);
        if (exec_paths[z])
          free(exec_paths[z]);
      }
      return EXIT_FAILURE;
    }
  }

  pid_t pids[128];
  for (int i = 0; i < segs_total; i++)
  {
    char **use_argv = exp_argvs[i] ? exp_argvs[i] : seg_argvs[i];
    int use_argc = exp_argvs[i] ? exp_argcs[i] : seg_argcs[i];

    pids[i] = fork();
    if (pids[i] < 0)
    {
      perror("fork");
      for (int k = 0; k < segs_total - 1; k++)
      {
        close(pipes[k][0]);
        close(pipes[k][1]);
      }
      for (int z = 0; z < segs_total; z++)
      {
        if (exp_argvs[z])
          free_heap_argv(exp_argvs[z], exp_argcs[z]);
        if (seg_argvs[z])
          free_heap_argv(seg_argvs[z], seg_argcs[z]);
        if (exec_paths[z])
          free(exec_paths[z]);
      }
      return EXIT_FAILURE;
    }

    if (pids[i] == 0)
    {
      if (i > 0)
      {
        if (dup2(pipes[i - 1][0], STDIN_FILENO) < 0)
          _exit(1);
      }
      if (i < segs_total - 1)
      {
        if (dup2(pipes[i][1], STDOUT_FILENO) < 0)
          _exit(1);
      }

      for (int k = 0; k < segs_total - 1; k++)
      {
        close(pipes[k][0]);
        close(pipes[k][1]);
      }

      if (is_builtin_name(use_argv[0]))
      {
        int code;
        if (strcmp(use_argv[0], "exit") == 0)
          code = builtin_exit(use_argc, use_argv);
        else if (strcmp(use_argv[0], "path") == 0)
          code = builtin_path(use_argc, use_argv);
        else if (strcmp(use_argv[0], "cd") == 0)
          code = builtin_cd(use_argc, use_argv);
        else if (strcmp(use_argv[0], "which") == 0)
          code = builtin_which(use_argc, use_argv);
        else if (strcmp(use_argv[0], "history") == 0)
          code = builtin_history(use_argc, use_argv);
        else if (strcmp(use_argv[0], "alias") == 0)
          code = builtin_alias(use_argc, use_argv);
        else if (strcmp(use_argv[0], "unalias") == 0)
          code = builtin_unalias(use_argc, use_argv);
        else
          code = EXIT_FAILURE;
        _exit(code == EXIT_SUCCESS ? 0 : 1);
      }
      else
      {
        if (exec_paths[i])
        {
          execv(exec_paths[i], use_argv);
        }
        else
        {
          execv(use_argv[0], use_argv);
        }
        fprintf(stderr, CMD_NOT_FOUND, use_argv[0]);
        _exit(1);
      }
    }
  }

  for (int k = 0; k < segs_total - 1; k++)
  {
    close(pipes[k][0]);
    close(pipes[k][1]);
  }

  int last_status = 0;
  for (int i = 0; i < segs_total; i++)
  {
    int st;
    if (waitpid(pids[i], &st, 0) >= 0)
    {
      if (i == segs_total - 1)
        last_status = st;
    }
  }

  for (int z = 0; z < segs_total; z++)
  {
    if (exp_argvs[z])
      free_heap_argv(exp_argvs[z], exp_argcs[z]);
    if (seg_argvs[z])
      free_heap_argv(seg_argvs[z], seg_argcs[z]);
    if (exec_paths[z])
      free(exec_paths[z]);
  }

  if (WIFEXITED(last_status))
  {
    return (WEXITSTATUS(last_status) == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
  }
  return EXIT_FAILURE;
}

static int run_command(char **argv, int argc)
{
  if (argc == 0)
    return EXIT_SUCCESS;

  int has_pipe = 0;
  for (int i = 0; i < argc; i++)
    if (strcmp(argv[i], "|") == 0)
    {
      has_pipe = 1;
      break;
    }
  if (has_pipe)
    return run_pipeline(argv, argc);

  char **use_argv = argv;
  int use_argc = argc;
  char **exp_argv = NULL;
  int exp_argc = 0;

  int expanded = maybe_expand_leading_alias(argv, argc, &exp_argv, &exp_argc);
  if (expanded)
  {
    use_argv = exp_argv;
    use_argc = exp_argc;
  }

  int code;
  if (is_builtin_name(use_argv[0]))
  {
    if (strcmp(use_argv[0], "exit") == 0)
      code = builtin_exit(use_argc, use_argv);
    else if (strcmp(use_argv[0], "path") == 0)
      code = builtin_path(use_argc, use_argv);
    else if (strcmp(use_argv[0], "cd") == 0)
      code = builtin_cd(use_argc, use_argv);
    else if (strcmp(use_argv[0], "which") == 0)
      code = builtin_which(use_argc, use_argv);
    else if (strcmp(use_argv[0], "history") == 0)
      code = builtin_history(use_argc, use_argv);
    else if (strcmp(use_argv[0], "alias") == 0)
      code = builtin_alias(use_argc, use_argv);
    else if (strcmp(use_argv[0], "unalias") == 0)
      code = builtin_unalias(use_argc, use_argv);
    else
      code = EXIT_FAILURE;
  }
  else
  {
    code = execute_one(use_argv);
  }

  if (expanded)
    free_heap_argv(exp_argv, exp_argc);
  return code;
}

void interactive_main(void)
{
  char line[MAX_LINE];
  char *argvv[MAX_ARGS];
  int argc, i;

  while (1)
  {
    printf(PROMPT);
    fflush(stdout);

    if (fgets(line, sizeof(line), stdin) == NULL)
    {
      if (ferror(stdin))
      {
        fprintf(stderr, "fgets error\n");
        rc = EXIT_FAILURE;
      }
      break;
    }

    argc = 0;
    parseline_no_subst(line, argvv, &argc);
    if (argc == 0)
      continue;

    int code = run_command(argvv, argc);
    if (code == RC_EXIT_REQUEST)
    {
      for (i = 0; i < argc; i++)
        free(argvv[i]);
      break; /* rc remains last non-exit code */
    }

    rc = code;
    history_add_raw_line(line);

    for (i = 0; i < argc; i++)
      free(argvv[i]);
  }
}

int batch_main(const char *script_file)
{
  FILE *fp = fopen(script_file, "r");
  if (!fp)
  {
    perror("fopen");
    return EXIT_FAILURE;
  }

  char line[MAX_LINE];
  char *argvv[MAX_ARGS];
  int argc, i;

  while (fgets(line, sizeof(line), fp) != NULL)
  {
    argc = 0;
    parseline_no_subst(line, argvv, &argc);
    if (argc > 0)
    {
      int code = run_command(argvv, argc);

      if (code == RC_EXIT_REQUEST)
      {
        for (i = 0; i < argc; i++)
          free(argvv[i]);
        fclose(fp);
        return rc;
      }

      rc = code;
      history_add_raw_line(line);

      for (i = 0; i < argc; i++)
        free(argvv[i]);
    }
  }

  if (ferror(fp))
  {
    perror("fgets");
    fclose(fp);
    return EXIT_FAILURE;
  }

  fclose(fp);
  return rc;
}

int main(int argc, char **argv)
{
  setvbuf(stdout, NULL, _IOLBF, 0);
  setvbuf(stderr, NULL, _IONBF, 0);

  alias_hm = hm_create();
  history_init();

  setenv("PATH", "/bin", 1);

  if (argc > 2)
  {
    wsh_warn(INVALID_WSH_USE);
    return EXIT_FAILURE;
  }

  if (argc == 1)
    interactive_main();
  else
    rc = batch_main(argv[1]);

  wsh_free();
  return rc;
}
