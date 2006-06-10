/*
  Build a 64net/2 kernel wedge from code fragments and a kernel space
  map.

  Relocates code fragments into the free spaces specified in the kernel
  map and generates a wedge loader to suit

*/

#include "../config.h"
#include <stdio.h>
#include <strings.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>

struct label {
  char *name;

  /* Has this label been resolved? */
  int resolvedP;

  /* value or instruction type? */
  int valueP;

  /* For label = value type */
  int value;

  /* Whether the label has been defined or not */
  int definedP;

  /* Whether must be zero page */
  int zpP;

  /* For label instruction type */
  struct assembled_byte *section;
  struct assembled_byte *first_byte;

  struct label *next;
};

struct assembled_byte {
  /* Compiled byte or label */
  int valueP;

  /* Value is relative to location of byte */
  int relativeP;

  /* If the byte is derived from a label,
     we need to know a few things */
  int label_offset;
  int bytes; /* 1=low, 2=high, 3=both */

  /* If this is a byte immediately after a label
     then which label? */
  struct label *label;
  
  int value;

  struct assembled_byte *next;
};

#define IM_IMM 1
#define IM_ACC 2
#define IM_IMP 3
#define IM_ZP 4
#define IM_ZPX 5
#define IM_ZPY 6
#define IM_ABS 7
#define IM_ABSX 8
#define IM_ABSY 10
#define IM_IND 11
#define IM_INDX 12
#define IM_INDY 13
#define IM_REL 14

struct instruction {
  char *name;
  int mode;
  int byte;
};

unsigned int romcopycode[]={
0xa9,0xa0,
0x85,0xfe,
0xa0,0x00,
0x84,0xfd,
0xb1,0xfd,
0x91,0xfd,
0xc8,
0xd0,0xf9,
0xe6,0xfe,
0xa5,0xfe,
0xf0,0x0a,
0xc9,0xc0,
0xd0,0xef,
0xa9,0xe0,
0x85,0xfe,
0xd0,0xe9,
0xa9,0x75,
0x85,0x01,
0x1000};

struct instruction instruction_set[]=
{
  {"brk",IM_IMP,0x00},  {"ora",IM_INDX,0x01},  {"ora",IM_ZP,0x05},
  {"asl",IM_ZP,0x06},  {"php",IM_IMP,0x08},  {"ora",IM_IMM,0x09},
  {"asl",IM_ACC,0x0a}, {"asl",IM_IMP,0x0a},
  {"ora",IM_ABS,0x0d},  {"asl",IM_ABS,0x0e},
  {"bpl",IM_REL,0x10},  {"ora",IM_INDY,0x11},  {"ora",IM_ZPX,0x15},
  {"asl",IM_ZPX,0x16},  {"clc",IM_IMP,0x18},  {"ora",IM_ABSY,0x19},
  {"ora",IM_ABSX,0x1d},  {"asl",IM_ABSX,0x1e},  {"jsr",IM_ABS,0x20},
  {"and",IM_INDX,0x21},  {"bit",IM_ZP,0x24},  {"and",IM_ZP,0x25},
  {"rol",IM_ZP,0x26},  {"plp",IM_IMP,0x28},  {"and",IM_IMM,0x29},
  {"rol",IM_ACC,0x2a},  {"bit",IM_ABS,0x2c},  {"and",IM_ABS,0x2d},
  {"rol",IM_ABS,0x2e},  {"bmi",IM_REL,0x30},  {"and",IM_INDY,0x31},
  {"and",IM_ZPX,0x35},  {"rol",IM_ZPX,0x36},  {"sec",IM_IMP,0x38},
  {"and",IM_ABSY,0x39},  {"and",IM_ABSX,0x3d},  {"rol",IM_ABSX,0x3e},
  {"rti",IM_IMP,0x40},  {"eor",IM_INDX,0x41},  {"eor",IM_ZP,0x45},
  {"lsr",IM_ZP,0x46},  {"pha",IM_IMP,0x48},  {"eor",IM_IMM,0x49},
  {"lsr",IM_ACC,0x4a},  {"jmp",IM_ABS,0x4c},  {"eor",IM_ABS,0x4d},
  {"lsr",IM_ABS,0x4e},  {"bvc",IM_REL ,0x50},  {"eor",IM_INDY,0x51},
  {"eor",IM_ZPX,0x55},  {"lsr",IM_ZPX,0x56},  {"cli",IM_IMP,0x58},
  {"eor",IM_ABSY,0x59},  {"eor",IM_ABSX,0x5d},  {"lsr",IM_ABSX,0x5e},
  {"rts",IM_IMP,0x60},  {"adc",IM_INDX,0x61},  {"adc",IM_ZP,0x65},
  {"ror",IM_ZP,0x66},  {"pla",IM_IMP,0x68},  {"adc",IM_IMP,0x69},
  {"ror",IM_ACC,0x6a},  {"jmp",IM_IND,0x6c},  {"adc",IM_ABS,0x6d},
  {"ror",IM_ABS,0x6e},  {"bvs",IM_REL,0x70},  {"adc",IM_INDY,0x71},
  {"adc",IM_ZPX,0x75},  {"ror",IM_ZPX,0x76},  {"sei",IM_IMP,0x78},
  {"adc",IM_ABSY,0x79},  {"adc",IM_ABSX,0x7d},  {"ror",IM_ABSX,0x7e},
  {"sta",IM_INDX,0x81},  {"sty",IM_ZP,0x84},  {"sta",IM_ZP,0x85},
  {"stx",IM_ZP,0x86},  {"dey",IM_IMP,0x88},  {"txa",IM_IMP,0x8a},
  {"sty",IM_ABS,0x8c},  {"sta",IM_ABS,0x8d},  {"stx",IM_ABS,0x8e},
  {"bcc",IM_REL,0x90},  {"sta",IM_INDY,0x91},  {"sty",IM_ZPX,0x94},
  {"sta",IM_ZPX,0x95},  {"stx",IM_ZPY,0x96},  {"tya",IM_IMP,0x98},
  {"sta",IM_ABSY,0x99},  {"txs",IM_IMP,0x9a},  {"sta",IM_ABSX,0x9d},
  {"ldy",IM_IMM,0xa0},  {"lda",IM_INDX,0xa1},  {"ldx",IM_IMM,0xa2},
  {"ldy",IM_ZP,0xa4},  {"lda",IM_ZP,0xa5},  {"ldx",IM_ZP,0xa6},
  {"tay",IM_IMP,0xa8},  {"lda",IM_IMM,0xa9},  {"tax",IM_IMP,0xaa},
  {"ldy",IM_ABS,0xac},  {"lda",IM_ABS,0xad},  {"ldx",IM_ABS,0xae},
  {"bcs",IM_REL,0xb0},  {"lda",IM_INDY,0xb1},  {"ldy",IM_ZPX,0xb4},
  {"lda",IM_ZPX,0xb5},  {"ldx",IM_ZPY,0xb6},  {"clv",IM_IMP,0xb8},
  {"lda",IM_ABSY,0xb9},  {"tsx",IM_IMP,0xba},  {"ldy",IM_ABSX,0xbc},
  {"lda",IM_ABSX,0xbd},  {"ldx",IM_ABSY,0xbe},  {"cpy",IM_IMM,0xc0},
  {"cmp",IM_INDX,0xc1},  {"cpy",IM_ZP,0xc4},  {"cmp",IM_ZP,0xc5},
  {"dec",IM_ZP,0xc6},  {"iny",IM_IMP,0xc8},  {"cmp",IM_IMM,0xc9},
  {"dex",IM_IMP,0xca},  {"cpy",IM_ABS,0xcc},  {"cmp",IM_ABS,0xcd},
  {"dec",IM_ABS,0xce},  {"bne",IM_REL,0xd0},  {"cmp",IM_INDY,0xd1},
  {"cmp",IM_ZPX,0xd5},  {"dec",IM_ZPX,0xd6},  {"cld",IM_IMP,0xd8},
  {"cmp",IM_ABSY,0xd9},  {"cmp",IM_ABSX,0xdd},  {"dec",IM_ABSX,0xde},
  {"cpx",IM_IMM,0xe0},  {"sbc",IM_INDX,0xe1},  {"cpx",IM_ZP,0xe4},
  {"sbc",IM_ZP,0xe5},  {"inc",IM_ZP,0xe6},  {"inx",IM_IMP,0xe8},
  {"sbc",IM_IMM,0xe9},  {"nop",IM_IMP,0xea},  {"cpx",IM_ABS,0xec},
  {"sbc",IM_ABS,0xed},  {"inc",IM_ABS,0xee},  {"beq",IM_REL,0xf0},
  {"sbc",IM_INDY,0xf1},  {"sbc",IM_ZPX,0xf5},  {"inc",IM_ZPX,0xf6},
  {"sec",IM_IMP,0xf8},  {"sbc",IM_ABSY,0xf9},  {"sbc",IM_ABSX,0xfd},
  {"inc",IM_ABSX,0xfe},
  {NULL,(int)0,(int)0}
};

