#include "/usr/include/linux/ioctl.h"
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <math.h>
//#define DEBUG


#define ECM_COLOR 1
#define PETSCII 2
#define KOALA 4
#define MULTICOL_CHARSET 3
#define CHARSETSIZE 0x800

const int color_ranges[] = {
/*	0x2,0x0,
	0x6,0x0,
	0x6,0x0,
	0x9,0x0,
	0xb,0x0,
	0x0,0x9,
	0x2,0x9,
	0x4,0x9,
	0x6,0x9,
	0x8,0x9,
	0xc,0x9,
	0xc,0x8,
	0x2,0x8,
	0x4,0x8,
	0x5,0x8,
	0xe,0x8,
	0xa,0x8,
	0xc,0xa,
	0xf,0xa,
	0xe,0xa,
	0x3,0xa,
	0x5,0xa
*/		
	0x2,0x0,
	0x6,0x0,
	0x9,0x0,
//	0xb,0x0,
	
	0x3,0x1,
	0x7,0x1,
	0xd,0x1,
	0xf,0x1,
	
	0x0,0x2,
	0x4,0x2,
//	0x6,0x2,
	0x8,0x2,
//	0x9,0x2,
	0xb,0x2,
//	0xc,0x2,
	
	0x1,0x3,
//	0x5,0x3,
//	0x7,0x3,
	0xc,0x3,
//	0xd,0x3,
//	0xf,0x3,
	
	0x2,0x4,
	0x6,0x4,
//	0x8,0x4,
//	0x9,0x4,
	0xb,0x4,
	0xc,0x4,
	
//	0x3,0x5,
//	0xc,0x5,
//	0xd,0x5,
//	0xe,0x5,
	0xf,0x5,
	
	0x0,0x6,
	0x4,0x6, //30
	0xb,0x6,

	0x1,0x7,
//	0x3,0x7,
//	0xd,0x7,
	0xf,0x7,

	0x2,0x8,
	0x9,0x8,
	0xa,0x8,
//	0xc,0x8, //ggf. drin lassen

	0x0,0x9,
//	0x2,0x9,
	0x8,0x9,
//	0xb,0x9,

	0x4,0xa,
	0x8,0xa,
//	0xc,0xa,
	0xf,0xa,

//	0x0,0xb,
	0xc,0xb,
	0x6,0xb, //50
	0x2,0xb,
	0x4,0xb,
//	0x9,0xb,

	0xb,0xc,
	0xf,0xc,
//	0x8,0xc,
//	0x5,0xc,
//	0xe,0xc,
//	0xa,0xc,

	0x1,0xd,
	0x5,0xd,
//	0x3,0xd,
//	0x7,0xd,
//	0xf,0xd,

	0x4,0xe,
//	0xc,0xe,
	0xf,0xe,
	0x3,0xe,
//	0xd,0xe,

	0xc,0xf,
	0xa,0xf,
	0x5,0xf,
	0xe,0xf,
	0x1,0xf,
	0x7,0xf,
//	0x3,0xf,
//	0xd,0xf,

	0x0,0x0

};



//c64 palette in RGB
const int rgb_values[] = {
	//0x21,0x21,0x21,
	0x00,0x00,0x00,
	0xff,0xff,0xff,
	0xb5,0x21,0x21,
	0x73,0xff,0xff,
	0xb5,0x21,0xb5,
	0x21,0xb5,0x21,
	0x21,0x21,0xb5,
	0xff,0xff,0x21,
	0xb5,0x73,0x21,
	0x94,0x42,0x21,
	0xff,0x73,0x73,
	0x73,0x73,0x73,
	0x94,0x94,0x94,
	0x73,0xff,0x73,
	0x73,0x73,0xff,
	0xb5,0xb5,0xb5
};

int mode=0;
int bgcols[]={0x0,0x9,0x8,0xa};
int colorlookup[]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
int range_count=0;
int bgcol1;
int bgcol2;
int colramcol;
int nuancen[]={0,0,0,0};
int sizex;
int sizey;
int bpp=3;
const int c64xchars=40;
const int c64ychars=25;
const int ecm_width=80;
const int ecm_height=50;
const int c64xres=320;
const int c64yres=200;
int address=0x1000;
unsigned char pic4bit[320*200];
unsigned char hires_charset[4096];
unsigned char *multicol_charset;
unsigned char *multicol_bitmaps;
int *multicol_charmaps;
int *multicol_resolve_map;
int *multicol_charset_stats;
int *multicol_charset_errors;

int *diffplanes;
int charset_lifetime;	//over how many farmes should the charset be valid?
//unsigned char multicol_charmap[1000];
unsigned char bitmap_block[8];
int multicol_frame_counter;

char* write_name="output.a64";
char* charset_name="chargen";
char* write_mode="w+";
char* read_name;
char* basename;
char* filemask;
char* suffix;
char* prefix;
char* startnumber;
char* read_mode="r";
unsigned char *result;
unsigned char *data;
int filenr;
int framecolramsize;
int framecolramstart;
int framesize;
FILE* fdw;

int load_frame(char* name);
void convert_to_ecm(); 
void convert_to_petscii(); 
void convert_to_multicol(); 
void convert_to_koala(); 

