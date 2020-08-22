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
#include <string.h>
#include <ctype.h>
#include "epsfs_struct.h"
#include "epsfs_core.h"
#include "epsfs.h"

/* O_BINARY important for Windows-based systems (esp. cygwin or VC++) */
#ifndef O_BINARY
#define O_BINARY 0
#endif /* !O_BINARY */



#define ROOT_NAME          "ROOT        "

/* names for filetypes (see epsfs_struct.h) */
struct{
  unsigned long nr;
  char *name;
}typenames[]={
  FILE_SUBDIR,        "./           ",
  FILE_SINGLE_INSTR,  "   instr eps ",
  FILE_PARENTDIR,     "..           ",
  FILE_MACRO_EPS,     "   macro eps ",
  FILE_BANK_EPSP,     "   bank  eps+",
  FILE_EFFECT_EPSP,   "   effct eps+",
  FILE_SEQ_EPSP,      "   seq   eps+",
  FILE_SONG_EPSP,     "   song  eps+",
  FILE_OS_EPSP,       "os       eps+",
  FILE_SEQ,           "   seq   asr ",
  FILE_SONG,          "   song  asr ",
  FILE_BANK,          "   bank  asr ",
  FILE_OS,            "os       asr ",
  FILE_EFFECT,        "   effct asr ",
  FILE_MACRO,         "   macro asr ",
  (unsigned long)-1,  "?? ???   ??? "
};



/* working directory */
path_t *work_dir; /* pointer in work_dir_path */
directory_t work_dir_cache;

path_t work_dir_path={ROOT_NAME,0,3,NULL,NULL}; /* start path with root */



/* epsfs_startup */

int
epsfs_startup(char *filename,int read_only)
{

  /* read_only=-1;*/
  if (open_device(filename)<0){
    return -1;
  }

  if (change_work_dir(GO_TO_ROOT))
    return -2;

  return 0;
}



/* directory */

int
change_work_dir(int file_nr)
{
  path_t *tmpp;

  if (file_nr>=DR_ENTRY_NR){
    fprintf(stderr,"error: invalid filenr.\n");
    return -3;
  }

  if (file_nr>=0){
    switch(TYPE_OF_FILE(file_nr)){
    case FILE_FREE:
      fprintf(stderr,"error: no such file or directory.\n");
      return 1;
    case FILE_SUBDIR:
      /* save old work_dir */
      tmpp=work_dir;

      /* new work_dir */
      if ((work_dir=(path_t *)malloc(sizeof(path_t)))==NULL){
	work_dir=tmpp; /* restore old value */
	fprintf(stderr,"error: can't allocate path entry.\n");
	return -1;
      }

      /* append new file entry to work_dir_path */
      tmpp->next=(void *)work_dir;

      /* new work_dir entry */
      /* get name from not yet updated work_dir_cache */
      NAME_OF_FILE(file_nr,WORK_DIR_NAME);
      WORK_DIR_FILENR=file_nr;
      WORK_DIR_BLK=START_BLK_OF_FILE(file_nr);
      WORK_DIR_PREV=tmpp; /* link to old work dir. */
      WORK_DIR_NEXT=NULL; /* end of path */
      break;
    case FILE_PARENTDIR:
      /* save old work_dir */
      tmpp=work_dir;
      /* go up in path */
      work_dir=(path_t *)(tmpp->prev);
      WORK_DIR_NEXT=NULL; /* end of path */
      /* delete dir. entry */
      free((void *)tmpp);
      break;
    default:
      fprintf(stderr,"error: file isn't directory.\n");
      return 1;
    }
  }else{
    /* filenr<0 => ROOT */
    /* free path */
    for (tmpp=work_dir_path.next;tmpp!=NULL;tmpp=tmpp->next)
      free((void *)tmpp);
    /* new work_dir */
    work_dir=&work_dir_path;
    WORK_DIR_NEXT=NULL; /* end of path */
  }

  /* read new work dir. (update work_dir_cache) */
  if (READ_WORK_DIR<0){
    fprintf(stderr,"error: can't read directory or data corrupt.\n");
    return -2;
  }
  return 0;
}

