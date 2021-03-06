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
/*-------------------------------------------------*/
/*-------------------------------------------------*/
/*                                                 */
/* wav2asr: convert wav-file to asr-10-instrument  */
/* Version 0.21 (8/19/1998),(2/11/97)              */
/*                                                 */
/* (c) by Klaus Michael Indlekofer                 */
/*                                                 */
/*-------------------------------------------------*/
/*-------------------------------------------------*/



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#ifdef __ultrix
#include <sys/uio.h>
#define ssize_t int
#endif /* __ultrix */
#include <sys/stat.h>
#include <fcntl.h>

/* O_BINARY important for Windows-based systems (esp. cygwin or VC++) */
#ifndef O_BINARY
#define O_BINARY 0
#endif /* !O_BINARY */



#ifndef __GCC__
#define inline static
#endif /* __GCC__ */



/*-------------------------------------------------*/



#define WAV_EXT ".wav"
#define INS_EXT ".ins"

#ifndef TEMPLATE
#define TEMPLATE "asr10.template"
#endif
#define TEMPSIZE 0xb10

#define SAMPLE_START_ADDR TEMPSIZE



#define FILE_NAME_SIZE 12    /* filename size in Bytes */
#define BLANK_NAME     "            "
#define FILE_NAME_ADDR 0x00a
#define FILE_NAME_INC  2



/* ins-file-size */
#define FSIZE2_ADDR 0x002
#define FSIZE1_ADDR 0x003
#define FSIZE0_ADDR 0x000

#define BLK_SIZE   512
/* reserve only complete blocks!, at least 16 Bytes space left */
#define FSIZE(x)   ((size_t)(((unsigned long)(x)+BLK_SIZE+15)&0xfffffe00))

/* x=file-length in Bytes */
#define FSIZE2(x)  ((unsigned char)((x)>>16))
#define FSIZE1(x)  ((unsigned char)(((x)>>8)&0xf0))
#define FSIZE0(x)  ((unsigned char)((x)>>4))



/* sample-start/end */
#define DEFAULT_START 0

#define START2_ADDR 0xae0
#define START1_ADDR 0xae2
#define START0_ADDR 0xae4
#define STOP2_ADDR  0xae8
#define STOP1_ADDR  0xaea
#define STOP0_ADDR  0xaec

#define BYTE2(x)   ((unsigned char)((x)>>16))
#define BYTE1(x)   ((unsigned char)((x)>>8))
#define BYTE0(x)   ((unsigned char)(x))



/* sample-length */
#define SAML2_ADDR  0x9f2
#define SAML1_ADDR  0x9f3
#define SAML0_ADDR  0x9f0

/* x=sample-length in Bytes */
#define SAML2(x)  ((unsigned char)((x+0x120)>>16))
#define SAML1(x)  ((unsigned char)(((x+0x120)>>8)&0xf0))
#define SAML0(x)  ((unsigned char)((x+0x120)>>4))

#define SAMLMASK  0xfffffff0 /* sample size should be 0 mod 16 */



/* space left in file from sample-end to file-end */
/* x=file-size, y=sample-size (in Bytes) */
#define ENDSPACE(x,y)  ((unsigned char)(((x)-(y)-SAMPLE_START_ADDR)>>4))
#define END_ADDR(y)     (SAMPLE_START_ADDR+(y))

#define SAMEND1   0x10 /* magic numbers */
#define SAMEND2   0x00
#define SAMEND3   0x00
#define SAMEND4   0x08
#define SAMEND5   0x40



/*-------------------------------------------------*/



/* little endian unsigned short int (16Bit) */
typedef struct{unsigned char b0,b1;} leushort_t;
/* little endian unsigned long int (32Bit) */
typedef struct{unsigned char b0,b1,b2,b3;} leulong_t;
/* little endian short int (16Bit) */
typedef struct{signed char b0,b1;} leshort_t;

/* convert to machine dependent type */
inline unsigned short
leushort2ushort(leushort_t x)
{
  return ((unsigned short)x.b0)+(((unsigned short)x.b1)<<8);
}

inline unsigned long
leulong2ulong(leulong_t x)
{
  return ((unsigned long)x.b0)+(((unsigned long)x.b1)<<8)
    +(((unsigned long)x.b2)<<16)+(((unsigned long)(x).b3)<<24);
}



/* RIFF WAVE format header */
struct RIFFHeader{
  char riff_str[4];      /* must be "RIFF" */
  char dummy1[4];
  char wave_str[8];      /* must be "WAVEfmt " */
  char dummy2[6];
  leushort_t chnr;       /* number of channels */
  leulong_t smplrate;    /* sample rate in Hz */
  char dummy3[6];
  leushort_t bitnr;      /* number of Bits per value */
  char data_str[4];      /* must be "data" */
  leulong_t smplsize;    /* total sample size in Bytes */
};
#define RIFFHEADERSIZE sizeof(struct RIFFHeader)
#define RIFFSTR "RIFF"
#define RIFFSTRLEN 4
#define WAVESTR "WAVEfmt "
#define WAVESTRLEN 8
#define DATASTR "data"
#define DATASTRLEN 4

