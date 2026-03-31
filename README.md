# Simple-shell-using-C
In this lab, we built a simple shell similar to zsh. The shell interprets and executes user commands. For instance, typing `ls` will launch a child process to run the `ls` program, while the parent shell waits for it to finish.


# Specifications

1. **Ctrl-D Handling**: Respond to Ctrl-D (EOF) to exit.

2. **Basic Commands**: Execute simple commands like `ls`, `date`, and `who`. It recognize the `PATH` environment variable to locate commands.

3. **Background Execution**: Allow running commands in the background, e.g., `sleep 30 &`.

4. **Piping**: Support one or more pipes between commands, e.g., `ls | grep out | wc -w`.

5. **I/O Redirection**: Enable standard input and output redirection to files, e.g., `wc -l < /etc/passwd > accounts`.

6. **Built-ins**: Provide `cd` and `exit` as built-in functions.

7. **Ctrl-C Handling**: Ctrl-C should terminate the current foreground process but not the shell itself.
                        Ctrl-C should not affect background jobs.

8. **No Zombies**: The shell does not leave any zombie processes behind.