int
seek_free_file(int filenr)
{
  int t;

  if ((filenr<0)||(filenr>=DR_ENTRY_NR)){
    fprintf(stderr,"error: invalid filenr.\n");
    return -1;
  }

  /* seek free file, begin at filenr */
  for (;(filenr<DR_ENTRY_NR)&&((t=TYPE_OF_FILE(filenr))!=FILE_FREE);filenr++)
    ;
  if (t==FILE_FREE)
    return filenr;
  return -2;
}

/*
 * if startb!=0: don't create chain, create dir.-entry only,
 *               leave free-blocks-counter!
 */
int
create_file(char *filename,unsigned char type,unsigned long nrblks,
	    unsigned long size_entry,unsigned long contig,int filenr,
	    unsigned long startb)
{
  unsigned long tmp,wd,sb;

  if (read_only){
    fprintf(stderr,"error: device readonly.\n");
    return -1;
  }

  if ((filenr<0)||(filenr>=DR_ENTRY_NR)){
    fprintf(stderr,"error: file not free.\n");
    return -2;
  }

  if (nrblks<1){
    fprintf(stderr,"error: zero length.\n");
    return -3;
  }

  if (nrblks>BLKS_FREE){            /* enough space? */
    fprintf(stderr,"error: not enough space left on device.\n");
    return -4;
  }

  if (nrblks>MAX_FILE_SIZE){
    fprintf(stderr,"error: max. file size exceeded.\n");
    return -5;
  }

  if (type==FILE_FREE){
    fprintf(stderr,"error: invalid type.\n");
    return -6;
  }

  if (startb==0){    /* create new chain? */
    sb=DATA_BLK_0;   /* create file, begin at 1st block behind FAT */
    if (create_file_raw(&sb,nrblks,&contig)<0) /* create file */
      return -7;
  }else              /* chain already exists: */
    sb=startb;       /* take startb as dir.-entry */

  filename=convert_name(filename,FILE_NAME_SIZE); /* correct name */

  /* set directory entry */
  SET_TYPE_OF_FILE(filenr,type);
  SET_NAME_OF_FILE(filenr,filename);
  SET_SIZE_OF_FILE(filenr,size_entry);
  SET_CONT_OF_FILE(filenr,contig);
  SET_START_BLK_OF_FILE(filenr,sb);
  SET_MULTI_INDEX_OF_FILE(filenr,0);
  SET_MULTI_SIZE_OF_FILE(filenr,0);

  /* write work dir. */
  if (WRITE_WORK_DIR<0){
    fprintf(stderr,"error: can't write directory.\n");
    /* try to delete chained blocks */
    if ((startb==0)&&delete_file_raw(sb,nrblks))
      fprintf(stderr,"error: blocks lost.\n");
    return -8;
  }

  /*
   * for non-root directories we must update the file-counter (file_size)
   * for the working directory entry in its parent directory.
   *
   * conventions:
   * note that file_size=number of files for a directory
   * and not size of dir.-file (==2)!
   * note that contiguous(PARENT_DIR entry)
   *           ==filenr(working directory entry in parent dir.)!
   */
  if (WORK_DIR_BLK!=ROOT_DR_BLK_0){
    /* filenr of workdir. in parentdir. */
    wd=CONT_OF_FILE(PARENT_DIR);
    /* change to parent dir. */
    if (change_work_dir(PARENT_DIR)){
      fprintf(stderr,"error: can't change to parent directory.\n");
      return -9;
    }
    /* update file-counter */
    tmp=SIZE_OF_FILE(wd);
    SET_SIZE_OF_FILE(wd,tmp+1);
    /* write parent directory */
    if (WRITE_WORK_DIR<0){
      fprintf(stderr,"error: can't write directory.\n");
      /* try to delete chained blocks */
      if ((startb==0)&&delete_file_raw(sb,nrblks))
	fprintf(stderr,"error: blocks lost.\n");
      return -10;
    }
    if (change_work_dir(wd)) /* back */
      fprintf(stderr,"error: can't change back to directory.\n");
  }

  return filenr;
}



