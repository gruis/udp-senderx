#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include <stdio.h>
#include <ctype.h> /* isspace */

#include "log.h"
#include "fifo.h"
#include "udp-sender.h"
#include "udpcast.h"
#include "udpc_process.h"

#define BLOCKSIZE 4096

#ifdef O_BINARY
#include <io.h>
#define HAVE_O_BINARY
#endif

#ifndef O_BINARY
# define O_BINARY 0
#endif

/* trim tailing and leading spaces 
 * http://stackoverflow.com/questions/122616/how-do-i-trim-leading-trailing-whitespace-in-a-standard-way
 **/
char *strtrim(char *str);
/* count occurences of c in s **/
size_t strcnt(const char* s, int c);
/* Provies a single file descriptor that is the concatenation of multiple files.
 **/
int catfood(const char *files);


int openFile(struct disk_config *config)
{
    if(config->fileName != NULL) {
        if (strchr(config->fileName, ',') != NULL || strchr(config->fileName, ';') != NULL)
            return catfood(config->fileName);  

        int in = open(config->fileName, O_RDONLY | O_BINARY, 0);
        if (in < 0) {
#ifdef NO_BB
            extern int errno;
#endif
            udpc_fatal(1, "Could not open file %s: %s\n", config->fileName,
                       strerror(errno));
        }
        return in;
    } else {
#ifdef __MINGW32__
        setmode(0, O_BINARY);
#endif
        return 0;
    }
}


int openPipe(struct disk_config *config, int in, int *pidp)
{
    /**
     * Open the pipe
     */
    *pidp=0;
    if(config->pipeName != NULL) {
        /* pipe */
        char *arg[256];
        int filedes[2];

        parseCommand(config->pipeName, arg);

        if(pipe(filedes) < 0) {
            perror("pipe");
            exit(1);
        }
#ifdef HAVE_O_BINARY
        setmode(filedes[0], O_BINARY);
        setmode(filedes[1], O_BINARY);
#endif
        *pidp=open2(in, filedes[1], arg, filedes[0]);
        close(filedes[1]);
        in = filedes[0];
    }
    return in;
}

/**
 * This file is reponsible for reading the data to be sent from disk
 */
int localReader(struct disk_config *config, struct fifo *fifo, int in)
{
    while(1) {
        int pos = pc_getConsumerPosition(fifo->freeMemQueue);
        int bytes = 
            pc_consumeContiguousMinAmount(fifo->freeMemQueue, BLOCKSIZE);
        if(bytes > (pos + bytes) % BLOCKSIZE)
            bytes -= (pos + bytes) % BLOCKSIZE;

        bytes = read(in, fifo->dataBuffer + pos, bytes);
        if(bytes < 0) {
            perror("read");
            exit(1);
        }

        if (bytes == 0) {
            /* the end */
            pc_produceEnd(fifo->data);
            break;
        } else {
            pc_consumed(fifo->freeMemQueue, bytes);
            pc_produce(fifo->data, bytes);
        }
    }
    return 0;
}


char *strtrim(char *str){
  char *end;

  while(isspace(*str))
    str++;

  if(*str == 0)
    return NULL;

  end = str + strlen(str) - 1;
  while(end > str && isspace(*end))
    end--;

  *(end+1) = 0;
  return str;
}

size_t strcnt(const char* s, int c)
{
  size_t n = 0;
  while(*s != '\0'){
    if(*s++ == c)
      n++;
  }
  return n;
}

/* @todo make the delimeters an argument
 **/
int catfood(const char *files)
{
  int i;
  int cnt             = 0;
  const char delims[] = ",;";
  char *fname;
  char *fnames[strcnt(files, ',') + strcnt(files, ';')];
  int rw[2];
  pid_t pid;

  if((fname = strtok(strdup(files), delims)) == NULL) 
    return -1;
  do {
    if ((fname = strtrim(fname)) == NULL) 
      continue;
    fnames[cnt++] = fname;
  } while ((fname = strtok(NULL, delims)) != NULL);
  fnames[cnt] = NULL;

  char *catcall[cnt + 1];
  catcall[0] = "cat";
  for(i = 0; fnames[i] != NULL; i++)
    catcall[i + 1] = fnames[i];
  catcall[i + 1] = NULL;

  if (pipe(rw) < 0) {
    perror("pipe error");
    exit(1);
  }

  if ((pid = fork()) < 0) {
    return -1;
  } else if (pid > 0) {
    close(rw[1]);
    return rw[0];
  } else {
    close(rw[0]);
    if (dup2(rw[1], STDOUT_FILENO) != STDOUT_FILENO) {
      perror("couldn't reset STDOUT");
      exit(1);
    }
    close(rw[1]);
    if (execvp(*catcall, catcall) < 0) {
      perror("can't execute cat");
      exit(1);
    }
  }
  
  return -1; // should never reach here
}
