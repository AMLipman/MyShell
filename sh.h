
#include "get_path.h"

struct HistoryNode {
	char* command;
	struct HistoryNode* prev;
	struct HistoryNode* next;
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





#define PROMPTMAX 32
#define MAXARGS 10