int
create_dir(char *name){
  static int first_time=1;
  /* temp_dir initialized with zeros ! good, since FILE_FREE==0 */
  static directory_t temp_dir;
  int i,dir;

  if (first_time){
    /* link to parent-directory */
    temp_dir.dir[0].file_type=long2byte_t(FILE_PARENTDIR);
    /* directory signature */
    bcopy(DR_SIG,temp_dir.sig,2);
    first_time=0;
  }

  /*
   * conventions:
   * note that file_size=number of files for a directory
   * and not size of dir.-file (==2)!
   * note that contiguous(PARENT_DIR entry)
   *           ==filenr(working directory entry in parent dir.)!
   */
  /* create dir.-file: contig should be 2 (?), no files (size_entry=0) */
  if ((dir=create_file(name,FILE_SUBDIR,2,0,2,seek_free_file(1),0))<0){
    fprintf(stderr,"error: can't create directory.\n");
    return -1;
  }
  /* parent-directory-name */
  bcopy(WORK_DIR_NAME,temp_dir.dir[PARENT_DIR].file_name,FILE_NAME_SIZE);
  /* link to parent-directory */
  temp_dir.dir[PARENT_DIR].file_ptr=long2long_t(WORK_DIR_BLK);
  /* filenr of new dir. in parentdir. */
  temp_dir.dir[PARENT_DIR].contiguous=long2word_t(dir);

  /* write new directory */
  if (rw_file((char *)&temp_dir,START_BLK_OF_FILE(dir),2,2,WRITE_FILE)<0){
    fprintf(stderr,"error: can't write directory.\n");
    /* try to delete chained blocks */
    if (delete_file_always(dir,1))
      fprintf(stderr,"error: blocks lost.\n");
    return -2;
  }

  return dir;
}

int
create_data_file(char *name,unsigned char type,unsigned long nrblks)
{

  switch (type){
  case FILE_FREE:
  case FILE_OS:
  case FILE_SUBDIR:
  case FILE_PARENTDIR:
    fprintf(stderr,"error: invalid type.\n");
    return -11;
  default:
    break;
  }

  return create_file(name,type,nrblks,nrblks,1,seek_free_file(1),0);
}

int
create_os_file(char *name,unsigned long nrblks)
{

  if (change_work_dir(GO_TO_ROOT)){
    fprintf(stderr,"error: can't change to root-directory.\n");
    return -11;
  }

  if (seek_free_file(0)){ /* OS should be in ROOT_DIR at filenr=0 */
    fprintf(stderr,"error: os place not empty.\n");
    return -12;
  }

  return create_file(name,FILE_OS,nrblks,nrblks,1,0,0);
}



/*
 * if del_chain==0: don't delete chain, delete dir.-entry only,
 *                  leave free-blocks-counter!
 */
int
delete_file_always(int filenr,int del_chain)
{
  unsigned long startb,tmp,wd;

  if (read_only){
    fprintf(stderr,"error: device readonly.\n");
    return -1;
  }

  if ((filenr<0)||(filenr>=DR_ENTRY_NR)){
    fprintf(stderr,"error: invalid filenr.\n");
    return -2;
  }

  if (TYPE_OF_FILE(filenr)==FILE_FREE){
    fprintf(stderr,"error: no file entry.\n");
    return -3;
  }

  if ((startb=START_BLK_OF_FILE(filenr))==ROOT_DR_BLK_0){
    fprintf(stderr,"error: can't delete root-directory.\n");
    return -4;
  }

  /* set directory entry: file free */
  SET_TYPE_OF_FILE(filenr,FILE_FREE);

  /* write work dir. */
  if (WRITE_WORK_DIR<0){
    fprintf(stderr,"error: can't write directory.\n");
    return -5;
  }

  /*
   * for non-root directories we must update the file-counter (file_size)
   * for the working directory entry in its parent directory.
   *
   * conventions:
   * note that file_size=number of files for a directory
   * and not size of dir.-file (==2)!
   * note that contiguous(PARENT_DIR entry)
   *           ==filenr(working directory entry in parent dir.)!
   */
  if (WORK_DIR_BLK!=ROOT_DR_BLK_0){
    /* filenr of workdir. in parentdir. */
    wd=CONT_OF_FILE(PARENT_DIR);
    /* change to parent dir. */
    if (change_work_dir(PARENT_DIR)){
      fprintf(stderr,"error: can't change to parent directory.\n");
      return -6;
    }
    /* update file-counter */
    tmp=SIZE_OF_FILE(wd);
    SET_SIZE_OF_FILE(wd,tmp-1);
    /* write parent directory */
    if (WRITE_WORK_DIR<0){
      fprintf(stderr,"error: can't write directory.\n");
      return -7;
    }
    if (change_work_dir(wd)){ /* back */
      fprintf(stderr,"error: can't change back to directory.\n");
      return -8;
    }
  }

  /* delete file completely */
  if (del_chain&&delete_file_raw(startb,(unsigned long)-1)){
    fprintf(stderr,"error: can't delete file data.\n");
    return -9;
  }

  return 0;
}

