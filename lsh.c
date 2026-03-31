/*
 * Main source code file for lsh shell program
 *
 * You are free to add functions to this file.
 * If you want to add functions in a separate file(s)
 * you will need to modify the CMakeLists.txt to compile
 * your additional file(s).
 *
 * Add appropriate comments in your code to make it
 * easier for us while grading your assignment.
 *
 * Using assert statements in your code is a great way to catch errors early and make debugging easier.
 * Think of them as mini self-checks that ensure your program behaves as expected.
 * By setting up these guardrails, you're creating a more robust and maintainable solution.
 * So go ahead, sprinkle some asserts in your code; they're your friends in disguise!
 *
 * All the best!
 */
#include <assert.h>
#include <ctype.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
// The <unistd.h> header is your gateway to the OS's process management facilities.
#include <unistd.h>

#include "parse.h"
#include <sys/stat.h>

typedef struct bg_pid
{
  pid_t pid;
  struct bg_pid *next;
} bg_pid, *ptr_bg_pid;
bg_pid *head = NULL;

static void print_cmd(Command *cmd);
static void print_pgm(Pgm *p);
void stripwhite(char *);
void run(Command *cmd);
void changeInputAndOutputStream(char *rstdin, char *rstdout);
void run_with_pipes(Pgm *pgm, char *rstdin, char *rstdout, int first, int background);

int add_bg_pid(pid_t pid)
{
  if(head == NULL){
    head = malloc(sizeof(bg_pid));
    head->next = NULL;
    head->pid = pid;
    return 0;
  }else{
    bg_pid *tmp_struct = malloc(sizeof(bg_pid));
    tmp_struct->next = head;
    tmp_struct->pid = pid;
    head = tmp_struct;
    return 0;
  }
  return -1;
}

int remove_bg_pid(pid_t pid)
{
  if (head == NULL)
    return -1;
  bg_pid *curr_pid = head;
  if (curr_pid->pid == pid)
  {
    bg_pid *tmp_pid = head->next;
    free(head);
    head = tmp_pid;
    return 0;
  }
  while (curr_pid->next != NULL)
  {
    if (curr_pid->next->pid == pid)
    {
      bg_pid *tmp_pid = curr_pid->next->next;
      free(curr_pid->next);
      curr_pid->next = tmp_pid;
      return 0;
    }
    curr_pid = curr_pid->next;
  }
  return -1;
}

void print_bg_pids()
{
  bg_pid *tmp_pid = head;
  while (tmp_pid != NULL)
  {
    printf("element has pid %d\n", tmp_pid->pid);
    tmp_pid = tmp_pid->next;
  }
}

void sigchld_handler()
{ // to avoid zombie process
  pid_t pid;
  while ((pid = waitpid(-1, NULL, WNOHANG)) > 0){
    remove_bg_pid(pid);
  }
}

void changeInputAndOutputStream(char *rstdin, char *rstdout)
{
  if (rstdin)
  {
    int in = open(rstdin, O_RDONLY);
    if (in == -1)
    { // error message
      perror("Failed to open input file");
      exit(EXIT_FAILURE);
    }
    dup2(in, STDIN_FILENO);
    close(in);
  }
  if (rstdout)
  {                                                                 
    int out = open(rstdout, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR); 
    if (out == -1)
    {
      perror("Failed to open output file");
      exit(EXIT_FAILURE);
    }
    dup2(out, STDOUT_FILENO);
    close(out);
  }
}

int main(void)
{
  signal(SIGCHLD, sigchld_handler); //handle signal SIGCHLD, when child terminates
  signal(SIGINT, SIG_IGN); //Ignore ctrl+c in shell process
  for (;;)
  {
    char *line;
    line = readline("> ");
    // ctrl d to exit the shell
    if (line == NULL || strcmp(line, "exit") == 0)
    {
      while(head != NULL) {//Kill all background processes
        kill(head->pid, SIGINT);
        remove_bg_pid(head->pid);
      }
      exit(0);
    }
    // Remove leading and trailing whitespace from the line
    stripwhite(line);
    // If stripped line not blank
    if (*line)
    {
      //handle cd built in command
      if (line[0] == 'c' && line[1] == 'd')
      {
        line = line + 2;
        stripwhite(line);
        if (chdir(line) < 0)
        {
          perror("error in changing directory");
        }
        continue;
      }

      add_history(line);

      Command cmd;
      if (parse(line, &cmd) == 1)
      {
        run(&cmd);
      }
      else
      {
        printf("Parse ERROR\n");
      }
    }
    // Clear memory
    free(line);
  }

  return 0;
}

