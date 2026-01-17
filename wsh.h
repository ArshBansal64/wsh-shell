#ifndef WSH_H
#define WSH_H

/**************************************************
 * Constants
 *************************************************/
#define MAX_LINE 1024 /* max line size */
#define MAX_ARGS 128  /* max args on a command line */

#define PROMPT "wsh> " /* prompt */
#define INVALID_WSH_USE "Invalid usage of wsh. Correct format: wsh | wsh batch_file\n"

#define CMD_NOT_FOUND "Command not found or not an executable: %s\n"
#define EMPTY_PIPE_SEGMENT "Empty command segment in pipeline\n"
#define EMPTY_PATH "PATH empty or not set\n"
#define MISSING_CLOSING_QUOTE "Missing Closing Quote\n"
#define UNMATCHED_PAREN "Unmatched parentheses in command substitution\n"

#define INVALID_PATH_USE "Incorrect usage of path. Correct format: path dir1:dir2:...:dirN\n"
#define INVALID_EXIT_USE "Incorrect usage of exit. Too many arguments\n"
#define INVALID_ALIAS_USE "Incorrect usage of alias. Correct format: alias | alias name = 'command'\n"
#define INVALID_UNALIAS_USE "Incorrect usage of unalias. Correct format: unalias name\n"
#define INVALID_WHICH_USE "Incorrect usage of which. Correct format: which name\n"
#define INVALID_CD_USE "Incorrect usage of cd. Correct format: cd | cd directory\n"
#define INVALID_HISTORY_USE "Incorrect usage of history. Correct format: history | history n\n"

#define WHICH_ALIAS "%s: aliased to '%s'\n"
#define WHICH_BUILTIN "%s: wsh builtin\n"
#define WHICH_EXTERNAL "%s: found at %s\n"
#define WHICH_NOT_FOUND "%s: not found\n"

#define CD_NO_HOME "cd: HOME not set\n"

#define HISTORY_INVALID_ARG "Invalid argument passed to history\n"

/**************************************************
 * Modes of Execution
 *************************************************/
void interactive_main(void); /* Print prompt and wait for user input */
int batch_main(const char *script_file); /* Read a commands from script_file line by line */

/**************************************************
 * Parsing
 *************************************************/
void parseline_no_subst(const char *cmdline, char **argv, int *argc);


/**************************************************
 * Helpers
 *************************************************/
void wsh_free(void); /* Free global allocated memory */
void clean_exit(int return_code); /* Free allocated memory and exit */
void wsh_warn(const char *msg, ...); /* Set the return code and print message to stderr */

#endif //WSH_H
