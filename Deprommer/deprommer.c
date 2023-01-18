/*
 ============================================================================
 Name        : deprommer.c
 Author      : Nicola Avanzi
 Version     : 0.1b
 Copyright   : Free for all
 License     : Creative Commons Attribution 4.0 International License
 Description : Dela Eprommer control software
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>

#include <exec/exec.h>
#include <dos/dos.h>

const char __ver[33] = "$VER: DePrommer 0.1b (10.11.2021)";

extern const STRPTR TEMPLATE = "WHAT=WhatToDo/A/K,D=DestFile/K,T=TypeOfRom/N";

typedef struct Arguments {
  STRPTR WhatToDo;
  STRPTR DestFile;
  LONG* TypeOfRom;
} Arguments;

/* EPROMMER control address
 * /LE latch write address $700002
 * /LE latch write data and some other settings $700004
 * /CE transceiver read data $70008
 *
 * WORD DATI
 *
 * latch write address
 * | 15  | 14  | 13  | 12  | 11  | 10  | 09 | 08 | 07 | 06 | 05 | 04 | 03 | 02 | 01 | 00 |
 * | A15 | A14 | A13 | A12 | A11 | A10 | A9 | A8 | A7 | A6 | A5 | A4 | A3 | A2 | A1 | A0 |
 *
 * latch write data
 * *OE bit 14 enable latch data
 * POW change voltage in VPP pin 12.5V or 21V
 * | 15  | 14  | 13  | 12  | 11  | 10  |  09  |  08 | 07 | 06 | 05 | 04 | 03 | 02 | 01 | 00 |
 * | NC  | OE* | VCC | POW | CE  | OE  | GVPP | VPP | D7 | D6 | D5 | D4 | D3 | D2 | D1 | D0 |
 *
 * transceiver read data
 * | D7 | D6 | D5 | D4 | D3 | D2 | D1 | D0 |
 *
 */

// data read from EPROM and address location

//int unsigned short address asm("address");
unsigned short address asm("address");
unsigned char wdatabyte asm("wdatabyte");

// set CIA register for timing
unsigned int ciaatalo asm("ciaatalo") = 0xbfe401;
unsigned int ciaatahi asm("ciaatahi") = 0xbfe501;
unsigned int ciaaicr  asm("ciaaicr") = 0xbfed01;
unsigned int ciaacra  asm("ciaacra") = 0xbfee01;


// this sub write 1 byte in EPROM 27c256
#define write_byte(int) \
({ \
register int count;\
\
__asm__ __volatile__( \
"   clr.l d5 \n" /* clear registry d5, is a counter of write */ \
"start: \n" \
"   clr.l d4 \n" /* clear registry d4, is for store byte read */ \
"   clr.l d6 \n" /* clear registry d6, is for store byte to write */ \
"   move.w  (address),0x700002 \n" /* write address on EPROM (A0 to A15) */ \
"   move.w  #0b0000010100000000,d6 \n" /* set VPP high, OE high, CE low, U5 OE low enable data in latch, don't touch first 8 bit of data ! */  \
"	move.b (wdatabyte),d6 \n" /* move wdatabyte in d6 before writing */ \
"	move.w d6,0x700004 \n" /* write wdatabyte in EPROM now CE il low */\
"	jsr	delay25 \n"  /* wait 100us program impulse  */\
"	jsr	delay25 \n" \
"	jsr	delay25 \n" \
"	jsr	delay25 \n" \
"   move.w  #0b0100110100000000,0x700004 \n" /*disable U5 data in latch, OE and CE High */ \
"	add #1,d5 \n" /* increment write counter */\
"	cmp.b #24,d5 \n" /* compare count with max 25 write retry */\
"	beq.s exit \n" /* if equal jump on exit and return count */\
"   move.w  #0b0100100100000000,0x700004 \n" /* OE low */ \
"	jsr	delay25 \n" \
"	move.w (0x700008),d4 \n" /* data out in d4 */ \
"	cmp.b d4,d6 \n" /* compare byte written if match whit byte read */\
"	bne start \n" /* if not match jump at start and repeat*/ \
"exit: \n" \
"   move.w  #0b0100110100000000,0x700004 \n" /* set EPROM CE and OE high  */ \
"	move.w	d5,%0\n": /* return write count; if < 24 write is success */ \
"=r"(count)::"cc" \
 );\
\
count;\
})


