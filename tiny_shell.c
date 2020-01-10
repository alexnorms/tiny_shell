#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <signal.h>
#include <fcntl.h>

#define MAX_CMD_LEN 100
#define BUFFER 2048

int hist_counter = 0;
char *history[MAX_CMD_LEN];


char *get_a_line(void){

    /* Standard function to retrieve user input
        using getline() from library */

    char *a_line = NULL;
    ssize_t buffer_size = 0;
    getline(&a_line, &buffer_size, stdin);
    return a_line;
}

char **tokenize(char *line){

    /* function to tokenize user input so it can be fed 
        to later functions as elements of an array
    */

    char *string;               //http://c-for-dummies.com/blog/?p=1769
    string = strdup(line);      // strdup used to avoid having to malloc 
    char *token;
    char **tokens = malloc(BUFFER*sizeof(char));   //allocaion memory for array with size of the user input
    const char *delim = " \n";

    int count = 0;
    while ((token = strsep(&string, delim)) != NULL){   //use of the strsep
        *(tokens + count) = token;              // from library to seperate all elements
        count ++;
    }
    tokens[count -1] = '\0';        // last elemt of array is set to null to respect execvp() function used later

    free(string);

    return tokens;

}

void add_cmd_to_history( char *command){

    /* Created a form of circular array that  adds elements to 100
    and when 101 is reached, the first element of list is removed  and
    everything is moved left by one, and latest command is added at [99]*/

    if(hist_counter < MAX_CMD_LEN){                 // max len defined as 100 above (number of commands kept in history desired)
        history[hist_counter] = strdup(command);    // strdup to avoid malloc
        hist_counter +=1 ;
    }
    else {
        free(history[0]);                           //remove first element
        for (int i = 1; i < MAX_CMD_LEN; i++) {
            history[i - 1] = history[i];            //move everything left 1
        }
        history[MAX_CMD_LEN - 1] = strdup(command); // add latest (latest command is last in array)
    }
}

int is_fifo (char *line){

    char *c = line;
    while (*c){
        if (*c == '|'){
            return 1;
        }
        c++;
    }
    return 0;

}


int my_system(char *line, char *fifo){

    char **tokens = tokenize(line);                 // create array with tokenized elements

    // --- CHDIR COMMAND ---                        //must be done in current process as current directory is process property

    if (strcmp(tokens[0], "chdir") == 0 || strcmp(tokens[0], "cd") == 0){

        if (tokens[1] == NULL){                     //must have new destination
                printf("Expected argument to \"chdir\"\n");
        }

        else {
            if (chdir(tokens[1]) != 0){             //use of library function
                perror("Unable to change directory");
            }
            else {
                add_cmd_to_history(line);
            }       
        }
    }

    // --- HISTORY COMMAND ---

    else if (strcmp(tokens[0], "history") == 0){

        add_cmd_to_history(line);

        for(int i=0;i<=(hist_counter-1);i++){           //add values to global array using previously define function and print
            printf("%d - %s",(i + 1), history[i]);
        }
    }

    // --- LIMIT COMMAND ---                           

    else if (strcmp(tokens[0], "limit") == 0){

        if (atoi(tokens[1]) > 0){

            struct rlimit rll = {atoi(tokens[1]) , 1000000000};
            if (setrlimit(RLIMIT_DATA, &rll) == 0){
                printf("Limit set successfully \n");
                add_cmd_to_history(line);
            }
            else{
                perror("limit");                }
            }
        else{
            printf("Invalid limit value");
        }
    }

    else if (is_fifo(line) == 1){

        /* set up command by seperating with | and 
        tokenizing each side*/

        int i = 0;

        char *pipe_one = strdup(line);
        strtok(pipe_one, "|");
        char *pipe_two = strchr(line, '|') + 1;

        char **command_one = tokenize(pipe_one);
        char **command_two = tokenize(pipe_two);

        if (strcmp(command_two[0], "") == 0){
            **command_two ++;
        }


        pid_t pid = fork();
        int fd;

        if (pid < 0){
            perror("Unable to start child");
        }

        else if (pid == 0){

            close(1);
            fd = open(fifo, O_WRONLY);
            dup(fd);
            if (execvp(command_one[0], command_one) == -1){   //use of library function to execute external commands such as ls and pwd
                perror("launch 1");
                exit(0);
            }
            close(fd);

        }
        
        else    {   
            pid_t pid2 = fork();

            if (pid2 < 0){
                perror("Unable to start child");
            }

            else if (pid2 == 0){

                close(0);
                fd = open(fifo, O_RDONLY);
                dup(fd);
                if (execvp(command_two[0], command_two) == -1){   //use of library function to execute external commands such as ls and pwd
                    perror("launch 2");
                    exit(0);
                }
                close(fd);

            }
            else{
                wait(NULL);
            }  
            wait(NULL);
        }


    }

    

    /* --- ANY OTHER CASE --- 
       for any other case we start a child process, and wait for the 
       child process to terminate before continuating parent */

    else {
    
        pid_t pid = fork();
        if (pid < 0){
            perror("Unable to start child");
        }

        else if (pid == 0){

            if (execvp(tokens[0], tokens) == -1){   //use of library function to execute external commands such as ls and pwd
                perror("launch");
                exit(0);
            } 
        }

         else {
            wait(NULL);
            add_cmd_to_history(line); // if child return succesfully, command is added to history array
        }
    }
    
    return 0;

}


void intCHandler(){
    /* COntrol C handler for both yes and no
        if yes then exit parent process 
        else continue
    */

    char c[128];
    write(1,"Would you like to exit? [y/n]: ", 31);
    read(0,c,2);

    if (c[0] == 'y' || c[0] == 'Y')
        exit(0);

    else {

    }
    
}

void intZHandler(){
    //SImply prints \n to ignore signal 
    write(1, " Ignored \n", 11);
}

int main (int argc, char *argv[]){

    //user input
    char *line;
    char *fifo = NULL;

    if (argc > 1){
        fifo = argv[1];
    }

    //INterrupt handlers
    signal(SIGINT, intCHandler);
    signal(SIGTSTP, intZHandler);

    while (1){
      
        line = get_a_line();
        int length = strlen(line);

        if ( length >= 1) {

            if( feof (stdin) ){
                break;
            }

            my_system(line, fifo);
            
        }
        else{
            printf("File executed \n");
            return 0;
        }
    }
    return 0;
}