#define T_INSTRUCTION 1
#define T_A 2
#define T_X 3
#define T_Y 4
#define T_HASH 5
#define T_COMMA 6
#define T_LBRACKET 8 
#define T_RBRACKET 9
#define T_LANGLE 10
#define T_RANGLE 11

#define T_LITERAL 51
#define T_NUMERIC 52
#define T_EQUALS 53
#define T_PLUS 54
#define T_MINUS 55
#define T_STRING 56

#define T_SEMICOLON 80
#define T_EOL 81
#define T_EOF 82

#define T_K_PATCH 100
#define T_K_FREE 101
#define T_K_SPACE 102
#define T_K_SECTION 103
#define T_K_PRERELOCATE 104
#define T_K_POSTRELOCATE 105
#define T_K_BYTE 106
#define T_K_WORD 107

char inputfile[8192]="input";
int linenum=0;
int linepos=0;
int linelen=0;
char linebuf[1024];
int token_queued=1;
int token_type;
int token_value;
char token_body[1024];
int token_body_len=0;
int token_quote_mode=0;

struct label *labels=NULL;
struct assembled_byte *sections[256]={NULL};
int section_lines[256]={0};
int section_lengths[256]={0};
int section_addresses[256]={-1};
int section_patchp[256]={0};
int section_count=0;
/* most recent section and byte structures */
struct assembled_byte *current_section=NULL;
struct assembled_byte *current_byte=NULL;

int special_section=(int)0;

struct assembled_byte *pre_relocate=NULL;
int pre_relocate_address=0;
int pre_relocate_length=0;
struct assembled_byte *post_relocate=NULL;
int post_relocate_address=0;
int post_relocate_length=0;

int load_address=0x0801;

int wedge_next_address=0x0801;
#define BASIC_HEADER_LENGTH 0x13

int free_space_starts[256]={0};
int free_space_ends[256]={-1};
int free_space_count=0;

/* Has a "hanging label" been declared, but not
   yet associated with a line of code? */
int pending_labels=0;
char *pending_label_name[16];

int assembleLabel(FILE *f);
int assembleInstruction(FILE *f);
int assembleLineDregs(FILE *f);

int syntaxError(char *msg);

int placeSections();
int resolveLabels();
int resolveValues();
int writeWedge(char *file);

int dumpSections();

struct assembled_byte *parseValueOperand(FILE *f);
int commitByte(struct assembled_byte *ab);

