#ifndef _EPSFS_H
#define _EPSFS_H
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



/* if not aready included */
#include "epsfs_struct.h"
#include "epsfs_core.h"



#define GO_TO_ROOT              -1 /* see change_work_dir */



/* working directory entry manipulation */

/* path_t entries of work dir. */
#define WORK_DIR_NAME (work_dir->name)
#define WORK_DIR_FILENR (work_dir->filenr)
#define WORK_DIR_BLK (work_dir->blk)
#define WORK_DIR_PREV (work_dir->prev)
#define WORK_DIR_NEXT (work_dir->next)

/* get work_dir_chache entries */
#define TYPE_OF_FILE(x)         byte_t2long(work_dir_cache.dir[x].file_type)
#define NAME_OF_FILE(x,t)\
  bcopy(work_dir_cache.dir[x].file_name,(t),FILE_NAME_SIZE);\
  (t)[FILE_NAME_SIZE]='\0'
#define RAW_NAME_OF_FILE(x)     (work_dir_cache.dir[x].file_name)
#define SIZE_OF_FILE(x)         word_t2long(work_dir_cache.dir[x].file_size)
#define CONT_OF_FILE(x)         word_t2long(work_dir_cache.dir[x].contiguous)
#define START_BLK_OF_FILE(x)    long_t2long(work_dir_cache.dir[x].file_ptr)

/* set work_dir_chache entries */
#define SET_TYPE_OF_FILE(filenr,type)\
  work_dir_cache.dir[filenr].file_type=long2byte_t((unsigned long)type)
#define SET_NAME_OF_FILE(filenr,filename)\
  convert_name(filename,FILE_NAME_SIZE);\
  bcopy(filename,work_dir_cache.dir[filenr].file_name,FILE_NAME_SIZE)
#define SET_SIZE_OF_FILE(filenr,size_entry)\
  work_dir_cache.dir[filenr].file_size=long2word_t(size_entry)
#define SET_CONT_OF_FILE(filenr,contig)\
  work_dir_cache.dir[filenr].contiguous=long2word_t(contig)
#define SET_START_BLK_OF_FILE(filenr,sb)\
  work_dir_cache.dir[filenr].file_ptr=long2long_t(sb)
#define SET_MULTI_INDEX_OF_FILE(filenr,mi)\
  work_dir_cache.dir[filenr].multi_index=long2byte_t(mi)
#define SET_MULTI_SIZE_OF_FILE(filenr,ms)\
  work_dir_cache.dir[filenr].multi_size=long2word_t(ms)

/*
 * write directory
 * note that contig=3>2 for root-dir
 * since FAT isn't consistent outside data-domain
 */
#define READ_WORK_DIR\
  rw_file((char *)&work_dir_cache,WORK_DIR_BLK,\
          ((WORK_DIR_BLK==ROOT_DR_BLK_0)?3:0),2,READ_FILE)
#define WRITE_WORK_DIR\
  rw_file((char *)&work_dir_cache,WORK_DIR_BLK,\
          ((WORK_DIR_BLK==ROOT_DR_BLK_0)?3:0),2,WRITE_FILE)



/* R/W file */
#define READ_FILE_NR(x,y)\
  rw_file((x),START_BLK_OF_FILE(y),CONT_OF_FILE(y),\
          (unsigned long)-1,READ_FILE)
#define WRITE_FILE_NR(x,y)\
  rw_file((x),START_BLK_OF_FILE(y),CONT_OF_FILE(y),\
          (unsigned long)-1,WRITE_FILE)



/* file-path */
typedef struct{
  char name[FILE_NAME_SIZE+1]; /* filename */
  int filenr;                  /* filenr in parent-dir. */
  unsigned long blk;           /* first fileblock */
  void *prev;                  /* ptr to previous path_t */
  void *next;                  /* ptr to next path_t */
}path_t;


/* working directory */
extern directory_t work_dir_cache;
extern path_t *work_dir;

extern path_t work_dir_path;



int epsfs_startup(char *filename,int read_only);

int change_work_dir(int file_nr);
int seek_free_file(int filenr);
int create_file(char *filename,unsigned char type,unsigned long nrblks,
		unsigned long size_entry,unsigned long contig,int filenr,
		unsigned long startb);
int create_dir(char *name);
int create_data_file(char *name,unsigned char type,unsigned long nblks);
int create_os_file(char *name,unsigned long nblks);
int delete_file_always(int filenr,int del_chain);
int delete_data_file(int filenr);
int delete_dir(int filenr);
int change_name(char *name,int filenr);
int change_type(unsigned char type,int filenr);
int mov_file(int movnr,int destnr);
int hardlink_file(int filenr);

char *convert_name(char *txt,int len);

unsigned long corr_free_blks(int correct);

void issue_label(void);
void issue_work_path(void);
void issue_typenames(void);
void issue_work_dir(void);
void issue_fat_chain(int file_nr);



#endif /* !_EPSFS_H */
