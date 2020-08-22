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
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#ifdef __ultrix
#include <sys/uio.h>
#define ssize_t int
#endif /* __ultrix */
#include <sys/stat.h>
#include <fcntl.h>
#include "epsfs_struct.h"
#include "epsfs_core.h"

/* O_BINARY important for Windows-based systems (esp. cygwin or VC++) */
#ifndef O_BINARY
#define O_BINARY 0
#endif /* !O_BINARY */



int read_only=0;        /* read only flag */

dev_id_t device_id;     /* device-ID block cache */
os_header_t os_header;  /* operating system block cache */



static int fildes;      /* filedescriptor for epsfs-file */

/* FAT cache */
#define FAT_CACHE_SIZE 8 /* number of cache blocks */
#define FAT_CACHE_NOT_MODIFIED 0
#define FAT_CACHE_MODIFIED 1
#define FAT_CACHE_NOT_USED ((unsigned long)(-1)) /* invalid blocknr */
static int fat_cache_modified[FAT_CACHE_SIZE];
static unsigned long fat_cache_blk[FAT_CACHE_SIZE];
static fat_t fat_cache[FAT_CACHE_SIZE];



/* static functions prototypes */
static int read_block(char *buffer,unsigned long blknr);
static int write_block(char *buffer,unsigned long blknr);
static int read_fat(unsigned long blknr);
/* static unsigned long next_block(unsigned long blk);*/
static unsigned long next_free_block(unsigned long startblk,
				     unsigned long endblk);
static unsigned long contig_free_block(unsigned long blk,unsigned long contig,
				       unsigned long endblk);
static int change_next_block(unsigned long where,unsigned long newblk);
static int create_chain(unsigned long *startblk,unsigned long *nrblks,
			unsigned long *contig);
static int delete_chain(unsigned long *blknr,unsigned long maxnr,
			unsigned long *delblk);



int
open_device(char *filename)
{

  if (read_only||((fildes=open(filename,O_RDWR|O_BINARY))==-1)){
    if ((fildes=open(filename,O_RDONLY|O_BINARY))==-1){
      fprintf(stderr,"can't open file `%s'.\n",filename);
      return -1;
    }
    fprintf(stderr,"open file `%s' readonly.\n",filename);
    read_only=1;
  }else
    read_only=0;

  init_fat_cache();

  if (read_label())
    return -2;

  return 0;
}



/*
 * raw file functions
 */

/* action==WRITE_FILE : write */
/* action==READ_FILE  : read  */
int
rw_file(char *buffer,unsigned long blknr,unsigned long contig,
	unsigned long maxnr,int action)
{
  unsigned long i;

  if ((action==WRITE_FILE)&&read_only){ /* write although read_only? */
    fprintf(stderr,"error: device readonly.\n");
    return -1;
  }

  if (contig) /* contiguous, only if contig>=2 */
    contig--;

  for (i=0;i<maxnr;i++){ /* max. maxnr times */

    if (action==WRITE_FILE){
      if (write_block(buffer,blknr)) /* write */
	return -1;
    }else{
      if (read_block(buffer,blknr))  /* read */
	return -1;
    }

    if (contig){ /* next block contiguous? */
      blknr++;
      if (blknr>=TOTAL_BLK_NR){
	fprintf(stderr,"error: blocknr too big.\n");
	return -5;
      }
      contig--;
    }else{
      if ((blknr=next_block(blknr))==(unsigned long)-1)
	return -2;

      switch(blknr){
      case BLOCK_FREE:
	fprintf(stderr,"error: free block in chain.\n");
	return -3;
      case BLOCK_EOF:
	return 0;
      case BLOCK_BAD:
	fprintf(stderr,"error: bad block in chain.\n");
	return -4;
      default:
	break;
      }
    }

    buffer+=BLK_SIZE;
  }
  return 1; /* maxnr reached without EOF */
}