int main(int argc, char **argv) {
	unsigned char temp[128];
	int c;
	int a;
	int b;
	charset_lifetime=0;
	if(argc==1) {
		fprintf(stderr, "Usage: convert -m <mode> -f <firstfile> [-o <output file> -l <lifetime>]\n");
		fprintf(stderr, "  Converts sequence of bmp-files into .a64 format.\n");
		fprintf(stderr, "  The following modes are accepted: petscii, multi, ecm.\n");
		exit(2);
	}
	for(a=1;a<argc;a++) {
		if(strcmp(argv[a],"-f")==0) {
			a++;
			if(argc>a) {
				basename=argv[a];
			}
			else {
				fprintf(stderr, "ERROR: No input file given.\n");
				exit(2);
			}
		}  
		if(strcmp(argv[a],"-l")==0) {
			a++;
			if(argc>a) {
				charset_lifetime=atoi(argv[a]);
				if(charset_lifetime<=0) {
					fprintf(stderr, "ERROR: lifetime must be > 0.\n");
					exit(2);
				}
			}
			else {
				fprintf(stderr, "ERROR: Missing lifetime value.\n");
				exit(2);
			}
		}  
		if (strcmp(argv[a],"-m")==0) {
			a++;
			if(argc>a) {
				if(strcmp(argv[a],"petscii")==0) mode=PETSCII;
				else if(strcmp(argv[a],"multi")==0) mode=MULTICOL_CHARSET;
				else if(strcmp(argv[a],"ecm")==0) mode=ECM_COLOR;
				else if(strcmp(argv[a],"koala")==0) mode=KOALA;
				else {
					fprintf(stderr, "Unknown mode.\n");
					exit(2);
				}

			}
			else {
				fprintf(stderr, "ERROR: No mode given.\n");
				exit(2);
			}
		}
		if (strcmp(argv[a],"-o")==0) {
			a++;
			if(argc>a) {
				write_name=argv[a];
			}
			else {
				fprintf(stderr, "ERROR: No output file given.\n");
				exit(2);
			}
		}
	}
	if(charset_lifetime==0 && mode==MULTICOL_CHARSET) {
		fprintf(stderr, "No charset lifetime set, setting it to default value of 2.\n");
		charset_lifetime=2;
	}

	if(basename==NULL) {
		fprintf(stderr, "ERROR: No input file given.\n");
		exit(2);
	}
	if(mode==0) {
		fprintf(stderr, "ERROR: No mode given.\n");
		exit(2);
	}
		
	for(a=strlen(basename);a>=0;a--) {
		if(basename[a]>='0' && basename[a]<='9') break;
	}
	for(b=a;b>=0;b--) {
		if(basename[b]<'0' || basename[b]>'9') break;
	}
	a++;
	b++;
	
	suffix=(char *)malloc(strlen(basename)-a+1);
	strncpy(suffix, basename+a, strlen(basename)-a);
	
	prefix=(char *)malloc(b+1);
	strncpy(prefix,basename,b);
	
	startnumber=(char *)malloc(a-b+1);
	strncpy(startnumber, basename+b, a-b);
	filenr=atoi(startnumber);
	
	filemask=(char *)malloc(strlen(basename)+1);
	read_name=(char *)malloc(strlen(basename)+1);
	
//	sprintf(filemask,"%s%%0%dd%s", prefix,suffix);

	sprintf(filemask,"%s%%0%dd%s", prefix,a-b,suffix);
	
	fdw = fopen (write_name, write_mode);
	if(fdw==NULL) { fprintf(stderr, "ERROR: can't open output file\n"); exit(0); }
	c=address&255;
	fwrite(&c,1,1,fdw);
	c=address>>8;
	fwrite(&c,1,1,fdw);
	create_structures();
	switch(mode) {
		case ECM_COLOR:
			convert_to_ecm();
		break;
		
		case PETSCII:
			convert_to_petscii();
		break;

		case MULTICOL_CHARSET:
			convert_to_multicol();
		break;

		case KOALA:
			convert_to_koala();
		break;

		default:
			fprintf(stderr,"ERROR: Mode not supported!\n");
			exit (0);
		break;
	}
	fclose(fdw);
	exit (0);
}

int create_structures() {
	int a;
	multicol_frame_counter=0;
	//load petscii charset
	load_charset(charset_name);
	range_count=0;
	//count possible color combinations
	while(color_ranges[range_count*2]!=color_ranges[range_count*2+1]) range_count++;
	range_count--;
	//printf("stat: %d\n",range_count);
	return 0;
}

int write_frame() {
	int c;
	int a;
	for(a=0;a<framesize;a++) { 
		c=result[a];
		fwrite(&c,1,1,fdw);
	}
	for(a=0;a<framecolramsize;a++) {
			c=result[framecolramstart+a];
			fwrite(&c,1,1,fdw);
	}
	for(a=0;a<framecolramstart+framecolramsize;a++) result[a]=0;
}

int load_charset(char* name) {
	FILE* fd;
	int c=0;
	int a=0;
	fd = fopen (name, read_mode);
	if(fd==NULL) { fprintf(stderr, "ERROR: Commodore charsetfile '%s' not found, aborting...\n", name); exit(2); }
	while(a<4096 && fread(&c,1,1,fd)) { hires_charset[a]=c; a++; c=0; }
	fclose(fd);
}