// We use first so that we don't pipe the output of the last command to the input of the parent process.
// If it was piped then the result will be seen as input, and therefore will be handled as a command
// and nothing will be written to the output.
void run_with_pipes(Pgm *pgm, char *rstdin, char *rstdout, int first, int background)
{
  if (pgm == NULL)
  {
    return;
  }
  if (pgm->next == NULL && rstdin != NULL)//We have reached the first command
  {
    changeInputAndOutputStream(rstdin, NULL);
  }
  int pipe_desc[2];
  if (!first)//To prevent pipeing the result output to the shell input
  {
    int res = pipe(pipe_desc);
    if (res < 0)
    {
      perror("pipe failed");
      exit(0);
    }
  }
  pid_t pid = fork();
  if (pid < 0)
  {
    perror("fork failed");
    exit(0);
  }
  else if (pid == 0)
  {
    if (rstdout != NULL)//Since the commands are in opposite order, the first in the pgm, will be actually the last one, and therefore its output should be redirected if rstdout != NULL
    {
      changeInputAndOutputStream(NULL, rstdout);
    }
    signal(SIGINT, SIG_DFL);//Accepting ctrl+c signal in child process
    if (background)
    {
      if (setpgid(0, 0) < 0)//Change group id so that it is not closed when ctrl+c is pressed to close foreground processes
      {
        perror("failed changing gid");
        exit(0);
      }
    }
    if (!first)
    {
      close(pipe_desc[0]);             // close read end of child
      int res = dup2(pipe_desc[1], 1); // connect write end with stdout
      close(pipe_desc[1]);             // close pipe_desc for output
      if (res < 0)
      {
        perror("dup failed 1\n");
        exit(0);
      }
    }
    run_with_pipes(pgm->next, rstdin, NULL, 0, background);//Recursive call to handle the rest of the piped commands
    if (execvp(pgm->pgmlist[0], pgm->pgmlist) < 0)
    {
      perror("execute failed");
      exit(0);
    }
  }
  else
  {
    if (!first)
    {
      close(pipe_desc[1]);//Close write end of pipe
      int res = dup2(pipe_desc[0], 0);//Connect stdin with read end of the pipe
      close(pipe_desc[0]);//Close the extra pipe end to avoid leakage
      if (res < 0)
      {
        perror("dup failed 2\n");
        exit(0);
      }
    }
    if (!background)
    {
      waitpid(pid, NULL, 0);
    }
    else {
      add_bg_pid(pid);//Add the process id to the linked list if the process is a bg process
    }
  }
}

// excute command
void run(Command *cmd)
{
  pid_t pid, w;
  int wstatus;
  if (cmd->pgm->next != NULL)
  {
    run_with_pipes(cmd->pgm, cmd->rstdin, cmd->rstdout, 1, cmd->background);
    return;
  }

  pid = fork();
  // check childprocess
  if (pid == -1)
  { // fork failure
    perror("fork");
    exit(EXIT_FAILURE);
  }
  else if (pid == 0)
  { // fork success
    signal(SIGINT, SIG_DFL);
    if (cmd->background)
    {
      if (setpgid(0, 0) < 0)
      {
        perror("failed changing gid");
        exit(0);
      }
    }

    if (cmd->rstdout != NULL || cmd->rstdin != NULL)
    {
      changeInputAndOutputStream(cmd->rstdin, cmd->rstdout);
    }
    if (execvp(cmd->pgm->pgmlist[0], cmd->pgm->pgmlist) == -1)
    { // debugg
      perror("execvp failed");
      exit(EXIT_FAILURE);
    }
  }
  else
  {
    if (!cmd->background)
    {
      w = waitpid(pid, &wstatus, 0);
      if (w == -1)
      {
        perror("waitpid");
        exit(EXIT_FAILURE);
      }
    }else{
      add_bg_pid(pid);
    }
  }
}

/*
 * Print a Command structure as returned by parse on stdout.
 *
 * Helper function, no need to change. Might be useful to study as inspiration.
 */
static void print_cmd(Command *cmd_list)
{
  printf("------------------------------\n");
  printf("Parse OK\n");
  printf("stdin:      %s\n", cmd_list->rstdin ? cmd_list->rstdin : "<none>");
  printf("stdout:     %s\n", cmd_list->rstdout ? cmd_list->rstdout : "<none>");
  printf("background: %s\n", cmd_list->background ? "true" : "false");
  printf("Pgms:\n");
  print_pgm(cmd_list->pgm);
  printf("------------------------------\n");
}

/* Print a (linked) list of Pgm:s.
 *
 * Helper function, no need to change. Might be useful to study as inpsiration.
 */
static void print_pgm(Pgm *p)
{
  if (p == NULL)
  {
    return;
  }
  else
  {
    char **pl = p->pgmlist;

    /* The list is in reversed order so print
     * it reversed to get right
     */
    print_pgm(p->next);
    printf("            * [ ");
    while (*pl)
    {
      printf("%s ", *pl++);
    }
    printf("]\n");
  }
}

/* Strip whitespace from the start and end of a string.
 *
 * Helper function, no need to change.
 */
void stripwhite(char *string)
{
  size_t i = 0;

  while (isspace(string[i]))
  {
    i++;
  }

  if (i)
  {
    memmove(string, string + i, strlen(string + i) + 1);
  }

  i = strlen(string) - 1;
  while (i > 0 && isspace(string[i]))
  {
    i--;
  }

  string[++i] = '\0';
}