// this sub write 1 byte in EPROM 27c128 and 2764
#define write_byte64(int) \
({ \
register int count;\
\
__asm__ __volatile__( \
"   clr.l d5 \n" /* clear registry d5, is a counter of write */ \
"start64: \n" \
"   clr.l d4 \n" /* clear registry d4, is for store byte read */ \
"   clr.l d6 \n" /* clear registry d6, is for store byte to write */ \
"   clr.l d7 \n" /* clear registry d7, is for store address */ \
"	move.w	(address),d7 \n" \
"	eor.w #0b0100000000000000,d7 \n" /* set PGM(14) High */ \
"   move.w  d7,0x700002 \n" /* write address on EPROM (A0 to A15) */ \
"   move.w  #0b0000010100000000,d6 \n" /* VPP to High, OE high, CE low, U5 OE low enable data in latch, don't touch first 8 bit of data ! */  \
"	move.b (wdatabyte),d6 \n" /* move wdatabyte in d6 before writing */ \
"	move.w d6,0x700004 \n" /* write wdatabyte in EPROM now CE il low */\
"	eor.w #0b0100000000000000,d7 \n" /* set PGM(14) Low in reg d7 */ \
"   move.w  d7,0x700002 \n" /* write address on EPROM (A0 to A15) */ \
"	jsr	delay25 \n"  /* wait 100us program impulse  */\
"	jsr	delay25 \n" \
"	jsr	delay25 \n" \
"	jsr	delay25 \n" \
"	eor.w #0b0100000000000000,d7 \n" /* set PGM(14) Hig in reg d7 */ \
"   move.w  d7,0x700002 \n" /* write address on EPROM (A0 to A15) */ \
"   move.w  #0b0100010100000000,0x700004 \n" /*disable U5 data in latch, OE Hig CE low */ \
"	add #1,d5 \n" /* increment write counter */\
"	cmp.b #24,d5 \n" /* compare count with max 25 write retry */\
"	beq.s exit64 \n" /* if equal jump on exit and return count */\
"   move.w  #0b0100000100000000,0x700004 \n" /* OE low */ \
"	jsr	delay25 \n" \
"	move.w (0x700008),d4 \n" /* data out in d4 */ \
"	cmp.b d4,d6 \n" /* compare byte written if match whit byte read */\
"	bne start64 \n" /* if not match jump at start and repeat*/ \
"exit64: \n" \
"   move.w  #0b0100110100000000,0x700004 \n" /* set EPROM CE and OE high  */ \
"	move.w	d5,%0\n": /* return write count; if < 24 write is success */ \
"=r"(count)::"cc" \
 );\
\
count;\
})

// this sub write 1 byte in EPROM 27c512
#define write_byte512(int) \
({ \
register int count;\
\
__asm__ __volatile__( \
"   clr.l d5 \n" /* clear registry d5, is a counter of write */ \
"start512: \n" \
"   clr.l d4 \n" /* clear registry d4, is for store byte read */ \
"   clr.l d6 \n" /* clear registry d6, is for store byte to write */ \
"   move.w  (address),0x700002 \n" /* write address on EPROM (A0 to A15) */ \
"   move.w  #0b0000101000000000,d6 \n" /* set GVPP high, CE high, U5 OE low enable data in latch */  \
"	move.b (wdatabyte),d6 \n" /* move wdatabyte in d6 before writing */ \
"	move.w d6,0x700004 \n" /* write wdatabyte in EPROM CE high */\
"	eor.w #0b0000100000000000,d6 \n" /* set CE low */  \
"	move.w d6,0x700004 \n" /* write wdatabyte in EPROM CE low */\
"	jsr	delay25 \n"  /* wait 100us program impulse  */\
"	jsr	delay25 \n" \
"	jsr	delay25 \n" \
"	jsr	delay25 \n" \
"   move.w  #0b0100100000000000,0x700004 \n" /* GVPP low, disable U5 data in latch, CE High */ \
"	add #1,d5 \n" /* increment write counter */\
"	cmp.b #24,d5 \n" /* compare count with max 25 write retry */\
"	beq.s exit \n" /* if equal jump on exit and return count */\
"   move.w  #0b0100000000000000,0x700004 \n" /* CE low */ \
"	jsr	delay25 \n" \
"	move.w (0x700008),d4 \n" /* data out in d4 */ \
"	cmp.b d4,d6 \n" /* compare byte written if match whit byte read */\
"	bne start512 \n" /* if not match jump at start and repeat*/ \
"exit512: \n" \
"   move.w  #0b0100100000000000,0x700004 \n" /* set EPROM CE high  */ \
"	move.w	d5,%0\n": /* return write count; if < 24 write is success */ \
"=r"(count)::"cc" \
 );\
\
count;\
})