/* if startb==NULL: don't create chain, update free-blocks-counter only */
int
create_file_raw(unsigned long *startblk,unsigned long nrblks,
		unsigned long *contig)
{
  unsigned long nb,tmp;

  if (read_only){
    fprintf(stderr,"error: device readonly.\n");
    return -1;
  }

  if (startblk!=NULL){                      /* make chain? */
    nb=nrblks;
    create_chain(startblk,&nrblks,contig);  /* make chain */
    if (nb!=nrblks){ /* requested nr of blocks chained? */
      fprintf(stderr,"error: can't create file.\n");
      /* try to delete chained blocks */
      if (delete_chain(startblk,nrblks,&tmp)<0){
	fprintf(stderr,"error: blocks lost.\n");
      }
      return -2;
    }
  }

  tmp=BLKS_FREE;
  tmp-=nrblks;
  os_header.blks_free=long2long_t(tmp); /* update blks_free */
  if (write_block((char *)&os_header,OS_BLK)){
    fprintf(stderr,"error: can't update free-blocks entry.\n");
    return -3;
  }

  return 0;
}

/* if blknr==0: don't delete chain, update free-blocks-counter only */
int
delete_file_raw(unsigned long blknr,unsigned long maxnr)
{
  unsigned long delblk,tmp;

  if (read_only){
    fprintf(stderr,"error: device readonly.\n");
    return -1;
  }

  if (blknr>0){ /* delete chain? */
    if (delete_chain(&blknr,maxnr,&delblk)<0){
      fprintf(stderr,"error: blocks lost.\n");
    }
  }else
    delblk=maxnr;

  tmp=BLKS_FREE;
  tmp+=delblk;
  os_header.blks_free=long2long_t(tmp); /* update blks_free */
  if (write_block((char *)&os_header,OS_BLK)){
    fprintf(stderr,"error: can't update free-blocks entry.\n");
    return -3;
  }

  return 0;
}



/*
 * device ID and OS header
 */

int
read_label(void)
{

  if (read_block((char *)&device_id,ID_BLK))
    return -1;
  if (read_block((char *)&os_header,OS_BLK))
    return -1;
  return 0;
}

int
write_label(void)
{

  if (read_only){
    fprintf(stderr,"error: device readonly.\n");
    return -1;
  }

  bcopy(OS_SIG,device_id.sig,2);
  bcopy(ID_SIG,os_header.sig,2);

  if (write_block((char *)&os_header,OS_BLK)<0){ /* write new label */
    fprintf(stderr,"error: can't write label.\n");
    return -2;
  }
  if (write_block((char *)&device_id,ID_BLK)<0){ /* write new label */
    fprintf(stderr,"error: can't write label.\n");
    return -3;
  }
  return 0;
}



/* create new filesystem */

/* write empty FAT and root directory, discard caches!!! */
int
create_newfs(void)
{
  unsigned long blk,maxblk;
  static fat_t emptyfat; /* all zeros: good, since BLOCK_FREE==0! */
  static directory_t emptydir; /* all zeros: good, since FILE_FREE==0! */

  bcopy(FB_SIG,emptyfat.sig,2); /* doesn't matter if we do this more than once */
  bcopy(DR_SIG,emptydir.sig,2);

  /*
   * FAT entries for blocks >= TOTAL_BLK_NR: BLOCK_FREE?
   * FAT entry for root directory blocks?
   */

  /* write new FAT */
  maxblk=DATA_BLK_0; /* first datablock */
  for (blk=FB_BLK_0;blk<maxblk;blk++){
    if (write_block((char *)&emptyfat,blk)){
      fprintf(stderr,"error: can't write new FAT, filesystem maybe corrupted.\n");
      return -1;
    }
  }

  /* write new root directory (2 blocks) */
  if (write_block((char *)&emptydir,ROOT_DR_BLK_0)){
    fprintf(stderr,"error: can't write new root dir., filesystem maybe corrupted.\n");
    return -2;
  }
  if (write_block(((char *)&emptydir)+BLK_SIZE,ROOT_DR_BLK_0+1)){
    fprintf(stderr,"error: can't write new root dir., filesystem maybe corrupted.\n");
    return -2;
  }

  /* reset cache!!! */
  init_fat_cache();

  return 0;
}



