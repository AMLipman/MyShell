/*

sh.c

Written by: Andrew Lipman and Rachel Kraft

*/

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <pwd.h>
#include <dirent.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>
#include <glob.h>
#include <stdbool.h>
#include "sh.h"

pid_t pid_gl;
char *prompt;

int sh( int argc, char **argv, char **envp )
{
  prompt = calloc(PROMPTMAX, sizeof(char));
  char *commandline = calloc(MAX_CANON+1, sizeof(char));
  char *command, *arg, *commandpath, *p, *pwd, *owd;
  char **args = calloc(MAXARGS, sizeof(char*));
  char **aliases = calloc(MAXARGS, sizeof(char*));
  char **history = calloc(MAXARGS, sizeof(char*));
  char *currentArg;
  int uid, i, status, argsct, go = 1;
  struct passwd *password_entry = (struct passwd*)malloc(sizeof(struct passwd));
  char *homedir = (char*)malloc(sizeof(char));
  struct pathelement *pathlist;
  struct HistoryNode* tail = NULL;
  char* previousDirectory = "";
    
    signal(SIGINT,quit_sig_handler);
    signal(SIGTSTP,quit_sig_handler);
    signal(SIGTERM,quit_sig_handler);

  uid = getuid();
  password_entry = getpwuid(uid);               /* get passwd info */
  homedir = password_entry->pw_dir;		/* Home directory to start
						  out with*/
    
  if ( (pwd = getcwd(NULL, PATH_MAX+1)) == NULL )
  {
    perror("getcwd");
    exit(2);
  }
  owd = calloc(strlen(pwd) + 1, sizeof(char));
  memcpy(owd, pwd, strlen(pwd));
  prompt[0] = ' '; prompt[1] = '\0';

  /* Put PATH into a linked list */
  pathlist = get_path();

  while ( go )
  {
    /* print your prompt */
	printf("%s [%s]>",prompt,getcwd(NULL, PATH_MAX+1));
	  char **args = calloc(MAXARGS, sizeof(char*));
    /* get command line and process */
	fflush(stdin);
	fflush(stdout);
	const char space[3] = " \n";
	const char* newLine = "\n";
      
      char *commandline = calloc(MAX_CANON, sizeof(char));
      char *condition = fgets(commandline,MAX_CANON,stdin);
      if (strcmp(commandline,"\n")!=0 && condition!=NULL){// new stuff
          commandline = strtok(commandline,newLine);
          tail = addToHistory(tail,commandline);
          
          //CHECK FOR ALIAS AND REPLACE COMMAND
          char* currentAlias = malloc(256);
          char* currentCmd = malloc(256);
          const char* colon = ":\n";
          int i = 0;
          while(aliases[i]){
              currentAlias = strtok(aliases[i],colon);
              currentCmd = strtok(NULL,colon);
              aliases[i] = malloc(256);
              strcpy(aliases[i],currentAlias);
              strcat(aliases[i],":");
              strcat(aliases[i],currentCmd);
              if (strcmp(commandline,currentAlias)==0){
                  commandline = calloc(MAX_CANON, sizeof(char));
                  strcpy(commandline,currentCmd);
              }
              i++;
          }
          
          
          // Parsing arguments
          currentArg = strtok(commandline,space);
          int currentPos = 0;
          
          while (currentArg != NULL){
              args[currentPos] = currentArg;
              currentPos++;
              currentArg = strtok(NULL,space);
          }
          
          // Print precurser executing statement
          PrintPrecurser(args[0]);
          /* check for each built in command and implement */
          if (args[0] == NULL){
          }
          else if (strcmp(args[0],"exit")==0){
              break;
          }
          else if (strcmp(args[0],"which")==0){
              printf("%s\n",which(args[1],pathlist));
          }
          else if (strcmp(args[0],"where")==0){
              char* result = where(args[1],pathlist);
              if (result!=NULL){
                  printf("%s\n",result);
              }
          }
          else if (strcmp(args[0],"cd")==0){
              if (args[2]== NULL){
                  char* dir = args[1];
                  char* tempdir;
                  if (currentPos>1){
                      for(i=2; i<currentPos;i++){
                          tempdir = malloc(strlen(dir));
                          strcpy(tempdir,dir);
                          dir = malloc(strlen(tempdir)+strlen(args[i])+2);
                          strcpy(dir,tempdir);
                          strcat(dir," ");
                          strcat(dir,args[i]);
                      }
                  }
                  previousDirectory = cd(dir,previousDirectory);
                  //free(tempdir);
              }
              else{
                  printf("cd: Too many arguments\n");
              }
              
          }
          else if (strcmp(args[0],"pwd")==0){
              printf("%s\n",getcwd(NULL, PATH_MAX+1));
          }
          else if (strcmp(args[0],"list")==0){
              char *dir = SetDir(args,currentPos);
              list(dir);
          }
          else if (strcmp(args[0],"pid")==0){
              printf("My process ID : %d\n", getpid());
          }
          else if (strcmp(args[0],"kill")==0){
              if (args[1]==NULL){
                  printf("%s\n","You must provide a PID to be killed");
              }
              else if (strncmp(&args[1][0],"-",1)==0){
                  memmove (args[1], args[1]+1, strlen (args[1]+1) + 1);
                  int sig = atoi(args[1]);
                  kill(atoi(args[2]),sig);
              }
              else{
                  kill(atoi(args[1]),15);
              }
          }
          else if (strcmp(args[0],"prompt")==0){
              if (args[1]==NULL){
                  printf("%s","input prompt prefix: ");
                  fgets(prompt,PROMPTMAX,stdin);
                  prompt = strtok(prompt,"\n");
                  
              }
              else{
                  strcpy(prompt,args[1]);
                  strcat(prompt,"\0");
              }
          }
          else if (strcmp(args[0],"printenv")==0){
              if (args[1]==NULL){
                  i = 0;
                  while (envp[i]) {
                      printf("%s\n",envp[i++]);
                  }
              }
              else if (args[2]!=NULL){
                  printf("%s\n","printenv: Too many arguments.");
              }
              else{
                  printf("%s\n",getenv(args[1]));
              }
          }
          else if (strcmp(args[0],"alias")==0){
              if (args[1]==NULL){
                  char* currentAlias = malloc(256);
                  char* currentCmd = malloc(256);
                  const char* colon = ":\n";
                  i = 0;
                  while(aliases[i]){
                      currentAlias = strtok(aliases[i],colon);
                      currentCmd = strtok(NULL,colon);
                      aliases[i] = malloc(256);
                      strcpy(aliases[i],currentAlias);
                      strcat(aliases[i],":");
                      strcat(aliases[i],currentCmd);
                      if (strpbrk(" ",currentCmd)==NULL){
                          printf("%s\t%s\n",currentAlias,currentCmd);
                      }
                      else{
                          printf("%s\t(%s)\n",currentAlias,currentCmd);
                      }
                      i++;
                  }
              }
              else if (args[2] == NULL){
                  char* currentAlias = malloc(256);
                  char* currentCmd = malloc(256);
                  const char* colon = ":\n";
                  i = 0;
                  while(aliases[i]){
                      currentAlias = strtok(aliases[i],colon);
                      currentCmd = strtok(NULL,colon);
                      aliases[i] = malloc(256);
                      strcpy(aliases[i],currentAlias);
                      strcat(aliases[i],":");
                      strcat(aliases[i],currentCmd);
                      if (strcmp(args[1],currentAlias)==0){
                          printf("%s\n",currentCmd);
                      }
                      i++;
                  }
              }
              else{
                  char* fullCmd = args[2];
                  char* newAlias = args[1];
                  char* tempCmd;
                  if (currentPos>2){
                      for(i=3; i<currentPos;i++){
                          tempCmd = malloc(strlen(fullCmd));
                          strcpy(tempCmd, fullCmd);
                          fullCmd = malloc(strlen(tempCmd)+strlen(args[i])+2);
                          strcpy(fullCmd, tempCmd);
                          strcat(fullCmd," ");
                          strcat(fullCmd,args[i]);
                      }
                  }
                  tempCmd = malloc(strlen(newAlias)+5);
                  strcpy(tempCmd,newAlias);
                  newAlias = malloc(strlen(tempCmd)+5+strlen(fullCmd));
                  strcpy(newAlias,tempCmd);
                  strcat(newAlias,":");
                  strcat(newAlias,fullCmd);
                  i = 0;
                  while (aliases[i]!=NULL){
                      i++;
                  }
                  aliases[i]=newAlias;
              }
              //free(currentAlias);
              //free(currentCmd);
          }
          else if (strcmp(args[0],"history")==0){
              int times = 10;
              int count = 0;
              if (args[1]!=NULL){
                  times = atoi(args[1]);
              }
              struct HistoryNode* current = tail;
              while (current!=NULL && count<times){
                  printf("%s\n",current->command);
                  current = current->prev;
                  count++;
              }
          }
          else if (strcmp(args[0],"setenv")==0){
              int overwrite = 1;
              i = 0;
              if (args[3] != NULL){
                  /*error*/
              }
              else if (args[1]==NULL){
                  while (envp[i]) {
                      printf("%s\n",envp[i++]);
                  }
              }
              else if (args[2] == NULL){
                  int error = setenv(args[1], "", overwrite);
                  while (envp[i]) {
                      i++;
                  }
                  envp[i]=malloc(256);
                  char* newString = malloc(strlen(args[1])+3);
                  strcat(newString,args[1]);
                  strcpy(envp[i], newString);
                  strcat(envp[i],"=");
                  envp[i+1]=NULL;
                  //free(newString);
              }
              else{
                  int error = setenv(args[1],args[2],overwrite);
                  while (envp[i]) {
                      i++;
                  }
                  envp[i]=malloc(256);
                  char* newString = malloc(strlen(args[1])+3+strlen(args[2]));
                  strcat(newString,args[1]);
                  strcat(newString,"=");
                  strcat(newString,args[2]);
                  strcpy(envp[i], newString);
                  envp[i+1]=NULL;
                  if (strcmp(args[1],"PATH")==0){
                      //free(pathlist);
                      pathlist = get_path();
                  }
                  //free(newString);
              }
              
          }
          else if (access(args[0],X_OK)==0){
              printf("%s\n",args[0]);
              int status;
              pid_t pid;
              pid = fork();
              
              if (pid==0){
                  execve(args[0], args, envp );
              }
              else {
                  pid_gl = pid;
                  printf("%d",pid);
                  waitpid(pid,&status, WEXITSTATUS(status));
              }
          }
          /*  else  program to exec */
          /* find it */
          /* do fork(), execve() and waitpid() */
          else if (access(which(args[0],pathlist),X_OK)==0){
              int status;
              args = CheckWCChars(args);
              for (i=0;args[i]!=NULL;i++){
                  printf("%s\n",args[i]);
              }
              pid_t pid;
              pid = fork();
              if (pid==0){
                  execve(which(args[0],pathlist), args, envp );
              }
              else {
                  pid_gl = pid;
                  waitpid(pid,&status, WEXITSTATUS(status));
                  
              }
          }
          
          
          else {
              fprintf(stderr, "%s: Command not found.\n", args[0]);
          }
      }
      else{
          if (condition == NULL){
              if (commandline[0]=='\0'){
                  printf("\n");
              }
              
              clearerr(stdin);
          }
      }
     
      
  }
    
    
    free(prompt);
    free(commandline);
    free(args);
    free(aliases);
    free(history);
    //free(password_entry);
    //free(homedir);
  return 0;
} /* sh() */