/* delay 25 milliseconds */
int delay25(void) {
  __asm__ __volatile__(  \
"delay25: \n" \
"	move.b  ciaacra,d0 \n"   \
"   and.b   #0b11000000,d0 \n" \
"	or.b    #0b00001000,d0 \n"  \
"	move.b  d0,ciaacra \n " \
"	move.b  #0b01111111,ciaaicr \n"  \
"	move.b  #(7092&255),ciaatalo \n"  \
"	move.b  #(7092>>8),ciaatahi \n"  \
"loop10:\n" \
"	btst.b  #0,ciaaicr \n" \
"   beq.s   loop10 \n" \
"rts \n" \
 \
  );
  return 1;
}

/* set to LOW all pin and cutoff VCC */
int switch_off(void) {
  __asm__ __volatile__(
"   move.w  #0b0110000000000000,0x700004 \n" /* set VCC pin low and U5 OE FOREVER HIGH */ \
"   move.w  #0b0000000000000000,0x700002 \n" /* set all address pin to low */ \
  );
  return 1;

}

// this sub read 1 byte from EPROM 27c256 and 27c512
#define read_byte(char) \
({ \
register int databyte;\
\
__asm__ __volatile__( \
"   move.w  #0b0100010000000000,0x700004 \n" /* set EPROM CE low and U5 OE FOREVER HIGH when READ ! */ \
"	jsr	delay25 \n" \
"   move.w  #0b0100000000000000,0x700004 \n" /* set EPROM OE low and U5 OE FOREVER HIGH when READ ! */  \
"	jsr	delay25 \n" \
"   move.w  (address),0x700002 \n" /* write address on EPROM (A0 to A15) */ \
"	jsr	delay25 \n" \
"	jsr	delay25 \n" \
"	jsr	delay25 \n" \
"   clr.l d5 \n" /* clear registry d5 */ \
"   move.w  (0x700008),d5 \n" /* read data byte from EPROM (D0 to D7) */ \
"	move.w  #0b0000000000000000,0x700002 \n" /* set all address pin to Low */ \
"   move.w  #0b0100110000000000,0x700004 \n" /* set EPROM CE and OE high and U5 OE FOREVER HIGH when READ ! */ \
"	move.b	d5,%0\n": /* return d5 registry value */ \
"=r"(databyte)::"cc" /* the byte read */ \
 );\
\
databyte;\
})


// this sub read 1 byte from EPROM 2764 or 27C128
#define read_byte64(char) \
({ \
register int databyte;\
\
__asm__ __volatile__( \
"	clr.l d5 \n" /* clear registry d5 */ \
"   move.w  #0b0100010000000000,0x700004 \n" /* set EPROM CE low and U5 OE FOREVER HIGH when READ ! */ \
"	jsr	delay25 \n" \
"   move.w  #0b0100000000000000,0x700004 \n" /* set EPROM OE low and U5 OE FOREVER HIGH when READ ! */ \
"	jsr	delay25 \n" \
"	move.w	(address),d5 \n" \
"	or.w #0b1100000000000000,d5 \n" /* set VPP(A15) and PGM(14) High */ \
"   move.w  d5,0x700002 \n" /* write address on EPROM (A0 to A15) */ \
"	jsr	delay25 \n" \
"	jsr	delay25 \n" \
"	jsr	delay25 \n" \
"   clr.l d5 \n" /* clear registry d5 */ \
"   move.w  (0x700008),d5 \n" /* read data byte from EPROM (D0 to D7) */ \
"	move.w  #0b0000000000000000,0x700002 \n" /* set all address pin to Low */ \
"   move.w  #0b0100110000000000,0x700004 \n" /* set EPROM CE and OE high and U5 OE FOREVER HIGH when READ ! */ \
"	move.b	d5,%0\n": /* return d5 registry value */ \
"=r"(databyte)::"cc" /* the byte read */ \
 );\
\
databyte;\
})