/* sample data */
typedef struct{signed char b0;} sample8_t;
typedef leshort_t sample16_t;



/*-------------------------------------------------*/



static int convwav(char *wavname,char *insname,char *inshdr);
static int readwav(int wavfd,char *sbuf,unsigned long smplsize,
		   unsigned short chnr,unsigned short bitnr);
static void insertfilename(char *inshdr,char *name);
static void convert_name(char *txt);



/*-------------------------------------------------*/



int
main(int argc,char **argv)
{
  int i;
  char *wavname;
  char insname[256];
  int insfd;
  char *inshdr;
  char *b;

  if (argc<2){
    fprintf(stderr,"usage: %s <wav-file(s)>\n",*argv);
    exit(-1);
  }

  fprintf(stderr,"\n--- wav2asr Version 0.21, (c) by K.M.Indlekofer ---\n\n");

  /* read ins header */
  if ((insfd=open(TEMPLATE,O_RDONLY|O_BINARY))==-1){
    fprintf(stderr,"error: can't open file `%s'.\n",TEMPLATE);
    exit(-1);
  }
  if ((inshdr=malloc(TEMPSIZE))==NULL){
    fprintf(stderr,"error: can't allocate memory.\n");
    exit(-1);
  }
  if (read(insfd,(void *)inshdr,TEMPSIZE)<TEMPSIZE){
    fprintf(stderr,"error: can't read data from file `%s'.\n",TEMPLATE);
    exit(-1);
  }
  close(insfd);

  for (i=1;i<argc;i++){
    wavname=argv[i];
    strcpy(insname,wavname);
    if (b=strstr(insname,WAV_EXT)) /* remove wav-ext if there is one */
      *b='\0';
    fprintf(stderr,"convert: wav-file=`%s'\n",wavname);
    fprintf(stderr,"         to ins-file=`%s'\n",insname);
    convwav(wavname,insname,inshdr); /* convert wav to ins */
  }

  exit(0);
}



/*-------------------------------------------------*/



static int
convwav(char *wavname,char *insname,char *inshdr)
{
  int wavfd,insfd;
  struct RIFFHeader hdr;
  unsigned short chnr,bitnr;
  unsigned long smplsize,inssize,ea;
  char *buf;

  /* open wav file */
  if ((wavfd=open(wavname,O_RDONLY|O_BINARY))==-1){
    fprintf(stderr,"error: can't open file `%s'.\n",wavname);
    return -1;
  }

  /* read header */
  if (read(wavfd,(char *)&hdr,RIFFHEADERSIZE)!=RIFFHEADERSIZE){
    fprintf(stderr,"error: can't read RIFF header.\n");
    close(wavfd);
    return -2;
  }

  /* check header strings */
  if ((strncmp(hdr.riff_str,RIFFSTR,RIFFSTRLEN)!=0)
      ||(strncmp(hdr.wave_str,WAVESTR,WAVESTRLEN)!=0)
      ||(strncmp(hdr.data_str,DATASTR,DATASTRLEN)!=0)){
    fprintf(stderr,"error: not a RIFF WAVE file.\n");
    close(wavfd);
    return -3;
  }

  /* sample rate */
  fprintf(stderr,"         sample rate=%lu Hz\n",leulong2ulong(hdr.smplrate));

  /* check number of channels */
  chnr=leushort2ushort(hdr.chnr);
  fprintf(stderr,"         channel nr.=%u\n",chnr);
  switch (chnr){
  case 1:
  case 2:
    break;
  default:
    fprintf(stderr,"error: unsupported channel number.\n");
    close(wavfd);
    return -4;
  }

  /* sample size */
  smplsize=leulong2ulong(hdr.smplsize);
  fprintf(stderr,"         sample size=%lu Bytes\n",smplsize);

  /* check number of Bits */
  bitnr=leushort2ushort(hdr.bitnr);
  fprintf(stderr,"         Bit nr.=%u\n",bitnr);
  switch (bitnr){
  case 8:
    smplsize*=2; /* expand to 16Bit later */
    break;
  case 16:
    break;
  default:
    fprintf(stderr,"error: unsupported Bit number.\n");
    close(wavfd);
    return -5;
  }

  smplsize&=SAMLMASK;                /* sample size should be 0 mod 16 */
  inssize=FSIZE(smplsize+TEMPSIZE);  /* size of ins-file in Bytes */

  /* read sample to buffer */
  if ((buf=malloc(inssize))==NULL){
    fprintf(stderr,"error: can't allocate memory.\n");
    close(wavfd);
    return -6;
  }
  if (readwav(wavfd,buf+SAMPLE_START_ADDR,smplsize,chnr,bitnr)<0){
    fprintf(stderr,"error: can't read sample data.\n");
    free((void *)buf);
    close(wavfd);
    return -7;
  }

  close(wavfd);

  /* open ins file */
  if ((insfd=open(insname,O_WRONLY|O_BINARY|O_TRUNC|O_CREAT,0600))==-1){ /* rw------- */
    fprintf(stderr,"error: can't open file `%s'.\n",insname);
    free((void *)buf);
    return -8;
  }

  /* ins header and tail */
  bcopy(inshdr,buf,TEMPSIZE);
  insertfilename(buf,insname);           /* insert file-name */
  buf[FSIZE2_ADDR]=FSIZE2(inssize);      /* file-length */
  buf[FSIZE1_ADDR]=FSIZE1(inssize);
  buf[FSIZE0_ADDR]=FSIZE0(inssize);
  buf[SAML2_ADDR]=SAML2(smplsize);       /* sample-length */
  buf[SAML1_ADDR]=SAML1(smplsize);
  buf[SAML0_ADDR]=SAML0(smplsize);
  buf[START2_ADDR]=BYTE2(DEFAULT_START); /* start sample */
  buf[START1_ADDR]=BYTE1(DEFAULT_START);
  buf[START0_ADDR]=BYTE0(DEFAULT_START);
  buf[STOP2_ADDR]=BYTE2(smplsize);       /* end sample */
  buf[STOP1_ADDR]=BYTE1(smplsize);
  buf[STOP0_ADDR]=BYTE0(smplsize);
  ea=END_ADDR(smplsize);
  buf[ea]=ENDSPACE(inssize,smplsize);    /* end-space in file */
  buf[ea+1]=SAMEND1;
  buf[ea+2]=SAMEND2;
  buf[ea+3]=SAMEND3;
  buf[ea+4]=SAMEND4;
  buf[ea+5]=SAMEND5;

  /* write to ins-file */
  if (write(insfd,(char *)buf,inssize)!=(ssize_t)inssize){
    fprintf(stderr,"error: not all data saved.\n");
    close(insfd);
    free((void *)buf);
    return -9;
  }

  close(insfd);

  free((void *)buf);
  return 0;
}