int accumulatedToken()
{
  /* Parse and return accumulated token */
  int i;
  int dec=1,hex=1;

  token_body[token_body_len]=0;

  if (token_quote_mode)
    {
      token_type=T_STRING;
      token_body_len=0;      
      token_quote_mode=0;
      return 0;
    }
  else
    {
      if (!strcasecmp(token_body,"a")) 
	{ token_type=T_A; token_body_len=0; return 0; }
      if (!strcasecmp(token_body,"x")) 
	{ token_type=T_X; token_body_len=0; return 0; }
      if (!strcasecmp(token_body,"y")) 
	{ token_type=T_Y; token_body_len=0; return 0; }
      
      if (!strcasecmp(token_body,".patch")) 
	{ token_type=T_K_PATCH; token_body_len=0; return 0; }
      if (!strcasecmp(token_body,".free")) 
	{ token_type=T_K_FREE; token_body_len=0; return 0; }
      if (!strcasecmp(token_body,".space")) 
	{ token_type=T_K_SPACE; token_body_len=0; return 0; }
      if (!strcasecmp(token_body,".section")) 
	{ token_type=T_K_SECTION; token_body_len=0; return 0; }
      if (!strcasecmp(token_body,".prerelocate")) 
	{ token_type=T_K_PRERELOCATE; token_body_len=0; return 0; }
      if (!strcasecmp(token_body,".postrelocate")) 
	{ token_type=T_K_POSTRELOCATE; token_body_len=0; return 0; }
      if (!strcasecmp(token_body,".byte")) 
	{ token_type=T_K_BYTE; token_body_len=0; return 0; }
      if (!strcasecmp(token_body,".word")) 
	{ token_type=T_K_WORD; token_body_len=0; return 0; }
      
      if ((token_body[0]=='\'')&&(token_body[2]=='\'')
	  &&(token_body_len==3))
	{
	  /* ascii (NOT PETSCII!) character */
	  token_value=token_body[1];
	  token_type=T_NUMERIC;
	  token_body_len=0; 
	  return 0;      
	}
      if (token_body[0]=='$')
	{
	  /* hex */
	  for(i=1;i<token_body_len;i++)
	    switch(tolower(token_body[i]))
	      {
	      case '0': case '1': case '2': case '3': case '4':
	      case '5': case '6': case '7': case '8': case '9':
	      case 'a': case 'b': case 'c': case 'd': case 'e':
	      case 'f':	
		break;
	      default:
		hex=0;
	      }
	  if (!hex) 
	    {
	      token_body_len=0; 
	      token_body[0]=0;
	      return syntaxError("Bad character in hexadecimal value");
	    }
	  token_value=strtol(&token_body[1],NULL,16);
	  token_type=T_NUMERIC;
	  token_body_len=0; 
	  token_body[0]=0;
	  return 0;
	}
      if (token_body[0]=='%')
	{
	  /* binary */
	  hex=1;
	  for(i=1;i<token_body_len;i++)
	    switch(tolower(token_body[i]))
	      {
	      case '0': case '1':
		break;
	      default:
		hex=0;
	      }
	  if (!hex) 
	    {
	      token_body_len=0; 
	      token_body[0]=0;
	      return syntaxError("Bad character in binary value");
	    }
	  token_value=strtol(&token_body[1],NULL,2);
	  token_type=T_NUMERIC;
	  token_body_len=0; 
	  return 0;
	}
      
      /* work out whether the token is a dec, hex or literal value */
      for(i=0;i<token_body_len;i++)
	{
	  switch(tolower(token_body[i]))
	    {
	    case '0': case '1': case '2': case '3': case '4':
	    case '5': case '6': case '7': case '8': case '9':
	      break;
	    default:
	      dec=0;
	    }
	}
      
      if (dec) 
	{
	  token_type=T_NUMERIC;
	  token_value=atoi(token_body);
	  token_body_len=0; 
	  return 0;
	}
      
      /* instruction or literal? */
      token_type=T_LITERAL;
      for(i=0;instruction_set[i].name;i++)
	if (!strcasecmp(instruction_set[i].name,token_body))
	  token_type=T_INSTRUCTION;
      
      token_body_len=0;      
      return 0;
    }
}

int getNextToken(FILE *f)
{
  if (token_queued)
    {
      token_queued=0; return 0;
    }

  while(1)
    {
      /* Make sure we have input */
      while (linepos>=linelen)
	{
	  linebuf[0]=0; fgets(linebuf,1024,f); linenum++;
	  if (!linebuf[0])
	    {
	      if (token_body_len) return accumulatedToken();
	      else
		{
		  token_type=T_EOF;
		  return 0;
		}
	    }
	  linelen=strlen(linebuf);
	  linepos=0;
	}

      if (token_quote_mode)
	{
	  switch(linebuf[linepos])
	    {
	    case '"': 
	      linepos++;
	      return accumulatedToken();
	    default:
	      /* Accumulate character */
	      if (token_body_len<1023)
		token_body[token_body_len++]=linebuf[linepos];
	      linepos++;
	    }
	}
      else
	{
	  switch(linebuf[linepos])
	    {
	    case '"':
	      token_quote_mode=1;
	      linepos++;
	      break;
	    case ' ': case '\t':
	      linepos++;
	      if (token_body_len)
		{
		  return accumulatedToken();
		}
	      break;
	    case '(': case ')': case ';': case ',': 
	    case '<': case '>': case '#': case '+':
	    case '-': case '=': case '\n': case '\r':
	      /* special single character tokens */
	      if (!token_body_len)
		{
		  switch(linebuf[linepos])
		    {
		    case '(': token_type=T_LBRACKET; break;
		    case ')': token_type=T_RBRACKET; break;
		    case ';': token_type=T_SEMICOLON; break;
		    case ',': token_type=T_COMMA; break;
		    case '<': token_type=T_LANGLE; break;
		    case '>': token_type=T_RANGLE; break;
		    case '#': token_type=T_HASH; break;
		    case '=': token_type=T_EQUALS; break;
		    case '+': token_type=T_PLUS; break;
		    case '-': token_type=T_MINUS; break;
		    case '\n': token_type=T_EOL; break;
		    case '\r': token_type=T_EOL; break;
		    }
		  token_body_len=0;
		  linepos++;
		  return 0;
		}
	      else
		return accumulatedToken();
	    default:
	      /* Accumulate character */
	      if (token_body_len<1023)
		token_body[token_body_len++]=linebuf[linepos];
	      linepos++;
	    }
	}
    }
}