/* malloc */
void *malloc(size_t size);


/* this function read all databyte in eprom and return char array pointer
 * of equivalent size of eprom
 */
char *do_read_buffer(int unsigned long eprom_size) {

	/* declare char buffer and allocate size */
	char *read_buffer;
	read_buffer = malloc(eprom_size*sizeof(char));
	if (read_buffer == NULL)
		return(TRUE);

	/* reset counter and address */
	address=0;

	for (eprom_size; eprom_size > 0; eprom_size--) {
			if (eprom_size <= 16383) { /* if eprom is a 2764 and 27128 use read_byte64*/
				read_buffer[address] = read_byte64();
			} else {
				read_buffer[address] = read_byte(); /* for all other eprom use read_byte*/
			}
			address++;
	}

	return read_buffer;

}

/* function to compare two char array buffer, one byte at once */

int compare_buffer(char fpbuffer[], int fpsize, char rbuffer[], int rsize) {

	int n = 0;
	int isdiff = 0;

	/* compare if file buffer is a size correct. */
	if ((fpsize ) != (rsize)){
		printf("wrong file size\n");
		return(1);
	}
		/* if size is wrong exit and return a error without comparing buffers*/
	/* only now compare the buffers*/
	do {
		if(rbuffer[n] != fpbuffer[n]) {
			isdiff = 1;
			exit;
		}
		n++;
	} while (n < fpsize);

	if (isdiff == 1) {
		printf("compare check FAIL\n");
		return(1);
	} else {
		printf("compare check PASS\n");
		return(0);
	}

}

/* function read n data byte from EPROM and write file */
int read_rom(char destfile[], int unsigned long eprom_size) {

	// declare char buffer and allocate size
	char *read_buffer;
	read_buffer = do_read_buffer(eprom_size); /* read eprom content in rom_buffer */

/* write file from EPROM read buffer  */
	FILE *fp;
	if (( fp = fopen(destfile, "wb")) == NULL) {
		printf("unable to create file");
		return(TRUE);
	} else {
		fwrite(read_buffer, eprom_size, 1, fp);
		fclose(fp);
		free(read_buffer);
		printf("read DONE!\n");
		return(TRUE);
	}

}


/* function read n data byte from EPROM and check BLANK (FF) */
int blank_rom(int unsigned long eprom_size) {

	char *rom_buffer;
	rom_buffer = do_read_buffer(eprom_size); /* read eprom content in rom_buffer */
	int notbl = 0;
	/* check if eprom is BLANK, all FF */
	printf("check in progres...\n");
	for (eprom_size; eprom_size > 0; eprom_size--) {
		if(rom_buffer[eprom_size-1] != 0xffffffff) {
			printf("blank check FAIL\n");
			return(0);
		}
	}

	printf("blank check PASS\n");
	return(0);

}

/*
 * Compare content of Eprom with file on disk
 *
 * */

