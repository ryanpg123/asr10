/*
 * Copyright (c) 1997, 1998, 2003, 2011 by Klaus Michael Indlekofer
 *
 * m.indlekofer@gmx.de
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
/*
 * epsfs_xxx Version 1.0
 * Written by Klaus Michael Indlekofer 22-AUG-1998
 */



#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#ifdef __ultrix
#include <sys/uio.h>
#define ssize_t int
#endif /* __ultrix */
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include "epsfs.h"

/* O_BINARY important for Windows-based systems (esp. cygwin or VC++) */
#ifndef O_BINARY
#define O_BINARY 0
#endif /* !O_BINARY */



/* hack for fpurge(stdin) */
#ifndef __FreeBSD__
#define FPURGE_STDIN_ANSWER\
	{\
	  char dummy=answer;\
\
	  while (dummy!='\n')\
	    scanf("%c",&dummy);\
	}
#endif /* !__FreeBSD__ */



/* commands */
enum cmdenum{
  CMD_EXIT,
  CMD_SYNC,
  CMD_LS,
  CMD_PWD,
  CMD_CD,
  CMD_FATLIST,
  CMD_RM,
  CMD_MV,
  CMD_LN,
  CMD_CP,
  CMD_RMDIR,
  CMD_MKDIR,
  CMD_CHNAME,
  CMD_CHTYPE,
  CMD_LABEL,
  CMD_FREE,
  CMD_CHLABEL,
  CMD_NEWFS,
  CMD_SAVE,
  CMD_LOAD,
  CMD_LOADOS,
  CMD_HELP,
  LISTEND
};

static struct cmdentry{
  char *name;
  char *usage;
  enum cmdenum nr;
}cmd[]={
  "exit","usage: exit (=q, bye)\n",CMD_EXIT,
  "q","usage: exit (=q, bye)\n",CMD_EXIT,
  "bye","usage: exit (=q, bye)\n",CMD_EXIT,
  "sync","usage: sync\n",CMD_SYNC,
  "ls","usage: ls [-t] (=dir)\n",CMD_LS,
  "dir","usage: ls [-t] (=dir)\n",CMD_LS,
  "pwd","usage: pwd\n",CMD_PWD,
  "cd","usage: cd [<dir.nr.>|..]\n",CMD_CD,
  "fatlist","usage: fatlist <filenr.>\n",CMD_FATLIST,
  "rm","usage: rm [-e] <filenr.> (=del)\n",CMD_RM,
  "del","usage: rm [-e] <filenr.> (=del)\n",CMD_RM,
  "mv","usage: mv <src.filenr.> <dest.dir.nr.>\n",CMD_MV,
  "ln","usage: ln <clone.filenr.>\n",CMD_LN,
  "cp","usage: cp <src.filenr.> <dest.name> (=copy)\n",CMD_CP,
  "copy","usage: cp <src.filenr.> <dest.name> (=copy)\n",CMD_CP,
  "rmdir","usage: rmdir <filenr.>\n",CMD_RMDIR,
  "mkdir","usage: mkdir <dir.name>\n",CMD_MKDIR,
  "chname","usage: chname <filename> <filenr.>\n",CMD_CHNAME,
  "chtype","usage: chtype <typeid> <filenr.>\n",CMD_CHTYPE,
  "label","usage: label\n",CMD_LABEL,
  "df","usage: df [-c]\n",CMD_FREE,
  "chlabel","usage: chlabel [<disk label>]\n",CMD_CHLABEL,
  "newfs","usage: newfs\n",CMD_NEWFS,
  "get","usage: get <inp.filenr.> <out.filename> (=save, export)\n",CMD_SAVE,
  "save","usage: get <inp.filenr.> <out.filename> (=save, export)\n",CMD_SAVE,
  "export","usage: get <inp.filenr.> <out.filename> (=save, export)\n",CMD_SAVE,
  "put","usage: put <inp.filename> [<out.filename>] <out.typeid> (=load, import)\n",CMD_LOAD,
  "load","usage: put <inp.filename> [<out.filename>] <out.typeid> (=load, import)\n",CMD_LOAD,
  "import","usage: put <inp.filename> [<out.filename>] <out.typeid> (=load, import)\n",CMD_LOAD,
  "loados","usage: loados <inp.filename> <out.filename>\n",CMD_LOADOS,
  "help","usage: help\n",CMD_HELP,
  NULL,NULL,LISTEND
};



/* static functions prototypes */