/*
 * read/write block
 */

static int
read_block(char *buffer,unsigned long blknr)
{

  /* block 0,1,2 are necessary for loading devid,... */
  if ((blknr>=3)&&(blknr>=TOTAL_BLK_NR)){
    fprintf(stderr,"error: can't access block beyond TOTAL_BLK_NR.\n");
    return -1;
  }
  if ((lseek(fildes,(off_t)(blknr*BLK_SIZE),SEEK_SET))==-1){
    fprintf(stderr,"error: can't seek block.\n");
    return -1;
  }
  if (read(fildes,(void *)buffer,(size_t)BLK_SIZE)<BLK_SIZE){
    fprintf(stderr,"error: can't read block.\n");
    return -2;
  }
  return 0;
}

static int
write_block(char *buffer,unsigned long blknr)
{

  if (read_only){
    fprintf(stderr,"error: device readonly.\n");
    return -1;
  }
  /* block 0,1,2 are necessary for loading devid,... */
  if ((blknr>=3)&&(blknr>=TOTAL_BLK_NR)){
    fprintf(stderr,"error: can't access block beyond TOTAL_BLK_NR.\n");
    return -1;
  }
  if ((lseek(fildes,(off_t)(blknr*BLK_SIZE),SEEK_SET))==-1){
    fprintf(stderr,"error: can't seek block.\n");
    return -2;
  }
  if (write(fildes,(void *)buffer,(size_t)BLK_SIZE)<BLK_SIZE){
    fprintf(stderr,"error: can't write block.\n");
    return -3;
  }
  return 0;
}



/*
 * FAT-cache-functions
 */

void
init_fat_cache(void)
{
  int i;

  for (i=0;i<FAT_CACHE_SIZE;i++){
    fat_cache_modified[i]=FAT_CACHE_NOT_MODIFIED;
    fat_cache_blk[i]=FAT_CACHE_NOT_USED;
  }
}

/* input: data-blocknr */
/* return index to fat_cache or <0 if error */
static int
read_fat(unsigned long blknr)
{
  int i,bfound;
  static int randindex=0;

  randindex=(randindex+1)%FAT_CACHE_SIZE; /* "random index" */

  blknr=BLK2FB_BLK(blknr); /* FAT-blocknr - FB_BLK_0 for given data-blknr */

  /* FAT-block already in cache? */
  for (i=0;i<FAT_CACHE_SIZE;i++)
    if (fat_cache_blk[i]==blknr)
      return i; /* index to fat_cache */

  /* FAT-block not in cache. */
  /* is there any free cache-entry? */
  bfound=-1;
  for (i=0;i<FAT_CACHE_SIZE;i++)
    if (fat_cache_blk[i]==FAT_CACHE_NOT_USED){
      bfound=i;
      break;
    }
  if (bfound<0){ /* no free entry? */
    /* is there any NOT_MODIFIED entry? (random for fairness!) */
    bfound=-1;
    for (i=0;i<FAT_CACHE_SIZE;i++)
      if (fat_cache_modified[(i+randindex)%FAT_CACHE_SIZE]
	  ==FAT_CACHE_NOT_MODIFIED){
	bfound=(i+randindex)%FAT_CACHE_SIZE;
	break;
      }
    /* to be fair we should not always take the NOT_MODIFIED ones... */
    if (randindex&1)
      bfound=-1;
    if (bfound<0){ /* all entries MODIFIED? */
      bfound=randindex; /* take random index */
      if (read_only)
	fprintf(stderr,"error: can't write FAT, device readonly.\n");
      else
	/* write modified cache entry */
	if (write_block((char *)(fat_cache+bfound),
			fat_cache_blk[bfound]+FB_BLK_0))
	  fprintf(stderr,"error: can't write FAT.\n");
    }
  }

  /* read new FAT-block into cache */
  if (read_block((char *)(fat_cache+bfound),blknr+FB_BLK_0))
    return -1;
  fat_cache_modified[bfound]=FAT_CACHE_NOT_MODIFIED;
  fat_cache_blk[bfound]=blknr;
  return bfound;
}