/* Helper Functions*/

char **CheckWCChars(char **args){
    char **newArgs = calloc(MAXARGS,sizeof(char*));
    newArgs[0] = args[0];
    int NACounter = 1;
    
    bool wildcardStar = false;
    bool wildcardQmark = false;
    char temp[256];
    int k;
    for(k=1;args[k]!=NULL;k++){
        char *temp = malloc(sizeof(args[k]));
        strcpy(temp,args[k]);
        int i;
        for(i=0; temp[i]!='\0';i++){
            if (temp[i]== '*' && !wildcardStar && !wildcardQmark){
                glob_t globWC;
                glob(temp,GLOB_NOCHECK,NULL,&globWC);
                int j;
                for(j=0;globWC.gl_pathv[j]!=NULL;j++){
                    newArgs[NACounter] = globWC.gl_pathv[j];
                    NACounter++;
                }
                wildcardStar = true;
            }
            
            else if (temp[i]== '?' && !wildcardQmark && !wildcardStar){
                glob_t globWC;
                glob(temp,GLOB_NOCHECK,NULL,&globWC);
                int j;
                for(j=0;globWC.gl_pathv[j]!=NULL;j++){
                    newArgs[NACounter] = globWC.gl_pathv[j];
                    NACounter++;
                }
                wildcardQmark = true;
            }
        }
        if (!wildcardQmark && !wildcardStar){
            newArgs[NACounter] = args[k];
            NACounter++;
        }
    }
    return newArgs;
}