int
delete_data_file(int filenr)
{

  if (read_only){
    fprintf(stderr,"error: device readonly.\n");
    return -1;
  }
  if ((filenr<0)||(filenr>=DR_ENTRY_NR)){
    fprintf(stderr,"error: invalid filenr.\n");
    return -2;
  }

  switch (TYPE_OF_FILE(filenr)){
  case FILE_FREE:
    fprintf(stderr,"error: no file entry.\n");
    return -3;
  case FILE_SUBDIR:
  case FILE_PARENTDIR:
    fprintf(stderr,"error: file is directory.\n");
    return -10;
  default:
    break;
  }

  return delete_file_always(filenr,1); /* ok, delete */
}

int
delete_dir(int filenr)
{

  if (read_only){
    fprintf(stderr,"error: device readonly.\n");
    return -1;
  }
  if ((filenr<0)||(filenr>=DR_ENTRY_NR)){
    fprintf(stderr,"error: invalid filenr.\n");
    return -2;
  }

  switch (TYPE_OF_FILE(filenr)){
  case FILE_SUBDIR:
    break;
  case FILE_PARENTDIR:
    fprintf(stderr,"error: file is parent-directory.\n");
    return -10;
  default:
    fprintf(stderr,"error: file is not directory.\n");
    return -11;
  }

  if (SIZE_OF_FILE(filenr)==0)
    return delete_file_always(filenr,1); /* ok, delete */

  fprintf(stderr,"error: directory is not empty.\n");
  return -12;
}

/*
 * change name of file
 * problem: if we change the name of a directory
 * all subdirectories should be changed too in their
 * parentdir.name entry (filenr.=0) !
 * this must be fixed in future releases...
 */
int
change_name(char *name,int filenr)
{

  if (read_only){
    fprintf(stderr,"error: device readonly.\n");
    return -1;
  }

  if ((filenr<0)||(filenr>=DR_ENTRY_NR)){
    fprintf(stderr,"error: invalid filenr.\n");
    return -2;
  }

  switch (TYPE_OF_FILE(filenr)){
  case FILE_FREE:
    fprintf(stderr,"error: no file entry.\n");
    return -3;
  case FILE_SUBDIR:
    fprintf(stderr,"warning: you should update parentname in subdirs.\n");
    break;
  case FILE_PARENTDIR:
    fprintf(stderr,"warning: should be the same as parentname.\n");
    break;
  default:
    break;
  }

  name=convert_name(name,FILE_NAME_SIZE); /* correct name */

  /* new name */
  SET_NAME_OF_FILE(filenr,name);
  /* write work dir. */
  if (WRITE_WORK_DIR<0){
    fprintf(stderr,"error: can't write directory.\n");
    return -4;
  }
  return 0;
}

