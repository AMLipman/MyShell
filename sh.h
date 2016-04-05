
#include "get_path.h"
#include <pthread.h>

//This struct is used to store the history of commands that were executed
struct HistoryNode {
	char* command;
	struct HistoryNode* prev;
	struct HistoryNode* next;
};

//This struct is used to maintain a linked list for what we are watching
struct WatchMailNode {
    char* filename;
    pthread_t tid;
    struct WatchMailNode *next;
};

//This struct is needed to pass arguments to the WatchMail background thread
struct WatchMailThreadParams {
    char* filename;
    pthread_t tid;
};

//This struct is needed to pass arguments to background threads created with &
struct execArgs {
    char** args;
    struct pathelement* pathlist;
    char** envp;
    int which;
};

//This struct is needed to keep track of users we are watching to see if they log on/off
struct users {
    char* user;
    struct users* next;
    int isOn;
    int foundThisTime;
};

int pid;

//This is our main function which runs the shell and calls all functions
int sh( int argc, char **argv, char **envp);

//This function contains our code for the builtin which command
char *which(char *command, struct pathelement *pathlist);

//This function contains our code for the builtin where command
char *where(char *command, struct pathelement *pathlist);

//This function contains our code for the builtin cd command
char* cd(char* dest, char* previousDirectory);

//This function allows us to create a linked list for the history nodes
struct HistoryNode* addToHistory(struct HistoryNode* tail, char* command);

//This function allows us to handle signals
void quit_sig_handler(int sig);

//This function creates a thread to deal with warn load
void *warnLoadThread(void *param);

//This function gets loads
int get_load(double *loads);

//This function creates a thread to deal with watch mail
void *watchMailThread(void *param);

//This function sets the directory for the ls command
char *SetDir(char **args,int currentPos);

//This function contains our code for the built in list function
void list ( char *dir );

//This function prints our prompt
void PrintPrompt();

//This function prints our precurser
void PrintPrecurser(char *arg);

//This function checks for wild card characters
char **CheckWCChars(char **args);

//This function creates a background thread for commands ran with &
void* runInBackground(void* neededArgs);

//This function adds nodes to the users linked list
struct users* addUserToList(struct users* list, char* newUser, pthread_mutex_t l);

//This function removes nodes from the users linked list
struct users* removeUserFromList(struct users* list, char* removeUser, pthread_mutex_t l);

//This function prints the users linked list
void printUserList(struct users* list, pthread_mutex_t l);

//This function maintains the thread used to watch users logging on and off
void* watchUserThread(void* args);

#define PROMPTMAX 32
#define MAXARGS 10