static int
readwav(int wavfd,char *sbuf,unsigned long smplsize,
	unsigned short chnr,unsigned short bitnr)
{
  unsigned long i;
  sample8_t s8;
  sample16_t s16;

  if ((chnr==1)&&(bitnr==8)){               /* 1channel 8Bit */
    for (i=0;i<smplsize;i++){
      if (read(wavfd,(char *)&s8,1)!=1){
	fprintf(stderr,"error: can't read file.\n");
	return -1;
      }
      /* save as big endian */
      sbuf[i]=(s8.b0)^0x80;
      i++;
      sbuf[i]=0;
    }
  }else if ((chnr==2)&&(bitnr==8)){         /* 2channel 8Bit */
    smplsize/=2;
    for (i=0;i<smplsize;i++){
      /* channel 1 */
      if (read(wavfd,(char *)&s8,1)!=1){
	fprintf(stderr,"error: can't read file.\n");
	return -1;
      }
      /* save as big endian */
      sbuf[i]=(s8.b0)^0x80;
      sbuf[i+1]=0;
      /* channel 2 */
      if (read(wavfd,(char *)&s8,1)!=1){
	fprintf(stderr,"error: can't read file.\n");
	return -1;
      }
      /* save as big endian */
      sbuf[i+smplsize]=(s8.b0)^0x80;
      i++;
      sbuf[i+smplsize]=0;
    }
  }else if ((chnr==1)&&(bitnr==16)){        /* 1channel 16Bit */
    for (i=0;i<smplsize;i++){
      if (read(wavfd,(char *)&s16,2)!=2){
	fprintf(stderr,"error: can't read file.\n");
	return -1;
      }
      /* save as big endian */
      sbuf[i]=s16.b1;
      i++;
      sbuf[i]=s16.b0;
    }
  }else if ((chnr==2)&&(bitnr==16)){        /* 2channel 16Bit */
    smplsize/=2;
    for (i=0;i<smplsize;i++){
      /* channel 1 */
      if (read(wavfd,(char *)&s16,2)!=2){
	fprintf(stderr,"error: can't read file.\n");
	return -1;
      }
      /* save as big endian */
      sbuf[i]=s16.b1;
      sbuf[i+1]=s16.b0;
      /* channel 2 */
      if (read(wavfd,(char *)&s16,2)!=2){
	fprintf(stderr,"error: can't read file.\n");
	return -1;
      }
      /* save as big endian */
      sbuf[i+smplsize]=s16.b1;
      i++;
      sbuf[i+smplsize]=s16.b0;
    }
  }

  return 0;
}




static void
insertfilename(char *inshdr,char *name){
  static char newn[FILE_NAME_SIZE+1];
  int i;

  bcopy(name,newn,FILE_NAME_SIZE);
  convert_name(newn);

  for (i=0;i<FILE_NAME_SIZE;i++)
    inshdr[(i<<1)+FILE_NAME_ADDR]=newn[i];
}



static void
convert_name(char *txt){
  int i;

  txt[FILE_NAME_SIZE]='\0'; /* terminate txt */
  for (i=0;i<FILE_NAME_SIZE;i++)
    txt[i]=(char)toupper((int)txt[i]); /* upper case letters */
  /* fill up with BLANK */
  bcopy(BLANK_NAME,txt+strlen(txt),FILE_NAME_SIZE-strlen(txt));
}



/* EOF */