int load_frame(char* name) {
	FILE* fdr;
	int c;
	int x, y;
	int data_start;
	sprintf(read_name, filemask, filenr);
	printf("Loading frame '%s'...\n",read_name);
	fdr = fopen (name, read_mode);
	if(fdr==NULL) { fprintf(stderr,"No more frames found. Finishing...\n"); return -1; }
	c=0; fread(&c,1,1,fdr); if(c!=0x42) { fprintf(stderr,"ERROR: No bmp file!\n"); return -1; }
	c=0; fread(&c,1,1,fdr); if(c!=0x4d) { fprintf(stderr,"ERROR: No bmp file!\n"); return -1; }
	fseek(fdr,8,SEEK_CUR); //skip 8 bytes;
	c=0; fread(&c,1,1,fdr); data_start=(int)c;
	c=0; fread(&c,1,1,fdr); data_start+=(int)c<<8;
	c=0; fread(&c,1,1,fdr); data_start+=(int)c<<16;
	c=0; fread(&c,1,1,fdr); data_start+=(int)c<<24;
	fseek(fdr,4,SEEK_CUR); //skip 4 bytes;
	c=0; fread(&c,1,1,fdr); sizex=(int)c;
	c=0; fread(&c,1,1,fdr); sizex+=(int)c<<8;
	c=0; fread(&c,1,1,fdr); sizex+=(int)c<<16;
	c=0; fread(&c,1,1,fdr); sizex+=(int)c<<24;
	c=0; fread(&c,1,1,fdr); sizey=(int)c;
	c=0; fread(&c,1,1,fdr); sizey+=(int)c<<8;
	c=0; fread(&c,1,1,fdr); sizey+=(int)c<<16;
	c=0; fread(&c,1,1,fdr); sizey+=(int)c<<24;
	fseek(fdr,2,SEEK_CUR); //skip 4 bytes;
	c=0; fread(&c,1,1,fdr); bpp=(int)c;
	c=0; fread(&c,1,1,fdr); bpp+=(int)c*256;
	fseek(fdr,data_start-30,SEEK_CUR); //skip 4 bytes;
	bpp/=8;
	if(bpp!=3) { fprintf(stderr,"ERROR: Colourdepth != 24\n"); return -1; }
#ifdef DEBUG
	printf("data_start: %d\n",data_start);
	printf("sizex: %d\n",sizex);
	printf("sizey: %d\n",sizey);
	printf("bytes per pixel: %d\n",bpp);
#endif
	if(data==NULL) { data = (unsigned char *)malloc(sizex*sizey*bpp); }
	for(y=sizey-1;y>=0;y--) {
		for(x=0;x<sizex*bpp;x++) {
			c=0;fread(&c,1,1,fdr);
			data[x+y*sizex*bpp]=c;
		}
	}
	fclose(fdr);
	filenr++;
}

void convert_to_ecm() {
	int a;
	int b;
	int x;
	int y;
	int conflicts=0;
	int simple_rows1=0;
	int simple_rows2=0;
	int simple_rams=0;
	int temp;
	
	result=(unsigned char *)malloc(0x400*3);
	
	while(load_frame(read_name)!=-1) {
		//now create colorlookup table to know what plane to add (0x0,0x40,0x80,0xc0)
		for(a=0;a<16;a++) colorlookup[a]=0;
		for(a=0;a<4;a++) {
			colorlookup[bgcols[a]]=(a<<6);
		}
		result[3048+0]=bgcols[0];
		result[3048+1]=bgcols[1];
		result[3048+2]=bgcols[2];
		result[3048+3]=bgcols[3];
	
		for(y=0;y<sizey;y+=2) {	
			for(x=0;x<sizex;x+=2) {
				find_best_pair(x,y);
	
				if(colramcol!=-1) result[2048+x/2+y/2*c64xchars]=colramcol;
	
				if(bgcol1!=1) result[x/2+y/2*c64xchars]=colorlookup[bgcol1]+nuancen[1]+nuancen[0]*5;
				else result[x/2+y/2*c64xchars]=nuancen[1]+nuancen[0]*5;
				
				if(bgcol2!=-1) result[1024+x/2+y/2*c64xchars]=colorlookup[bgcol2]+nuancen[3]+nuancen[2]*5;
				else result[1024+x/2+y/2*c64xchars]=nuancen[3]+nuancen[2]*5;
			}
		}
#ifdef DEBUG
		printf("Konflikte: %d\n",conflicts);
		printf("Simplerows 1: %d\n",simple_rows1);
		printf("Simplerows 2: %d\n",simple_rows2);
		printf("Unused rams: %d\n",simple_rams);
#endif
		rle_ecm_frame();
		write_frame();
	}
	free(data);
	free(result);
}