int
change_type(unsigned char type,int filenr)
{

  if (read_only){
    fprintf(stderr,"error: device readonly.\n");
    return -1;
  }

  if ((filenr<0)||(filenr>=DR_ENTRY_NR)){
    fprintf(stderr,"error: invalid filenr.\n");
    return -2;
  }

  switch (type){
  case FILE_FREE:
  case FILE_SUBDIR:
  case FILE_PARENTDIR:
    fprintf(stderr,"error: invalid type.\n");
    return -3;
  default:
    break;
  }

  switch (TYPE_OF_FILE(filenr)){
  case FILE_FREE:
    fprintf(stderr,"error: no file entry.\n");
    return -4;
  case FILE_SUBDIR:
  case FILE_PARENTDIR:
    fprintf(stderr,"error: can't change type of directory.\n");
    return -5;
  default:
    break;
  }

  /* new type */
  SET_TYPE_OF_FILE(filenr,type);
  /* write work dir. */
  if (WRITE_WORK_DIR<0){
    fprintf(stderr,"error: can't write directory.\n");
    return -6;
  }
  return 0;
}

int
mov_file(int movnr,int destnr)
{
  dr_entry_t de;
  unsigned long pp;
  int back,newnr,mov_dir;
  char txt[FILE_NAME_SIZE];

  if (read_only){
    fprintf(stderr,"error: device readonly.\n");
    return -1;
  }

  if ((movnr<0)||(movnr>=DR_ENTRY_NR)){
    fprintf(stderr,"error: invalid filenr.\n");
    return -2;
  }

  if ((destnr<0)||(destnr>=DR_ENTRY_NR)){
    fprintf(stderr,"error: invalid filenr.\n");
    return -3;
  }

  if (destnr==movnr){
    fprintf(stderr,"error: can't move to same directory.\n");
    return -13;
  }

  switch (TYPE_OF_FILE(movnr)){
  case FILE_FREE:
    fprintf(stderr,"error: no file entry.\n");
    return -4;
  case FILE_SUBDIR:
    mov_dir=-1; /* move directory */
    break;
  case FILE_PARENTDIR:
    fprintf(stderr,"error: can't move parent-directory.\n");
    return -5;
  default:
    mov_dir=0; /* move data-file */
  }

  switch (TYPE_OF_FILE(destnr)){
  case FILE_FREE:
    fprintf(stderr,"error: no dest.-dir. entry.\n");
    return -6;
  case FILE_SUBDIR:
    back=PARENT_DIR;
    break;
  case FILE_PARENTDIR:
    /* filenr of work_dir in parent-dir. */
    back=(int)CONT_OF_FILE(PARENT_DIR);
    break;
  default:
    fprintf(stderr,"error: dest. is not directory.\n");
    return -7;
  }

  de=work_dir_cache.dir[movnr]; /* old dir.-entry */

  /* change to dest.-dir. */
  if (change_work_dir(destnr)){
    fprintf(stderr,"error: can't change to dest.-directory.\n");
    return -8;
  }

  /* create new dir.-entry from old dir.-entry de */
  if ((newnr=create_file(de.file_name,(unsigned char)byte_t2long(de.file_type),
			 1,word_t2long(de.file_size),word_t2long(de.contiguous),
			 seek_free_file(1),long_t2long(de.file_ptr)))<0){
    fprintf(stderr,"error: can't create dir.-entry in dest.-directory.\n");
    if (change_work_dir(back)) /* back */
      fprintf(stderr,"error: can't change back to src.-directory.\n");
    return -9;
  }

  /* move directory? */
  if (mov_dir){
    /* save work dir.-name */
    bcopy(WORK_DIR_NAME,txt,FILE_NAME_SIZE);
    /* save pointer to work dir. */
    pp=WORK_DIR_BLK;

    /* change to new dir. */
    if (change_work_dir(newnr))
      fprintf(stderr,"error: can't change to new directory.\n");
    else{
      /*
       * conventions:
       * note that contiguous(PARENT_DIR entry)
       *           ==filenr(working directory entry in parent dir.)!
       */
      /* new parent-dir. name */
      SET_NAME_OF_FILE(PARENT_DIR,txt);
      /* pointer to parent-dir. */
      SET_START_BLK_OF_FILE(PARENT_DIR,pp);
      /* filenr of work_dir in parent-dir. */
      SET_CONT_OF_FILE(PARENT_DIR,newnr);
      /* write directory */
      if (WRITE_WORK_DIR<0)
	fprintf(stderr,"error: can't write directory.\n");

      if (change_work_dir(PARENT_DIR)){ /* back */
	fprintf(stderr,"error: can't change to parent-dir. of new directory.\n");
	return -10;
      }
    }
  }

  if (change_work_dir(back)){ /* back */
    fprintf(stderr,"error: can't change back to src.-directory.\n");
    return -11;
  }

  if (delete_file_always(movnr,0)){ /* delete old dir.-entry */
    fprintf(stderr,"error: can't delete dir.-entry.\n");
    return -12;
  }

  return 0;
}