static int interpret(void);
static int save_file(char *name,int filenr);
static int load_file(char *namefrom,char *nameto,unsigned char type,int is_os);
static int copy_file(char *name,int filenr);



int
main(int argc,char **argv)
{

  if (argc<2){
    printf("usage: %s <device/image file>\n",argv[0]);
    exit(-1);
  }

  fprintf(stderr,"\n*************************************\n");
  fprintf(stderr,"*                                   *\n");
  fprintf(stderr,"*  ASR-10 File Manager              *\n");
  fprintf(stderr,"*  Rev. 1.0                         *\n");
  fprintf(stderr,"*  (c) 1997-2003 by K.M.Indlekofer  *\n");
  fprintf(stderr,"*                                   *\n");
  fprintf(stderr,"*************************************\n\n");
  fprintf(stderr,"try \"help\" for infos\n\n");

  if (epsfs_startup(argv[1],0))
    exit(-1);

  return interpret();
}



/* command interpreter */
static int
interpret(void)
{
#define MAX_CMDLINE_LEN 512
  char cmdline[MAX_CMDLINE_LEN];
  int i;
  int gotcha;
  char *cmdp;
#define MAXARGC 16 /* max. number of arguments +1 */
  int argc;
  char *argv[MAXARGC];

  for (;;){

#if 0
    fprintf(stderr,"\n> ");
    fflush(stderr);
#else
    fprintf(stderr,"asr > ");
#endif

    /* read command line: incl. \n and terminating \0 */
    if (fgets(cmdline,MAX_CMDLINE_LEN,stdin)==NULL) /* EOF or error? */
      return 0;

#define SEPARATION_CHARS " \t\n"
    /* get command string */
    cmdp=strtok(cmdline,SEPARATION_CHARS);
    if (cmdp==NULL)
      continue;
    gotcha=-1;
    for (i=0;cmd[i].nr!=LISTEND;i++)
      if (!strcmp(cmd[i].name,cmdp)){
        gotcha=i;
        break;
      }
    if (gotcha<0){
      fprintf(stderr,"invalid command. (Try `help'.)\n");
      continue;
    }
    /* get arguments */
    argv[0]=cmdp;
    for (argc=1;argc<MAXARGC;argc++)
      if ((argv[argc]=strtok(NULL,SEPARATION_CHARS))==NULL)
	break;


    switch (cmd[gotcha].nr){
    case CMD_EXIT:
      flush_fat_cache();
      return 0;
    case CMD_SYNC:
      flush_fat_cache();
      break;
    case CMD_LS:
      switch (argc){
      case 1:
	issue_work_dir();
	break;
      case 2:
	if (strcmp(argv[1],"-t")==0)
	  issue_typenames();
	else
	  fprintf(stderr,"%s",cmd[gotcha].usage);
	break;
      default:
	fprintf(stderr,"%s",cmd[gotcha].usage);
      }
      break;
    case CMD_CD:
      switch (argc){
      case 1:
	change_work_dir(GO_TO_ROOT);
	break;
      case 2:
	/* note that atoi(not_numeric)==0 */
	change_work_dir(atoi(argv[1]));
	break;
      default:
	fprintf(stderr,"%s",cmd[gotcha].usage);
      }
      break;
    case CMD_PWD:
      issue_work_path();
      break;
    case CMD_FATLIST:
      if (argc!=2){
	fprintf(stderr,"%s",cmd[gotcha].usage);
	break;
      }
      issue_fat_chain(atoi(argv[1]));
      break;
    case CMD_RM:
      switch (argc){
      case 2:
	delete_data_file(atoi(argv[1]));
	break;
      case 3:
	if (strcmp(argv[1],"-e")==0){
	  fprintf(stderr,"warning: if there is no further link to that file\n");
	  fprintf(stderr,"         its blocks will be lost!\n");
	  fprintf(stderr,"If you really want to delete the directory entry type `y' >");
#if 0
	  fflush(stderr);
#endif
	  {
	    char answer='n';

	    scanf("%c",&answer);
#ifdef __FreeBSD__
	fpurge(stdin);
#else
	FPURGE_STDIN_ANSWER;
#endif /* __FreeBSD__ */
	    if (answer=='y')
	      if (delete_file_always(atoi(argv[2]),0)==0)
		fprintf(stderr,"directory entry deleted!\n");
	  }
	}else
	  fprintf(stderr,"%s",cmd[gotcha].usage);
	break;
      default:
	fprintf(stderr,"%s",cmd[gotcha].usage);
      }
      break;
    case CMD_MV:
      if (argc!=3){
	fprintf(stderr,"%s",cmd[gotcha].usage);
	break;
      }
      mov_file(atoi(argv[1]),atoi(argv[2]));
      break;
    case CMD_LN:
      if (argc!=2){
	fprintf(stderr,"%s",cmd[gotcha].usage);
	break;
      }
      fprintf(stderr,"warning: the new directory entry points to the same file.\n");
      fprintf(stderr,"         Be careful if you want to delete such a file.\n");
      fprintf(stderr,"         (Remove the directory entry only for a link count>1!).\n");
      fprintf(stderr,"If you really want to create a new entry type `y' >");
#if 0
      fflush(stderr);
#endif
      {
	char answer='n';

	scanf("%c",&answer);
#ifdef __FreeBSD__
	fpurge(stdin);
#else
	FPURGE_STDIN_ANSWER;
#endif /* __FreeBSD__ */
	if (answer=='y')
	  if (hardlink_file(atoi(argv[1]))==0)
	    fprintf(stderr,"link created!\n");
      }
      break;
    case CMD_CP:
      if (argc!=3){
	fprintf(stderr,"%s",cmd[gotcha].usage);
	break;
      }
      copy_file(argv[2],atoi(argv[1]));
      break;
    case CMD_RMDIR:
      if (argc!=2){
	fprintf(stderr,"%s",cmd[gotcha].usage);
	break;
      }
      delete_dir(atoi(argv[1]));
      break;
    case CMD_MKDIR:
      if (argc!=2){
	fprintf(stderr,"%s",cmd[gotcha].usage);
	break;
      }
      create_dir(argv[1]);
      break;
    case CMD_CHNAME:
      if (argc!=3){
	fprintf(stderr,"%s",cmd[gotcha].usage);
	break;
      }
      change_name(argv[1],atoi(argv[2]));
      break;
    case CMD_CHTYPE:
      if (argc!=3){
	fprintf(stderr,"%s",cmd[gotcha].usage);
	break;
      }
      change_type(atoi(argv[1]),atoi(argv[2]));
      break;
    case CMD_LABEL:
      issue_label();
      break;
    case CMD_FREE:
      switch (argc){
      case 1:
	printf("blocks free (FAT)=%u\n",corr_free_blks(0));
	break;
      case 2:
	if (strcmp(argv[1],"-c")==0)
	  printf("blocks free (FAT)=%u\n",corr_free_blks(-1));
	else
	  fprintf(stderr,"%s",cmd[gotcha].usage);
	break;
      default:
	fprintf(stderr,"%s",cmd[gotcha].usage);
      }
      break;
    case CMD_CHLABEL:
      if (argc==2){
	bcopy(convert_name(argv[1],LABEL_NAME_SIZE),
	      device_id.label,LABEL_NAME_SIZE);
	write_label();
	break;
      }
      if (argc!=1){
	fprintf(stderr,"%s",cmd[gotcha].usage);
	break;
      }
      fprintf(stderr,"warning: a new label will be created.\n");
      fprintf(stderr,"If you really want to create a new label type `y' >");
#if 0
      fflush(stderr);
#endif
      {
	char answer='n';

	scanf("%c",&answer);
#ifdef __FreeBSD__
	fpurge(stdin);
#else
	FPURGE_STDIN_ANSWER;
#endif /* __FreeBSD__ */
	if (answer!='y')
	  break;
      }

      fprintf(stderr,"\nnr of sectors [%lu]:",word_t2long(device_id.sec_nr));
#if 0
      fflush(stderr);
#endif
      if (fgets(cmdline,MAX_CMDLINE_LEN,stdin)!=NULL)
	if (*cmdline!='\n'){
	  device_id.sec_nr=long2word_t(atoi(cmdline));
	}
      fprintf(stderr,"nr of hds [%lu]:",word_t2long(device_id.hd_nr));
#if 0
      fflush(stderr);
#endif
      if (fgets(cmdline,MAX_CMDLINE_LEN,stdin)!=NULL)
	if (*cmdline!='\n'){
	  device_id.hd_nr=long2word_t(atoi(cmdline));
	}
      fprintf(stderr,"nr of cylinders [%lu]:",
	      word_t2long(device_id.cyl_nr));
#if 0
      fflush(stderr);
#endif
      if (fgets(cmdline,MAX_CMDLINE_LEN,stdin)!=NULL)
	if (*cmdline!='\n'){
	  device_id.cyl_nr=long2word_t(atoi(cmdline));
	}
      fprintf(stderr,"nr of bytes per block=%lu\n",BLK_SIZE);
      device_id.blk_size=long2long_t(BLK_SIZE);
      device_id.blk_nr=long2long_t(word_t2long(device_id.sec_nr)
				   *word_t2long(device_id.hd_nr)
				   *word_t2long(device_id.cyl_nr));
      fprintf(stderr,"-> total nr of blocks [%i]:",TOTAL_BLK_NR);
#if 0
      fflush(stderr);
#endif
      if (fgets(cmdline,MAX_CMDLINE_LEN,stdin)!=NULL)
	if (*cmdline!='\n'){
	  device_id.blk_nr=long2long_t(atoi(cmdline));
	}
      os_header.blks_free=long2long_t(TOTAL_BLK_NR-DATA_BLK_0);
      fprintf(stderr,"-> blocks free=%lu (you should call `newfs'!)\n",
	      BLKS_FREE);
      bcopy(device_id.label,cmdline,LABEL_NAME_SIZE);
      cmdline[LABEL_NAME_SIZE]='\0';
      fprintf(stderr,"disk label [%s]:",cmdline);
#if 0
      fflush(stderr);
#endif
      if (fgets(cmdline,MAX_CMDLINE_LEN,stdin)!=NULL)
	if (*cmdline!='\n'){
	  cmdline[strlen(cmdline)-1]='\0'; /* remove '\n' */
	  bcopy(convert_name(cmdline,LABEL_NAME_SIZE),
		device_id.label,LABEL_NAME_SIZE);
	}

      fprintf(stderr,"\nwarning: a new label will be written.\n");
      fprintf(stderr,"If you really want to write a new label type `y' >");
#if 0
      fflush(stderr);
#endif
      {
	char answer='n';

	scanf("%c",&answer);
#ifdef __FreeBSD__
	fpurge(stdin);
#else
	FPURGE_STDIN_ANSWER;
#endif /* __FreeBSD__ */
	if (answer=='y')
	  if (write_label()==0)
	    fprintf(stderr,"new label written!\n");
      }
      break;
    case CMD_NEWFS:
      if (argc!=1){
	fprintf(stderr,"%s",cmd[gotcha].usage);
	break;
      }
      fprintf(stderr,"warning: a new filesystem will be written and all files will be lost.\n");
      fprintf(stderr,"If you really want to create a new filesystem type `y' >");
#if 0
      fflush(stderr);
#endif
      {
	char answer='n';

	scanf("%c",&answer);
#ifdef __FreeBSD__
	fpurge(stdin);
#else
	FPURGE_STDIN_ANSWER;
#endif /* __FreeBSD__ */
	if (answer=='y')
	  if (create_newfs()==0){
	    fprintf(stderr,"new filesystem created!\n");
	    printf("blocks free (FAT)=%u\n",corr_free_blks(0));
	    change_work_dir(GO_TO_ROOT); /* disscard old work dir. cache!!! */
	  }
      }
      break;
    case CMD_SAVE:
      switch (argc){
      case 3:
	save_file(argv[2],atoi(argv[1]));
	break;
      default:
	fprintf(stderr,"%s",cmd[gotcha].usage);
      }
      break;
    case CMD_LOAD:
      switch (argc){
      case 3:
	load_file(argv[1],argv[1],atoi(argv[2]),0);
	break;
      case 4:
	load_file(argv[1],argv[2],atoi(argv[3]),0);
	break;
      default:
	fprintf(stderr,"%s",cmd[gotcha].usage);
      }
      break;
    case CMD_LOADOS:
      if (argc!=3){
	fprintf(stderr,"%s",cmd[gotcha].usage);
	break;
      }
      load_file(argv[1],argv[2],0,-1);
      break;
    case CMD_HELP:
      for (i=0;cmd[i].nr!=LISTEND;i++)
        if ((i>0)&&(cmd[i].nr!=cmd[i-1].nr))
	  printf("%s",cmd[i].usage);
      break;
    default:
      fprintf(stderr,"invalid command. (Try `help'.)\n");
      break;
    }
  }
}