int parseFile(FILE *f)
{
  token_type=0;
  while(token_type!=T_EOF)
    {
      getNextToken(f);
      switch(token_type)
	{
	case 0: case T_EOL: break;
	case T_INSTRUCTION:
	  /* Try to assemble the instruction */
	  if (assembleInstruction(f)) return -1;
	  break;
	case T_K_BYTE:
	  /* Byte stream */
	  token_type=T_COMMA;
	  while(token_type==T_COMMA)
	    {
	      struct assembled_byte *ab=parseValueOperand(f);
	      if (ab->bytes==3) ab->bytes=1;
	      commitByte(ab);
	      getNextToken(f);
	    }
	  token_queued=1;
	  break;
	case T_LITERAL:
	  /* It's a label */
	  if (assembleLabel(f)) return -1;
	  break;
	case T_K_PATCH:
	  /* Start a patch here, i.e. a section at a set location */
	  if (pending_labels)
	    return syntaxError("Cannot label a PATCH directive");
	  current_section=NULL;
	  current_byte=NULL;
	  special_section=(int)0;
	  getNextToken(f);
	  if (token_type!=T_NUMERIC)
	    return syntaxError("Expect $nnnn after .patch");
	  section_addresses[section_count]=token_value;
	  section_patchp[section_count]=1;
	  if ((token_value<0)||(token_value>0xffff))
	    return syntaxError("Patch address outside of address space");
	  break;
	case T_K_SECTION:
	  if (pending_labels)
	    return syntaxError("Cannot label a SECTION directive");
	  current_section=sections[section_count];
	  current_byte=NULL;
	  special_section=(int)0;
	  break;
	case T_K_PRERELOCATE:
	  if (pending_labels)
	    return syntaxError("Cannot label an PRERELOCATE directive");
	  if (pre_relocate)
	    return syntaxError("Cannot have multiple PRERELOCATE directives");
	  current_section=pre_relocate;
	  current_byte=NULL;
	  special_section=T_K_PRERELOCATE;
	  break;
	case T_K_POSTRELOCATE:
	  if (pending_labels)
	    return syntaxError("Cannot label an POSTRELOCATE directive");
	  if (post_relocate)
	    return syntaxError("Cannot have multiple POSTRELOCATE directives");
	  current_section=post_relocate;
	  current_byte=NULL;
	  special_section=T_K_POSTRELOCATE;
	  break;
	case T_K_SPACE:
	  getNextToken(f);
	  if (token_type!=T_NUMERIC)
	    return syntaxError("Expect $nnnn after .SPACE");
	  free_space_starts[free_space_count]=token_value;
	  getNextToken(f);
	  if (token_type!=T_NUMERIC)
	    return syntaxError("Expect $nnnn $nnnn after .SPACE");
	  free_space_ends[free_space_count++]=token_value;
	  if (assembleLineDregs(f)) return -1;	   
	  break;
	case T_SEMICOLON:
	  token_queued=1;
	  if(assembleLineDregs(f)) return -1;
	  break;
	case T_HASH:
	  /* C Pre-processor information line.
	     Record current file name and line number.
	  */
	  {
	    getNextToken(f);
	    if (token_type!=T_NUMERIC)
	      return syntaxError("Expect line number after #");
	    linenum=token_value-1;
	    getNextToken(f);
	    if ((token_type!=T_LITERAL)&&(token_type!=T_STRING))
	      return syntaxError("Expect filename after '# linenumber '");
	    strcpy(inputfile,token_body);
	  }
	  break;
	case T_EOF:
	  return 0;
	  break;
	default:
	  return syntaxError("unexpected token");
	}

    }  
  return 0;
}

int dumpFreeSpace()
{
  int i;
  for(i=0;i<free_space_count;i++)
    {
      if (free_space_starts[i]<free_space_ends[i])
	printf("  $%04x - $%04x (%d bytes)\n",
	       free_space_starts[i],free_space_ends[i],
	       free_space_ends[i]-free_space_starts[i]+1
	       );
    }
  return 0;
}

int main(int argc,char **argv)
{
  FILE *f;
  char tpl[1024]="/tmp/build_wedge.XXXXXXXX";

  if (argc!=3)
    {
      fprintf(stderr,
	      "Usage: build_wedge <specification file> <output file>\n\n");
      fprintf(stderr,
	      "  Assembles, places and links the wedge code in the address spaces\n");
      fprintf(stderr,
	      "  specified.  Code is only fragmented where explicitly allowed\n");
      exit(2);
    }

  f=fopen(argv[1],"r");
  if (!f) return fprintf(stderr,"Failed to open input file '%s'\n",argv[1]);
  fclose(f);

  /* Pre-process input file */
  {
    char cmd[8192];
    int fd;
    cmd[8191]=0;
    fd=mkstemp(tpl);
    if (fd==-1) 
      { 
	fprintf(stderr,"Could not create temporary file %s\n",tpl);
	exit(1);
      }
    close(fd);
    snprintf(cmd,8192,"cpp -D__64net2__=%d.%d -D__64net__=2.0 -C %s > %s",
	     VER_MAJ,VER_MIN,argv[1],tpl);
    if (cmd[8191])
      {
	fprintf(stderr,"C Pre-processor command too long.\n");
	exit(1);
      }
    printf("%s\n",cmd);
    system(cmd);

    f=fopen(tpl,"r");
    if (!f) 
      return fprintf(stderr,"Failed to open pre-processed input file '%s'\n",
		     tpl);    
  }

  printf("\nPass 1 - Parse and assemble\n-----------------------\n");
  parseFile(f);
  /* dumpSections(); */
  printf("\nPass 2 - Place sections\n-----------------------\n");
  placeSections();
  printf("\nPass 3 - Resolve labels\n-----------------------\n");
  resolveLabels();
  printf("\nPass 4 - Resolve remaining values\n-----------------------\n");
  resolveValues();
  /* dumpSections(); */
  printf("\nRemaining Free Space\n-----------------------\n");
  dumpFreeSpace();

  writeWedge(argv[2]);

  /* Clean up the temporary file */
  unlink(tpl);

  return 0;
}

