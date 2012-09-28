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

 #define S_ISDIR(e)       (((e)&_S_IFDIR)!=0)
 #define PATHSEP          '\\'
 #define PATHSEP2         '/'
 #define PERMISSIONS      (_S_IREAD|_S_IWRITE)
#else
 #define PATHSEP          '/'
 #define PERMISSIONS      (0777)
#endif

/**
 * Check that the path is an existing directory.
 */
static int exists(char *path)
{ struct _stat s={0};
  char buf[2048]={0};
  int i,n;
  // normalize path string for stat
  TRY((n=strlen(path))<2048);
  memcpy(buf,path,n);
#ifdef _MSC_VER // only necessary on windows
  if(buf[n-1]==PATHSEP || buf[n-1]==PATHSEP2) // remove terminating path seperator
    buf[n-1]='\0';
  for(i=0;i<n;++i)
    if(buf[i]==PATHSEP) 
      buf[i]=PATHSEP2;
#endif
  TRY(stat(buf,&s)==0);
  TRY(S_ISDIR(s.st_mode));
  return 1;
Error:
  perror(path);
  return 0;  
}

/**
 * Make all the directories leading up to \a path, then create \a path.
 *
 * \returns 1 on success, 0 otherwise.
 */
int mkpath(char* path)
{ char* p=0;
  int any=0;
  TRY(path && *path);
  for (p=strchr(path+1, PATHSEP); p; p=strchr(p+1, PATHSEP)) 
  { *p='\0';
    mkdir(path,PERMISSIONS);
    *p=PATHSEP; // exists() has trouble with windows path seperators, so switch to unix style
  }
  mkdir(path,PERMISSIONS);
  TRY(exists(path));
  return 1;
Error:  
  if(p) *p=PATHSEP;   
  perror(path);
  return 0;
}