/**
 * \file
 * Make path.
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>  //for logging
#include <string.h> //for strchr
#include <errno.h>

#define ENDL     "\n"
#define LOG(...) fprintf(stderr,__VA_ARGS__) 
#define TRY(e)   do{if(!(e)) { LOG("%s(%d): %s()"ENDL "\tExpression evaluated as false."ENDL "\t%s"ENDL,__FILE__,__LINE__,__FUNCTION__,#e); goto Error;}} while(0)

#ifdef _MSC_VER
 #include <direct.h>

 #define mkdir(name,mode) _mkdir(name)
 #define stat             _stat
 #define chmod            _chmod
 #define umask            _umask

 #define PATHSEP          '\\'
 #define PERMISSIONS      (_S_IREAD|_S_IWRITE)
#else
 #define PATHSEP          '/'
 #define PERMISSIONS      (0777)
#endif

/**
 * Make all the directories leading up to \a path, then create \a path.
 *
 * \returns 1 on success, 0 otherwise.
 */
int mkpath(char* path)
{ char* p=0;
  TRY(path && *path);
  for (p=strchr(path+1, PATHSEP); p; p=strchr(p+1, PATHSEP)) 
  { *p='\0';
    if(mkdir(path,PERMISSIONS)==-1)
      TRY(errno==EEXIST);
    *p=PATHSEP;
  }
  if(mkdir(path,PERMISSIONS)==-1)
    TRY(errno==EEXIST);
  return 1;
Error:  
  if(p)
  { *p=PATHSEP;
    perror(path);
  } 
  return 0;
}