/* static functions */

static int
save_file(char *name,int filenr)
{
  char *buf;
  int fd;
  unsigned long nrblks;

  if (!(buf=(char *)malloc((size_t)(BLK_SIZE*SIZE_OF_FILE(filenr))))){
    fprintf(stderr,"error: can't allocate buffer.\n");
    return -1;
  }

  if (READ_FILE_NR(buf,filenr)){
    fprintf(stderr,"error: can't read file.\n");
    free((void *)buf);
    return -1;
  }

  if ((fd=open(name,O_WRONLY|O_BINARY|O_CREAT,0644))==-1){ /* rw-r--r-- */
    fprintf(stderr,"error: can't open file `%s'.\n",name);
    free((void *)buf);
    return -1;
  }

  switch (TYPE_OF_FILE(filenr)){
  case FILE_PARENTDIR:
  case FILE_SUBDIR:
    /*
     * note that file_size=number of files for a directory
     * and not size of dir.-file!
     */
    nrblks=2;
    break;
  default:
    nrblks=SIZE_OF_FILE(filenr);
  }

  if (write(fd,buf,(size_t)(BLK_SIZE*nrblks))
      !=(ssize_t)BLK_SIZE*SIZE_OF_FILE(filenr))
    fprintf(stderr,"error: not all data saved.\n");

  close(fd);
  free((void *)buf);
  return 0;
}