int syntaxError(char *msg)
{
  fprintf(stderr,"?SYNTAX  ERROR IN %d\n",linenum);
  fprintf(stderr,"%s:%d.%d: %s\n",inputfile,linenum,linepos,msg);
  fprintf(stderr,"last token type=%d\n",token_type);
  if ((token_type==T_LITERAL)||(token_type==T_STRING))
    fprintf(stderr,"token_body=[%s]\n",token_body);
  fprintf(stderr,"Line of input is:\n%s\n",linebuf);
  exit(1);
}

struct label *newLabel(char *name,
		       int r,int d,int zpP,int vp,int v,
		       struct assembled_byte *section,
		       struct assembled_byte *first_byte,
		       struct label *next)
{
  struct label *l,*insert_spot=NULL;

  /* Check that label name is unique */
  l=labels;
  while(l)
    {
      insert_spot=l;
      if (!strcasecmp(l->name,name))
	{
	  if (l->definedP)
	    {
	      syntaxError("Duplicate label name in section");
	      return NULL;
	    }
	  insert_spot=NULL;
	  break;
	}
      l=l->next;
    }

  /* Allocate structure */
  if (!l) 
    {
      l=(struct label*)malloc(sizeof(struct label));
      bzero(l,sizeof(struct label));
      l->next=next;
    }
  if (!l) 
    {
      syntaxError("malloc() failed creating label");
      return NULL;
    }

  /* Fill out structure */
  if (l->name)
    {
    if (strcasecmp(name,l->name))
      {
	free(l->name);
	l->name=strdup(name);
      }
    }
  else
    l->name=strdup(name);
  l->resolvedP=r;
  l->definedP=d;
  if (!l->zpP) l->zpP=zpP;
  l->valueP=vp;
  l->value=v;
  l->section=section;
  l->first_byte=first_byte;

  /* add to list and return */
  if (insert_spot) insert_spot->next=l;
  if (!labels) labels=l;
  return l;
}

int assembleLabel(FILE *f)
{
  /* Assemble a label at the start of a line,
     and maybe an instruction and some dregs as well */

  /* printf("label [%s]\n",token_body); */

  /* Record label */
  if (pending_labels<16)
    {
      pending_label_name[pending_labels]=strdup(token_body);
      pending_labels++;
    }
  else
    {
      syntaxError("Too many consecutive labels");
    }

  /* Check out next token */
  getNextToken(f);
  switch(token_type)
    {
    case T_INSTRUCTION: return assembleInstruction(f);
    case T_EOL: return 0;
    case T_EOF: return syntaxError("Dangling label at end of file");
    case T_EQUALS: 
      /* Label is a value type */
      getNextToken(f);
      if (token_type!=T_NUMERIC) 
	return syntaxError("Expected dec, hex or binary value after =");
      while(pending_labels>0)
	{
	  pending_labels--;
	  if (!newLabel(pending_label_name[pending_labels],1,1,0,1,token_value,
			current_section,current_byte,NULL))
	    return -1;
	}
      pending_labels=0;
      return assembleLineDregs(f);
    case T_SEMICOLON:
      /* label followed by a comment */
      token_queued=1;
      assembleLineDregs(f);
      return 0;
    default:
      return syntaxError("Unexpected stuff after a label");      
    }
}

int assembleLineDregs(FILE *f)
{
  getNextToken(f);
  while((token_type!=T_EOL)&&(token_type!=T_EOF))
    {
      switch(token_type)
	{
	case T_SEMICOLON:
	  token_queued=0;
	  linepos=linelen-1;
	  while((token_type!=T_EOL)&&(token_type!=T_EOF))
	    { 
	      getNextToken(f);
	    }
	  return 0;
	default:
	  return syntaxError("Unexpected stuff at the end of the line");      
	}
      getNextToken(f);
    }
  return 0;
}

struct assembled_byte *parseValueOperand(FILE *f)
{
  int bytes=3;
  struct assembled_byte *ab=NULL;

  /* Check for high byte/low byte angles */
  getNextToken(f);
  if (token_type==T_RANGLE) { bytes=2; getNextToken(f); }
  else
    if (token_type==T_LANGLE) { bytes=1; getNextToken(f); }

  switch(token_type)
    {
    case T_LITERAL:
      /* it might be a label */
      {
	struct label *l=labels;
	while(l&&(strcasecmp(l->name,token_body)))
	  l=l->next;
	if (!l)
	  {
	    /* un-defined label.
	       Create as undefined */
	    /* printf("Creating undefined label [%s]\n",token_body); */
	    l=newLabel(token_body,0,0,0,1,0,
		    NULL,NULL,NULL);	    
	  }
	
	ab=(struct assembled_byte*)calloc(1,sizeof(struct assembled_byte));
	if (!ab) 
	  {
	    syntaxError("malloc() failure in assembleInstruction()");
	    return NULL;
	  }
	ab->valueP=0;
	ab->relativeP=0;
	ab->label=l;
	ab->label_offset=0;
	ab->bytes=bytes;
	ab->next=NULL;
      }
      break;
    case T_NUMERIC:
      ab=(struct assembled_byte*)calloc(1,sizeof(struct assembled_byte));
      if (!ab) 
	{
	  syntaxError("malloc() failure in assembleInstruction()");
	  return NULL;
	}
      ab->valueP=1;
      ab->relativeP=0;
      ab->label=NULL;
      ab->value=token_value;
      ab->label_offset=0;
      ab->bytes=bytes;
      ab->next=NULL;
      break;
    default:
      syntaxError("Expected dec, hex, binary value or label");
      return NULL;
    }

  /* Check for +/- value */
  getNextToken(f);
  switch(token_type)
    {
    case T_PLUS: 
      getNextToken(f);
      if (token_type!=T_NUMERIC)
	{
	  syntaxError("Expected a number after value+");
	  free(ab);
	  return NULL;
	}
      ab->label_offset=token_value;
      return ab;
    case T_MINUS: 
      getNextToken(f);
      if (token_type!=T_NUMERIC)
	{
	  syntaxError("Expected a number after value-");
	  free(ab);
	  return NULL;
	}
      ab->label_offset=token_value;
      return ab;
    default:
      token_queued=1;
      return ab;
    }
}

