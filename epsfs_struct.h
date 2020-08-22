#ifndef _EPSFS_STRUCT_H
#define _EPSFS_STRUCT_H
/*
 * Copyright (c) 1997, 1998, 2003, 2011 by K. M. Indlekofer
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



/* MAX_NAME_SIZE must be >= max. of all *_NAME_SIZE */
#define MAX_NAME_SIZE 16



/* block size */
#define BLK_SIZE         512 /* block-size in bytes */



/* user-block */
#define USER_BLK           0



/* big endian types */
typedef unsigned char byte_t;
typedef struct{unsigned char b1,b0;}word_t;
typedef struct{unsigned char b2,b1,b0;}medi_t;
typedef struct{unsigned char b3,b2,b1,b0;}long_t;



/* device-ID-block */
#define ID_SIZE            1
#define ID_BLK             1
#define ID_SIG             "ID"
#define LABEL_NAME_SIZE    7 /* disklabelname size in bytes */
#define LABEL_HEADER    0xff

typedef struct{
  byte_t peri_dev_type;         /* peripheral device type */
  byte_t rem_dev_type;          /* removable media device type */
  byte_t std_ver;               /* various standard version nr */
  byte_t scsi_reserved;         /* reserved for SCSI */
  word_t sec_nr;                /* nr of sectors */
  word_t hd_nr;                 /* nr of r/w heads */
  word_t cyl_nr;                /* nr of cylinders (=tracks) */
  long_t blk_size;              /* nr of bytes per block should be BLK_SIZE */
  long_t blk_nr;                /* total nr of blocks */
  byte_t scsi_type;             /* SCSI medium type */
  byte_t scsi_dens;             /* SCSI density code */
  byte_t reserved[10];          /* reserved for future use */
  byte_t label_header;          /* label-header 0xff if disk-label else 0x00 */
  char label[LABEL_NAME_SIZE];  /* disk-label */
  char sig[2];                  /* signature ID */
  char dummy[BLK_SIZE-LABEL_NAME_SIZE-33];
}dev_id_t;



/* operating-system-block */
#define OS_SIZE            1
#define OS_BLK             2
#define OS_SIG             "OS"

typedef struct{
  long_t blks_free;
  byte_t os_major;              /* OS version major.minor */
  byte_t os_minor;
  byte_t rom_major;             /* ROM version major.minor */
  byte_t rom_minor;
  byte_t zero[20];              /* 20 * 0x00 */
  char sig[2];                  /* signature OS */
  char dummy[BLK_SIZE-30];
}os_header_t;



/* directory */
#define DR_SIZE            2
#define ROOT_DR_BLK_0      3 /* 1st root-directory block */
#define DR_ENTRY_NR       39 /* nr of files per directory */
#define DR_SIG             "DR"
#define FILE_NAME_SIZE    12 /* filename size in bytes */
#define MAX_FILE_SIZE 0xffff /* max. file-size in blocks=max. word_t */
#define PARENT_DIR         0 /* filenr of parent-directory */

                             /* file-types: */
#define FILE_FREE          0 /* free=0 */
#define FILE_SUBDIR        2 /* subdirectory */
#define FILE_SINGLE_INSTR  3 /* single instrument */
#define FILE_PARENTDIR     8 /* parent directory */
#define FILE_MACRO_EPS     9 /* eps macro */
#define FILE_BANK_EPSP    23 /* eps+ bank */
#define FILE_EFFECT_EPSP  24 /* eps+ effect */
#define FILE_SEQ_EPSP     25 /* eps+ sequence */
#define FILE_SONG_EPSP    26 /* eps+ song */
#define FILE_OS_EPSP      27 /* eps+ operating-system */
#define FILE_SEQ          28 /* asr sequence */
#define FILE_SONG         29 /* asr song */
#define FILE_BANK         30 /* asr bank */
#define FILE_OS           32 /* asr operating-system */
#define FILE_EFFECT       33 /* asr effect */
#define FILE_MACRO        34 /* asr macro */

typedef struct{
  byte_t zero1;                 /* 0x00 */
  byte_t file_type;             /* file-type */
  char file_name[FILE_NAME_SIZE]; /* file-name */
  word_t file_size;             /* file-size in blocks or: nr of files if file is dir. */
  word_t contiguous;            /* contiguous blocks or: filenr in parentdir. of curr.dir. if filenr==PARENT_DIR */
  long_t file_ptr;              /* file-pointer=1st block of file */
  byte_t multi_index;           /* 0x00 if not multi */
  byte_t zero2;                 /* 0x00 */
  word_t multi_size;            /* multi-size */
}dr_entry_t;

typedef struct{
  dr_entry_t dir[DR_ENTRY_NR];  /* directory of DR_ENTRY_NR files */
  byte_t zero[8];               /* 8 * 0x00 */
  char sig[2];                  /* signature DR */
}directory_t;
/* note that sizeof(directory_t)==1024 (==2*BLK_SIZE) */



/* file-allocation-table */

/*
 * next_blk: free=0, EOF=1, bad block=2
 * else: next_blk points to next blocknr of file
 * note that for every used block there must be an fb-entry
 * even the block is contiguous, in order to show that the
 * block is not free; the information contained in this
 * fb_entry-chain (termimated by EOF) will be used in the
 * case the file is deleted.
 * the chain begins in the directory-entry:
 * file_ptr=1st block, 2nd block=next_blk(1st block)
 * if n=contiguous => next_blk(b)=b+1
 *   for (1st block)<=b<(n+1st block-1)
 */

#define FB_SIZE            1
#define FB_BLK_0           5 /* 1st file allocation block */
#define FB_ENTRY_NR      170 /* nr of entries per FB-block */
#define FB_SIG             "FB"
#define BLOCK_FREE         0 /* block is free */
#define BLOCK_EOF          1 /* last block of file */
#define BLOCK_BAD          2 /* bad block */

                             /* total nr of FB-blocks for x=total nr of blocks */
#define FB_BLK_NR(x)     ((unsigned long)(((x)-1)/FB_ENTRY_NR)+1)

                 /* each block in data-domain can be mapped on a corresponding */
                 /* uniquely defined FB-entry within one of the FB-blocks:     */
                             /* corresponding FB-blocknr for x=blocknr */
#define BLK2FB_BLK(x)    ((x)/FB_ENTRY_NR)

#define FB_ENTRY_SIZE      3 /* =sizeof(fb_entry_t) */
                             /* corresponding FB-entryaddress for x=blocknr */
#define BLK2FB_ENTRY(x)  (((x)%FB_ENTRY_NR)*FB_ENTRY_SIZE)


typedef medi_t fb_entry_t;      /* next fb_entry in chain */

typedef struct{
  fb_entry_t fat[FB_ENTRY_NR];  /* file-allocation-table */
  char sig[2];                  /* signature FB */
}fat_t;



#endif /* !_EPSFS_STRUCT_H */