static int
load_file(char *namefrom,char *nameto,unsigned char type,int is_os)
{
  char *buf;
  int fd;
  struct stat st;
  unsigned long nrblks;
  size_t size;
  int filenr;

  if (stat(namefrom,&st)){
    fprintf(stderr,"error: can't read size of input-file.\n");
    return -1;
  }
  nrblks=(BLK_SIZE-1+(unsigned long)(size=st.st_size))/BLK_SIZE;

  if (!(buf=(char *)malloc((size_t)(BLK_SIZE*nrblks)))){
    fprintf(stderr,"error: can't allocate buffer.\n");
    return -1;
  }

  if ((fd=open(namefrom,O_RDONLY|O_BINARY))==-1){
    fprintf(stderr,"error: can't open file `%s'.\n",namefrom);
    free((void *)buf);
    return -1;
  }

  if (read(fd,(void *)buf,size)!=(ssize_t)size){
    fprintf(stderr,"error: can't read file.\n");
    close(fd);
    free((void *)buf);
    return -1;
  }
  close(fd);

  if (is_os)
    filenr=create_os_file(nameto,nrblks);
  else
    filenr=create_data_file(nameto,type,nrblks);

  if (filenr<0){
    fprintf(stderr,"error: can't create file.\n");
    free((void *)buf);
    return -1;
  }

  if (WRITE_FILE_NR(buf,filenr)){
    fprintf(stderr,"error: can't write file.\n");
    delete_data_file(filenr);
    free((void *)buf);
    return -1;
  }

  free((void *)buf);
  return 0;
}