void PrintPrompt(){
    printf("%s [%s]>",prompt,getcwd(NULL, PATH_MAX+1));
}

void PrintPrecurser(char *arg){
    printf("[Executing: %s]\n",arg);
}

char *SetDir(char **args,int currentPos){
    char* dir = NULL;
    char* tempdir;
    int i;
    if (currentPos>1){
        for(i=1; i<currentPos;i++){
            if (dir!=NULL){
                tempdir = malloc(strlen(dir));
                strcpy(tempdir,dir);
                dir = malloc(strlen(tempdir)+strlen(args[i])+5);
                strcpy(dir,tempdir);
            }
            else{
                dir = malloc(strlen(args[i]+5));
            }
            char * end_char = malloc(5);
            strcpy(end_char,&args[i][strlen(args[i])-1]);
            strcat(dir,args[i]);
            strcat(dir,":");
            free(end_char);
        }
        
    }
    
    //free(tempdir);
    return dir;
}

char *which(char *command, struct pathelement *pathlist )
{
	char* filePath;
	char* notFound = ": Command Not Found";
	char* notFoundFull;
	struct pathelement * head = pathlist;
	
    if (command == NULL){
        return("which: Too few arguments");
    }
    else{
        while (head!=NULL){
            filePath = malloc(strlen(head->element)+2+strlen(command));
            strcpy(filePath, head->element); /* copy name into the new var */
            strcat(filePath,"/");
            strcat(filePath, command);
            if( access( filePath, F_OK ) != -1 ) {
                return filePath;
            }
            head = head->next;
        }
        notFoundFull = malloc(strlen(command)+1+strlen(notFound));
        strcpy(notFoundFull,command);
        strcat(notFoundFull,notFound);
        //free(filePath);
        return notFoundFull;
    }
	
	

} /* which() */