void convert_to_multicol() {
	int x,y;
	int a;
	int error;
	
	multicol_frame_counter=0;

	result=(unsigned char *)calloc((0x400+CHARSETSIZE/charset_lifetime)*charset_lifetime,1);
	multicol_charmaps=(int *)calloc(0x400*charset_lifetime,4);
	multicol_bitmaps=(unsigned char *)calloc(8000*charset_lifetime,1);
	multicol_charset=(unsigned char *)calloc(8000*charset_lifetime,1);
	multicol_charset_stats=(int *)calloc(1000*charset_lifetime,4);
	multicol_charset_errors=(int *)calloc(1000*charset_lifetime,4);
	multicol_resolve_map=(int *)calloc(1000*charset_lifetime,4);

	while(error!=-1) {
	 	while((multicol_frame_counter<charset_lifetime) && (error=load_frame(read_name))!=-1) {	//we get x frames
			rgb_to_multicol_bitmap(multicol_frame_counter);
			multicol_frame_counter++;
		}	
		if(charset_lifetime>1) printf("Processing %d frames...\n",charset_lifetime);
		else  printf("Processing frame...\n");
		multicol_frame_counter=0;
		
		extract_multicol_charset(charset_lifetime);		//get charset over those x frames
		create_multicol_charmaps(charset_lifetime);
		
		framesize=(0x400+CHARSETSIZE/charset_lifetime)*charset_lifetime;
		framecolramstart=framesize;
		framecolramsize=0;
#ifdef DEBUG
		printf("Writing %d frames\n",charset_lifetime);
#endif
		write_frame();
	}
	
	free(multicol_charmaps);
	free(multicol_bitmaps);
	free(multicol_charset);
	free(multicol_charset_stats);
	free(multicol_charset_errors);
	free(multicol_resolve_map);
	free(data);
	free(result);
}


void convert_to_koala() {
	int x,y;
	int a;
	
	result=(unsigned char *)calloc((0x800+0x1f40),1);
	multicol_charmaps=(int *)calloc(0x400*2,4);
	multicol_bitmaps=(unsigned char *)calloc(8000,1);

	if(load_frame(read_name)!=-1) {
		rgb_to_multicol_bitmap(0);
		for(a=0;a<8000;a++) result[a]=multicol_bitmaps[a];
		framesize=0x800+0x1f40;
		framecolramstart=framesize;
		framecolramsize=0;
#ifdef DEBUG
		printf("Writing %d frames\n",charset_lifetime);
#endif
		write_frame();
	}
	
	free(multicol_charmaps);
	free(multicol_bitmaps);
	free(data);
	free(result);
}

int create_multicol_charmaps(int frames, int lastchar) {
	int a;
	int frame;
	for(frame=0;frame<frames;frame++) {
		for(a=0;a<1000;a++) {
			result[CHARSETSIZE+(frame*0x400)+a]=multicol_resolve_map[multicol_charmaps[frame*1000+a]];
		}
	}
	return 0;
}

int condense_multicol_charset(int lastchar) {
	int newpos=0;
	int pos;
	int y;
	int temp;

	for(pos=0;pos<lastchar;pos++) {
		temp=multicol_charset_stats[pos];
		if(temp>0) {
			for(y=0;y<8;y++) result[newpos*8+y]=multicol_charset[pos*8+y];
			multicol_resolve_map[pos]=newpos;
			newpos++;
		}
	}
	for(pos=0;pos<lastchar;pos++) {
		temp=multicol_charset_stats[pos];
		if(temp<0) {
			while(1) {
				temp=0-temp;
				if(multicol_charset_stats[temp]>=0) break; 					//has this char been replaced?
				temp=multicol_charset_stats[temp];						//yes, get alternative char
			}
			multicol_resolve_map[pos]=multicol_resolve_map[temp];
		}
	}
	return 0;
}

void find_favourite(int charpos, int lastchar, int* diff, int* best) {
	int a;
	int temp;
	int yy;
	unsigned char row1, row2;
	
	best[charpos]=0;
	diff[charpos]=-1;

	for(a=0;a<lastchar;a++) {
		if((charpos!=a) && (multicol_charset_stats[a]>0)) {
			temp=0;
			for(yy=0;yy<8;yy++) {
				row1=multicol_charset[a*8+yy];
				row2=multicol_charset[charpos*8+yy];
				temp+=abs( ((row1>>6)&3)-((row2>>6)&3) ); //1<<x makes maybe better results but is slooooooow.
				temp+=abs( ((row1>>4)&3)-((row2>>4)&3) );
				temp+=abs( ((row1>>2)&3)-((row2>>2)&3) );
				temp+=abs( ((row1>>0)&3)-((row2>>0)&3) );
			}
			//is it a better result? if so, store in array
			if((diff[charpos]>temp) || (diff[charpos]<0)) {
				diff[charpos]=temp;
				best[charpos]=a;
			}
		}
	}
	return;
}