static int
copy_file(char *name,int filenr)
{
  char *buf;
  int fd;
  unsigned long nrblks;

  switch (TYPE_OF_FILE(filenr)){
  case FILE_FREE:
    fprintf(stderr,"error: no file entry.\n");
    break;
  case FILE_OS:
    fprintf(stderr,"warning: OS file.\n");
    break;
  case FILE_SUBDIR:
  case FILE_PARENTDIR:
    /*
     * to copy directories means creating hardlinks of all files
     * in that directory. this could be allowed but the user
     * should use hardlink and mkdir instead.
     */
    fprintf(stderr,"error: not allowed to copy directory.\n");
    break;
  default:
    break;
  }

  if (!(buf=(char *)malloc((size_t)(BLK_SIZE*SIZE_OF_FILE(filenr))))){
    fprintf(stderr,"error: can't allocate buffer.\n");
    return -1;
  }

  if (READ_FILE_NR(buf,filenr)){
    fprintf(stderr,"error: can't read file.\n");
    free((void *)buf);
    return -1;
  }

  /* create new identical file */
  if ((filenr=create_file(name,
			 TYPE_OF_FILE(filenr),
			 SIZE_OF_FILE(filenr),
			 SIZE_OF_FILE(filenr),
			 1,seek_free_file(1),0))<0){
    fprintf(stderr,"error: can't create new file.\n");
    return -1;
  }

  if (WRITE_FILE_NR(buf,filenr)){
    fprintf(stderr,"error: can't write file.\n");
    delete_data_file(filenr);
    free((void *)buf);
    return -1;
  }

  free((void *)buf);
  return 0;
}



/* EOF */