int commitByte(struct assembled_byte *ab)
{
  if (!ab) 
    return syntaxError("commitByte() passed NULL!");

  /* dumpSections(); */

	if(special_section==0) {
		if ((!current_section)) { /* ||(!current_byte)) */
			/* Start first section */
		        section_lines[section_count]=linenum;
			sections[section_count++]=ab;
			current_section=ab;
			current_byte=ab;
		}
		else {
			current_byte->next=ab;
			current_byte=ab;
		}
      
		/* Tally section lengths */
		section_lengths[section_count-1]++;
		if ((ab->bytes==3)&&(!ab->relativeP))
		section_lengths[section_count-1]++;
      
		if (pending_labels) {
			/* Declare the pending label as being here */
			while(pending_labels>0) {
				pending_labels--;
				if (!newLabel(pending_label_name[pending_labels],
				0,1,0,1,token_value,
				current_section,current_byte,NULL))
				return -1;
			}
		}
	}
	else if(special_section==T_K_PRERELOCATE) {
		if (!pre_relocate) { 
			pre_relocate=ab; current_section=ab; current_byte=ab;
		}
		else { 
			current_byte->next=ab; current_byte=ab; 
		}
		pre_relocate_length++;
		if ((ab->bytes==3)&&(!ab->relativeP)) pre_relocate_length++;
		while (pending_labels) {
			/* Declare the pending label as being here */
			pending_labels--;
			if (!newLabel(pending_label_name[pending_labels],0,1,0,1,token_value,current_section,current_byte,NULL))
			return -1;
		}
	}
	else if(special_section==T_K_POSTRELOCATE) {
		if (!post_relocate) { 
			post_relocate=ab; current_section=ab; current_byte=ab;
		}
		else { 
			current_byte->next=ab; current_byte=ab; 
		}
		post_relocate_length++;      
		if ((ab->bytes==3)&&(!ab->relativeP)) post_relocate_length++;
		while (pending_labels) {
			/* Declare the pending label as being here */
			pending_labels--;
			if (!newLabel(pending_label_name[pending_labels],0,1,0,1,token_value,current_section,current_byte,NULL))
			return -1;
		} 
	}
	return 0;
}

int commitOpcode(char *inst,int mode)
{
  int i;
  struct assembled_byte *ab;

  /* Find opcode */
  for(i=0;instruction_set[i].name;i++)
    if ((!strcasecmp(inst,instruction_set[i].name))
	&&(instruction_set[i].mode==mode))
      break;

  if (!instruction_set[i].name)
    {
      char temp[1024];
      snprintf(temp,1024,"Illegal addressing mode #%d for '%s'",
	       mode,inst);
      return syntaxError(temp);
    }

  /* Create byte structure and add to list */
  ab=(struct assembled_byte*)calloc(1,sizeof(struct assembled_byte));
  if (!ab) return syntaxError("malloc() failed in commitOpcode()");

  ab->valueP=1;
  ab->relativeP=0;
  ab->value=instruction_set[i].byte;
  ab->label_offset=0;
  ab->bytes=1;
  ab->next=NULL;
  return commitByte(ab);
}

int assembleInstruction(FILE *f)
{
  /* Parse instruction mode arguments */
  char *inst=strdup(token_body);
  struct assembled_byte *ab;
  int mode=IM_IMP;
  int absP=1;
  int indxP=0;
  int indyP=0;
  
  getNextToken(f);
  switch(token_type)
    {
    case T_A:
      return commitOpcode(inst,IM_ACC);
    case T_SEMICOLON:
      token_queued=1;
      assembleLineDregs(f);
      /* fall through */
    case T_EOL: case T_EOF:
      token_queued=1;
      return commitOpcode(inst,IM_IMP);
    case T_HASH:
      /* Immediate mode */
      ab=parseValueOperand(f);
      if (!ab) return -1;
      /* Commit instruction */
      if (commitOpcode(inst,IM_IMM)) return -1;
      /* Demote 16bit values to 8 if necessary */
      if (ab->bytes==3) ab->bytes=1;
      return commitByte(ab);
    case T_NUMERIC:
    case T_LITERAL:
      /* abs, zp or rel group of modes */
      token_queued=1;
      ab=parseValueOperand(f);
      mode=IM_ABS;
      /* look for , at end */
      getNextToken(f);
      if (token_type==T_COMMA)
	{
	  /* Indexed modes */
	  getNextToken(f);
	  switch (token_type)
	    {
	    case T_X: indxP=1; break;
	    case T_Y: indyP=1; break;
	    default:
	      free(ab);
	      return syntaxError("Illegal index - expected X or Y");
	    }
	}
      else 
	{
	  /* Wasn't a comma - so work out what to do with it */
	  token_queued=1;
	}
      /* Work out if we are using ABS or ZP.
	 For now guess based on number of bytes specified by ab,
	 or if ab refers to a literal value < 256 */
      if (ab->bytes!=3)	absP=0;
      if (ab->valueP&&(ab->value<256)) 
	{ absP=0; ab->bytes=1; }
      if (ab->label!=NULL)
	{
	  if (ab->label->resolvedP==1)
	    {
	      if (((ab->label->value+ab->label_offset)>=0)
		  &&((ab->label->value+ab->label_offset)<256))
		{ absP=0; ab->bytes=1; }
	    }
	}
      if (absP)
	{
	  mode=IM_ABS;
	  if (indxP) mode=IM_ABSX;
	  if (indyP) mode=IM_ABSY;
	}
      else
	{
	  mode=IM_ZP;
	  if (indxP) mode=IM_ZPX;
	  if (indyP) mode=IM_ZPY;
	}
      /* Work out if relative mode for branches */
      if ((tolower(inst[0])=='b')&&
	  (strcasecmp(inst,"bit"))&&
	  ((mode==IM_ABS)||(mode==IM_ZP)))
	{
	  mode=IM_REL;
	  absP=0;
	  ab->relativeP=1;	  
	}

      /* Promote ZP to ABS where necessary (i.e. JSR/JMP) */
      if ((tolower(inst[0])=='j')&&(mode==IM_ZP))
	{
	  mode=IM_ABS;
	  ab->bytes=3;
	}

      /* Record instruction */
      if (commitOpcode(inst,mode)) return -1;
      return commitByte(ab);      
    case T_LBRACKET:
      /* indirect addressing modes */
      ab=parseValueOperand(f);
      mode=IM_IND;
      getNextToken(f);
      switch(token_type)
	{
	case T_RBRACKET:
	  getNextToken(f);
	  if (token_type==T_COMMA)
	    {
	      /* indirect Y ? */
	      getNextToken(f);
	      if (token_type==T_Y)
		{ mode=IM_INDY; ab->bytes=1;
		 if (ab->label) ab->label->zpP=1; }
	      else
		return syntaxError("Illegal indirect indexed mode");
	    }
	  else
	    token_queued=1;
	  /* Record instruction */
	  if (commitOpcode(inst,mode)) return -1;
	  return commitByte(ab);
	case T_COMMA:
	  getNextToken(f);
	  if (token_type==T_X)
	    { mode=IM_INDX; ab->bytes=1;
	    if (ab->label) ab->label->zpP=1; }
	  else
	    return syntaxError("Illegal indirect indexed mode");
	  getNextToken(f);
	  if (token_type!=T_RBRACKET)
	    return syntaxError("Junk in indirect indexed mode");
	  /* Record instruction */
	  if (commitOpcode(inst,mode)) return -1;
	  return commitByte(ab);
	default:
	  return syntaxError("Junk in indirect operand");
	}
    default:
      return syntaxError("You call that an operand?");
    }
}