int extract_multicol_charset(int charset_lifetime) {
	int y;
	int lastchar=0;
	int bitmappos=0;
	int found_char;
	int x;
	int temp;
	int a;
	
	int killedchars=0;
	int charpos;
	int diff[1000*charset_lifetime];
	int best[1000*charset_lifetime];
	int xx,yy;
	unsigned char row1,row2;
	for(a=0;a<1000*charset_lifetime;a++) { multicol_charset_stats[a]=0; multicol_charset_errors[a]=0; }
	copy_multicol_block_to_charset(lastchar,bitmappos);
	multicol_charset_stats[lastchar]++;
	lastchar++;

	//merge identic chars and build a statistic over all chars
	for(bitmappos=0;bitmappos<(8000*charset_lifetime);bitmappos+=8) {
		found_char=find_multicol_char(lastchar,bitmappos);
		if(found_char!=-1) {
			multicol_charmaps[bitmappos/8]=found_char;
			multicol_charset_stats[found_char]++;
		}
		else {
			multicol_charmaps[bitmappos/8]=lastchar;
			copy_multicol_block_to_charset(lastchar, bitmappos);
			lastchar++;
			multicol_charset_stats[lastchar]++;
		}
	}

#ifdef DEBUG
	printf("lastchar %d\n",lastchar);
	for(y=0;y<lastchar;y++) printf("%d  ",multicol_charset_stats[y]);
	printf("\n");
	printf("Melting down charset");
#endif
	
	/* do preparations, so walk through yet too large charset and find best substitute 
	 * for each char, remember the diff too */ 
	for(charpos=0;charpos<lastchar;charpos++) find_favourite(charpos, lastchar, diff, best);

	//now walk through all chars until it fits into a standard charset of 255 different chars
	while(lastchar-killedchars>256) {
		temp=-1;
		//walk through set and find char with lowest difference
		for(a=0;a<lastchar;a++) {
			//but only take care of chars that were not sorted out already
			if((multicol_charset_stats[a]>0)) {
				if((temp<0) || (diff[a]*multicol_charset_stats[a])<temp) {
					temp=diff[a]*multicol_charset_stats[a];
					found_char=a;
				}
			}
		}

		//found most unworthy char and will kick it now by merging with its favourite
		multicol_charset_stats[best[found_char]]+=multicol_charset_stats[found_char];
		multicol_charset_stats[found_char]=0-best[found_char]; //set negative charpos here to have a pointer to new char
		killedchars++;
		
		//now let's search for all chars that elected our kicked char as favourite
		for(a=0;a<lastchar;a++) {
			if(best[a]==found_char) {
				//find a new favourite for them, so that they don't be sad and alone anymore
				find_favourite(a,lastchar,diff,best);
			}
		}
	}
	condense_multicol_charset(lastchar);
	return 0;
}

int multicol_sort_charset(int lastchar) {
	int x,y,temp,a;
	//now sort the statistic and the charset in the same move
	for(x=0;x<lastchar;x++) {
		for(y=x;y<lastchar;y++) {
			if(multicol_charset_stats[x]<multicol_charset_stats[y]) {
				//swap stat values
				temp=multicol_charset_stats[y];
				multicol_charset_stats[y]=multicol_charset_stats[x];
				multicol_charset_stats[x]=temp;
				//swap also the correspondig char in its position
				for(a=0;a<8;a++) {
					temp=multicol_charset[y*8+a];
					multicol_charset[y*8+a]=multicol_charset[x*8+a];
					multicol_charset[x*8+a]=temp;
				}
			}
		}
	}
	return 0;
}

int find_multicol_char(int lastchar, int bitmappos) {
	int charpos;
	int y;
	for(charpos=0;charpos<lastchar;charpos++) {
		y=0;
		while(y<8 && multicol_bitmaps[bitmappos+y]==multicol_charset[charpos*8+y]) y++;
		if(y==8) return charpos;
	}
	return -1;
}

int copy_multicol_block_to_charset(int charpos, int bitmappos) {
	int y;
	for(y=0;y<8;y++) multicol_charset[charpos*8+y]=multicol_bitmaps[bitmappos+y];
	return 0;
}

int rgb_to_multicol_bitmap(int offset) {
	int x,y,xx,yy;
	int c;
	unsigned char row;
	int dither;
	for(y=0;y<sizey;y+=8) {
		for(x=0;x<sizex;x+=8) {
			for(yy=0;yy<8;yy++) {
				row=0;
				for(xx=0;xx<8;xx+=2) {
					row=row<<2;
					c=data[(x+xx)*bpp+(y+yy)*sizex*bpp+2];
					c+=data[(x+xx)*bpp+(y+yy)*sizex*bpp+1];
					c+=data[(x+xx)*bpp+(y+yy)*sizex*bpp+0];
					c=c/3;
					c=(int)((float)c/((float)256)*(float)7); ///7 scheisse sein tun
//					c=(int)((float)1/((float)256/(float)7)); ///7 scheisse sein tun
					//now we have 2 bit grayscale
					
					if(c&1==1) { //mischfarbe, also dithern
						dither=((xx/2)&1)^(yy&1);	//kreuzmuster
					}
					else {
						dither=0;
					}
					row=row | ((c>>1)+dither);
				}
				multicol_bitmaps[y*c64xchars+x+yy+offset*8000]=row^0xff; //$d800 should get color 00!
			}
#ifdef DEBUG
			if(row>15) printf("%x",row);
			else printf("0%x",row);
#endif
		}
#ifdef DEBUG
		printf("\n");
#endif
	}
	return 0;
}

int best_color_for_multi_block(int xoff,int yoff, int bgcol1, int bgcol2, int bgcol3) {
	int x,y;
	int temp=-1;
	int color=0;
	int best_color=0;
	int stat[]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	int c;
	for(y=0;y<8;y++) {
		for(x=0;x<8;x+2) {
			c=pic4bit[x+xoff+(y+yoff)*sizex];
			stat[c]++;
		}
	}
	for(color=0;color<16;color++) {
		if(color!=bgcol1 && color!=bgcol2 && color!=bgcol3) {
			if(temp==-1 || temp<stat[color]) { temp=stat[color]; best_color=color; }
		}
	}
	return best_color;
}