/* write modified entries in FAT-cache */
int
flush_fat_cache(void)
{
  int i,retval=0;

  if (read_only){
    fprintf(stderr,"error: can't write FAT, device readonly.\n");
    return -2;
  }

  for (i=0;i<FAT_CACHE_SIZE;i++)
    if (fat_cache_modified[i]==FAT_CACHE_MODIFIED){
      if (write_block((char *)(fat_cache+i),fat_cache_blk[i]+FB_BLK_0))
	retval=-1;
      else
	fat_cache_modified[i]=FAT_CACHE_NOT_MODIFIED;
    }
  return retval;
}



/*
 * FAT-chain-functions
 */

/* FAT: free blocks */
unsigned long
count_free_blks(void)
{
  unsigned long sb,eb,c;

  sb=DATA_BLK_0-1;    /* first data-block -1 */
  eb=TOTAL_BLK_NR;    /* last data-block +1  */

  /* count free blocks in FAT */
  for (c=0;(sb=next_free_block(sb+1,eb))!=(unsigned long)-1;c++)
    ;
  return c;
}

/* this should be static...but it could be interesting for somebody: */
unsigned long
next_block(unsigned long blk)
{
  static fb_entry_t tmp;
  int fbindex;

  /*
   * should we consider the case sb>max. datablock ???
   * or are such blocks automatically marked not_free in FAT ???
   */
  if (blk>=TOTAL_BLK_NR)
    return BLOCK_BAD;
  if ((fbindex=read_fat(blk))<0)
    return (unsigned long)-1;
  bcopy(((char *)(fat_cache+fbindex))+BLK2FB_ENTRY(blk),
	 (char *)&tmp,FB_ENTRY_SIZE);

  return medi_t2long(tmp);
}

static unsigned long
next_free_block(unsigned long startblk,unsigned long endblk)
{
  static fb_entry_t tmp;
  int fbindex;

  while (startblk<endblk){
    if ((fbindex=read_fat(startblk))<0)
      return (unsigned long)-1;
    bcopy(((char *)(fat_cache+fbindex))+BLK2FB_ENTRY(startblk),
	  (char *)&tmp,FB_ENTRY_SIZE);
    if (medi_t2long(tmp)==BLOCK_FREE)
      return startblk;
    startblk++;
  }

  return (unsigned long)-1; /* if startblk>=endblk */
}

/* search contigous free blocks */
static unsigned long
contig_free_block(unsigned long blk,unsigned long contig,unsigned long endblk)
{
  unsigned long i,b,n;

  if (contig) /* contiguous, only if contig(=nr of adjacent free blocks) >=2 */
    contig--;

  b=next_free_block(blk,endblk); /* 1st free block */

  for (i=0;i<contig;){ /* contig adjacent free blocks found? */

    /* next free block */
    if ((n=next_free_block(b+i+1,endblk))==(unsigned long)-1)
      return (unsigned long)-1; /* error */

    if (n!=b+i+1){ /* not adjacent? */
      b=n; /* next chance */
      i=0; /* new begin */
    }else
      i++; /* next adjacent block found */

    if ((b+i+1)>endblk)
      return (unsigned long)-1; /* not found */
  }

  return b;
}