int compare_rom(char destfile[], int unsigned long eprom_size) {

/* read file on disk and save in file_buffer  */

/* the two buffer to compare */
	char *file_buffer;
	char *rom_buffer;
	rom_buffer = do_read_buffer(eprom_size); /* read eprom content in rom_buffer */

/* open file and fill buffer */
	FILE *fp;
	if ((fp=fopen(destfile, "rb")) == NULL) {
		printf("file not found!\n");
		return(TRUE);
	} else {
		fseek(fp, 0, SEEK_END);
		long file_size = ftell(fp); /* check file size */
		rewind(fp);

		file_buffer = malloc(file_size*sizeof(char));
		if (file_buffer == NULL)
			return(TRUE);
/* fill file buffer with content of file*/
		unsigned int read = fread(file_buffer, sizeof(char), file_size, fp);

/* now compare the two buffer byte per byte
 * if found difference or file is the wrong size return 1
 * */
		printf("verifying in progress...\n");
		compare_buffer(file_buffer, file_size, rom_buffer, eprom_size);

/* clean all buffer and close file */
		free(rom_buffer);
		free(file_buffer);
		fclose(fp);
		return(TRUE);
	}
}

/*
 *  WRITE EPROM
 */

int write_rom(char destfile[], int unsigned short eprom_size) {

	/* file and eprom read data buffer */
	char *file_buffer;
	char *rom_buffer;

	/* open file to write in eprom */
		FILE *fp;
		if ((fp=fopen(destfile, "rb")) == NULL) {
			printf("file not found!\n");
			return(TRUE);
		} else {
			fseek(fp, 0, SEEK_END);
			long file_size = ftell(fp); /* check file size */
			rewind(fp);
			file_buffer = malloc(file_size*sizeof(char));
			if (file_buffer == NULL)
				return(TRUE);
	/* fill file buffer with content of file*/
			unsigned int read = fread(file_buffer, sizeof(char), file_size, fp);
		}
	int n = 0;
	int wcount = 0;
	address = 0;
	printf("programming in progress... please wait\n");
	do {

			if (eprom_size <= 16383) { /* if eprom is a 2764 and 27128 use write_byte64*/
				wdatabyte = file_buffer[n]; /* read 1 byte from buffer and store in wdatabyte for writing*/
				wcount=write_byte64(); /* write databyte in eprom and return write count */
			} else if (eprom_size ==  32768) { /* if eprom is 27c256 use write_byte*/
				wdatabyte = file_buffer[n]; /* read 1 byte from buffer and store in wdatabyte for writing*/
				wcount=write_byte(); /* write databyte in eprom and return write count */
			} else { /* if eprom is 27c512 use write_byte512*/
				wdatabyte = file_buffer[n]; /* read 1 byte from buffer and store in wdatabyte for writing*/
				wcount=write_byte512(); /* write databyte in eprom and return write count */
			}

			if (wcount >= 24 ) { /* if wcount is over the retry limit exit and return a error  */
				printf("write error in address 0x%02X\n", address);
				return(TRUE);

			} else { /* write success */
				wcount = 0;
				address++;
				n++;
			}

		} while (n < eprom_size);

	/*
	now if you are here, all writings in eprom are successful
	is time of compare all eprom content whit source file
	*/
	rom_buffer = do_read_buffer(eprom_size); /* read eprom content in rom_buffer */

	/* now compare the two buffer byte per byte
	 * if found difference or file is the wrong size return 1
	 * */
	if ((compare_buffer(file_buffer, ftell(fp), rom_buffer, eprom_size)) == 0 ) {
		printf("EPROM programming successful\n");
	} else {
		printf("EPROM programming fail\n");
	}
	/* clean all buffer and close file */
	free(rom_buffer);
	free(file_buffer);
	fclose(fp);
	return(TRUE);
}