void convert_to_petscii() {
	int x,y;
	int fgcol, bgcol;
	int bchar;
	
	result=(unsigned char *)malloc(0x400*2);
	diffplanes=(int *)malloc(320*200*16*4);
	
	while(load_frame(read_name)!=-1) {
		printf("Encoding frame '%s'...\n",read_name);
	
		printf("sizex:%d sizey:%d\n",sizex,sizey);
		bgcol=find_bgcol_petscii();
		
		for(y=0;y<sizey;y+=8) {
			for(x=0;x<sizex;x+=8) {
	//			printf("x:%d y:%d\n",x,y);
				fgcol=best_color_for_hires_block(x,y,bgcol);
				bchar=find_best_hires_char(x,y,fgcol,bgcol);
				result[y/8*c64xchars+x/8]=bchar;
				result[(y/8*c64xchars+x/8)+0x400]=fgcol;
#ifdef DEBUG
				printf("%x",fgcol);
#endif
			}
#ifdef DEBUG
			printf("\n");
#endif
		}
#ifdef DEBUG
		printf("bgcol: %d\n",bgcol);
#endif
		framesize=2048;
		framecolramstart=framesize;
		framecolramsize=0;
		result[2047]=bgcol;
		write_frame();
	}
	free(diffplanes);
	free(data);
	free(result);
}

int find_best_hires_char(int x, int y, int fgcol, int bgcol) {
	int fcol;
	int bdiff,fdiff,diff;
	int best_char=0;
	int temp=-1;
	int btemp,ftemp;
	int a,xx,yy;
	int r1,g1,b1;
	int rfg,gfg,bfg;
	int rbg,gbg,bbg;
	int fgdiff, bgdiff;
	
	unsigned char row1, row2;
	//walk through charset
//	rfg = rgb_values[fgcol*3];
//	gfg = rgb_values[fgcol*3+1];
//	bfg = rgb_values[fgcol*3+2];
	
//	rbg = rgb_values[bgcol*3];
//	gbg = rgb_values[bgcol*3+1];
//	bbg = rgb_values[bgcol*3+2];
	
	//XXX walk through all 16 cols and maybe redefine fgcol?
//	for(fcol=0;fcol<16;fcol++) {
		for(a=0;a<256;a++) {
		//for(a=102;a<103;a++) {
			diff=0; fdiff=0; bdiff=0;
			//walk through block
			for(yy=0;yy<8;yy++) {
				row1=hires_charset[a*8+yy];
				for(xx=0;xx<8;xx++) {
				//	r1=data[(x+xx)*bpp+(y+yy)*sizex*bpp+2];
				//	g1=data[(x+xx)*bpp+(y+yy)*sizex*bpp+1];
				//	b1=data[(x+xx)*bpp+(y+yy)*sizex*bpp+0];
			//		if((row1&0x80)!=0) diff+=sqrt((r1-rfg)*(r1-rfg)+(g1-gfg)*(g1-gfg)+(b1-bfg)*(b1-bfg));
			//		else diff+=sqrt((r1-rbg)*(r1-rbg)+(g1-gbg)*(g1-gbg)+(b1-bbg)*(b1-bbg));
			//		//XXX calc brightness over whole char: und source
			//		//char der n�her dran:  gewinnt
			//		//brightness diff auf diff aufadieren
			//		br+=diffplanes[((y+yy)*sizex+x+xx)+(*fgcol*sizex*sizey)]-diffplanes[((y+yy)*sizex+x+xx)+(*bgcol*sizex*sizey)]
					if((row1&0x80)!=0) {
						fdiff+=diffplanes[((y+yy)*sizex+x+xx)+(fgcol*sizex*sizey)];
					}
					else {
						bdiff+=diffplanes[((y+yy)*sizex+x+xx)+(bgcol*sizex*sizey)];
					}
					row1=row1<<2;
					//if(diff>temp && temp!=-1) { break; break; }	//skip, as we lost already
				}
			}
			
			//if we found a better char, remember
			diff=(fdiff+bdiff)+abs(fdiff-bdiff);
//			if( (abs(fdiff-bdiff)<500)) {
//				temp=diff; best_char=a; 
//				btemp=bdiff;
//				ftemp=fdiff;
//				a=102;
//				break;
//			}
			if((temp==-1) || (temp>=diff)) { 
				temp=diff; best_char=a; 
				btemp=bdiff;
				ftemp=fdiff;
				
				/*fgcol=(int*)fcol;*/ 
			}
		}
		
//	}
	
	return best_char;
}