int
hardlink_file(int filenr)
{
  dr_entry_t de;

  if (read_only){
    fprintf(stderr,"error: device readonly.\n");
    return -1;
  }

  if ((filenr<0)||(filenr>=DR_ENTRY_NR)){
    fprintf(stderr,"error: invalid filenr.\n");
    return -2;
  }

  switch (TYPE_OF_FILE(filenr)){
  case FILE_FREE:
    fprintf(stderr,"error: no file entry.\n");
    return -4;
  case FILE_SUBDIR:
    fprintf(stderr,"warning: linking directory.\n");
    break;
  case FILE_PARENTDIR:
    fprintf(stderr,"error: can't link parent-directory.\n");
    return -5;
  default:
    break;
  }

  de=work_dir_cache.dir[filenr]; /* dir.-entry */

  /* create new dir.-entry */
  if (create_file(de.file_name,(unsigned char)byte_t2long(de.file_type),
		  1,word_t2long(de.file_size),word_t2long(de.contiguous),
		  seek_free_file(1),long_t2long(de.file_ptr))<0){
    fprintf(stderr,"error: can't create dir.-entry.\n");
    return -9;
  }

  return 0;
}



/* filenames should be uppercase */

char *
convert_name(char *txt,int len)
{
  static char name[MAX_NAME_SIZE];
  int slen,i;

  if (len>MAX_NAME_SIZE)
    len=MAX_NAME_SIZE;
  slen=strlen(txt);
  if (slen>len)
    slen=len;

  for (i=0;i<slen;i++)
    name[i]=(char)toupper((int)txt[i]); /* upper case letters */
  for (;i<len;i++)
    name[i]=' '; /* blank */

  return name;
}



/* count free blocks and correct */

unsigned long
corr_free_blks(int correct)
{
  unsigned long c;

  c=count_free_blks();

  if (c!=BLKS_FREE){
    fprintf(stderr,"warning: nr of free blks inconsistent.\n");
    if (correct){     /* take correct value? */
      if (read_only){
	fprintf(stderr,"error: can't correct, device readonly.\n");
	return (unsigned long)-1;
      }else{
	delete_file_raw(0,c-BLKS_FREE); /* BLKS_FREE=c */
	fprintf(stderr,"warning: free blks counter adjusted.\n");
      }
    }
  }

  return c;
}



/* print functions */

void
issue_label(void)
{
  static char txt[LABEL_NAME_SIZE+1];

  printf("OS header:\n");
  printf("  blocks free=%lu\n",BLKS_FREE);
  printf("  OS version major=%lu\n",byte_t2long(os_header.os_major));
  printf("  OS version minor=%lu\n",byte_t2long(os_header.os_minor));
  printf("  ROM version major=%lu\n",byte_t2long(os_header.rom_major));
  printf("  ROM version minor=%lu\n",byte_t2long(os_header.rom_minor));
  printf("device id:\n");
  printf("  peripheral device type=%lu\n",byte_t2long(device_id.peri_dev_type));
  printf("  removable media device type=%lu\n",
	 byte_t2long(device_id.rem_dev_type));
  printf("  standard version nr=%lu\n",byte_t2long(device_id.std_ver));
  printf("  SCSI=%lu\n",byte_t2long(device_id.scsi_reserved));
  printf("  nr of sectors=%lu\n",word_t2long(device_id.sec_nr));
  printf("  nr of heads=%lu\n",word_t2long(device_id.hd_nr));
  printf("  nr of cylinders=%lu\n",word_t2long(device_id.cyl_nr));
  printf("  nr of bytes per block=%lu\n",long_t2long(device_id.blk_size));
  printf("  total nr of blocks=%lu\n",long_t2long(device_id.blk_nr));
  printf("  SCSI medium type=%lu\n",byte_t2long(device_id.scsi_type));
  printf("  SCSI density code=%lu\n",byte_t2long(device_id.scsi_dens));
  bcopy(device_id.label,txt,LABEL_NAME_SIZE);
  txt[LABEL_NAME_SIZE]='\0';
  printf("  disk label=%s\n",txt);
}

