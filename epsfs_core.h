#ifndef _EPSFS_CORE_H
#define _EPSFS_CORE_H
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



#ifndef __GCC__
#define inline static
#endif /* __GCC__ */



#define TOTAL_BLK_NR            long_t2long(device_id.blk_nr)
#define BLKS_FREE               long_t2long(os_header.blks_free)

/* first block befind FAT */
#define DATA_BLK_0 (FB_BLK_0+FB_BLK_NR(long_t2long(device_id.blk_nr)))



#define READ_FILE 0 /* for rw_file */
#define WRITE_FILE -1



extern int read_only;        /* read only flag */

extern dev_id_t device_id;
extern os_header_t os_header;



/* conversion to/from unsigned long */
#define byte_t2long(x) ((unsigned long)(x))
#define long2byte_t(x) ((byte_t)(x))

/*
 * note that we don't employ macros for the following conversions
 * since they would eval. their arguments more than once
 */

inline unsigned long
word_t2long(word_t w)
{

  return (unsigned long)(w.b0)
    +(((unsigned long)(w.b1))<<8);
}

inline unsigned long
long_t2long(long_t l)
{

  return (unsigned long)(l.b0)
    +(((unsigned long)(l.b1))<<8)
    +(((unsigned long)(l.b2))<<16)
    +(((unsigned long)(l.b3))<<24);
}

inline unsigned long
medi_t2long(medi_t m)
{

  return (unsigned long)(m.b0)
    +(((unsigned long)(m.b1))<<8)
    +(((unsigned long)(m.b2))<<16);
}

inline word_t
long2word_t(long l)
{
  static word_t res;

  res.b0=(unsigned char)l;
  res.b1=(unsigned char)(l>>8);
  return res;
}

inline medi_t
long2medi_t(long l)
{
  static medi_t res;

  res.b0=(unsigned char)l;
  res.b1=(unsigned char)(l>>8);
  res.b2=(unsigned char)(l>>16);
  return res;
}

inline long_t
long2long_t(long l)
{
  static long_t res;

  res.b0=(unsigned char)l;
  res.b1=(unsigned char)(l>>8);
  res.b2=(unsigned char)(l>>16);
  res.b3=(unsigned char)(l>>24);
  return res;
}



int open_device(char *filename);
int rw_file(char *buffer,unsigned long blknr,unsigned long contig,
	    unsigned long maxnr,int action);
int create_file_raw(unsigned long *startblk,unsigned long nrblks,
		    unsigned long *contig);
int delete_file_raw(unsigned long blknr,unsigned long maxnr);
int read_label(void);
int write_label(void);
int create_newfs(void);
void init_fat_cache(void);
int flush_fat_cache(void);
unsigned long count_free_blks(void);
/* this should be static...but it could be interesting for somebody: */
unsigned long next_block(unsigned long blk);



#endif /* !_EPSFS_CORE_H */