int find_bgcol_petscii() {
	//make statistic over all blocks
	//blocks that contain only one color don't count into statistic for backgroundcolor. 
	//We only want those blocks thet contain a backgroundcolor together with other colors, as we can work out details here with chars
	//ergo: What we seek for is the color that is most used and only in combination with others. The empty blocks can later on be easily displayed with char $a0
	int x,y,xx,yy;
	int r1,g1,b1;
	int r2,g2,b2;
	int diffzw[]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	int diff[]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	int temp=-1;
	int result;
	int c;
	int a;
	int col=0;

	for(y=0;y<sizey;y+=8) {
		for(x=0;x<sizex;x+=8) {
			//count in blocks, now step pixelwise through block
			for(yy=0;yy<8;yy++) {
				for(xx=0;xx<8;xx++) {
					r1=data[(x+xx)*bpp+(y+yy)*sizex*bpp+2];
					g1=data[(x+xx)*bpp+(y+yy)*sizex*bpp+1];
					b1=data[(x+xx)*bpp+(y+yy)*sizex*bpp+0];
					temp=-1;
					//see what color fits this pixel best
					for(a=0;a<16;a++) {
						r2 = rgb_values[a*3];
						g2 = rgb_values[a*3+1];
						b2 = rgb_values[a*3+2];

						c=sqrt((r1-r2)*(r1-r2)+(g1-g2)*(g1-g2)+(b1-b2)*(b1-b2));
						diffplanes[((y+yy)*sizex+x+xx)+(a*sizex*sizey)]=c;
						if(temp==-1 || c<=temp) { temp=c; col=a; } 
					}
					//found perfect color, add to blockstatistic and 4bit map
					diffzw[col]++;
					pic4bit[(x+xx)+(y+yy)*sizex]=col;
				}
			}
#ifdef DEBUG
			printf("%x ",pic4bit[x+y*sizex]);
#endif
			//now lets see if we only found one color (a block that we display with $a0
			for(a=0;a<16;a++) {
				if(diffzw[a]<64) { diff[a]+=diffzw[a]; }	//no, so add block to picture statistic
				diffzw[a]=0;					//indeed
			}
		}
#ifdef DEBUG
		printf("\n");
#endif
	}
	temp=-1;
	//finished with blocks
	//now select the best color by statistic
	for(a=0;a<16;a++) {
		if(diff[a]>temp || temp==-1) { temp=diff[a]; result=a; }
#ifdef DEBUG
		printf("%x  ",diff[a]);
#endif
	}
#ifdef DEBUG
	printf("\n");
#endif
//	result=15;
	return result;
}


int best_color_for_hires_block(int xoff,int yoff, int bgcol) {
	int x,y;
	int temp=-1;
	int color=0;
	int best_color=0;
	int stat[]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	int c;
	//make statistic of block
	for(c=0;c<16;c++) {
		for(y=0;y<8;y++) {
			for(x=0;x<8;x++) {
				stat[c]+=diffplanes[((yoff+y)*sizex+xoff+x)+(c*sizex*sizey)];
			}
		}
	}
	//pick out winner
	for(c=0;c<16;c++) {
		if(c!=bgcol) {
			if(temp==-1 || temp>stat[c]) { temp=stat[c]; best_color=c; }
		}
	}
	return best_color;
}

int find_best_pair(int x, int y) {
	int a,b;
	int diff1;
	int diff2;
	int diff3;
	int diff4;
	int best_col_diff=0x7fffffff;
	int best_bg1_diff=0x7fffffff;
	int best_bg2_diff=0x7fffffff;
	int best_pair[]={0,0,0,0};
	int c;
	int temp;
	colramcol=-1;
	bgcol1=-1;
	bgcol2=-1;
	for(a=0;a<range_count;a++) {
		temp=color_ranges[a*2+1];
		if(temp==bgcols[0] || temp==bgcols[1] || temp==bgcols[2] || temp==bgcols[3]) {
			for(b=0;b<16;b++) {
				if(color_ranges[a*2]==b) {
					diff1=calc_pair(x,y,a);
					diff2=calc_pair(x+1,y,a);
					diff3=calc_pair(x,y+1,a);
					diff4=calc_pair(x+1,y+1,a);
					if(diff1+diff2+diff3+diff4<best_col_diff) { 
						best_col_diff=diff1+diff2+diff3+diff4; colramcol=b; 
						best_pair[0]=a;
						best_pair[1]=a;
						best_pair[2]=a;
						best_pair[3]=a;
					}
					for(c=0;c<4;c++) {
						if(color_ranges[a*2+1]==bgcols[c]) { 
							if(diff1+diff2<best_bg1_diff) { 
								best_bg1_diff=diff1+diff2; bgcol1=bgcols[c]; 
								best_pair[0]=a;
								best_pair[1]=a;
							}
							if(diff3+diff4<best_bg2_diff) { 
								best_bg2_diff=diff3+diff4; bgcol2=bgcols[c]; 
								best_pair[2]=a;
								best_pair[3]=a;
							}
						}
					}
				}
			}
		}
	}
	nuancen[0]=find_offset(x,y,best_pair[0]);
	nuancen[1]=find_offset(x+1,y,best_pair[1]);
	nuancen[2]=find_offset(x,y+1,best_pair[2]);
	nuancen[3]=find_offset(x+1,y+1,best_pair[3]);
	return colramcol;
}

