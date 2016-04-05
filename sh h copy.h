
#include "get_path.h"

struct HistoryNode {
	char* command;
	struct HistoryNode* prev;
	struct HistoryNode* next;
};

struct execArgs {
	char** args;
	struct pathelement* pathlist;
	char** envp;
	int which;
};

struct users {
	char* user;
	struct users* next;
	int isOn;
	int foundThisTime;
};

int pid;
int sh( int argc, char **argv, char **envp);
char *which(char *command, struct pathelement *pathlist);
char *where(char *command, struct pathelement *pathlist);
char* cd(char* dest, char* previousDirectory);
void list ( char *dir );
struct HistoryNode* addToHistory(struct HistoryNode* tail, char* command);
void quit_sig_handler(int sig);
char *SetDir(char **args,int currentPos);
void PrintPrompt();
void PrintPrecurser(char *arg);
char **CheckWCChars(char **args);
void* runInBackground(void* neededArgs);
struct users* addUserToList(struct users* list, char* newUser, pthread_mutex_t l);
struct users* removeUserFromList(struct users* list, char* removeUser, pthread_mutex_t l);
void printUserList(struct users* list, pthread_mutex_t l);
void* watchUserThread(void* args);





#define PROMPTMAX 32
#define MAXARGS 10
