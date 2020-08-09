#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h> //wait() functions
#include <sys/types.h>
#include <ctype.h>    //for isspace
#include <sys/stat.h> //for redirection: open()
#include <fcntl.h>    //for redirection: open()
//global variable to exit
int EXIT = 0;
//make global variable for redirection
char *filename;
int adv_redir = 0;
void myPrint(char *msg)
{
    write(STDOUT_FILENO, msg, strlen(msg));
}
/*myExit
exit builtin command
*/
int myExit(char **args)
{
    //buildin format error
    if (args[1] != NULL)
    {
        myPrint("An error has occurred\n");
        return 1;
    }
    else
    {
        EXIT = 1;
        return 0;
    }
}
/*myCD
cd builtin command
*/
int myCD(char **args)
{
    //command line just be cd -> go to home (use getenv())
    if (args[1] == NULL)
    {
        if (chdir(getenv("HOME")) != 0)
        {
            //perror("myshell: ");
            myPrint("An error has occurred\n");
        }
    }
    //call chdir()
    else
    {
        //too many args check
        if (args[2] != NULL)
        {
            myPrint("An error has occurred\n");
        }
        //error check
        else if (chdir(args[1]) != 0)
        {
            //perror("myshell: ");
            myPrint("An error has occurred\n");
        }
    }
    return 1;
}
/*myPwd
pwd builtin command
*/
int myPwd(char **args)
{
    //format buildin check
    if (args[1] != NULL)
    {
        myPrint("An error has occurred\n");
    }
    else
    {
        char cwd[1024];
        getcwd(cwd, sizeof(cwd));
        myPrint(cwd);
        myPrint("\n");
    }

    return 1;
}
//want to have array for builtin commands
char *builtin[] = {"cd", "pwd", "exit"};
// Array of function pointers to call
int (*builtin_func[])(char **) = {&myCD, &myPwd, &myExit};
/*redirection
function to create file and call dup2
*/
int redirection(char *file)
{
    //myPrint(file);

    //check if file exists
    if (access(file, F_OK) != -1)
    {
        myPrint("An error has occurred\n");
        return 0;
    }

    int fd = open(file, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    //check return value of fd
    if (fd == -1)
    {
        myPrint("An error has occurred\n");
        return 0;
    }
    dup2(fd, STDOUT_FILENO);
    close(fd);
    return 1;
}
/*advanced_redir
function for advanced redirection 
*/
int advanced_redir(char *file)
{
    //see if file exists
    if (access(file, F_OK) != -1)
    {

        int temp = open("temp", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
        //int temp = open("temp", O_TMPFILE | O_RDWR, S_IRUSR | S_IWUSR);
        int fd = open(file, O_RDWR, S_IRUSR | S_IWUSR);
        int r1;
        char buffer[514];
        //read file and put it into temp
        while ((r1 = read(fd, buffer, 514)))
        {
            write(temp, buffer, r1);
        }

        //truncate file
        fd = open(file, O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR);
        //then put output to file
        lseek(fd, 0, SEEK_SET);
        dup2(fd, STDOUT_FILENO);

        close(fd);
        close(temp);
        //advanced redir stuff
        //create temp file
        //add stuff in file to temp file
        //truncate file and add output to file
        //append temp to file
        return 2;
    }
    else
    {
        return redirection(file);
    }
}
//count number of redirection signs
int count_redirection(char *arg)
{
    int i = 0;
    int count = 0;
    while (arg[i] != '\0')
    {
        if (arg[i] == '>')
        {
            count++;
        }
        i++;
    }
    return count;
}
/*myshell_launch
fork to launch process
always return 1 to continue
*/
int myshell_launch(char **args)
{
    pid_t pid;
    int status;

    pid = fork();

    //child process
    if (pid == 0)
    {

        //redirection
        int redir = 0;
        if (filename != NULL)
        {
            if (adv_redir == 0)
            {
                redir = redirection(filename);
            }
            else
            {
                redir = advanced_redir(filename);
            }

            //exit if file already exists
            if (redir == 0)
            {
                exit(EXIT_FAILURE);
            }
        }
        int ret = execvp(args[0], args);

        //error
        if (ret < 0)
        {
            /*
            myPrint(args[0]);
            myPrint(args[1]);
            myPrint(args[2]);
            if (args[3] == NULL)
            {
                myPrint("hello");
            }
            myPrint("\n");
            perror("child error: ");
            */
            myPrint("An error has occurred\n");
        }

        exit(0);
    }
    //error in fork
    else if (pid < 0)
    {
        //perror("fork error ");
        myPrint("An error has occurred\n");
    }
    //parent process
    else
    {
        do
        {
            waitpid(pid, &status, 0);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));

        //append temp to file for advanced redirection
        int temp;
        if ((temp = open("temp", O_RDWR, S_IRUSR | S_IWUSR)) >= 0)
        {
            int fd2 = open(filename, O_RDWR | O_APPEND, S_IRUSR | S_IWUSR);
            //lseek to end of file and lseek to beginning of temp
            lseek(fd2, 0, SEEK_END);
            lseek(temp, 0, SEEK_SET);
            //append temp to file
            int r2;
            char buffer2[514];
            while ((r2 = read(temp, buffer2, 514)))
            {
                write(fd2, buffer2, r2);
            }

            close(fd2);
            close(temp);
            //delete temp
            unlink("temp");
        }
    }
    return 1;
}
/*myshell_execute
execute builtin or launch
return 1 if shell continue running or 0 if terminate
*/
int myshell_execute(char **args)
{
    int i;
    //empty command -> just continue
    if (args[0] == NULL)
    {
        return 1;
    }

    //for loop for builtin functions
    for (i = 0; i < 3; i++)
    {
        //use strcmp to see if one of builtins
        if (strcmp(args[0], builtin[i]) == 0)
        {
            return (*builtin_func[i])(args); //if it is call the builtin func
        }
    }
    return myshell_launch(args);
}
//for interactive mode
void myshell_interact()
{
    char cmd_buff[1024];
    char *pinput;
    while (EXIT == 0)
    {
        myPrint("myshell> ");
        pinput = fgets(cmd_buff, 1024, stdin); //if batch change stdin to file
        if (!pinput)
        {
            exit(0);
        }
        //myPrint(cmd_buff);
        if (strlen(pinput) > 514)
        {
            myPrint("An error has occurred\n");
            continue;
        }

        //multiple commands function
        //use strtok_r  to get single command

        char *save;
        char *arg = strtok_r(pinput, ";", &save);
        while (arg != NULL)
        {
            //int adv_redir = 0;
            //check if advanced
            if (strstr(arg, ">+") != NULL)
            {
                adv_redir = 1;
            }

            //count num of redirection signs
            int credir = count_redirection(arg);
            if (credir > 1)
            {
                myPrint("An error has occurred\n");
            }
            else if (credir == 1)
            {

                char *left;
                char *right;
                char *save2;

                if (adv_redir == 0)
                {
                    left = strtok_r(arg, ">", &save2);
                    right = strtok_r(NULL, ">", &save2);
                }
                else
                {
                    left = strtok_r(arg, ">+", &save2);
                    right = strtok_r(NULL, ">+", &save2);
                }

                if (right != NULL)
                {
                    filename = strtok(right, " \t\n");
                }
                /*
                else
                {
                    filename = NULL;
                }
                */

                //spaces
                char *leftdup = strdup(left);
                char *newLeft = strtok(left, " \t\n");
                int num_tok = 0;
                //find how much to malloc
                while (newLeft != NULL)
                {
                    //count num of tokens
                    num_tok++;
                    newLeft = strtok(NULL, " \t\n");
                }
                //now malloc
                char **arrayofArgs = (char **)malloc(sizeof(char *) * (num_tok + 1));
                //reperform strtok on spaces
                char *parse = strtok(leftdup, " \t\n");
                int x = 0;
                while (parse != NULL)
                {
                    arrayofArgs[x] = parse;
                    x++;
                    parse = strtok(NULL, " \t\n");
                }
                arrayofArgs[x] = NULL;
                //myPrint("hello\n");
                //check errors
                if (num_tok == 1 || filename == NULL)
                {
                    myPrint("An error has occurred\n");
                }
                else if (arrayofArgs[0] == NULL)
                {
                    myPrint("An error has occurred\n");
                }
                //if built in then error
                else if (strcmp(arrayofArgs[0], "cd") == 0 || strcmp(arrayofArgs[0], "pwd") == 0 || strcmp(arrayofArgs[0], "exit") == 0)
                {
                    myPrint("An error has occurred\n");
                }

                else
                {
                    myshell_launch(arrayofArgs);
                }
                //myPrint("hello\n");
                free(arrayofArgs);
                //reset adv_redir variable
                adv_redir = 0;
            }
            else //credir == 0
            {
                //spaces
                char *argdup = strdup(arg);
                char *newLeft = strtok(arg, " \t\n");
                int num_tok = 0;
                //find how much to malloc
                while (newLeft != NULL)
                {
                    //count num of tokens
                    num_tok++;
                    newLeft = strtok(NULL, " \t\n");
                }
                //now malloc
                char **arrayofArgs = (char **)malloc(sizeof(char *) * (num_tok + 1));
                //reperform strtok on spaces
                char *parse = strtok(argdup, " \t\n");
                int x = 0;
                while (parse != NULL)
                {
                    arrayofArgs[x] = parse;
                    x++;
                    parse = strtok(NULL, " \t\n");
                }
                arrayofArgs[x] = NULL;
                myshell_execute(arrayofArgs);
                free(arrayofArgs);
            }
            arg = strtok_r(NULL, ";", &save);
            //arg = strtok(NULL, ";");
        }
        //everytime run loop get token which is single command
        //check number of redirection signs: if > 1 error
        //use strtok_r to strip off redirection signs on single command
        //call it twice to remove redirection: 1st is left 2nd is right
        //another loop for strtok on spaces; do this for left
        //duplicate left side use function strdup and put this to argCountloop
        //make sure have copy of command with spaces
        //count number of tokens strtok and then malloc arrayofArgs
        //reperform strtok of spaces on left + assign each token to arrayofArgs (use original left)
        //now arrayofArgs is args
        //after loop finishes; do spaces for right
        //before come to next command: free malloc
    }
}
/*is_empty
helper function to see if line is just empty
*/
int is_empty(const char *s)
{
    while (*s != '\0')
    {
        int x = isspace((unsigned char)*s);
        if (!x)
            return 0;
        s++;
    }
    return 1;
}
/*myshell_batch
function for batch mode/script
*/
void myshell_batch(char *batch)
{
    //open script
    FILE *file = fopen(batch, "r");
    //can't open file
    if (file == NULL)
    {
        myPrint("An error has occurred\n");
    }
    char cmd_buff[1024];
    char *pinput;
    while (EXIT == 0)
    {
        //myPrint("myshell> ");
        pinput = fgets(cmd_buff, 1024, file); //if batch change stdin to file
        if (!pinput)
        {
            exit(0);
        }
        if (is_empty(pinput) == 1)
        {
            continue;
        }
        myPrint(cmd_buff);

        if (strlen(pinput) > 514)
        {
            myPrint("An error has occurred\n");
            continue;
        }

        //multiple commands function
        //use strtok_r  to get single command

        char *save;
        char *arg = strtok_r(pinput, ";", &save);
        //char* arg = strtok(pinput, ";");
        while (arg != NULL)
        {
            //check if advanced
            if (strstr(arg, ">+") != NULL)
            {
                adv_redir = 1;
            }

            //count num of redirection signs
            int credir = count_redirection(arg);
            if (credir > 1)
            {
                myPrint("An error has occurred\n");
            }
            else if (credir == 1)
            {
                char *left;
                char *right;
                char *save2;
                if (adv_redir == 0)
                {
                    left = strtok_r(arg, ">", &save2);
                    right = strtok_r(NULL, ">", &save2);
                }
                else
                {
                    left = strtok_r(arg, ">+", &save2);
                    right = strtok_r(NULL, ">+", &save2);
                }
                if (right != NULL)
                {
                    filename = strtok(right, " \t\n");
                }

                //spaces
                char *leftdup = strdup(left);
                char *newLeft = strtok(left, " \t\n");
                int num_tok = 0;
                //find how much to malloc
                while (newLeft != NULL)
                {
                    //count num of tokens
                    num_tok++;
                    newLeft = strtok(NULL, " \t\n");
                }
                //now malloc
                char **arrayofArgs = (char **)malloc(sizeof(char *) * (num_tok + 1));
                //reperform strtok on spaces
                char *parse = strtok(leftdup, " \t\n");
                int x = 0;
                while (parse != NULL)
                {
                    arrayofArgs[x] = parse;
                    x++;
                    parse = strtok(NULL, " \t\n");
                }
                arrayofArgs[x] = NULL;
                //check errors
                if (num_tok == 1 || filename == NULL)
                {
                    myPrint("An error has occurred\n");
                }
                else if (arrayofArgs[0] == NULL)
                {
                    myPrint("An error has occurred\n");
                }
                //if built in then error
                else if (strcmp(arrayofArgs[0], "cd") == 0 || strcmp(arrayofArgs[0], "pwd") == 0 || strcmp(arrayofArgs[0], "exit") == 0)
                {
                    myPrint("An error has occurred\n");
                }
                else
                {
                    myshell_launch(arrayofArgs);
                }
                free(arrayofArgs);
                //reset adv_redir variable
                adv_redir = 0;
            }
            else //credir == 0
            {
                //spaces
                char *argdup = strdup(arg);
                char *newLeft = strtok(arg, " \t\n");
                int num_tok = 0;
                //find how much to malloc
                while (newLeft != NULL)
                {
                    //count num of tokens
                    num_tok++;
                    newLeft = strtok(NULL, " \t\n");
                }
                //now malloc
                char **arrayofArgs = (char **)malloc(sizeof(char *) * (num_tok + 1));
                //reperform strtok on spaces
                char *parse = strtok(argdup, " \t\n");
                int x = 0;
                while (parse != NULL)
                {
                    arrayofArgs[x] = parse;
                    x++;
                    parse = strtok(NULL, " \t\n");
                }
                arrayofArgs[x] = NULL;
                myshell_execute(arrayofArgs);
                free(arrayofArgs);
            }
            arg = strtok_r(NULL, ";", &save);
        }
    }
}
int main(int argc, char *argv[])
{
    //only ./myshell
    if (argc == 1)
    {
        myshell_interact();
    }
    //if have ./myshell batchfile then run script shell
    else if (argc == 2)
    {
        myshell_batch(argv[1]);
    }
    else
    {
        myPrint("An error has occurred\n");
    }

    return 0;
}