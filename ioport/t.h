#ifndef _T_H_
#define _T_H_

#define DRV_READ_IO_8 'r'
#define DRV_WRITE_IO_8 'w'
#define DRV_READ_IO_16 'r16'
#define DRV_WRITE_IO_16 'w16'

typedef struct IO_Tuple
{
	uint32 Port;
	uint8  Data;
	uint16 Data16;
} IO_Tuple;

#endif