int dumpByte(struct assembled_byte *b)
{
  if (b->relativeP)
    printf("@");

  if (b->valueP)
    {
      switch(b->bytes)
	{
	case 1: printf("$%02x ",b->value&0xff); break;
	case 2: printf("$%02x ",(b->value>>8)&0xff); break;
	case 3: 
	  if (!b->relativeP)
	    printf("$%02x,$%02x ",
		     b->value&0xff,(b->value>>8)&0xff);
	  else
	    printf("$%04x ",
		   b->value&0xffff); 
	  break;
	}
    }
  else
    {
      /* Label derived value */
      printf("%s%s[%s] ",
	     b->bytes==3 ? "" : (b->bytes==2 ? "<" : ">"),
	     b->label->definedP ? "" : "*",
	     b->label->name);
    }
  return 0;
}

int dumpBytes(struct assembled_byte *b)
{
  while(b)
    {
      dumpByte(b);
      b=b->next;
    }
  printf("\n");
  return 0;
}

int dumpSections()
{
  int i;
  for(i=0;i<section_count;i++)
    {
      printf("\nSection %d (%d bytes):\n-----------\n",i,section_lengths[i]);
      dumpBytes(sections[i]);
    }

  printf("\nPre-relocate (%d bytes):\n-----------\n",pre_relocate_length);
  dumpBytes(pre_relocate);

  printf("\nPost-relocate (%d bytes):\n-----------\n",post_relocate_length);
  dumpBytes(post_relocate);

  return 0;
}

int placeSections()
{
  /* Place sections into the free space available */
  int i,j;
  int smallest_space=-1;
  int smallest_space_len=-1;

  dumpFreeSpace();
  printf("\n");

  for(i=0;i<section_count;i++)
    if (!section_patchp[i])
      {
	smallest_space=-1;
	for(j=0;j<free_space_count;j++)
	  {
	    if ((free_space_ends[j]-free_space_starts[j]+1)
		>=section_lengths[i])
	      {
		if ((smallest_space==-1)
		    ||(smallest_space_len>
		       (free_space_ends[j]-free_space_starts[j]+1)))
		  {
		    smallest_space_len
		      =(free_space_ends[j]-free_space_starts[j]+1);
		    smallest_space=j;
		  }
	      }
	  }
	if (smallest_space==-1)
	  {
	    fprintf(stderr,"Could not place section %d, which is defined in line %d\n",i,section_lines[i]);
	    fprintf(stderr,"Need %d contiguous bytes\n",section_lengths[i]);
	    printf("Remaining Free Space:\n----------------\n");
	    dumpFreeSpace();
	    printf("Byte stream of section:\n");
	    dumpBytes(sections[i]);
	    exit(1);
	  }
	else
	  {
	    printf("Placing section %d at $%04x-$%04x\n",
		   i,
		   free_space_starts[smallest_space],
		   free_space_starts[smallest_space]
		   +section_lengths[i]-1);
	    section_addresses[i]=free_space_starts[smallest_space];
	    free_space_starts[smallest_space]+=section_lengths[i];
	  }
      }
  else
    {
      printf("Patch %d at $%04x-$%04x\n",
	     i,
	     section_addresses[i],
	     section_addresses[i]
	     +section_lengths[i]-1);
    }

  /* Now that we have placed sections, and know how many there are,
     we can calculate the address for the post-relocate/entry routine */

  /* Address of wedge after SYS statement */
  pre_relocate_address=wedge_next_address+BASIC_HEADER_LENGTH;
  /* The total length of all sections */
  for(i=0;i<section_count;i++) pre_relocate_address+=section_lengths[i];

  /* 13 bytes of relocation code for each section */
  post_relocate_address=pre_relocate_address+pre_relocate_length;
  post_relocate_address+=13*section_count;

  printf("Pre-Entry point is $%04x\n",pre_relocate_address);
  printf("Post-Entry point is $%04x\n",post_relocate_address);

  return 0;
}

