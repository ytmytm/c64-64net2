/*
   DOS Error message thingy 

 */

#include "config.h"
#include "client-comm.h"

void set_drive_status (unsigned char*, int);

int
set_error (int en, int t, int s)
{
  /* display a DOS error message */

  uchar temp[256];
  char mesg[81][32] =
  {
    "OK",
    "FILES SCRATCHED",
    "PARTITION SELECTED",
    "DIR X-LINK FIXED",
    "FILE X-LINK FIXED",
    "05", "06", "07", "08", "09", "10", "11", "12", "13", "14", "15", "16", "17",
    "18", "19",
    "READ ERROR",
    "READ ERROR",
    "READ ERROR",
    "READ ERROR",
    "READ ERROR",
    "WRITE ERROR",
    "WRITE PROTECT ON",
    "READ ERROR",
    "WRITE ERROR",
    "DISK ID MISMATCH",
    "SYNTAX ERROR",
    "SYNTAX ERROR",
    "SYNTAX ERROR",
    "SYNTAX ERROR",
    "SYNTAX ERROR",
    "35", "36", "37",
    "OPERATION UNIMPLEMENTED",
    "DIR NOT FOUND",
    "40", "41", "42", "43", "44", "45", "46", "47",
    "ILLEGAL JOB",
    "49",
    "RECORD NOT PRESENT",
    "OVERFLOW IN RECORD",
    "FILE TOO LARGE",
    "53", "54", "55", "56", "57", "58", "59",
    "WRITE FILE OPEN",
    "FILE NOT OPEN",
    "FILE NOT FOUND",
    "FILE EXISTS",
    "FILE TYPE MISMATCH",
    "NO BLOCK",
    "ILLEGAL BLOCK",
    "ILLEGAL BLOCK",
    "68", "69",
    "NO CHANNEL",
    "DIRECTORY ERROR",
    "PARTITION FULL",
    "64NET/2 (C) 1996",
    "DRIVE NOT READY",
    "FILE SYSTEM INCONSISTANT",
    "MEDIA TYPE MISMATCH",
    "ILLEGAL PARTITION",
    "RECURSIVE FILESYSTEM",
    "INVALID MEDIA",
    "END OF FILE"
  };

  if ((en < 20) || (en == 73) || (en == 80))
    client_error (0);
  else
    client_error (1);

  sprintf ((char*)temp, "%02d, %s,%02d,%02d\r", en, mesg[en], t, s);
  //debug_msg ("%s\n", temp);

  /* set the message into the real status variable */
  set_drive_status (temp, strlen (temp));
  return (0);

}