int main(int argc, char **argv) {

	/* Declare our variables */
	struct RDArgs* rda = NULL;
	Arguments* args = NULL;
	APTR pool = NULL;

	/* Create our memory pool with 4K pools and 1K puddles */
	pool = CreatePool(MEMF_CLEAR | MEMF_PUBLIC, 4096, 1024);
	if (!pool) { /* Handle inability to allocate memory */ }

	/* Allocate memory for our arguments */
	args = AllocPooled(pool, sizeof(Arguments));
	if (!args) { /* Handle inability to allocate memory */ }

	/* Allocate a RDArgs object using dos.library; if you are not
	   using something like SAS/C which opens libraries for you,
	   you will need to make your calls to OpenLibrary() as well */
	if ((rda = (struct RDArgs *)AllocDosObject(DOS_RDARGS, 0))) {

		/* Here we pass our TEMPLATE, and we cast our args object to
	    an array of LONG values. Exactly what we need. */
		if (!ReadArgs(TEMPLATE, (LONG*)args, rda)) {
			/* Here we handle a failure. Failures mean the wrong args
	    	have been passed or none at all. */
			DeletePool(pool);
			Printf("*******************************************************\n");
			Printf("* Dela Amiga 500 Eprommer Software by Nicola Avanzi   *\n");
			Printf("* Eprom programmer for 2764 27C128 27C256 and 27C512  *\n");
			Printf("* http://amiga.resource.cx/exp/delaeprommer           *\n");
			Printf("\n");
			Printf("Option WHAT=\n");
			Printf("	READ -> to read an EPROM.\n");
			Printf("	WRITE -> to write an EPROM.\n");
			Printf("	WRITEH -> to write an EPROM. with 21.5V VPP\n");
			Printf("	COMPARE -> to check an EPROM.\n");
			Printf("	BLANK -> to check an empty EPROM.\n");
			Printf("\n");
			Printf("Option D=\n");
			Printf("	file name to read or write\n");
			Printf("\n");
			Printf("Option T=\n");
			Printf("	type of eprom\n");
			Printf("	64 for 2764\n");
			Printf("	128 for 27C128\n");
			Printf("	256 for 27C256\n");
			Printf("	512 for 27C512\n");
			return 5;
	    }



		/* parameter filename
		if (args->WhatToDo != NULL){

			Printf("Destination File: %s\n", args->DestFile);

		} */
		unsigned long eprom_size;
		//check type of rom
		//printf("type of rom is: %ld\n", *args->TypeOfRom);
			switch (*args->TypeOfRom) {
				case 64:
					eprom_size = 8192;
					break;
				case 128:
					eprom_size = 16384;
					break;
				case 256:
					eprom_size = 32768;
					break;
				case 512:
				    eprom_size = 65536;
					break;
				default:
					Printf("choose 64, 128, 256 or 512 \n");
					return EXIT_SUCCESS;
					break;
			}

		    /* Check WhatToDo parameter   */
		    if (strcmp(args->WhatToDo,"READ") == 0) {
		    	if ((args->DestFile != NULL) && (*args->TypeOfRom != NULL))
			    	{
		    			read_rom(args->DestFile,eprom_size);
			    		switch_off();
			    	} else {
			    			printf("Please specify destination file and type of rom\n");
			    	}
		    } else if (strcmp(args->WhatToDo,"WRITE") == 0) {
		    	if ((args->DestFile != NULL) && (*args->TypeOfRom != NULL))
			    	{
		    			write_rom(args->DestFile,eprom_size);
			    		switch_off();
			    	} else {
			    			printf("Please specify source file and type of rom\n");
			    	}
		    } else if (strcmp(args->WhatToDo,"WRITEH") == 0) {
		    	if ((args->DestFile != NULL) && (*args->TypeOfRom != NULL))
		    	   	{
		    			printf("non implemented, coming soon\n");
		    	   		switch_off();
		    	   	} else {
		    	   			printf("Please specify source file and type of rom\n");
		    	   	}
		    } else if (strcmp(args->WhatToDo,"COMPARE") == 0) {
		    	if ((args->DestFile != NULL) && (*args->TypeOfRom != NULL))
			    	{
		    			compare_rom(args->DestFile,eprom_size);
			    		switch_off();
			    	} else {
			    			printf("Please specify source file and type of rom\n");
			    	}
		    } else if (strcmp(args->WhatToDo,"BLANK") == 0) {
		    	if (*args->TypeOfRom != NULL)
		    		{
		    			blank_rom(eprom_size);
		    			switch_off();
		    		} else {
		    				printf("Please specify type of rom\n");
		    		}
    		} else {
    			Printf("you can choose one of this option: READ, WRITE, WRITEH, COMPARE, BLANK \n");
    		}

	    /* Cleanup as we are done */
	    FreeArgs(rda);
	    FreeDosObject(DOS_RDARGS, rda);

	  }

	return EXIT_SUCCESS;
}