int resolveLabels()
{
  struct label *l=labels;
  int c=0;

  while(l)
    {
      if (!l->resolvedP)
	{
	  struct assembled_byte *b;
	  int section_offset=0,section_num=-1;
	  int section_address=0;
	  for(section_num=0;section_num<section_count;section_num++)
	    if (l->section==sections[section_num]) break;
	  if (section_num>=section_count)
	    {
	      if (l->section==pre_relocate) 
		section_address=pre_relocate_address;
	      if (l->section==post_relocate) 
		section_address=post_relocate_address;
	    }
	  else
	    section_address=section_addresses[section_num];
	  b=l->section;
	  while(b&&(b!=l->first_byte))
	    {
	      section_offset++;
	      if ((b->bytes==3)&&(!b->relativeP)) section_offset++;
	      b=b->next;
	    }
	  if (!b)
	    {
	      fprintf(stderr,"Could not resolve label [%s]\n",l->name);
	      return -1;
	    }
	  l->value=section_address+section_offset;
	  l->resolvedP=1;
	}
      c+=strlen(l->name)+10;
      if (c>79) { printf("\n"); c=strlen(l->name)+7; }
      printf("%s = $%04x, ",l->name,l->value);
      if (l->zpP&&(l->value>255))
	{
	  fprintf(stderr,"WARNING: Label %s > 255, but used as ZP operand\n",l->name);
	}
      l=l->next;
    }
  if (c) printf("\n");
  return 0;
}

int resolveSectionValues(struct assembled_byte *b,
			 int section_address)
{
  int section_offset=0;
  while(b)
    {
      if (!b->valueP)
	{
	  b->valueP=1;
	  if (!b->label->resolvedP) 
	    {
	      fprintf(stderr,"Label '%s' could not be resolved.\n",
		      b->label->name);
	      return -1;
	    }
	  /* else
	     printf("[%s] is resolved as $%04x\n",
	     b->label->name,b->label->value); */
	  b->value=b->label->value+b->label_offset;
	  if (b->relativeP)
	    {
	      b->value-=section_address+section_offset+1;
	      b->bytes=1;
	      if ((b->value<-128)||(b->value>127))
		{
		  fprintf(stderr,"Branch out of range branching to %s\n",
			  b->label->name);
		  return -1;
		}
	      b->relativeP=0;
	    }
	}
      else
	{
	  if (b->label)
	    {
	      printf("Ignored [%s] is %sresolved as $%04x\n",
		     b->label->name,
		     b->label->resolvedP ? "":"not ",
		     b->label->value);

	    }
	}
      section_offset++;
      if ((b->bytes==3)&&(!b->relativeP)) section_offset++;
      
      b=b->next;
    }
  return 0;
}

int resolveValues()
{
  int s;
  struct assembled_byte *b;
  
  for(s=0;s<section_count;s++)
    {
      b=sections[s];
      resolveSectionValues(b,section_addresses[s]);
    }

  resolveSectionValues(pre_relocate,pre_relocate_address);
  resolveSectionValues(post_relocate,post_relocate_address);

  return 0;
}

int writeWedge(char *file)
{
  /* Write out a c64 wedge */
  FILE *f;
  int i;

  f=fopen(file,"w");
  if (!f)
    return fprintf(stderr,"Could not open wedge file '%s' for write\n",file);

  wedge_next_address=load_address;

  /* Output BASIC sys jump start */ 
  fprintf(f,"%c%c%c%c%c%c%c%05d,$%04X%c%c%c",
	  wedge_next_address&0xff,(wedge_next_address>>8),
	  (wedge_next_address+BASIC_HEADER_LENGTH-2)&0xff,
	  ((wedge_next_address+BASIC_HEADER_LENGTH-2)>>8),
	  2005&0xff,2005>>8,
	  0x9e,
	  pre_relocate_address,
	  pre_relocate_address,
	  0,0,0);
  /* printf("File offset after BASIC header: %ld\n",ftell(f)); */
  wedge_next_address+=BASIC_HEADER_LENGTH;

  /* output code sections */
  for(i=0;i<section_count;i++)
    {
      struct assembled_byte *b;
      b=sections[i];
      while(b)
	{
	  if (b->bytes&1) fputc(b->value&0xff,f);
	  if (b->bytes&2) fputc((b->value>>8)&0xff,f);
	  b=b->next;
	}
    }

  /* printf("File offset at entry point: %ld (i.e. $%04lX)\n",
     ftell(f),ftell(f)+load_address-2); */

  /* Output ROM copy code */
  /* for(i=0;romcopycode[i]!=0x1000;i++)
     fprintf(f,"%c",romcopycode[i]);
     printf("Output %d bytes of ROM copy code\n",i); */

  /* output pre-relocate code */
  /* printf("Next address is $%04x\n",wedge_next_address); */

  /* printf("File offset at pre-relocation entry point: %ld (i.e. $%04lX)\n",
     ftell(f),ftell(f)+load_address-2); */
     
  {
    struct assembled_byte *b;
    b=pre_relocate;
    while(b)
      {
	if (b->bytes&1) fputc(b->value&0xff,f);
	if (b->bytes&2) fputc((b->value>>8)&0xff,f);
	b=b->next;
      }
  }

  /* output section copy code */
  for(i=0;i<section_count;i++)
    {
      fprintf(f,"%c%c%c%c%c%c%c%c%c%c%c%c%c",
	      0xa2,0x00,
	      0xbd,wedge_next_address&0xff,wedge_next_address>>8,
	      0x9d,section_addresses[i]&0xff,section_addresses[i]>>8,
	      0xe8,
	      0xe0,section_lengths[i],
	      0xd0,0xf5);
      wedge_next_address+=section_lengths[i];
    }

  wedge_next_address+=13*section_count;

  /* output post-relocate code */
  /* printf("Next address is $%04x\n",wedge_next_address); */

  /* printf("File offset at post-relocation entry point: %ld (i.e. $%04lX)\n",
     ftell(f),ftell(f)+load_address-2); */

  {
    struct assembled_byte *b;
    b=post_relocate;
    while(b)
      {
	if (b->bytes&1) fputc(b->value&0xff,f);
	if (b->bytes&2) fputc((b->value>>8)&0xff,f);
	b=b->next;
      }
  }

  /* RTS at end of wedge */
  fprintf(f,"%c",0x60);

  fclose(f);
  return 0;
}
