/*
 
 sh.c
 change
 
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
#include <utmpx.h>
#include <glob.h>
#include <signal.h>
#include <sys/param.h>
#include <kstat.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <pthread.h>
#include <time.h>
#include <fcntl.h>
#include "sh.h"

pid_t pid_gl;
char *prompt;
int isThreadRunningGl = 0;
float warnLoadGl = 0.0f;
struct WatchMailNode *WMhead = NULL;
struct WatchMailNode *WMtail = NULL;
pthread_mutex_t WatchMailMutex;
int runningBackgroundThreads;
struct users* watchOn;
pthread_mutex_t mutexOn;
int watchUserThreadCreated = 0;


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
    char* background = malloc(1);
    pthread_t threads[100];
    int threadCurr = 0;
    runningBackgroundThreads = 0;
    watchOn = NULL;
    int redirectFlag = 0;
    int noclobber = 0;
    int noclobberProceed = 1;
    int fid;
    int saved_stdout;
    int saved_stdin;
    int saved_stderr;
    
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
      int status2;
      /* clean up zombie processes */
      waitpid(-1,&status2,WNOHANG);
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
      if (strcmp(commandline,"\n")!=0 && condition!=NULL){
          // new stuff
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
          
          background = &args[currentPos-1][strlen(args[currentPos-1]) - 1];
          if (strcmp(background,"&")==0){
              args[currentPos-1][strlen(args[currentPos-1])-1]='\0';
              background = "&";
          }
          
          PrintPrecurser(args[0]);
          /* check for each built in command and implement */
          
          // dealing with redirects
          i = -1;
          while (args[++i]!=NULL){
              if (strcmp(args[i],">")==0){
                  if (noclobber){
                      if( access( args[i+1], F_OK ) != -1 ) {
                          noclobberProceed = 0;
                          printf("%s: File exists.\n",args[i+1]);
                          args[0]=NULL;
                      }
                  }
                  if (noclobberProceed){
                      redirectFlag = 1;
                      saved_stdout = dup(1);
                      fid = open(args[i + 1],O_WRONLY|O_CREAT|O_TRUNC,0666);
                      close(1);
                      dup(fid);
                      close(fid);
                      args[i]=NULL;
                  }
                  else{
                      noclobberProceed = 1;
                  }
              }
              else if (strcmp(args[i],">&")==0){
                  if (noclobber){
                      if( access( args[i+1], F_OK ) != -1 ) {
                          noclobberProceed = 0;
                          printf("%s: File exists.\n",args[i+1]);
                          args[0]=NULL;
                      }
                  }
                  if (noclobberProceed){
                      redirectFlag = 2;
                      saved_stdout = dup(1);
                      saved_stderr = dup(2);
                      fid = open(args[i + 1],O_WRONLY|O_CREAT|O_TRUNC,0666);
                      close(1);
                      dup(fid);
                      close(2);
                      dup(fid);
                      close(fid);
                      args[i]=NULL;
                  }
                  else{
                      noclobberProceed = 1;
                  }
              }
              else if (strcmp(args[i],">>")==0){
                  if (noclobber){
                      if( access( args[i+1], F_OK ) == -1 ) {
                          noclobberProceed = 0;
                          printf("%s: No such file or directory.\n",args[i+1]);
                          args[0]=NULL;
                      }
                  }
                  if (noclobberProceed){
                      redirectFlag = 1;
                      saved_stdout = dup(1);
                      fid = open(args[i + 1],O_WRONLY|O_CREAT|O_APPEND,0666);
                      close(1);
                      dup(fid);
                      close(fid);
                      args[i]=NULL;
                  }
                  else{
                      noclobberProceed = 1;
                  }
              }
              else if (strcmp(args[i],">>&")==0){
                  if (noclobber){
                      if( access( args[i+1], F_OK ) == -1 ) {
                          noclobberProceed = 0;
                          printf("%s: No such file or directory.\n",args[i+1]);
                          args[0]=NULL;
                      }
                  }
                  if (noclobberProceed){
                      redirectFlag = 2;
                      saved_stdout = dup(1);
                      saved_stderr = dup(2);
                      fid = open(args[i + 1],O_WRONLY|O_CREAT|O_APPEND,0666);
                      close(1);
                      dup(fid);
                      close(2);
                      dup(fid);
                      close(fid);
                      args[i]=NULL;
                  }
                  else{
                      noclobberProceed = 1;
                  }
              }
              else if (strcmp(args[i],"<")==0){
                  if( access( args[i+1], F_OK ) != -1 ) {
                      redirectFlag = 3;
                      saved_stdin = dup(0);
                      fid = open(args[i + 1],O_RDONLY,0666);
                      close(0);
                      dup(fid);
                      close(fid);
                      args[i]=NULL;
                  }
                  else
                  {
                      printf("%s: No such file or directory\n",args[i+1]);
                      args[0] = NULL;
                  }
              }
          }
          
          //check if we are pipelining
          int piped = 0;
          for(i=0; args[i]!=NULL;i++){
              if (strcmp(args[i],"|")==0 || strcmp(args[i],"|&")==0){
                  piped = 1;
                  char **args1 = calloc(i+1, sizeof(char*));
                  char **args2 = calloc(MAXARGS, sizeof(char*));
                  
                  int j;
                  for (j=0;args[j]!=NULL;j++){
                      if (j<i){
                          args1[j] = args[j];
                      }
                  }
                  
                  int z;
                  int y=0;
                  for (z=0;args[z]!=NULL;z++){
                      if (z>i){
                          args2[y] = args[z];
                          y++;
                      }
                  }
                  
                  int pfds[2];
                  char buf[100];
                  
                  pipe(pfds);
                  
                  pid_t pid1;
                  pid1 = fork();
                  if (pid1 == 0){
                      close(1);// close stdout
                      dup(pfds[1]);// make pfds[1] stdout
                      close (pfds[0]);
                      if (strcmp(args[i],"|&")==0){
                          close(2);
                          dup(pfds[1]);
                      }
                      args = args1;
                  }
                  else{
                      pid_t pid2;
                      pid2 = fork();
                      if (pid2 == 0){
                          close(0);
                          dup(pfds[0]);
                          close(pfds[1]);
                          if (access(args2[0],X_OK)==0){
                              execve(args2[0], args2, envp);
                          }
                          else{
                              execve(which(args2[0],pathlist),args2,envp);
                          }
                      }
                      else{
                          close(pfds[0]);
                          close(pfds[1]);
                          waitpid(pid1,&status,0);//assumming that we want the parent to wait for both piped processess
                          waitpid(pid2,&status,0);
                          args[0]= NULL;
                      }
                  }
                  
                  
              }
          }
          
          if (args[0] == NULL){
          }
          else if (strcmp(args[0],"exit")==0){
              break;
          }
          else if (strcmp(args[0],"noclobber")==0){
              if (noclobber == 0){
                  noclobber = 1;
              }
              else{
                  noclobber = 0;
              }
              printf("%d\n",noclobber);
          }
          else if (strcmp(args[0],"which")==0){
              int i;
              for (i=1;args[i]!=NULL;i++){
                  printf("%s\n",which(args[i],pathlist));
              }
              
          }
          else if (strcmp(args[0],"where")==0){
              int i;
              for (i=1;args[i]!=NULL;i++){
                  char* result = where(args[i],pathlist);
                  if (result!=NULL){
                      printf("%s\n",result);
                  }
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
              int inPath = 0;
              i = 0;
              if (args[3] != NULL){
                  printf("%s\n","setting: Too many arguments.");
              }
              else if (args[1]==NULL){
                  while (envp[i]) {
                      printf("%s\n",envp[i++]);
                  }
              }
              else if (args[2] == NULL){
                  int error = setenv(args[1], "", overwrite);
                  while (envp[i]) {
                      if (strncmp(envp[i],args[1],strlen(args[1]))==0){
                          inPath = 1;
                          strcpy(envp[i],args[1]);
                          strcat(envp[i],"=");
                      }
                      i++;
                  }
                  if (!inPath){
                      envp[i]=malloc(256);
                      char* newString = malloc(strlen(args[1])+3);
                      strcat(newString,args[1]);
                      strcpy(envp[i], newString);
                      strcat(envp[i],"=");
                      envp[i+1]=NULL;
                  }
                  if (strcmp(args[1],"PATH")==0){
                      pathlist = get_path();
                  }
              }
              else{
                  int error = setenv(args[1],args[2],overwrite);
                  while (envp[i]) {
                      if (strncmp(envp[i],args[1],strlen(args[1]))==0){
                          inPath = 1;
                          envp[i] = malloc(sizeof(args[1])+5+sizeof(args[2]));
                          strcpy(envp[i],args[1]);
                          strcat(envp[i],"=");
                          strcat(envp[i],args[2]);
                      }
                      i++;
                  }
                  if (!inPath){
                      envp[i]=malloc(256);
                      char* newString = malloc(strlen(args[1])+3+strlen(args[2]));
                      strcat(newString,args[1]);
                      strcat(newString,"=");
                      strcat(newString,args[2]);
                      strcpy(envp[i], newString);
                      envp[i+1]=NULL;
                  }
                  if (strcmp(args[1],"PATH")==0){
                      pathlist = get_path();
                  }
                  //free(newString);
              }
              
          }
          
          else if (strcmp(args[0],"warnload")==0){
              if (isThreadRunningGl ==1){
                  warnLoadGl = atof(args[1]);
              }
              else{
                  pthread_t tid1;
                  float param = atof(args[1]);
                  pthread_create(&tid1, NULL,warnLoadThread, (void *)&param);
                  isThreadRunningGl = 1;
              }
          }
          else if (strcmp(args[0],"watchmail")==0){
              
              if (args[1]!= NULL){
                  if (args[2] == NULL){
                      char *fileName;
                      DIR           *d;
                      struct dirent *dir;
                      d = opendir(".");
                      if (d)	{
                          while ((dir = readdir(d)) != NULL) {
                              if (strcmp(args[1],dir->d_name) == 0){
                                  fileName = (char *)malloc(sizeof(dir->d_name));
                                  strcpy(fileName,dir->d_name);
                              }
                          }
                          closedir(d);
                      }
                      else{
                          perror("opendir");
                      }
                      
                      if(fileName != NULL){
                          pthread_t tid1;
                          struct WatchMailThreadParams *params = (struct WatchMailThreadParams *)malloc(sizeof(struct WatchMailThreadParams));
                          params->filename = fileName;
                          params->tid = tid1;
                          pthread_create(&tid1,NULL, watchMailThread , params);
                         
                      }
                      else{
                          printf("Error: File does not exist\n");
                      }
                      
                  }
                  else if (strcmp(args[2],"off") == 0){
                      struct WatchMailNode *curr = WMhead;
                      struct WatchMailNode *prev = NULL;
                      while(curr != NULL){
                          if (strcmp(curr->filename,args[1])==0){
                              printf("filename matching thread: %s", curr->filename);
                              //cancel thread
                              pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);
                              int error = pthread_cancel(curr->tid);
                              if (error == ESRCH){
                                  printf("what?");
                              }
                              printf("error: %d\n", error);
                              
                              //remove from list
                              if (prev ==NULL){//at head
                                  WMhead = curr->next;
                              }
                              else{
                                  prev->next = curr->next;
                                  
                              }
                          }
                          prev = curr;
                          curr = curr->next;
                      }
                      //free(curr);
                      //free(prev);
                      struct WatchMailNode *temp = WMhead;
                      while (temp != NULL){
                          printf("Linked List: %s\n",temp->filename);
                          temp = temp->next;
                      }
                  }
                  else{
                      printf("Error: invalid input\n");
                  }
              }
              else{
                  printf("Error: Not enough arguments\n");
              }
              
          }
          else if (strcmp(args[0],"watchuser")==0){
              if (args[1]==NULL){
                  printf("Need to give a username to watch\n");
              }
              else if (args[2]==NULL){
                  //on
                  if (watchUserThreadCreated==0){
                      pthread_create(&threads[99],NULL,watchUserThread,NULL);
                  }
                  watchOn = addUserToList(watchOn,args[1],mutexOn);
              }
              else if (args[3]!=NULL){
                  printf("Too many arguments\n");
              }
              else{
                  //off
                  if (strcmp(args[2],"off")!=0){
                      printf("Only valid second argument is 'off'\n");
                  }
                  else{
                      watchOn = removeUserFromList(watchOn,args[1],mutexOn);
                  }
              }
          }
     /*  else  program to exec */
       /* find it */
       /* do fork(), execve() and waitpid() */
          else{
              if (access(args[0],X_OK)==0){
                  printf("command in current dir\n");
                  
                  if (piped == 0){
                      if (strcmp(background,"&")==0){
                          struct execArgs* currentArgs = (struct execArgs*)malloc(sizeof(struct execArgs));
                          currentArgs->args = (char**) args;
                          currentArgs->pathlist = (struct pathelement*) pathlist;
                          currentArgs->envp = (char**) envp;
                          currentArgs->which = 0;
                          if(threadCurr<98){
                              pthread_create(&threads[++threadCurr],NULL,runInBackground,(void *)currentArgs);
                          }
                      }
                      else{
                          int status;
                          pid_t pid;
                          pid = fork();
                          
                          if (pid==0){
                              
                              execve(args[0], args, envp );
                          }
                          else {
                              pid_gl = pid;
                              waitpid(pid,&status, WEXITSTATUS(status));
                          }
                      }
                      
                  }
                  else{
                      printf("piped is true\n");
                      execve(args[0], args, envp );
                  }
                  
              }
              else if (access(which(args[0],pathlist),X_OK)==0){
                  printf("command using which\n");
                  int status;
                  args = CheckWCChars(args);
                  
                  if (piped == 0){
                      if (strcmp(background,"&")==0){
                          struct execArgs* currentArgs = (struct execArgs*)malloc(sizeof(struct execArgs));
                          currentArgs->args = (char**) args;
                          currentArgs->pathlist = (struct pathelement*) pathlist;
                          currentArgs->envp = (char**) envp;
                          currentArgs->which = 1;
                          if(threadCurr<98){
                              pthread_create(&threads[++threadCurr],NULL,runInBackground,(void *)currentArgs);
                          }
                      }
                      else{
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
                  }
                  else{
                      printf("piped is true\n");
                      execve(which(args[0],pathlist), args, envp );
                  }
              }
              else{
                  int i;
                  if (args[0][0]== '.'||args[0][0]== '/'){
                      fprintf(stderr,"%s: Path does not exist\n",args[0]);
                  }
                  else{
                      fprintf(stderr, "%s: Command not found.\n", args[0]);
                  }
              }
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
      if (redirectFlag == 1){
          redirectFlag = 0;
          close(1);
          dup(saved_stdout);
          close(saved_stdout);
      }
      else if (redirectFlag == 2){
          redirectFlag = 0;
          close(1);
          dup(saved_stdout);
          close(2);
          dup(saved_stderr);
          close(saved_stdout);
          close(saved_stderr);
      }
      else if (redirectFlag == 3){
          redirectFlag = 0;
          close(0);
          dup(saved_stdin);
          close(saved_stdin);
      }
  }
    free(prompt);
    free(commandline);
    free(args);
    free(aliases);
    free(history);
  return 0;
} /* sh() */


void *watchMailThread(void *param){
    
    struct WatchMailThreadParams *params = (struct WatchMailThreadParams *)param;
    char *fileName = params->filename;
    pthread_t tid1 = params->tid;
    
    struct stat *fileInfo = (struct stat *)malloc(sizeof(struct stat));
    stat(fileName, fileInfo);
    signed long long fileSize = (signed long long)malloc(sizeof(signed long long));
    fileSize = fileInfo->st_size;
    
    struct WatchMailNode *newNode = (struct WatchMailNode *)malloc(sizeof(struct WatchMailNode));
    
    if (WMtail == NULL){
        pthread_mutex_lock(&WatchMailMutex);
        WMhead = newNode;
        WMtail = WMhead;
        WMhead->next = NULL;
        pthread_mutex_unlock(&WatchMailMutex);
     }
     else{
         pthread_mutex_lock(&WatchMailMutex);
         WMtail->next = newNode;
         WMtail = newNode;
         pthread_mutex_unlock(&WatchMailMutex);
    }
    pthread_mutex_lock(&WatchMailMutex);
    WMtail->filename = fileName;
    WMtail->tid = tid1;
    pthread_mutex_unlock(&WatchMailMutex);
    
    struct WatchMailNode *tempNode = WMhead;
    printf("Linked List: \n");
    while(tempNode !=NULL){
        printf("%s\n",tempNode->filename);
        tempNode = tempNode->next;
    }
    
    while(1){
        stat(fileName,fileInfo);
        signed long long newFileSize = (signed long long)malloc(sizeof(signed long long));
        newFileSize = fileInfo->st_size;
        printf("New File Size %lld\n",newFileSize);
        if (newFileSize > fileSize){
            printf("\a\n"); //Should cause an audible beep!
            struct timeval *currentTime = (struct timeval *)malloc(sizeof(struct timeval));
            if (gettimeofday(currentTime,NULL)== 0){
                char *time = ctime(&currentTime->tv_sec);
                printf("BEEP You've Got Mail in %s at %s",fileName,time);
            }
            else{
                printf("%s\n",strerror(errno));
            }
            fileSize = newFileSize;
        }
       
        sleep(15);
    }
}

void *warnLoadThread(void *param){
    warnLoadGl = *((float *)param);
    while(1){
        
        if(warnLoadGl == 0.0){
            isThreadRunningGl = 0;
            pthread_exit(&param);
        }
        else{
            double loads[1];
            if ( !get_load(loads) ) {
             if ((float)(loads[0]/100.0f)>warnLoadGl){
             printf("WARNING Load is:%f\n",(float)loads[0]/100.0f);
             PrintPrompt();
             }
            }
        }
        sleep(30);
    }
}


int get_load(double *loads){
    kstat_ctl_t *kc;
    kstat_t *ksp;
    kstat_named_t *kn;
    
    kc = kstat_open();
    if (kc == 0)
    {
        perror("kstat_open");
        exit(1);
    }
    
    ksp = kstat_lookup(kc, "unix", 0, "system_misc");
    if (ksp == 0)
    {
        perror("kstat_lookup");
        exit(1);
    }
    if (kstat_read(kc, ksp,0) == -1)
    {
        perror("kstat_read");
        exit(1);
    }
    
    kn = kstat_data_lookup(ksp, "avenrun_1min");
    if (kn == 0)
    {
        fprintf(stderr,"not found\n");
        exit(1);
    }
    loads[0] = kn->value.ul/(FSCALE/100);
    kstat_close(kc);
    return 0;

}

char **CheckWCChars(char **args){
    char **newArgs = calloc(MAXARGS,sizeof(char*));
    newArgs[0] = args[0];
    int NACounter = 1;
    
    int wildcardStar = 0;
    int wildcardQmark = 0;
    char temp[256];
    int k;
    for(k=1;args[k]!=NULL;k++){
        char *temp = malloc(sizeof(args[k]));
        strcpy(temp,args[k]);
        int i;
        for(i=0; temp[i]!='\0';i++){
            if (temp[i]== '*' && !(wildcardStar==1) && !(wildcardQmark==1)){
                glob_t globWC;
                glob(temp,GLOB_NOCHECK,NULL,&globWC);
                int j;
                for(j=0;globWC.gl_pathv[j]!=NULL;j++){
                    newArgs[NACounter] = globWC.gl_pathv[j];
                    NACounter++;
                }
                wildcardStar = 1;
            }
            
            else if (temp[i]== '?' && !(wildcardQmark==1) && !(wildcardStar==1)){
                glob_t globWC;
                glob(temp,GLOB_NOCHECK,NULL,&globWC);
                int j;
                for(j=0;globWC.gl_pathv[j]!=NULL;j++){
                    newArgs[NACounter] = globWC.gl_pathv[j];
                    NACounter++;
                }
                wildcardQmark = 1;
            }
        }
        if (!(wildcardQmark==1)&& !(wildcardStar==1)){
            newArgs[NACounter] = args[k];
            NACounter++;
        }
    }
    newArgs[NACounter]= 0;
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
   /* loop through pathlist until finding command and return it.  Return
   NULL when not found. */
    char* filePath;
    char* notFound = ": Command Not Found";
    char* notFoundFull;
    struct pathelement * head = pathlist;
    
    if (command == NULL){
        return("which: Too few arguments");
    }
    else{
        //printf("Command is %s\n", command);
        while (head!=NULL){
            filePath = malloc(strlen(head->element)+2+strlen(command));
            strcpy(filePath, head->element); /* copy name into the new var */
            strcat(filePath,"/");
            strcat(filePath, command);
            //printf("FP: %s\n",filePath);
            if( access( filePath, F_OK ) != -1 ) {
                //printf("file exists\n");
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
  /* similarly loop through finding all locations of command */
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
  /* see man page for opendir() and readdir() and print out filenames for
  the directory passed */
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
void* runInBackground(void* neededArgs){
    runningBackgroundThreads++;
    struct execArgs* currentArgs = (struct execArgs*) neededArgs;
    if (currentArgs->which==0){
        int status;
        pid_t pid;
        pid = fork();
        if (pid==0){
            execve(currentArgs->args[0], currentArgs->args, currentArgs->envp );
        }
        else {
            pid_gl = pid;
            printf("[%d] %d\n",runningBackgroundThreads,pid);
            waitpid(pid,&status, WEXITSTATUS(status));
            runningBackgroundThreads--;
        }
    }
    else{
        int status;
        currentArgs->args = CheckWCChars(currentArgs->args);
        pid_t pid;
        pid = fork();
        if (pid==0){
            execve(which(currentArgs->args[0],currentArgs->pathlist), currentArgs->args, currentArgs->envp );
        }
        else {
            pid_gl = pid;
            printf("[%d] %d\n",runningBackgroundThreads,pid);
            waitpid(pid,&status, WEXITSTATUS(status));
            runningBackgroundThreads--;
        }
    }
    pthread_exit(NULL);
}

struct users* addUserToList(struct users* list, char* newUser, pthread_mutex_t l){
    pthread_mutex_lock(&l);
    char* user = malloc(strlen(newUser));
    strcpy(user,newUser);
    if (list==NULL){
        struct users* newNode = (struct users*)malloc(sizeof(struct users*));
        newNode->user = user;
        newNode->next = NULL;
        newNode->isOn = 0;
        newNode->foundThisTime=0;
        pthread_mutex_unlock(&l);
        return newNode;
    }
    else{
        struct users* current = list;
        while (current->next!=NULL){
            current = current->next;
        }
        struct users* newNode = (struct users*)malloc(sizeof(struct users*));
        newNode->user = user;
        newNode->next = NULL;
        newNode->isOn = 0;
        newNode->foundThisTime=0;
        current->next = newNode;
        pthread_mutex_unlock(&l);
        return list;
    }
}
struct users* removeUserFromList(struct users* list, char* removeUser, pthread_mutex_t l){
    pthread_mutex_lock(&l);
    if (list!=NULL){
        struct users* current = list;
        if (strcmp(current->user,removeUser)==0){
            if (list->next==NULL){
                list = NULL;
            }
            else{
                list = list->next;
            }
        }
        else if (current->next!=NULL){
            current = current->next;
            struct users* prevCurrent = list;
            while (current!=NULL){
                if (strcmp(current->user,removeUser)==0){
                    prevCurrent->next = current->next;
                    free(current);
                }
                current = current->next;
                prevCurrent = prevCurrent->next;
            }
        }
    }
    pthread_mutex_unlock(&l);
    return list;
}

void printUserList(struct users* list, pthread_mutex_t l){
    pthread_mutex_lock(&l);
    printf("In here\n");
    if (list==NULL){
        printf("There are no entries\n");
    }
    else{
        printf("User: %s\n",list->user);
        while (list->next!=NULL){
            list = list->next;
            printf("User: %s\n",list->user);
        }
    }
    
    pthread_mutex_unlock(&l);
}

void* watchUserThread(void* args){
    struct utmpx *up;
    time_t retTime;
    while (1){
        pthread_mutex_lock(&mutexOn);
        setutxent();			/* start at beginning */
        while ((up = getutxent() ))	/* get an entry */
        {
            struct users* current = watchOn;
            while (current!=NULL){
                if ( up->ut_type == USER_PROCESS ){
                    if (strcmp(current->user,up->ut_user)==0){
                        if (current->isOn == 0){
                            printf("\n%s has logged on %s from %s\n", up->ut_user, up->ut_line, up ->ut_host);
                            current->isOn = 1;
                        }
                        current->foundThisTime = 1;
                    }
                }
                current = current->next;
            }
        }
        struct users* current = watchOn;
        while (current!=NULL){
            if (current->foundThisTime==0){
                if(current->isOn == 1){
                    printf("\n%s has logged off\n",current->user);
                    current->isOn = 0;
                }
            }
            else{
                current->foundThisTime = 0;
            }
            current = current->next;
        }
        pthread_mutex_unlock(&mutexOn);
        retTime = time(0) + 2;     // Get finishing time.
        while (time(0) < retTime);
    }
}