char *where(char *command, struct pathelement *pathlist )
{
 	char* filePath;
	char* result;
	struct pathelement * head = pathlist;
	int firstTime = 1;
	
    if (command == NULL){
        return("where: Too few arguments");
    }
    else{
        while (head!=NULL){
            filePath = malloc(strlen(head->element)+2+strlen(command));
            strcpy(filePath, head->element); /* copy name into the new var */
            strcat(filePath,"/");
            strcat(filePath, command);
            if( access( filePath, F_OK ) != -1 ) {
                result = malloc(strlen(head->element) + 2 + strlen(filePath));
                if (firstTime){
                    firstTime = 0;
                }
                else{
                    strcat(result,"\n");
                }
                strcat(result,filePath);
            } 
            head = head->next;
        }
        //free(filePath);
        return result;
    }
	
} /* where() */

char* cd(char* dest,char* previousDirectory){
	if (dest==NULL){
		dest = getenv("HOME");
	}
	char* currentDirectory = getcwd(NULL, PATH_MAX+1);
	if (strcmp(dest,"-")==0){
		strcpy(dest,previousDirectory);
	}
    if (chdir (dest) == -1) {
        printf ("%s: %s\n", dest, strerror (errno));
    }
    else {
		return currentDirectory;
	}
	return previousDirectory;
}

void list ( char *dir )
{
    
	if (dir == NULL){
		DIR           *d;
		struct dirent *dir;
		d = opendir(".");
		if (d)	{
			while ((dir = readdir(d)) != NULL) {
				printf("%s\n", dir->d_name);
			}
			closedir(d);
		}
        else{
            perror("opendir");
        }
	}
	else{
        const char colon[3] = ":\n";
        char *currentDir = strtok(dir,colon);
        while(currentDir !=NULL){
            DIR           *d;
            struct dirent *dir;
            d = opendir(currentDir);
            if (d)	{
                printf("\n%s:\n",currentDir);
                while ((dir = readdir(d)) != NULL) {
                    printf("%s\n", dir->d_name);
                }
                closedir(d);
            }
            else{
                perror("opendir");
            }
            
            currentDir = strtok(NULL,colon);
        }
    }

} /* list() */

struct HistoryNode* addToHistory(struct HistoryNode* tail, char* command){
	struct HistoryNode* new = (struct HistoryNode*)malloc(sizeof(struct HistoryNode));
	new->command = malloc(strlen(command)+1); 
	strcpy(new->command,command);
	if (tail!=NULL){
		new->prev = tail;
		new->next = NULL;
		tail->next = new;
	}
	else{ 
		new->prev = NULL;
		new->next = NULL;
	}
	return new;
}



void quit_sig_handler(int sig){
    switch(sig){
        case SIGINT:
            if (pid_gl != 0){
                kill(getpid(),SIGCONT) ;
                printf("\n%s [%s]>",prompt,getcwd(NULL, PATH_MAX+1));
            }
            signal(SIGINT,quit_sig_handler);
            break;
        case SIGTSTP:
            printf("\n%s [%s]>",prompt,getcwd(NULL, PATH_MAX+1));
            signal(SIGTSTP,quit_sig_handler);
            break;
        case SIGTERM:
            printf("\n%s [%s]>",prompt,getcwd(NULL, PATH_MAX+1));
            signal(SIGTERM,quit_sig_handler);
            break;
            
    }
     fflush(stdout);
}