static int
change_next_block(unsigned long where,unsigned long newblk)
{
  static fb_entry_t tmp;
  int fbindex;

  if (read_only){
    fprintf(stderr,"error: device readonly.\n");
    return -1;
  }
  if ((where>=TOTAL_BLK_NR)||(newblk>=TOTAL_BLK_NR))
    return -1;
  if ((fbindex=read_fat(where))<0)
    return -1;
  tmp=long2medi_t(newblk);
  bcopy((char *)&tmp,((char *)(fat_cache+fbindex))+BLK2FB_ENTRY(where),
	FB_ENTRY_SIZE);
  fat_cache_modified[fbindex]=FAT_CACHE_MODIFIED; /* mark modified */
  return 0;
}



/* returns: nrblks=nr of chained blocks
 *          startblk=first free block or: old value if no free block
 *          contig=contiguous or: old value if no free block
 *          return-value=0 if contig free blocks found
 *          return-value=-1 if no free block
 *                       -2 if not enough free blocks
 *                       -3       ''
 *                       -4 if no EOF
 */
static int
create_chain(unsigned long *startblk,unsigned long *nrblks,
	     unsigned long *contig)
{
  unsigned long i,n,next,now,cont,end;
  unsigned int cont_flag;
  int ret=0;

  if (read_only){
    fprintf(stderr,"error: device readonly.\n");
    *nrblks=0;   /* no blocks chained */
    return -1;
  }

  if ((n=*nrblks)<1)
    return 0;

  if (n<(*contig)){
    *nrblks=0;   /* no blocks chained */
    return -1;
  }

  if (!(*contig))
    *contig=1;    /* problems with unsigned<0... */

  end=TOTAL_BLK_NR;

  if ((now=contig_free_block(*startblk,*contig,end))
      ==(unsigned long)-1){   /* seek first contig free blocks */
    fprintf(stderr,"error: can't find contiguous free blocks.\n");
    *nrblks=0;   /* no blocks chained */
    return -1;
  }
  *startblk=now; /* first free block */

  cont_flag=1; /* =may_inc_contig */
  for (i=1;i<n;){

    if (i<(*contig))
      next=now+1;   /* already found above */
    else{
      if ((next=next_free_block(now+1,end))
	==(unsigned long)-1){  /* seek next free block */
	fprintf(stderr,"error: can't find enough free blocks.\n");
	ret=-2;
	break;
      }

      if (cont_flag)
	if (next==(now+1))
	  (*contig)++; /* one more cont. block */
	else
	  cont_flag=0;
    }

    if (change_next_block(now,next)){                    /* make chain */
      fprintf(stderr,"error: can't set next block.\n");
      ret=-3;
      break;
    }

    now=next;
    i++; /* one more block */
  }

  if (change_next_block(now,BLOCK_EOF)){        /* set EOF */
    fprintf(stderr,"error: can't set EOF.\n");
    ret=-4;
    i--; /* last block belongs not to chain if error */
  }

  *nrblks=i;     /* nr of chained blocks */
  return ret;
}

/*
 * returns *delblk : number of deleted blocks
 *         *blknr  : last block
 */
static int
delete_chain(unsigned long *blknr,unsigned long maxnr,unsigned long *delblk)
{
  unsigned long i,newb;

  *delblk=0;

  if (read_only){
    fprintf(stderr,"error: device readonly.\n");
    return -1;
  }

  for (i=0;i<maxnr;i++){ /* max. maxnr times */
    if ((newb=next_block(*blknr))==(unsigned long)-1)
      return -2; /* can't get next block nr. */
    switch(newb){
    case BLOCK_FREE:
      return -6; /* block already free! */
    case BLOCK_EOF:
      if (change_next_block(*blknr,BLOCK_FREE))
	return -3; /* block cannot be deleted */
      (*delblk)++;
      return 0; /* end */
    case BLOCK_BAD:
      fprintf(stderr,"error: bad block in chain.\n");
      return -4;
    default:
      if (change_next_block(*blknr,BLOCK_FREE))
	return -3; /* block cannot be deleted */
      (*delblk)++;
    }
    *blknr=newb;
  }
  return -5; /* maxnr reached without EOF */
}



/* EOF */