int stat_pic(int color) {
	int x,y;
	int r1,g1,b1;
	int r2,g2,b2;
	int diff=0;

	r2 = rgb_values[color*3];
	g2 = rgb_values[color*3+1];
	b2 = rgb_values[color*3+2];

	for(x=0;x<sizex;x++) {
		for(y=0;y<sizey;y++) {
			r1=data[x*bpp+y*sizex*bpp+2];
			g1=data[x*bpp+y*sizex*bpp+1];
			b1=data[x*bpp+y*sizex*bpp+0];

			diff+=sqrt((r1-r2)*(r1-r2)+(g1-g2)*(g1-g2)+(b1-b2)*(b1-b2));
		}
	}
	return diff;
}


int calc_pair(int x, int y, int pair) {
	int r1,g1,b1;
	int r2,g2,b2;
	int r3,g3,b3;
	int act_dist;
	int end_dist;
	int start_dist;
	int diff;
	
	r3=data[x*bpp+y*sizex*bpp+2];
	g3=data[x*bpp+y*sizex*bpp+1];
	b3=data[x*bpp+y*sizex*bpp+0];
	r1 = rgb_values[color_ranges[pair*2]*3];
	g1 = rgb_values[color_ranges[pair*2]*3+1];
	b1 = rgb_values[color_ranges[pair*2]*3+2];
	r2 = rgb_values[color_ranges[pair*2+1]*3];
	g2 = rgb_values[color_ranges[pair*2+1]*3+1];
	b2 = rgb_values[color_ranges[pair*2+1]*3+2];

	act_dist=sqrt((r1-r2)*(r1-r2)+(g1-g2)*(g1-g2)+(b1-b2)*(b1-b2));
	start_dist=sqrt((r1-r3)*(r1-r3)+(g1-g3)*(g1-g3)+(b1-b3)*(b1-b3));
	end_dist=sqrt((r2-r3)*(r2-r3)+(g2-g3)*(g2-g3)+(b2-b3)*(b2-b3));

	diff=start_dist+end_dist-act_dist;

	return diff;
}

int find_offset(int x, int y, int best) {
        float ar,ag,ab;
        float br,bg,bb;
        float cr,cg,cb;
	float wr,wg,wb;
	float vr,vg,vb;
	float yr,yg,yb;
	float t;
	float d;
	float diff;
	float ac_dist;
	float ab_dist;
	float bc_dist;
	float cd_dist;
	float ad_dist;
	float bd_dist;

        cr=data[x*bpp+y*sizex*bpp+2];
        cg=data[x*bpp+y*sizex*bpp+1];
        cb=data[x*bpp+y*sizex*bpp+0];

        ar = rgb_values[color_ranges[best*2]*3];
        ag = rgb_values[color_ranges[best*2]*3+1];
        ab = rgb_values[color_ranges[best*2]*3+2];
        br = rgb_values[color_ranges[best*2+1]*3];
        bg = rgb_values[color_ranges[best*2+1]*3+1];
        bb = rgb_values[color_ranges[best*2+1]*3+2];

	vr=br-ar;
	vg=bg-ag;
	vb=bb-ab;

	wr=cr-ar;
	wg=cg-ag;
	wb=cb-ab;

	yr=cr-br;
	yg=cg-bg;
	yb=cb-bb;

	ac_dist=sqrt(wr*wr+wg*wg+wb*wb);
	ab_dist=sqrt(vr*vr+vg*vg+vb*vb);
	bc_dist=sqrt(yr*yr+yg*yg+yb*yb);
	

	t=(wr*vr+wg*vg+wb*vb)/(vr*vr+vg*vg+vb*vb);

	wr=wr-t*vr;
	wg=wg-t*vg;
	wb=wb-t*vb;
	
	cd_dist=sqrt(wr*wr+wg*wg+wb*wb);

	ad_dist=sqrt(ac_dist*ac_dist-d*d);
	bd_dist=sqrt(bc_dist*bc_dist-d*d);
	diff=ad_dist;
	if(ad_dist>ab_dist) diff=ab_dist; //schnittpunkt �ber B!
	if(bd_dist>ab_dist) diff=0; //schnittpunkt unterhalb A!
	
	diff=diff/ab_dist*4.0f;

	if(diff<0 || t<0) diff=0;
	if(diff>4 || t>1) diff=4;
        return 4-((int)(diff+0.5f));
}

int rle_ecm_frame() {
	int dest=0;
	int a;
	int temp;
	int count=0;
	framesize=2048;
	framecolramsize=1024;
	temp=result[2048];
	for(a=0;a<1024;a=a) {
		count=0;
		while(count<15 && result[2048+a+count+1]==temp && (a+count+1)<1024) count++;
		result[2048+dest]=(count<<4)|temp;
		a+=count+1;
		temp=result[2048+a];
		dest++;
	}
	framecolramsize=dest;
	framecolramstart=2048;
/*
	//now also rle the rest of the frame
	count=0;
	dest=0;
	temp=result[0];
	for(a=0;a<2048;a++) {
		count=0;
		while(count<31 && result[a+count+1]==temp && (a+count+1)<2048) count++;
		if(count>=2) {
			result[dest]=(count+0xe0);
			result[dest+1]=temp;
			dest+=2;
			a+=count;
			temp=result[a];
		}
		else {
			result[dest]=result[a];
			temp=result[a+1];
			dest++;
		}
	}
	framesize=dest;
	*/
//	printf("size of bgcols: %x\n",framesize);
	return 0;
}