void
issue_work_path(void)
{
  path_t *p;

  printf("working directory: %s\npath=\n",WORK_DIR_NAME);

  for (p=&work_dir_path;p!=NULL;p=p->next)
    printf("  (%2i)%s @%lu\n",p->filenr,p->name,p->blk);
}

void
issue_typenames(void)
{
  static int j;

  printf("  type     dev  id \n");
  printf("-------------------\n");
  for (j=0;typenames[j].nr!=(unsigned long)-1;j++)
    printf("  %s%3lu\n",typenames[j].name,typenames[j].nr);
  printf("-------------------\n");
}

void
issue_work_dir(void)
{
  static char txt[FILE_NAME_SIZE+1];
  static int i,j;
  static unsigned long ftype;

  printf(" nr | type     dev  id | name         | size  | cont  | startblk \n");
  printf("-----------------------------------------------------------------\n");
  for (i=0;i<DR_ENTRY_NR;i++){
    if ((ftype=TYPE_OF_FILE(i))==FILE_FREE)
      continue;
    printf("%3i | ",i);
    for (j=0;typenames[j].nr!=(unsigned long)-1;j++)
      if (ftype==typenames[j].nr)
	break;
    NAME_OF_FILE(i,txt);
    printf("%s%3lu |%13s |%6lu |%6lu |%9lu\n",
	   typenames[j].name,
	   ftype,
	   txt,
	   SIZE_OF_FILE(i),
	   CONT_OF_FILE(i),
	   START_BLK_OF_FILE(i));
  }
  printf("-----------------------------------------------------------------\n");
}

void
issue_fat_chain(int file_nr)
{
  static unsigned long t,b,l;

  if ((file_nr<0)||(file_nr>=DR_ENTRY_NR)){
    fprintf(stderr,"error: invalid filenr.\n");
    return;
  }

  switch (t=TYPE_OF_FILE(file_nr)){
  case FILE_FREE:
    fprintf(stderr,"error: no file entry.\n");
    return;
  case FILE_SUBDIR:
    printf("\nsubdirectory.");
    break;
  case FILE_PARENTDIR:
    printf("\nparentdirectory.");
    break;
  default:
    printf("\ndata-file.");
  }

  printf("\nFAT-block-chain of file nr. %i:\nstartblk=\n",file_nr);
  b=START_BLK_OF_FILE(file_nr);
  for (l=0;b;l++){
    printf("%lu\n",b);
    switch(b=next_block(b)){
    case BLOCK_FREE:
      fprintf(stderr,"warning: free block in chain.\n");
      b=0;
      break;
    case BLOCK_EOF:
      printf("=EOF.\n");
      b=0;
      break;
    case BLOCK_BAD:
      fprintf(stderr,"warning: bad block in chain.\n");
      b=0;
      break;
    default:
      break;
    }
  }

  switch (t){
  case FILE_SUBDIR:
    printf("contiguous(dir.entry)=%lu\n",CONT_OF_FILE(file_nr));
    printf("nr. of files=%lu\n",b=SIZE_OF_FILE(file_nr));
    if (l!=2)
      fprintf(stderr,"warning: directory-size(FAT) wrong.\n");
    break;
  case FILE_PARENTDIR:
    printf("filenr. of workdir. in parentdir.=%lu\n",CONT_OF_FILE(file_nr));
    if (l!=2)
      fprintf(stderr,"warning: parentdirectory-size(FAT) wrong.\n");
    break;
  default:
    printf("contiguous(dir.entry)=%lu\n",CONT_OF_FILE(file_nr));
    printf("size(dir.entry)=%lu\n",b=SIZE_OF_FILE(file_nr));
    printf("size(FAT)=%lu\n",l);
    if (l!=b)
      fprintf(stderr,"warning: file-size inconsistent.\n");
  }
}



/* EOF */
