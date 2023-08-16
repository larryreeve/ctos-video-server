/*******************************************************************************
Note: The following conditionals affect compilation:

        STVER   - Generate Conventional Memory Version
        XMVER   - Generate Extended Memory Version
        BARCODE - Include DataComm Routines
*******************************************************************************/

#include <ctype.h>
#include <dos.h>
#include <stdlib.h>
#include <string.h>

#include "vks.h"

#ifdef XMVER
#include "xmslib.h"
#endif


/*******************************************************************************
                   External Variable Settings
*******************************************************************************/
extern  WORD    _stklen             = 512;                  /* Stack Size     */
extern  WORD    _heaplen            = 0x01;
extern  int     directvideo         = 0x00;                 /* 0=BIOS Output  */


/*******************************************************************************
                    Global Variable Settings
*******************************************************************************/
static  BYTE    gfHelp              = FALSE;            /* FLAG - Display Help            */
static  BYTE    gfLoadHigh          = FALSE;            /* FLAG - Server Loaded High      */
static  BYTE    gfRemove            = FALSE;            /* FLAG - Remove Server           */
static  BYTE    gfColor             = FALSE;            /* FLAG - Monitor is Color        */
static  BYTE    gfSkip101           = FALSE;            /* FLAG - Skip 101 Kbd Check      */
static  BYTE    gfBoldBitOn         = FALSE;            /* FLAG - Bold Bit is Always On   */
static  BYTE    gfCapsOn            = FALSE;            /* FLAG - Turn CapsLock On        */
/* static  BYTE    gfPrtSc             = TRUE;                FLAG - DO NOT make PrtSc Copy  */
static  BYTE    gDVMajor            = 0;                /* Desqview Release #             */
static  BYTE    gDVMinor            = 0;                /* Desqview Point Release #       */
static  BYTE    gVideoMode          = modeTRIDENT;      /* Mode to get 80x30 columns      */
static  BYTE    gsVideoMode         = 0x00;             /* Mode on Entry to Server        */
static  BYTE    gMapDepth           = 0xFF;             /* Stack Pointer to MuMap   Calls */
static  BYTE    gLastShiftState     = 0xFF;             /* Tracks Keyboard Indicators     */
static  BYTE    gCTOSAttrMap[29][80];                   /* Unconverted CTOS Attributes    */
static  BYTE    gColorPalette[8]    = {0x0C,0x08,0x0F,0x0A,0x3F,0x2A,0x3C,0x30};

static  BYTE    gIntNum             = DEFINT;           /* Interrupt Number               */
static  WORD    gStationNum         = 0;                /* CTOS Station Number            */
static  WORD    gPspSeg             = 0;                /* Save PSP Segment for Release   */
static  WORD    gVideoSegment       = 0xB000;           /* Start  of PC Mono  Memory      */
static  WORD    gVideoOffset        = 0x0000;           /* Offset of PC Mono  Memory      */

static  char    gStationName[13]    = "No Name";        /* CTOS User Name                 */
static  char    gDTString[32];                          /* Date and Time Formatted String */

static  DWORD   gNbrRqServed        = 0;                /* Number of Requests Serviced    */
static  DWORD   gMemSize            = 0;                /* Size of server in memory       */
static  ADSWORD gEnvSeg;                                /* Save Env Segment for Release   */
static  ADSBYTE gInDOSPtr;                              /* Pointer to InDOS Flag          */

static  WORD    gHeapSP             = 0;

#ifdef STVER
static  BYTE    gSaveScreen[FULLSCREENSIZE];            /* Saved screen for MuSuspend     */
static  BYTE    gMapArray[MAXVIDMAPS * VIDMAPSIZE];     /* Saved Screen Maps              */
static  BYTE    gHeap[MUARRAYSIZE];                     /* MuArray Calls Heap Area        */
#endif

#ifdef XMVER
static  WORD    gXHScreen         = 0;
static  WORD    gXHMap            = 0;
static  WORD    gXHHeap           = 0;
#endif

static  void    interrupt  (*old_int_func) ();          /* Old Interrupt Vector           */
/************************
static  void    interrupt  (*old_int_5)    ();
************************/
static  void    interrupt  (*old_int_1C)   ();          /* Old Timer Interrupt            */
static  void    interrupt  (*old_int_9)    ();          /* Old Keyboard Interrupt         */

static  struct  VideoControlBlock          gVCB;        /* Global Video Control Block     */
static  struct  ColorControlBlock          gClrCtrl;    /* Color Control Block            */
static  struct  VidStateStruct             gVidState;   /* Video mode BEFORE ResetVideo   */
static  struct  VidStateStruct             gMuVidState; /* To track MuSuspend/Resume      */

static  char    gDays[7][4]         = {"Sun", "Mon", "Tue", "Wed",
                                       "Thu", "Fri", "Sat"};
static  char    gMonths[12][4]      = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                       "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};


/* CTOS -> Lower ASCII Conversion */
static  BYTE    gNonIBMTran[256]    =
                            {
 ' ', ' ', ' ', '*', '*', '*', '*', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',

 ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',

 ' ', '!','\"', '#', '$', '%', '&','\'', '(', ')', '*', '+', ',', '-', '.', '/',

 '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ':', ';', '<', '=', '>', '?',

 '@', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',

 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '[','\\', ']', '^', '-',

'\'', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',

 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '{', '|', '}', '~', '',

 'c', 'u', 'e', 'a', 'a', 'a', 'a', 'c', 'e', 'e', 'e', 'i', 'i', 'i', 'A', 'A',

 'E', '@', '@', 'o', 'o', 'o', 'u', 'u', 'y', 'o', 'u', 'c', 'L', 'Y', 'P', 'f',

 'a', 'i', 'o', 'u', 'n', 'N', 'a', 'o', '?', '+', '+', '2', '4', 'i', '<', '>',

 '@', '@', '@', '|', '+', '+', '+', '+', '+', '+', '|', '+', '+', '+', '+', '+',

 '+', '+', '+', '+', '-', '+', '+', '+', '+', '+', '+', '+', '+', '=', '+', '+',

 '+', '+', '+', '+', '+', '+', '+', '+', '+', '+', '+', '-', '-', '|', '|', '-',

 'a', 'b', 'r', 'n', 'E', '@', '@', 'T', '@', '@', '@', '@', '@', 'o', 'e', 'n',

 '=', '+', '>', '<', '@', '@', '+', '=', 'o', '-', '-', '@', 'n', '@', '@', '@'
                            };


/* IBM -> CTOS Conversion */
static  BYTE    gIBMTran[256]   =
                            {
        0x20, 0x18, 0x20, 0x9B, 0xFE, 0xFE, 0xAB, 0x20,
        0x11, 0x10, 0x20, 0x19, 0x20, 0x20, 0x1B, 0x12,
        0xAC, 0x17, 0x1A, 0x20, 0x20, 0xF6, 0xB3, 0x15,
        0x20, 0x20, 0x14, 0x08, 0xAA, 0xF3, 0xF1, 0xF2,
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
        0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
        0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
        0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
        0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
        0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
        0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
        0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
        0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
        0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
        0x20, 0x7C, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35,
        0x36, 0x37, 0x38, 0x39, 0x30, 0x31, 0x32, 0x33,
        0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0x31,
        0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
        0x8F, 0x85, 0x8E, 0x84, 0x94, 0x95, 0xED, 0xED,
        0x9A, 0x81, 0x80, 0x88, 0x82, 0x8A, 0x92, 0x91,
        0xE1, 0x9C, 0xF8, 0x20, 0x20, 0x20, 0x7C, 0x31,
        0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
        0xC7, 0xB6, 0xD7, 0xCE, 0xCC, 0xB9, 0xBA, 0xB9,
        0xCC, 0xD1, 0xCF, 0xD8, 0xC8, 0xCA, 0xCD, 0xCA,
        0xCB, 0xC7, 0xB6, 0xBA, 0xD7, 0xCE, 0xD1, 0xCF,
        0xCD, 0xD8, 0xC4, 0xC5, 0xC2, 0xC1, 0xD0, 0xD2,
        0xC8, 0xB3, 0xC3, 0xB4, 0xC9, 0xC6, 0xB5, 0xBC,
        0xBB, 0xC8, 0xBC, 0xC9, 0xBB, 0xC0, 0xD9, 0xDA,
        0xBF, 0xCC, 0xCB, 0xB9, 0xCA, 0xCC, 0xCB, 0xB9,
        0xCA, 0xCC, 0xCB, 0xB9, 0xCA, 0xB0, 0xB1, 0xB2,
                            };


volatile DWORD  gDTTicks            =    0;
volatile DWORD  gDTWait             = 1092;             /* 1 minute                    */
volatile BYTE   gfDTOn              = FALSE;            /* FLAG - Date/Time Display ON */

volatile DWORD  gScrOutTicks        =    0;
volatile DWORD  gScrOutWait         = 1092;             /* 1 minute                    */
volatile BYTE   gfScrOutOn          = FALSE;            /* FLAG - Timeout is activated */
volatile BYTE   gfScreenOff         = FALSE;            /* FLAG - Video is ON          */

volatile BYTE   gfKbdBusy           = FALSE;


/*/////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//
//                     BEGIN DOS API SECTION
//
///////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////*/
void DOS_GetDate(ADSWORD Year, ADSWORD Month, ADSWORD Day, ADSWORD DoW)
{
   _AH = 0x2A;          /* Function Number          */
   geninterrupt(0x21);  /* General DOS Interrupt    */
   *DoW      = _AL;     /* Day Of Week (0 - 6)      */
   *Year     = _CX;     /* 4-digit Year             */
   *Month    = (_DH - 1);   /* Month, 1 relative    */
   *Day      = _DL;     /* Day Of Month             */
}


ADSBYTE DOS_GetInDOSPtr(void)
{
    _AH = 0x34;
    geninterrupt(0x21);
    return ((ADSBYTE) MK_FP(_ES,_BX));
}


void DOS_GetTime(ADSWORD Hour, ADSWORD Minute, ADSWORD Second)
{
   _AH = 0x2C;          /* Function Number          */
   geninterrupt(0x21);  /* General DOS Interrupt    */
   if (! (_FLAGS & CARRYFLAG))
     {
     *Hour     = _CH;
     *Minute   = _CL;
     *Second   = _DL;
     }
}


void DOS_Pause(void)
{
    geninterrupt(0x28);         /* Call DOS Idle Interrupt              */
    _AX = 0x1680;               /* Call OS/2 and Windows Enhanced Mode  */
    geninterrupt(0x2F);         /* Release TimeSlice Call               */
}


/*/////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//
//                     BEGIN BIOS API SECTION
//
///////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////*/
WORD BIOS_GetActiveMode(void)
{
    _AH = 0x0F;         /* Function Number          */
    geninterrupt(0x10); /* General BIOS Interrupt   */
    return(_AL);
}


WORD BIOS_GetActivePage(void)
{
    _AH = 0x0F;         /* Function Number          */
    geninterrupt(0x10); /* General BIOS Interrupt   */
    return(_BH);
}


void BIOS_GetCursorPos(ADSWORD iLine, ADSWORD iCol)
{
    _AH = 0x03;
    _BH = VIDEOPAGE;
    geninterrupt(0x10);
    *iLine = (WORD) _DH;
    *iCol  = (WORD) _DL;
}


WORD BIOS_IsKeyReady(void)
{
    _AH = 0x11;
    geninterrupt(0x16);
    if ((_FLAGS & ZEROFLAG))
      return (FALSE);
    else
      return (TRUE);
}


void BIOS_PosCursor(WORD iLine, WORD iCol)
{
    _AH = 0x02;         /* Function Number          */
    _BH = VIDEOPAGE;    /* BIOS Video Page          */
    _DH = iLine;        /* Starting Line            */
    _DL = iCol;         /* Starting Column          */
    geninterrupt(0x10); /* General BIOS Interrupt   */
}


void BIOS_ReadExtKey(ADSBYTE KeyAH, ADSBYTE KeyAL)
{
    _AH = 0x10;
    geninterrupt(0x16);
    *KeyAH = _AH;
    *KeyAL = _AL;
}


void BIOS_SetActivePage(WORD page)
{
    _AH = 0x05;         /* Function Number          */
    _AL = page;         /* BIOS Video Page          */
    geninterrupt(0x10); /* General BIOS Interrupt   */
}


void BIOS_SetCursorSize(BYTE StartScanLine, BYTE EndScanLine)
{
    _AH = 0x01;         /* Function Number          */
    _CH = StartScanLine;
    _CL = EndScanLine;
    geninterrupt(0x10); /* General BIOS Interrupt   */
}


void BIOS_CursorOff(void)
{
    BIOS_SetCursorSize(32,32);
}


void BIOS_CursorOn(void)
{
    ADSBYTE ShiftFlagPtr;

    ShiftFlagPtr = (ADSBYTE) MK_FP(0x0000,0x0417);
    if (*ShiftFlagPtr & 0x80) /* Insert On */
      {
      if (gfColor == TRUE)
        BIOS_SetCursorSize(4,7);
      else
        BIOS_SetCursorSize(6,13);
      }
    else
      {
      if (gfColor == TRUE)
        BIOS_SetCursorSize(6,7);
      else
        BIOS_SetCursorSize(12,13);
      }
}


BYTE BIOS_GetKeyFlags(void)
{
    _AH = 0x02;
    geninterrupt(0x16);
    return(_AL);
}


BYTE BIOS_GetExtKeyFlags(void)
{
    _AH = 0x12;
    geninterrupt(0x16);
    return(_AL);
}


void BIOS_SetActiveMode(WORD mode)
{
    _AH = 0x00;         /* Function Number          */
    _AL = mode;         /* Video Mode               */
    geninterrupt(0x10); /* General BIOS Interrupt   */
}


WORD BIOS_StuffExtKey(BYTE KeyCH, BYTE KeyCL)
{
    _CH = KeyCH;
    _CL = KeyCL;
    _AH = 0x05;
    geninterrupt(0x16);
    if (_AL == 0)
      return (TRUE);
    else
      return (FALSE);
}


void BIOS_VideoOff(void)
{
    _AH = 0x12;         /* Function Number          */
    _AL = 0x01;         /* Turn OFF Video = 0x01; Turn ON Video = 0x00 */
    _BL = 0x36;
    geninterrupt(0x10); /* General BIOS Interrupt   */
}


void BIOS_VideoOn(void)
{
    _AH = 0x12;         /* Function Number          */
    _AL = 0x00;         /* Turn OFF Video = 0x01; Turn ON Video = 0x00 */
    _BL = 0x36;
    geninterrupt(0x10); /* General BIOS Interrupt   */
}


void BIOS_WriteChar(BYTE ch, BYTE attr, WORD len)
{
    _AH = 0x09;         /* Function Number          */
    _BH = VIDEOPAGE;    /* BIOS Video Page          */
    _AL = ch;           /* Character to Write       */
    _BL = attr;         /* Attribute Byte           */
    _CX = len;          /* Repeat Counter           */
    geninterrupt(0x10); /* General BIOS Interrupt   */
}


void BIOS_WriteString(WORD iLine, WORD iCol, ADSCHAR pString, WORD attr)
{
    for (; *pString != '\0'; pString++)
      {
      BIOS_PosCursor(iLine,iCol);
      BIOS_WriteChar((BYTE) *pString,attr,1);
      iCol ++;
      if (iCol > 79)
        {
        iLine ++;
        iCol = 0;
        }
      }
}


/*/////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//
//                     BEGIN DESQVIEW API SECTION
//
///////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////*/
WORD DV_IsDesqviewPresent(void)
{
    _CX = 'DE';
    _DX = 'SQ';
    _AX = 0x2B01;
    geninterrupt(0x21);
    if (_AL == 0xFF)
      return (FALSE);
    else
      {
      gDVMajor = _AH;
      gDVMinor = _AL;
      return (TRUE);
      }
}



/*//////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//
//                  BEGIN MISCELLANEOUS FUNCTION SECTION
//
////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////*/
void GetDateTimeString();       /* FORWARD DECLARATION */
void DisplayDateTime();         /* FORWARD DECLARATION */

void SayLocus(char * LocStr)
{
    BIOS_WriteString(29,0,"                                                         ",0x07);
    BIOS_WriteString(29,0,(ADSCHAR) LocStr,0x07);
}


BYTE ConvertAttribute(WORD CTOSAttr)
/*****************************************************************************
                        Bit         Meaning Mono        Meaning Color
                        ---         ------------        -------------
CTOS Attribute BYTE:     0          Half-Bright         Palette Index 0
                         1          Underline           Underline
                         2          Reverse Video       Reverse Video
                         3          Blinking            Blinking
                         4          Bold                Bold
                         5          Struck Through      Struck Through
                         6          Not Used            Palette Index 1
                         7          Not Used            Palette Index 2

Note: Color Palette Index formed by 7,6,0

DOS  Attribute BYTE:     0          Blue        - FORE
                         1          Green       - FORE
                         2          Red         - FORE
                         3          Intensity   - FORE
                         4          Blue        - BACK
                         5          Green       - BACK
                         6          Red         - BACK
                         7          Blinking    - FORE
******************************************************************************/

{
    BYTE DOSAttr;
    BYTE fReverseVideo;
    BYTE ColorIndex;

    if (gfColor == FALSE)       /* Monochrome Attributes */
      {
      if ((CTOSAttr & 0x02))                    /* Underline */
        DOSAttr = 0x01;
      else if ((CTOSAttr & 0x04))               /* Reverse   */
        DOSAttr = 0x70;
      else
        DOSAttr = 0x07;                         /* Normal    */

      if ((CTOSAttr & 0x10))                    /* Bold      */
        DOSAttr = DOSAttr | 0x10;

      if ((CTOSAttr & 0x80))                    /* Blinking  */
        DOSAttr = DOSAttr | 0x80;
      }
    else                        /* Color Attributes */
    /**************************************************************************
    Note:   DOS Maps the CTOS Palette Index Fields as follows:
            Full Intensity  --> DOS Intensity On
            2/3  Intensity  --> DOS Intensity Off
            1/3  Intensity  --> DOS Intensity Off
            OFF             --> DOS Intensity Off, Color Bit Off (R, G, or B)
    **************************************************************************/
      {
        /*************************/
        /* Determine Color Index */
        /*************************/
        ColorIndex = 0;
        if ((CTOSAttr & 1))
          ColorIndex += 1;

        if ((CTOSAttr & 64))
          ColorIndex += 2;

        if ((CTOSAttr & 128))
          ColorIndex += 4;

        if (gfBoldBitOn == FALSE)
          DOSAttr = 0x00;
        else
          DOSAttr = 0x08;

        if (CTOSAttr & 4)                       /* Reverse Video        */
          fReverseVideo = TRUE;
        else
          fReverseVideo = FALSE;

        if (CTOSAttr & 8)                       /* Blinking             */
          DOSAttr = DOSAttr | 128;

        if ((CTOSAttr & 16) || (CTOSAttr & 2))  /* Bold OR Underline    */
          if (fReverseVideo == FALSE)
            if (gfBoldBitOn == FALSE)
              DOSAttr = DOSAttr | 8;
            else
              DOSAttr = DOSAttr ^ 8;

        if (fReverseVideo == FALSE)
          {
          if (gColorPalette[ColorIndex] &  1)
            DOSAttr = DOSAttr |  1;

          if (gColorPalette[ColorIndex] &  2)
            DOSAttr = DOSAttr |  2;

          if (gColorPalette[ColorIndex] &  4)
            DOSAttr = DOSAttr |  4;

          if (gColorPalette[ColorIndex] &  8)
            DOSAttr = DOSAttr | 16;

          if (gColorPalette[ColorIndex] & 16)
            DOSAttr = DOSAttr | 32;

          if (gColorPalette[ColorIndex] & 32)
            DOSAttr = DOSAttr | 64;
          }
        else
          {
          if (gColorPalette[ColorIndex] &  1)
            DOSAttr = DOSAttr | 16;

          if (gColorPalette[ColorIndex] &  2)
            DOSAttr = DOSAttr | 32;

          if (gColorPalette[ColorIndex] &  4)
            DOSAttr = DOSAttr | 64;

          if (gColorPalette[ColorIndex] &  8)
            DOSAttr = DOSAttr | 1;

          if (gColorPalette[ColorIndex] & 16)
            DOSAttr = DOSAttr | 2;

          if (gColorPalette[ColorIndex] & 32)
            DOSAttr = DOSAttr | 4;
          }
      }
    return (DOSAttr);
}


WORD CW(WORD w) /* ConvertWord */
{
  return (w << 8) | (w >> WORDSIZE - 8);  /* rol */
}


void DisplayShiftState(void)
{
    ADSBYTE ShiftFlagPtr;
    WORD    iLine,iCol;

    if (gfDTOn == FALSE)    /* MuSoft App Not Running */
      return;

    ShiftFlagPtr = (ADSBYTE) MK_FP(0x0000,0x0417);

    if (*ShiftFlagPtr != gLastShiftState)
      {
      BIOS_GetCursorPos((ADSWORD) &iLine,(ADSWORD) &iCol);
      gLastShiftState = *ShiftFlagPtr;

      if (gLastShiftState & 0x80)
        BIOS_WriteString(29,65,(ADSCHAR) "Ins",0x07);
      else
        BIOS_WriteString(29,65,(ADSCHAR) "Ovr",0x07);

      if (gLastShiftState & 0x10)
        BIOS_WriteString(29,69,(ADSCHAR) "Srl",0x07);
      else
        BIOS_WriteString(29,69,(ADSCHAR) "   ",0x07);

      if (gLastShiftState & 0x20)
        BIOS_WriteString(29,73,(ADSCHAR) "Num",0x07);
      else
        BIOS_WriteString(29,73,(ADSCHAR) "   ",0x07);

      if (gLastShiftState & 0x40)
        BIOS_WriteString(29,77,(ADSCHAR) "Cap",0x07);
      else
        BIOS_WriteString(29,77,(ADSCHAR) "   ",0x07);

      BIOS_PosCursor(iLine,iCol);
      }
}


void memcopy(ADSCHAR Source, ADSCHAR Dest, WORD count)
{
    register int i;

    for (i=count; i > 0; i--)
      *Dest++ = *Source++;
}


void GetDateTimeString()
{
    char      IsAmPm[3];
    char      NumStr[10];
    WORD      Year,Month,Day,DoW;
    WORD      Hour,Minute,Second;

    DOS_GetDate(&Year,&Month,&Day,&DoW);
    DOS_GetTime(&Hour,&Minute,&Second);

    if (Hour >= 12)
      strcpy(IsAmPm,"PM");
    else
      strcpy(IsAmPm,"AM");

    if (Hour > 12)
      Hour = Hour - 12;

    /* Day of Week */
    strcpy(gDTString,gDays[DoW]);
    strcat(gDTString," ");

    /* Month of Year */
    strcat(gDTString,gMonths[Month]);
    strcat(gDTString," ");
    itoa(Day,NumStr,10);
    strcat(gDTString,NumStr);

    /* Year */
    itoa(Year,NumStr,10);
    strcat(gDTString,", ");
    strcat(gDTString,NumStr);
    strcat(gDTString," ");

    /* Hour */
    itoa(Hour,NumStr,10);
    strcat(gDTString,NumStr);
    strcat(gDTString,":");

    /* Minute */
    if (Minute < 10)
      strcat(gDTString,"0");
    itoa(Minute,NumStr,10);
    strcat(gDTString,NumStr);
    strcat(gDTString," ");
    strcat(gDTString,IsAmPm);
}


void DOS_SetVideoPtr(void)
{
   ADSBYTE  MiscByte;

   MiscByte   = (ADSBYTE) MK_FP(0x0000,0x0487);

   gVideoOffset = 0x0000;

   if ((*MiscByte & 0x04) == 0)
     {
     gfColor = TRUE;
     gVideoSegment = 0xB800;
     }
   else
     {
     gfColor = FALSE;
     gVideoSegment = 0xB000;
     }

   if (DV_IsDesqviewPresent() == TRUE)
     {
     _ES = gVideoSegment;
     _DI = gVideoOffset;
     _AH = 0xFE;
     geninterrupt(0x10);
     gVideoSegment = _ES;
     gVideoOffset  = _DI;
     }
}


WORD stringlength(ADSCHAR pString)
{
    register int i;

    for (i = 0; (*pString != '\0'); i++)
      pString++;
    return (i);
}


WORD ValidateFrameParams(WORD iFrame,
                         WORD iColStart,
                         WORD iLineStart,
                         WORD nCols,
                         WORD nLines)
{
  WORD erc;

  /* Frame Out Of Bounds    */
  if (iFrame > gVCB.cFrames)
    erc = ercVDMInvalidVCBParams;

  /* Column Out Of Bounds   */
  else if (iColStart > (gVCB.rgbRgFrame[iFrame].iColStart +
                        gVCB.rgbRgFrame[iFrame].cCols))
    erc = ercVDMInvalidVCBParams;

  /* Line Out Of Bounds     */
  else if (iLineStart > (gVCB.rgbRgFrame[iFrame].iLineStart +
                         gVCB.rgbRgFrame[iFrame].cLines))
    erc = ercVDMInvalidVCBParams;

  /* Line Off The Screen    */
  else if ((gVCB.rgbRgFrame[iFrame].iLineStart + nLines) > gVCB.cLinesMax)
    erc = ercVDMInvalidVCBParams;

  /* Column Off The Screen  */
  else if ((gVCB.rgbRgFrame[iFrame].iColStart  + nCols)  > gVCB.cColsMax)
    erc = ercVDMInvalidVCBParams;

  /* Parameters OKAY */
  else
    erc = ercOK;

  return (erc);
}


/*/////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//
//                     BEGIN CTOS API SECTION
//
///////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////*/


/******************************************************************************
*
*                       CTOS FUNCTION Beep
*
*   Calling Sequence:
*       AX      = fnBeep
*
*   Return:
*       AX      = erc
*
******************************************************************************/
WORD Beep(void)
{
    sound(900); delay(100); nosound();
    return (ercOK);
}


/******************************************************************************
*
*                       CTOS FUNCTION GetWSUserName
*
*   Calling Sequence:
*       AX      = fnGetWSUserName
*
*   Return:
*       AX      = erc
*
******************************************************************************/
#pragma argsused
WORD GetWSUserName(WORD    WSNum,
                   ADSCHAR pWSUserNameRet,
                   WORD    sWSUserNameRetMax)
{
    BYTE len;

    len = (BYTE) strlen(gStationName);
    if (len > sWSUserNameRetMax)
      len = (BYTE) sWSUserNameRetMax;

    *pWSUserNameRet = (char) len; pWSUserNameRet++;
    memcopy((ADSCHAR) gStationName, pWSUserNameRet, len);

    return (ercOK);
}


/******************************************************************************
*
*                       CTOS FUNCTION InitCharMap
*
*   Calling Sequence:
*       AX      = fnInitCharMap
*
*   Return:
*       AX      = erc
*
*
*   Note:   pMap = MK_FP(0x0000,0x0000)
*           sMap = size as given in the psMapRet parameter in ResetVideo
*
******************************************************************************/
#pragma argsused
WORD InitCharMap(ADSMEM pMap, WORD sMap)
{
    register int i;

    /*******************************/
    /* Reset Frame 0 Attribute Map */
    /*******************************/
    setmem(gCTOSAttrMap, 2320, 0x00);

    /***********************/
    /* Reset the Video Map */
    /***********************/
    BIOS_PosCursor(0,0);

    /* Clear the Video Map */
    BIOS_WriteChar(gVCB.bSpace,
                   gVCB.bAttr,
                   (gVCB.cColsMax * gVCB.cLinesMax));

    /***************************/
    /* Establish Frame Borders */
    /***************************/
    for (i=0; i < gVCB.cFrames; i++)
      {
      if ((gVCB.rgbRgFrame[i].bBorderDesc & 0x01))  /* TOP */
        {
        /* Set Cursor */
        BIOS_PosCursor(gVCB.rgbRgFrame[i].iLineStart - 1,
                       gVCB.rgbRgFrame[i].iColStart);

        /* Draw Border */
        BIOS_WriteChar(gVCB.rgbRgFrame[i].bBorderChar,
                       ConvertAttribute(gVCB.rgbRgFrame[i].bBorderAttr),
                       gVCB.rgbRgFrame[i].iColStart + gVCB.rgbRgFrame[i].cCols);
        }

      if ((gVCB.rgbRgFrame[i].bBorderDesc & 0x04))  /* BOTTOM */
        {
        /* Set Cursor */
        BIOS_PosCursor(gVCB.rgbRgFrame[i].iLineStart+1,
                       gVCB.rgbRgFrame[i].iColStart);

        /* Draw Border */
        BIOS_WriteChar(gVCB.rgbRgFrame[i].bBorderChar,
                       ConvertAttribute(gVCB.rgbRgFrame[i].bBorderAttr),
                       gVCB.rgbRgFrame[i].iColStart + gVCB.rgbRgFrame[i].cCols);
        }
      }
  return (ercOK);
}


/******************************************************************************
*
*                       CTOS FUNCTION InitVidFrame
*
*   Calling Sequence:
*       AX      = fnInitVidFrame
*
*   Return:
*       AX      = erc
*
*
*   Note: Border Description bits are as follows:
*           BIT 0   -   Top         (& 0x1)
*           BIT 1   -   Right       (& 0x2)
*           BIT 2   -   Bottom      (& 0x4)
*           BIT 3   -   Left        (& 0x8)
*
******************************************************************************/
WORD InitVidFrame(WORD iFrame,
                  WORD iColStart,
                  WORD iLineStart,
                  WORD nCols,
                  WORD nLines,
                  BYTE borderDesc,
                  BYTE bBorderChar,
                  BYTE bBorderAttr,
                  WORD fDblHigh,
                  WORD fDblWide)
{
  if (iFrame > gVCB.cFrames)
    return (ercVDMInvalidArg);

  if ((iLineStart + nLines) > gVCB.cLinesMax)
    return (ercVDMInvalidArg);

  if ((iColStart + nCols) > gVCB.cColsMax)
    return (ercVDMInvalidArg);

  gVCB.rgbRgFrame[iFrame].iLineStart    = iLineStart;
  gVCB.rgbRgFrame[iFrame].iColStart     = iColStart;
  gVCB.rgbRgFrame[iFrame].cLines        = nLines;
  gVCB.rgbRgFrame[iFrame].cCols         = nCols;
  gVCB.rgbRgFrame[iFrame].iLineLeftOff  = 0;
  gVCB.rgbRgFrame[iFrame].iColLeftOff   = 0;
  gVCB.rgbRgFrame[iFrame].bBorderDesc   = borderDesc;
  gVCB.rgbRgFrame[iFrame].bBorderChar   = gIBMTran[bBorderChar];
  gVCB.rgbRgFrame[iFrame].bBorderAttr   = bBorderAttr;
  gVCB.rgbRgFrame[iFrame].iLinePause    = FALSE;
  gVCB.rgbRgFrame[iFrame].iLineCursor   = 0;
  gVCB.rgbRgFrame[iFrame].iColCursor    = 0;
  gVCB.rgbRgFrame[iFrame].fDblHigh      = fDblHigh;
  gVCB.rgbRgFrame[iFrame].fDblWide      = fDblWide;

  return (ercOK);
}


/******************************************************************************
*
*                       CTOS FUNCTION PosFrameCursor
*
*   Calling Sequence:
*       AX      = fnPosFrameCursor
*
*   Return:
*       AX      = erc
*
******************************************************************************/
WORD PosFrameCursor(WORD iFrame,
                    WORD iCol,
                    WORD iLine)
{
  WORD erc;

  /* Turn Cursor Off */
  if ((iCol == 0xFF) && (iLine == 0xFF))
    BIOS_CursorOff();
  else
    {
    erc = ValidateFrameParams(iFrame,iCol,iLine,0,0);
    if (erc != ercOK)
      return (erc);

    BIOS_CursorOn();
    BIOS_PosCursor(gVCB.rgbRgFrame[iFrame].iLineStart + iLine,
                   gVCB.rgbRgFrame[iFrame].iColStart  + iCol);
    }

  return (ercOK);
}


/******************************************************************************
*
*                       CTOS FUNCTION ProgramColorMapper
*
*   Calling Sequence:
*       AX      = fnProgramColorMapper
*
*   Return:
*       AX      = erc
*
******************************************************************************/
WORD ProgramColorMapper(ADSMEM  pNewPalette,
                        WORD    sNewPalette,
                        ADSMEM  pNewControl,
                        WORD    sNewControl,
                        ADSMEM  pOldPaletteRet,
                        WORD    sOldPaletteRet,
                        ADSMEM  pOldControlRet,
                        WORD    sOldControlRet)
{
    ADSBYTE  pMap;
    ADSBYTE  pAttrMap;
    register int i;

    if (sOldPaletteRet  > 0)
      memcopy((ADSCHAR) gColorPalette,(ADSCHAR) pOldPaletteRet,sOldPaletteRet);

    if (sOldControlRet  > 0)
      memcopy((ADSCHAR) &gClrCtrl, (ADSCHAR) pOldControlRet, sOldControlRet);

    if (sNewPalette     > 0)
      memcopy((ADSCHAR) pNewPalette, (ADSCHAR) gColorPalette, sNewPalette);

    if (sNewControl     > 0)
      memcopy((ADSCHAR) pNewControl, (ADSCHAR) &gClrCtrl, sNewControl);


    /***********************************************************/
    /* Convert All Attributes on Screen to Reflect New Palette */
    /***********************************************************/
    pMap     = (ADSBYTE) MK_FP(gVideoSegment,gVideoOffset + 1);
    pAttrMap = (ADSBYTE) gCTOSAttrMap;

    for (i=0; i < 290; i++)     /* Strength Reduction Loop */
      {
      *pMap = ConvertAttribute(*pAttrMap); pMap += 2; pAttrMap++;
      *pMap = ConvertAttribute(*pAttrMap); pMap += 2; pAttrMap++;
      *pMap = ConvertAttribute(*pAttrMap); pMap += 2; pAttrMap++;
      *pMap = ConvertAttribute(*pAttrMap); pMap += 2; pAttrMap++;
      *pMap = ConvertAttribute(*pAttrMap); pMap += 2; pAttrMap++;
      *pMap = ConvertAttribute(*pAttrMap); pMap += 2; pAttrMap++;
      *pMap = ConvertAttribute(*pAttrMap); pMap += 2; pAttrMap++;
      *pMap = ConvertAttribute(*pAttrMap); pMap += 2; pAttrMap++;
      }

    return(ercOK);
}


/******************************************************************************
*
*                       CTOS FUNCTION PutFrameAttrs
*
*   Calling Sequence:
*       AX      = fnPutFrameAttrs
*
*   Return:
*       AX      = erc
*
*   Note: No frame overlap is assumed!
******************************************************************************/
WORD PutFrameAttrs(WORD iFrame,
                   WORD iCol,
                   WORD iLine,
                   WORD attr,
                   WORD nPos)
{
  WORD      erc;
  ADSBYTE   pMap;
  BYTE      DOSAttr;
  WORD      StartRow,StartCol;
  WORD      StopRow, StopCol;
  WORD      EndRow,EndCol,RowInc;
  register  WORD Row,Col;

  erc = ValidateFrameParams(iFrame,iCol,iLine,0,0);
  if (erc != ercOK)
    return (erc);

  StartCol = gVCB.rgbRgFrame[iFrame].iColStart  + iCol;

  StopCol  = StartCol + (gVCB.rgbRgFrame[iFrame].cCols - iCol);

  StartRow = gVCB.rgbRgFrame[iFrame].iLineStart + iLine;

  StopRow  = StartRow + (nPos / (StopCol - StartCol));
  if ((nPos % (StopCol - StartCol)) != 0)
    StopRow++;

  RowInc   = 160 - (StopCol - StartCol) * 2;
  EndRow   = StartRow + (StopRow - StartRow);
  EndCol   = StartCol + (StopCol - StartCol);

  pMap    = (ADSCHAR) MK_FP(gVideoSegment,gVideoOffset +
                                          ((StartRow * 160) +
                                           (StartCol * 2)   +
                                           1));
  DOSAttr = ConvertAttribute((BYTE) attr);

  for (Row = StartRow; ((Row < EndRow) && (nPos > 0)); ++Row)
    {
    for (Col = StartCol; ((Col < EndCol) && (nPos > 0)); ++Col)
      {
      *pMap = DOSAttr;
      pMap += 2;
      gCTOSAttrMap[Row][Col] = attr;
      nPos--;
      }
    pMap += RowInc;
    }
  return (ercOK);
}


/******************************************************************************
*
*                       CTOS FUNCTION PutFrameChars
*
*   Calling Sequence:
*       AX      = fnPutFrameChars
*
*   Return:
*       AX      = erc
*
* Note: Need to optimize for line at a time vs. char at a time!
*
******************************************************************************/
WORD PutFrameChars(WORD     iFrame,
                   WORD     iCol,
                   WORD     iLine,
                   ADSCHAR  pbText,
                   WORD     cbText)
{
  WORD      erc;
  ADSBYTE   pMap;
  BYTE      DOSAttr;
  WORD      StartRow,StartCol;
  WORD      StopRow, StopCol;
  WORD      EndRow,EndCol,RowInc;
  register  WORD Row,Col;

  erc = ValidateFrameParams(iFrame,iCol,iLine,0,0);
  if (erc != ercOK)
    return (erc);

  StartCol = gVCB.rgbRgFrame[iFrame].iColStart  + iCol;

  StopCol  = StartCol + (gVCB.rgbRgFrame[iFrame].cCols - iCol);

  StartRow = gVCB.rgbRgFrame[iFrame].iLineStart + iLine;

  StopRow  = StartRow + (cbText / (StopCol - StartCol));
  if ((cbText % (StopCol - StartCol)) != 0)
    StopRow++;

  RowInc   = 160 - (StopCol - StartCol) * 2;
  EndRow   = StartRow + (StopRow - StartRow);
  EndCol   = StartCol + (StopCol - StartCol);

  pMap    = (ADSCHAR) MK_FP(gVideoSegment,gVideoOffset +
                                          ((StartRow * 160) +
                                           (StartCol * 2)));

  for (Row = StartRow; ((Row < EndRow) && (cbText > 0)); ++Row)
    {
    for (Col = StartCol; ((Col < EndCol) && (cbText > 0)); ++Col)
      {
      *pMap = gIBMTran[(BYTE) *pbText++];
      pMap += 2;
      cbText--;
      }
    pMap += RowInc;
    }
  return (ercOK);
}


/******************************************************************************
*
*                       CTOS FUNCTION QueryFrameAttrs
*
*   Calling Sequence:
*       AX      = fnQueryFrameAttrs
*
*   Return:
*       AX      = erc
*
*   Note: No conversion of the attributes is done.
*
******************************************************************************/
WORD QueryFrameAttrs(WORD    iFrame,
                     WORD    iCol,
                     WORD    iLine,
                     WORD    sBuff,
                     ADSBYTE pRetBuf,
                     ADSWORD pcbRet)
{
  WORD      erc;
  WORD      Line;
  WORD      Col;
  register  WORD      i;

  erc = ValidateFrameParams(iFrame,iCol,iLine,0,0);
  if (erc != ercOK)
    return (erc);

  Line = gVCB.rgbRgFrame[iFrame].iLineStart + iLine;
  Col  = gVCB.rgbRgFrame[iFrame].iColStart  + iCol;

  for (i = 0; i < sBuff; i++)
    {
    *pRetBuf = gCTOSAttrMap[Line][Col];
    pRetBuf++;

    Col ++;
    if (Col+1 > (gVCB.rgbRgFrame[iFrame].iColStart + gVCB.rgbRgFrame[iFrame].cCols))
      {
      Line++;
      Col = gVCB.rgbRgFrame[iFrame].iColStart + iCol;
      }
    }
  *pcbRet = i;
  return (erc);
}


/******************************************************************************
*
*                       CTOS FUNCTION QueryFrameString
*
*   Calling Sequence:
*       AX      = fnQueryFrameString
*
*   Return:
*       AX      = erc
*
*   Note: No conversion of the attributes is done.
*
******************************************************************************/
WORD QueryFrameString(WORD      iFrame,
                      WORD      iCol,
                      WORD      iLine,
                      WORD      sBuff,
                      ADSCHAR   pRetBuf,
                      ADSWORD   pcbRet)
{
  WORD      erc;
  WORD      startoffset;
  ADSCHAR   pMap;
  register WORD      i;

  erc = ValidateFrameParams(iFrame,iCol,iLine,0,0);
  if (erc != ercOK)
    return (erc);

  startoffset = (((gVCB.rgbRgFrame[iFrame].iLineStart + iLine) * gVCB.cColsMax) * 2) +
                 ((gVCB.rgbRgFrame[iFrame].iColStart  + iCol)  * 2);

  pMap = (ADSCHAR) MK_FP(gVideoSegment,gVideoOffset + startoffset);

  for (i=0; i < sBuff; i++)
    {
    *pRetBuf = *pMap;
    pMap     += 2;          /* Skip to Next Char, Ignoring Attributes */
    pRetBuf  ++;
    }

  *pcbRet = i;
  return (erc);
}


/******************************************************************************
*
*                       CTOS FUNCTION QueryVidHdw
*
*   Calling Sequence:
*       AX      = fnQueryVidHdw
*       BX      = sBuffer
*       DS:DX   = pBuffer
*
*   Return:
*       AX      = erc
*
******************************************************************************/
#pragma argsused
WORD QueryVidHdw(ADSMEM pBuffer,
                 WORD   sBuffer)
{
  WORD      erc;
  ADSBYTE   nColPtr;
/*  ADSBYTE   nRowPtr; */
  ADSBYTE   MiscByte;
  struct    QueryVidHdwBlock far * qvhb;

  qvhb      = (struct QueryVidHdwBlock far *) pBuffer;
  nColPtr   = (ADSBYTE) MK_FP(0x0000,0x044A);
/*  nRowPtr   = (ADSBYTE) MK_FP(0x0000,0x0484); */
  MiscByte  = (ADSBYTE) MK_FP(0x0000,0x0487);

  qvhb->level                   = 4;
  qvhb->nLinesMax               = 29; /* *nRowPtr + 1; */
  if (qvhb->nLinesMax > 30)
    qvhb->nLinesMax             = 29;
  qvhb->nColsNarrow             = *nColPtr;
  qvhb->nColsWide               = *nColPtr;
  qvhb->nPixelsHigh             = 0;
  qvhb->nPixelsWide             = 0;
  qvhb->saGraphicsBoard         = gVideoSegment;
  qvhb->ioPort                  = 0;
  qvhb->wBytesPerLine           = 0;
  qvhb->nCharHeight             = 0;
  qvhb->nCharWidthNarrow        = 0;
  qvhb->nCharWidthWide          = 0;
  qvhb->pBitmap                 = (ADSMEM) MK_FP(gVideoSegment,gVideoOffset);
  qvhb->pFont                   = (ADSMEM) MK_FP(0x0000,0x0000);
  qvhb->bModuleType             = 0;
  qvhb->bModulePos              = 0;
  qvhb->wModuleEar              = 0;
  qvhb->nYCenter                = 0;
  qvhb->nXCenterNarrow          = 0;
  qvhb->nXCenterWide            = 0;
  qvhb->wxAspect                = 0;
  qvhb->wyAspect                = 0;
  qvhb->nPlanes                 = 0;
  if ((*MiscByte & 0x04) == 0)
    {
    qvhb->fColorMonitor         = TRUE;
    qvhb->nColors               = 64;
    qvhb->fBackgroundColor      = FALSE;
    qvhb->graphicsVersion       = 5;          /* VGA */
    }
  else
    {
    qvhb->fColorMonitor         = FALSE;
    qvhb->nColors               = 2;
    qvhb->fBackgroundColor      = FALSE;
    qvhb->graphicsVersion       = 2;            /* Monochrome */
    }
   qvhb->fHardwareCharMap       = TRUE;
   qvhb->nAlternateLinesMax     = 30;
   qvhb->nAlternateCharHeight   = 0;
   qvhb->wVidRelease            = 3;
   qvhb->wVidVersion            = 2;
   qvhb->fGenericColor          = TRUE;
   qvhb->fVariableCharMap       = TRUE;
   qvhb->fMultiSystemFonts      = TRUE;
   qvhb->fUseWsLine             = FALSE;
   qvhb->wLevelsPerPrimary      = 0;
   qvhb->fWhiteBackground       = FALSE;
   qvhb->nExtraRasterLines      = 0;

   return (ercOK);
}


/******************************************************************************
*
*                       CTOS FUNCTION QueryWsNum
*
*   Calling Sequence:
*       AX      = fnQueryWsNum
*
*   Return:
*       AX      = erc
*       DS:DX   = pWsNumRet
*
******************************************************************************/
WORD QueryWsNum(ADSWORD pWsNumRet)
{
    *pWsNumRet = CW(gStationNum);
    return (ercOK);
}


/******************************************************************************
*
*                       CTOS FUNCTION ReadKbdDirect
*
*   NOTE: parameter 'mode' is one of the following:
*           0: Wait until a key is available, then return it.
*           1: If key is available, return it; otherwise return ercTimeout.
*           2: Wait until a key is available, then return a copy of it.
*           3: If key is available, return a copy of it; otherwise return
*              ercTimeout.
*
*
*   Calling Sequence:
*       AX      = fnReadKbdDirect
*
*   Return
*       AX      = erc
*
******************************************************************************/
WORD ReadKbdDirect(WORD     mode,
                   ADSBYTE  pCharRet)
{
    WORD    erc;
    BYTE    KeyAH,KeyAL,RetKey,Flags,ValidKey;
    ADSBYTE ShiftFlagPtr;

    ShiftFlagPtr = (ADSBYTE) MK_FP(0x0000,0x0417);

ReadKeyLoop:
    erc      = ercOK;
    RetKey   = 0x00;
    ValidKey = TRUE;

    switch( mode )
      {

        case 0      :   for (; BIOS_IsKeyReady() == FALSE; )
                          DOS_Pause();
                        BIOS_ReadExtKey((ADSBYTE) &KeyAH, (ADSBYTE) &KeyAL);
                        break;

        case 1      :   if (BIOS_IsKeyReady() == TRUE)
                          BIOS_ReadExtKey((ADSBYTE) &KeyAH, (ADSBYTE) &KeyAL);
                        else
                          {
                          erc = ercKbdTimeout;
                          DOS_Pause();
                          }
                        break;

        default     :   erc = ercNotImplemented;
                        break;
      }

    /*****************************************
        Translate Key From MS-DOS to CTOS
    *****************************************/
    if (erc == ercOK)   /* Translate Key */
      {
      if (KeyAH == 0x4E)        /* + Sign on # Pad */
        RetKey = ctkGO;
      else
      if (KeyAL == 0xE0)
        {
        switch (KeyAH)
          {
          case      mdkUPARROW      : RetKey = ctkUPARROW;
                                      break;
          case      mdkDOWNARROW    : RetKey = ctkDOWNARROW;
                                      break;
          case      mdkLEFTARROW    : RetKey = ctkLEFTARROW;
                                      break;
          case      mdkRIGHTARROW   : RetKey = ctkRIGHTARROW;
                                      break;
          case      mdkPGDN         : RetKey = ctkPREVPAGE;
                                      break;
          case      mdkPGUP         : RetKey = ctkNEXTPAGE;
                                      break;
          case      mdkHOME         : if (*ShiftFlagPtr & 0x0010) /* Scroll Lock ON */
                                        RetKey = ctkMARK;
                                      else
                                        RetKey = ctkSCROLLUP;
                                      break;
          case      mdkEND          : if (*ShiftFlagPtr & 0x0010) /* Scroll Lock ON */
                                        RetKey = ctkBOUND;
                                      else
                                        RetKey = ctkSCROLLDOWN;
                                      break;
          case      mdkDELETE       : RetKey = ctkDELETE;
                                      break;
          case      mdkCTRLDELETE   : RetKey = ctkCODEDELETE;
                                      break;
          case      mdkOVERTYPE     : RetKey = ctkOVERTYPE;
                                      break;
          default                   : ValidKey = FALSE;
                                      break;
          }
        }
      else if (KeyAH == 0x01)
        {
        switch (KeyAL)
          {
          case      mdkESCAPE       : RetKey = ctkCANCEL;
                                      break;
          case      mdkALTESCAPE    : RetKey = ctkFINISH;
                                      break;
          default                   : ValidKey = FALSE;
                                      break;
          }
        }
      else if (KeyAH == 0xE0)
        {
        switch (KeyAL)
          {
          case      mdkPADENTER     : RetKey = ctkGO;
                                      break;
          case      mdkBACKSPACE    : RetKey = ctkBACKSPACE;
                                      break;
          default                   : ValidKey = FALSE;
                                      break;
          }
        }
      else if (KeyAL == 0x00)
        {
        switch (KeyAH)
          {
          case      mdkALTREVQUOTE  : RetKey = ctkCANCEL;
                                      SayLocus("");
                                      break;
          case      mdkUPARROW      : RetKey = ctkUPARROW;
                                      break;
          case      mdkDOWNARROW    : RetKey = ctkDOWNARROW;
                                      break;
          case      mdkLEFTARROW    : RetKey = ctkLEFTARROW;
                                      break;
          case      mdkRIGHTARROW   : RetKey = ctkRIGHTARROW;
                                      break;
          case      mdkPGDN         : RetKey = ctkPREVPAGE;
                                      break;
          case      mdkPGUP         : RetKey = ctkNEXTPAGE;
                                      break;
          case      mdkF1           : RetKey = ctkF1;
                                      break;
          case      mdkF2           : RetKey = ctkF2;
                                      break;
          case      mdkF3           : RetKey = ctkF3;
                                      break;
          case      mdkF4           : RetKey = ctkF4;
                                      break;
          case      mdkF5           : RetKey = ctkF5;
                                      break;
          case      mdkF6           : RetKey = ctkF6;
                                      break;
          case      mdkF7           : RetKey = ctkF7;
                                      break;
          case      mdkF8           : RetKey = ctkF8;
                                      break;
          case      mdkF9           : RetKey = ctkF9;
                                      break;
          case      mdkF10          : RetKey = ctkF10;
                                      break;
          case      mdkF11          : RetKey = ctkCOPY;
                                      break;
          case      mdkF12          : RetKey = ctkFINISH;
                                      break;
          case      mdkSHIFTF1      : RetKey = ctkSHIFTF1;
                                      break;
          case      mdkSHIFTF2      : RetKey = ctkSHIFTF2;
                                      break;
          case      mdkSHIFTF3      : RetKey = ctkSHIFTF3;
                                      break;
          case      mdkSHIFTF4      : RetKey = ctkSHIFTF4;
                                      break;
          case      mdkSHIFTF5      : RetKey = ctkSHIFTF5;
                                      break;
          case      mdkSHIFTF6      : RetKey = ctkSHIFTF6;
                                      break;
          case      mdkSHIFTF7      : RetKey = ctkSHIFTF7;
                                      break;
          case      mdkSHIFTF8      : RetKey = ctkSHIFTF8;
                                      break;
          case      mdkSHIFTF9      : RetKey = ctkSHIFTF9;
                                      break;
          case      mdkSHIFTF10     : RetKey = ctkSHIFTF10;
                                      break;
          case      mdkCTRLF1       : RetKey = ctkCODEF1;
                                      break;
          case      mdkCTRLF2       : RetKey = ctkCODEF2;
                                      break;
          case      mdkCTRLF3       : RetKey = ctkCODEF3;
                                      break;
          case      mdkCTRLF4       : RetKey = ctkCODEF4;
                                      break;
          case      mdkCTRLF5       : RetKey = ctkCODEF5;
                                      break;
          case      mdkCTRLF6       : RetKey = ctkCODEF6;
                                      break;
          case      mdkCTRLF7       : RetKey = ctkCODEF7;
                                      break;
          case      mdkCTRLF8       : RetKey = ctkCODEF8;
                                      break;
          case      mdkCTRLF9       : RetKey = ctkCODEF9;
                                      break;
          case      mdkCTRLF10      : RetKey = ctkCODEF10;
                                      break;
          case      mdkCTRLDELETE   : RetKey = ctkCODEDELETE;
                                      break;
          case      mdkALTF1        : RetKey = ctkHELP;
                                      break;
          case      mdkALTF2        : RetKey = ctkCODEHELP;
                                      break;
          case      mdkALTF3        : RetKey = ctkSHIFTHELP;
                                      break;
          case      mdkALTF5        : RetKey = ctkSHIFTDELETE;
                                      break;
          case      mdkALTF7        : RetKey = ctkMARK;
                                      break;
          case      mdkALTF8        : RetKey = ctkBOUND;
                                      break;
          case      mdkALTF9        : RetKey = ctkMOVE;
                                      break;
          case      mdkALTF10       : RetKey = ctkCOPY;
                                      break;
          case      mdkALTM         : RetKey = ctkMOVE;
                                      break;
          case      mdkALTC         : RetKey = ctkCOPY;
                                      break;
          case      mdkHOME         : if (*ShiftFlagPtr & 0x0010) /* Scroll Lock ON */
                                        RetKey = ctkMARK;
                                      else
                                        RetKey = ctkSCROLLUP;
                                      break;
          case      mdkEND          : if (*ShiftFlagPtr & 0x0010) /* Scroll Lock ON */
                                        RetKey = ctkBOUND;
                                      else
                                        RetKey = ctkSCROLLDOWN;
                                      break;
          case      mdkDELETE       : RetKey = ctkDELETE;
                                      break;
          case      mdkOVERTYPE     : RetKey = ctkOVERTYPE;
                                      break;
          default                   : ValidKey = FALSE;
                                      break;
          }
        }
      else
        {
        switch (KeyAL)
          {
          case      mdkENTER        : RetKey = ctkRETURN;
                                      break;
          default                   : RetKey = KeyAL;
                                      break;
          }
        }
      if (ValidKey == FALSE)
        goto ReadKeyLoop;
      else
        *pCharRet = RetKey;
      }

    return (erc);
}


/******************************************************************************
*
*                       CTOS FUNCTION ResetFrame
*
*   Calling Sequence:
*       AX      = fnResetFrame
*       BX      = iFrame
*
*   Return:
*       AX      = erc
*
******************************************************************************/
WORD ResetFrame(WORD iFrame)
{
  register  int  i,j;
  BYTE      DOSAttr;

  if (iFrame > gVCB.cFrames)
    return (iFrame);

  DOSAttr = ConvertAttribute(0);

  /****************************/
  /* Blank All Lines of Frame */
  /****************************/
  for (i =  gVCB.rgbRgFrame[iFrame].iLineStart;
       i < (gVCB.rgbRgFrame[iFrame].iLineStart + gVCB.rgbRgFrame[iFrame].cLines);
       i++)
    {
    /*------------------------*/
    /* Clear a Line On Screen */
    /*------------------------*/
    BIOS_PosCursor(i,gVCB.rgbRgFrame[iFrame].iColStart);
    BIOS_WriteChar(gVCB.bSpace, DOSAttr, gVCB.rgbRgFrame[iFrame].cCols);

    /*-----------------------------------*/
    /* Clear Internal CTOS Attribute Map */
    /*-----------------------------------*/
    for (j = gVCB.rgbRgFrame[iFrame].iColStart;
         j < (gVCB.rgbRgFrame[iFrame].iColStart + gVCB.rgbRgFrame[iFrame].cCols);
         j++)
      gCTOSAttrMap[i][j] = 0;
    }

  gVCB.rgbRgFrame[iFrame].iLinePause    = 0xFF;
  gVCB.rgbRgFrame[iFrame].iLineCursor   = 0xFF;
  gVCB.rgbRgFrame[iFrame].iColCursor    = 0xFF;
  gVCB.rgbRgFrame[iFrame].iLineLeftOff  = 0xFF;
  gVCB.rgbRgFrame[iFrame].iColLeftOff   = 0xFF;

  return (ercOK);
}


/******************************************************************************
*
*                       CTOS FUNCTION ResetVideo
*
*   Calling Sequence:
*       AX      = fnResetVideo
*
*   Return:
*       AX      = erc
*
******************************************************************************/
WORD ResetVideo(WORD    nCols,
                WORD    nLines,
                WORD    fAttr,
                WORD    bSpace,
                ADSWORD psMapRet)
{
  WORD      erc;
  register  int  i;
  ADSBYTE   pStartScanLine;
  ADSBYTE   pEndScanLine;
  ADSBYTE   kbdstatus;

  pEndScanLine    = (ADSBYTE) MK_FP(0x0000,0x0460);
  pStartScanLine  = (ADSBYTE) MK_FP(0x0000,0x0461);
  kbdstatus       = MK_FP(0x0000,0x0417);

  erc = ercOK;

  if (*psMapRet != 0)           /* Application is Terminating */
    {
    *kbdstatus = gVidState.KbdStatus;
    BIOS_SetActiveMode(gVidState.Mode);
    BIOS_SetActivePage(gVidState.Page);
    BIOS_SetCursorSize(gVidState.StartScanLine,gVidState.EndScanLine);
    BIOS_VideoOn();
    gfDTOn = FALSE;
    return(erc);
    }
  else
    {
    gVidState.Mode          = BIOS_GetActiveMode();
    gVidState.Page          = BIOS_GetActivePage();
    gVidState.StartScanLine = *pStartScanLine;
    gVidState.EndScanLine   = *pEndScanLine;
    gVidState.KbdStatus     = *kbdstatus;
    gLastShiftState         = 0xFF;
    *kbdstatus = *kbdstatus | 0x80;             /* Turn Overtype Off */
    if (gfCapsOn == TRUE)
      *kbdstatus = *kbdstatus | 0x40;           /* Turn Caps Lock On */
    gfDTOn = TRUE;
    }

  DOS_SetVideoPtr();

  /* Set Video Mode */
  BIOS_SetActiveMode(gVideoMode);

  /* Set Active Display Page */
  BIOS_SetActivePage(VIDEOPAGE);

  /* Turn Cursor Off */
  BIOS_CursorOff();

  /* Turn Video Off */
  BIOS_VideoOff();

  /* Display Keyboard Status Indicators */
  DisplayShiftState();

  /* Reset Saved Maps From Previous User */
  gMapDepth = 0xFF;

  /* Reset Saved Data From Previous User */
  gHeapSP   = 0;

  /* Initialize Video Control Block */
  gVCB.level             = 6;        /* Bit map workstation */
  gVCB.fCharAttrs        = fAttr;
  gVCB.fReverseVideo     = FALSE;
  gVCB.fHalfBright       = FALSE;
  gVCB.pMap              = MK_FP(gVideoSegment,gVideoOffset);
  gVCB.sMap              = (WORD) ((nCols * nLines) * 2);
  gVCB.cFrames           = MAXVIDFRAMES - 1;
  gVCB.cColsMax          = nCols;
  gVCB.cLinesMax         = nLines;
  gVCB.sLine             = 0;
  gVCB.defaultWindowId   = 0;
  gVCB.bSpace            = bSpace;
  gVCB.bAttr             = ConvertAttribute(0x00);
  gVCB.SAR               = 0;
  gVCB.nPixelsWide       = 0;
  gVCB.nPixelsHigh       = 0;
  gVCB.wsLine            = gVCB.sLine;
  gVCB.nPlanes           = 0;
  gVCB.fBackgroundColor  = FALSE;
  gVCB.cLinesMaxScreen   = gVCB.cLinesMax;
  gVCB.fHardwareCharMap  = TRUE;

  for (i=0; i < gVCB.cFrames; i++)
    {
    gVCB.rgbRgFrame[i].iLineStart    = 0;
    gVCB.rgbRgFrame[i].iColStart     = 0;
    gVCB.rgbRgFrame[i].cLines        = 0;
    gVCB.rgbRgFrame[i].cCols         = 0;
    gVCB.rgbRgFrame[i].iLineLeftOff  = 0;
    gVCB.rgbRgFrame[i].iColLeftOff   = 0;
    gVCB.rgbRgFrame[i].bBorderDesc   = 0;
    gVCB.rgbRgFrame[i].bBorderChar   = 0;
    gVCB.rgbRgFrame[i].bBorderAttr   = 0;
    gVCB.rgbRgFrame[i].iLinePause    = 0;
    gVCB.rgbRgFrame[i].iLineCursor   = 0;
    gVCB.rgbRgFrame[i].iColCursor    = 0;
    gVCB.rgbRgFrame[i].fDblHigh      = FALSE;
    gVCB.rgbRgFrame[i].fDblWide      = FALSE;
    }

  *psMapRet = gVCB.sMap;

  return (erc);
}


/******************************************************************************
*
*                       CTOS FUNCTION SetAlphaColorDefault
*
*   Calling Sequence:
*       AX      = fnSetAlphaColorDefault
*
*   Return:
*       AX      = erc
*
*   Note: 1 = ON, 2 = OFF for flag fields
*
******************************************************************************/
WORD SetAlphaColorDefault(WORD bMode)
{
    if (bMode != 0)
      return (ercSetAlphaColorDefault);

    gClrCtrl.bGraphicsPalette      = 2;
    gClrCtrl.bAlphaEnabled         = 1;
    gClrCtrl.bAlphaColorEnabled    = 1;
    gClrCtrl.bGraphicsEnabled      = 2;
    gClrCtrl.bGraphicsColorEnabled = 2;
    gClrCtrl.bFormat               = 1;
    gClrCtrl.wIndexStart           = 0;

    gColorPalette[0] = 0x0C;        /*  Green               */
    gColorPalette[1] = 0x08;        /*  Half-Bright Green   */
    gColorPalette[2] = 0x0F;        /*  Cyan                */
    gColorPalette[3] = 0x0A;        /*  Half-Bright Cyan    */
    gColorPalette[4] = 0x3F;        /*  White               */
    gColorPalette[5] = 0x2A;        /*  Half-Bright White   */
    gColorPalette[6] = 0x3C;        /*  Yellow              */
    gColorPalette[7] = 0x30;        /*  Red                 */

    return (ercOK);
}


/******************************************************************************
*
*                       CTOS FUNCTION SetKbdLed
*
*   Calling Sequence:
*       AX      = fnSetKbdLed
*
*   Return:
*       AX      = erc
*
*   iLED is:        0 - F10
*                   1 - F9
*                   2 - F8
*                   3 - F3
*                   4 - F2
*                   5 - F1
*                   6 - LOCK
*                   7 - OVERTYPE
*
*   DOS Keyboard Status Byte at 417h is Coded as:
*            Bit
*       7 6 5 4 3 2 1 0
*       | | | | | | | +---Right Shift Depressed
*       | | | | | | +-----Left Shift Depressed
*       | | | | | +-------Ctrl Shift Depressed
*       | | | | +---------Alt Shift Depressed
*       | | | +-----------Scroll Lock Active
*       | | +-------------Num Lock Active
*       | +---------------Caps Lock Active
*       +-----------------Insert Active
*
******************************************************************************/
WORD SetKbdLed(WORD iLED, BYTE fOn)
{
    ADSBYTE kbdstatus;

    kbdstatus = (ADSBYTE) MK_FP(0x0000,0x0417);

    switch( iLED )
    {
        case    6   : if (fOn == FALSE)
                        *kbdstatus = *kbdstatus & 0xBF;
                      else
                        *kbdstatus = *kbdstatus | 0x40;
                      break;

        case    7   : if (fOn == FALSE)
                        *kbdstatus = *kbdstatus | 0x80;
                      else
                        *kbdstatus = *kbdstatus & 0x7F;
                      break;

        default     : break;
    }
    DisplayShiftState();
    return (ercOK);
}


/******************************************************************************
*
*                       CTOS FUNCTION SetScreenVidAttr
*
*   Calling Sequence:
*       AX      = fnSetScreenVidAttr
*
*   Return:
*       AX      = erc
*
*   iAttr is:       0 - Reverse Video
*                   1 - Video Refresh
*                   2 - Half-Bright
*
******************************************************************************/
WORD SetScreenVidAttr(WORD iAttr,
                      WORD fOn)
{
    WORD erc;
    switch( iAttr )
    {
        case    1   : if (fOn == FALSE)
                        BIOS_VideoOff();
                      else
                        BIOS_VideoOn();
                      erc = ercOK;
                      break;

        default     : erc = ercNotImplemented;
                      break;
    }
    return (erc);
}


/******************************************************************************
*
*                       CTOS FUNCTION SetVideoTimeout
*
*   Calling Sequence:
*       AX      = fnSetVideoTimeout
*
*   Return:
*       AX      = erc
*
******************************************************************************/
WORD SetVideoTimeout(WORD nMinutes)
{
  if ((nMinutes >= 0) && (nMinutes <= 109))
    {
    if (nMinutes == 0)
      {
      gfScrOutOn  = FALSE;
      nMinutes    = 1;
      }
    else
      gfScrOutOn  = TRUE;

    gScrOutWait = (DWORD) (nMinutes * ONEMINUTE);
    return(ercOK);
    }
  else
    return(ercVDMIntervalTooLarge);
}


/*/////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//
//                     BEGIN MUSOFT FUNCTION SECTION
//
///////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////*/


/******************************************************************************
*
*                       MuSoft FUNCTION MuLogicalAND
*
*   Calling Sequence:
*       AX      = fnMuLogicalAND
*
*   Return:
*       AX      = Result
*
******************************************************************************/
BYTE  MuLogicalAND(BYTE Value1, BYTE Value2)
{
    return (Value1 & Value2);
}


/******************************************************************************
*
*                       MuSoft FUNCTION MuLogicalOR
*
*   Calling Sequence:
*       AX      = fnMuLogicalOR
*
*   Return:
*       AX      = Result
*
******************************************************************************/
BYTE MuLogicalOR(BYTE Value1, BYTE Value2)
{
    return (Value1 | Value2);
}


/******************************************************************************
*
*                       MuSoft FUNCTION LogicalXOR
*
*   Calling Sequence:
*       AX      = fnMuLogicalXOR
*
*   Return:
*       AX      = Result
*
******************************************************************************/
BYTE  MuLogicalXOR(BYTE Value1, BYTE Value2)
{
    return (Value1 ^ Value2);
}


/******************************************************************************
*
*                       MuSoft FUNCTION MuLowerUpper
*
*   Calling Sequence:
*       AX      = fnMuLowerUpper
*
*   Return:
*       AX      = erc
*
******************************************************************************/
WORD MuLowerUpper(ADSCHAR ArrayPtr,
                  WORD    ArraySize)
{
    register int i;

    for (i = ArraySize; i > 0; i--, ArrayPtr++)
      if (islower((BYTE) *ArrayPtr))
        *ArrayPtr = (char) _toupper((BYTE) *ArrayPtr);
    return (ercOK);
}


/******************************************************************************
*
*                       MuSoft FUNCTION MuPrintTran
*
*   Calling Sequence:
*       AX      = fnMuPrintTran
*
*   Return:
*       AX      = erc
*
******************************************************************************/
WORD MuPrintTran(ADSCHAR    ArrayPtr,
                 WORD       ArraySize,
                 BYTE       IBMPrinter)
{
  register int i;

  if ((IBMPrinter == 'N') || (IBMPrinter == 'n'))
    {
    for (i = ArraySize; i > 0; i--,ArrayPtr++)
      *ArrayPtr = gNonIBMTran[(BYTE) *ArrayPtr];
    }
  return (ercOK);
}


/******************************************************************************
*
*                       MuSoft FUNCTION MuPurgeArray
*
*   Calling Sequence:
*       AX      = fnMuPurgeArray
*
*   Return:
*       AX      = erc
*
******************************************************************************/
#pragma argsused
WORD MuPurgeArray(WORD ArraySize)
{
  if (gHeapSP == 0)
    return (ercAllocNotEnough);
  else
    {
    gHeapSP -= ArraySize;
    return (ercOK);
    }
}


/******************************************************************************
*
*                       MuSoft FUNCTION MuRestoreArray
*
*   Calling Sequence:
*       AX      = fnMuRestoreArray
*
*   Return:
*       AX      = erc
*
******************************************************************************/
WORD MuRestoreArray(WORD ArraySize, ADSCHAR ArrayPtr)
{
  if (gHeapSP == 0)
    return (ercAllocNotEnough);
  else
    {
    gHeapSP = gHeapSP - ArraySize;
#ifdef STVER
    memcopy((ADSCHAR) &gHeap[gHeapSP],ArrayPtr,ArraySize);
#endif

#ifdef XMVER
    XMMcopyfrom(ArraySize,gXHHeap,gHeapSP,ArrayPtr);
#endif
    }

  return (ercOK);
}


/******************************************************************************
*
*                       MuSoft FUNCTION MuRestoreMap
*
*   Calling Sequence:
*       AX      = fnMuRestoreMap
*
*   Return:
*       AX      = erc
*
******************************************************************************/
#pragma argsused
WORD MuRestoreMap(ADSCHAR ArrayPtr, WORD ArraySize)
{
  WORD      startoffset;
  WORD      iFrame,iCol,iLine;
  ADSCHAR   pMap;
  ADSCHAR   pSaveMap;
  ADSCHAR   pAttrMap;
  register  int i;
  DWORD     XMOffset;

  /*-------------------------*/
  /* Check Map Stack Pointer */
  /*-------------------------*/
  if (gMapDepth == 0xFF)    /* 0xFF indicates Map Stack Underflow */
    return (ercAllocNotEnough);

  iFrame = 0, iCol = 0, iLine = 0;
  startoffset = (((gVCB.rgbRgFrame[iFrame].iLineStart + iLine) * gVCB.cColsMax) * 2) +
                 ((gVCB.rgbRgFrame[iFrame].iColStart  + iCol)  * 2);

  /*----------------------------------*/
  /* Pointer to Physical Video Memory */
  /*----------------------------------*/
  pMap      = (ADSCHAR) MK_FP(gVideoSegment,gVideoOffset + startoffset);

  /*-------------------------*/
  /* Pointer to Local Buffer */
  /*-------------------------*/
#ifdef STVER
  pSaveMap  = (ADSCHAR) &gMapArray[gMapDepth * VIDMAPSIZE];
#endif

#ifdef XMVER
  XMOffset = (DWORD) (gMapDepth * (VIDMAPSIZE + CTOSATTRMAPSIZE));
#endif

  /*-------------------------------------*/
  /* Pointer to Local CTOS Attribute Map */
  /*-------------------------------------*/
  pAttrMap  = (ADSCHAR)  &gCTOSAttrMap[gVCB.rgbRgFrame[iFrame].iLineStart + iLine]
                                      [gVCB.rgbRgFrame[iFrame].iColStart  + iCol];


#ifdef STVER
  for (i=0; i < VIDMAPSIZE; i+=16)
    {
    *pMap++ = *pSaveMap++; *pMap++ = ConvertAttribute(*pSaveMap); *pAttrMap++ = *pSaveMap++;
    *pMap++ = *pSaveMap++; *pMap++ = ConvertAttribute(*pSaveMap); *pAttrMap++ = *pSaveMap++;
    *pMap++ = *pSaveMap++; *pMap++ = ConvertAttribute(*pSaveMap); *pAttrMap++ = *pSaveMap++;
    *pMap++ = *pSaveMap++; *pMap++ = ConvertAttribute(*pSaveMap); *pAttrMap++ = *pSaveMap++;
    *pMap++ = *pSaveMap++; *pMap++ = ConvertAttribute(*pSaveMap); *pAttrMap++ = *pSaveMap++;
    *pMap++ = *pSaveMap++; *pMap++ = ConvertAttribute(*pSaveMap); *pAttrMap++ = *pSaveMap++;
    *pMap++ = *pSaveMap++; *pMap++ = ConvertAttribute(*pSaveMap); *pAttrMap++ = *pSaveMap++;
    *pMap++ = *pSaveMap++; *pMap++ = ConvertAttribute(*pSaveMap); *pAttrMap++ = *pSaveMap++;
    }
#endif

#ifdef XMVER
    XMMcopyfrom(VIDMAPSIZE,gXHMap,XMOffset,pMap);
    XMMcopyfrom(CTOSATTRMAPSIZE,gXHMap,XMOffset+VIDMAPSIZE,pAttrMap);
#endif

  /*-------------------------*/
  /* Decrement Stack Pointer */
  /*-------------------------*/
  if (gMapDepth == 0)
    gMapDepth = 0xFF;
  else
    gMapDepth --;

  return (ercOK);
}


/******************************************************************************
*
*                       MuSoft FUNCTION MuResume
*
*   Calling Sequence:
*       AX      = fnMuResume
*
*   Return:
*       AX      = erc
*
******************************************************************************/
WORD MuResume(void)
{
  ADSBYTE   kbdstatus;
  ADSCHAR   pMap;

  kbdstatus = MK_FP(0x0040,0x0017);

  /*---------------------------*/
  /* Restore Saved Video State */
  /*---------------------------*/
  *kbdstatus = gMuVidState.KbdStatus;
  BIOS_SetActiveMode(gMuVidState.Mode);
  BIOS_SetActivePage(gMuVidState.Page);
  BIOS_PosCursor(gMuVidState.iLine,gMuVidState.iCol);
  BIOS_SetCursorSize(gMuVidState.StartScanLine,gMuVidState.EndScanLine);
  BIOS_PosCursor(gMuVidState.iLine,gMuVidState.iCol);
  BIOS_VideoOn();

  /*-----------------------*/
  /* Restore MuSoft Screen */
  /*-----------------------*/
  pMap  = (ADSCHAR) MK_FP(gVideoSegment,gVideoOffset);

#ifdef STVER
  memcopy((ADSCHAR) &gSaveScreen[0],pMap,FULLSCREENSIZE);
#endif

#ifdef XMVER
  XMMcopyfrom(FULLSCREENSIZE,gXHScreen,0,pMap);
#endif

  /*--------------------------*/
  /* Resume Date/Time Display */
  /*--------------------------*/
  gfDTOn = TRUE;
  GetDateTimeString();
  DisplayDateTime();

  return (ercOK);
}


/******************************************************************************
*
*                       MuSoft FUNCTION MuSaveArray
*
*   Calling Sequence:
*       AX      = fnMuSaveArray
*
*   Return:
*       AX      = erc
*
******************************************************************************/
WORD MuSaveArray(WORD ArraySize, ADSCHAR ArrayPtr)
{
  if ((MUARRAYSIZE - gHeapSP) < ArraySize)
    return (ercAllocNotEnough);
  else
    {

#ifdef STVER
    memcopy(ArrayPtr, (ADSCHAR) &gHeap[gHeapSP], ArraySize);
#endif

#ifdef XMVER
    XMMcopyto(ArraySize,ArrayPtr,gXHHeap,gHeapSP);
#endif

    gHeapSP += ArraySize;
    return (ercOK);
    }
}


/******************************************************************************
*
*                       MuSoft FUNCTION MuSaveMap
*
*   Calling Sequence:
*       AX      = fnMuSaveMap
*
*   Return:
*       AX      = erc
*
******************************************************************************/
#pragma argsused
WORD MuSaveMap(ADSCHAR ArrayPtr, WORD ArraySize)
{
  WORD      erc;
  WORD      startoffset;
  WORD      iFrame,iCol,iLine;
  ADSCHAR   pMap;
  ADSCHAR   pAttrMap;
  ADSCHAR   pSaveMap;
  DWORD     XMOffset;
  register  int i;

  /*---------------------------------*/
  /* Increment SaveMap Stack Pointer */
  /*---------------------------------*/
  if (gMapDepth == 0xFF)
    gMapDepth = 0;
  else
    {
    gMapDepth ++;
    if (gMapDepth  > (MAXVIDMAPS - 1))
      {
      gMapDepth --;
      return (ercAllocNotEnough);
      }
    }

  iFrame = 0, iCol = 0, iLine = 0;
  startoffset = (((gVCB.rgbRgFrame[iFrame].iLineStart + iLine) * gVCB.cColsMax) * 2) +
                 ((gVCB.rgbRgFrame[iFrame].iColStart  + iCol)  * 2);


  /*----------------------------------*/
  /* Pointer to Physical Video Memory */
  /*----------------------------------*/
  pMap      = (ADSCHAR) MK_FP(gVideoSegment,gVideoOffset+startoffset);

  /*-------------------------*/
  /* Pointer to Local Buffer */
  /*-------------------------*/
#ifdef STVER
  pSaveMap  = (ADSCHAR) &gMapArray[gMapDepth * VIDMAPSIZE];
#endif

#ifdef XMVER
  XMOffset = gMapDepth * (VIDMAPSIZE + CTOSATTRMAPSIZE);
#endif

  /*-------------------------------------*/
  /* Pointer to Local CTOS Attribute Map */
  /*-------------------------------------*/
  pAttrMap  = (ADSCHAR) &gCTOSAttrMap[gVCB.rgbRgFrame[iFrame].iLineStart + iLine]
                                     [gVCB.rgbRgFrame[iFrame].iColStart  + iCol];


  /*-----------------------------------*/
  /* Save Characters from Video Memory */
  /*-----------------------------------*/
#ifdef STVER
  for (i=0; i < VIDMAPSIZE; i+=16)
    {
    *pSaveMap++ = *pMap; pMap += 2; *pSaveMap++ = *pAttrMap++;
    *pSaveMap++ = *pMap; pMap += 2; *pSaveMap++ = *pAttrMap++;
    *pSaveMap++ = *pMap; pMap += 2; *pSaveMap++ = *pAttrMap++;
    *pSaveMap++ = *pMap; pMap += 2; *pSaveMap++ = *pAttrMap++;
    *pSaveMap++ = *pMap; pMap += 2; *pSaveMap++ = *pAttrMap++;
    *pSaveMap++ = *pMap; pMap += 2; *pSaveMap++ = *pAttrMap++;
    *pSaveMap++ = *pMap; pMap += 2; *pSaveMap++ = *pAttrMap++;
    *pSaveMap++ = *pMap; pMap += 2; *pSaveMap++ = *pAttrMap++;
    }
#endif

#ifdef XMVER
  XMMcopyto(VIDMAPSIZE,pMap,gXHMap,XMOffset);
  XMMcopyto(CTOSATTRMAPSIZE,pAttrMap,gXHMap,XMOffset+VIDMAPSIZE);
#endif

  return (ercOK);
}


/******************************************************************************
*
*                       MuSoft FUNCTION MuSuspend
*
*   Calling Sequence:
*       AX      = fnMuSuspend
*
*   Return:
*       AX      = erc
*
******************************************************************************/
WORD MuSuspend(void)
{
  ADSBYTE   pStartScanLine;
  ADSBYTE   pEndScanLine;
  ADSBYTE   kbdstatus;
  ADSCHAR   pMap;

  pEndScanLine    = (ADSBYTE) MK_FP(0x0000,0x0460);
  pStartScanLine  = (ADSBYTE) MK_FP(0x0000,0x0461);
  kbdstatus       = (ADSBYTE) MK_FP(0x0040,0x0017);

  /*--------------------------*/
  /* Save Current Video State */
  /*--------------------------*/
  gMuVidState.Mode          = BIOS_GetActiveMode();
  gMuVidState.Page          = BIOS_GetActivePage();
  gMuVidState.StartScanLine = *pStartScanLine;
  gMuVidState.EndScanLine   = *pEndScanLine;
  gMuVidState.KbdStatus     = *kbdstatus;
  BIOS_GetCursorPos((ADSWORD) &gMuVidState.iLine,
                    (ADSWORD) &gMuVidState.iCol);

  /*----------------------------*/
  /* Copy All of Current Screen */
  /*----------------------------*/
   pMap  = (ADSCHAR) MK_FP(gVideoSegment,gVideoOffset);
#ifdef STVER
   memcopy(pMap,(ADSCHAR) &gSaveScreen[0],FULLSCREENSIZE);
#endif

#ifdef XMVER
    XMMcopyto(FULLSCREENSIZE,pMap,gXHScreen,0);
#endif

  /*------------------------------------*/
  /* Restore Original Video Environment */
  /*------------------------------------*/
  *kbdstatus = gVidState.KbdStatus;
  BIOS_SetActiveMode(gVidState.Mode);
  BIOS_SetActivePage(gVidState.Page);
  BIOS_SetCursorSize(gVidState.StartScanLine,gVidState.EndScanLine);
  BIOS_VideoOn();

  /*---------------------------*/
  /* Suspend Date/Time Display */
  /*---------------------------*/
  gfDTOn                    = FALSE;

  return (ercOK);
}


/*/////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//
//                         MS-DOS TSR ROUTINES
//
///////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////*/

/****************************************************************************
*
*   Function to Free a DOS Segment, specifically: the DOS environment space
*   for the server.
*
****************************************************************************/
WORD DOSFreeSegment(WORD SegToFree)
{
  _ES = SegToFree;
  _AH = 0x49;
  geninterrupt(0x21);
  if (_FLAGS & CARRYFLAG)
    return (_AX);
  else
    return (ercOK);
}


/******************************************************************************
*
*                       FUNCTION GetServerStatistics
*
*   Calling Sequence:
*       AX      = fnGetStatus
*       DS:DX   = GetServerStats
*
*   Return:
*       AX      = erc
*
******************************************************************************/
WORD GetServerStatistics(ADSMEM GSS)
{
    struct GetServerStats far * gss;

    gss = (struct GetServerStats far *) GSS;

    gss->InterruptNumber    = gIntNum;
    gss->ServerMemorySize   = (gMemSize * 16L);
    gss->fLoadedHigh        = gfLoadHigh;
    gss->fScrOutOn          = gfScrOutOn;
    gss->fDTOn              = gfDTOn;
    gss->ScrOutValue        = gScrOutWait;
    gss->StationNumber      = gStationNum;
    gss->VideoMode          = gVideoMode;
    gss->NbrRqServed        = gNbrRqServed;
    gss->VerMajor           = (BYTE) VERMAJOR;
    gss->VerMinor           = (BYTE) VERMINOR;
    gss->StackSize          = (WORD) _stklen;
    gss->HeapSize           = (DWORD) MUARRAYSIZE;
    gss->HeapLeft           = MUARRAYSIZE - gHeapSP;

    memcopy((ADSCHAR) gStationName,
            (ADSCHAR) gss->StationName,
            strlen(gStationName) + 1);

    memcopy((ADSCHAR) VERSTRING,
            (ADSCHAR) gss->ServerString,
            strlen(VERSTRING) + 1);

    memcopy((ADSCHAR) __DATE__, (ADSCHAR) gss->CompileDate, strlen(__DATE__) + 1);

    memcopy((ADSCHAR) __TIME__, (ADSCHAR) gss->CompileTime, strlen(__TIME__) + 1);

    return (ercOK);
}


/****************************************************************************
*
*   Function to Ask Server for Its Version Number
*
****************************************************************************/
void GetServerVersion(ADSBYTE major, ADSBYTE minor)
{
  union     REGS    r;
  struct    SREGS   s;

  r.x.ax = fnGETVERNUM;
  int86x(gIntNum,&r,&r,&s);

  *major = r.h.bh;
  *minor = r.h.bl;
}


/****************************************************************************
*
*   Function to Issue a Request to Terminate the Server
*
****************************************************************************/
void TerminateServer(void)
{
  union     REGS    r;
  struct    SREGS   s;
  WORD              erc;

  r.x.ax = fnTERMINATE;
  int86x(gIntNum,&r,&r,&s);

  if (r.x.bx == 0xFF)
    {
    printf("\n-ERROR:");
    printf(" Other programs have been loaded after the server.\n");
    }
  else
    {
    erc = DOSFreeSegment(r.x.bx);
    if (erc != ercOK)
      {
      printf("\n-ERROR:");
      printf(" Could not free memory. (MS-DOS ERC %u)\n",erc);
      }
    else
      {
      gMemSize        = ((s.ds   * 256L) + r.x.dx) * 16L;
      gNbrRqServed    = ((r.x.ax * 256L) + r.x.cx);
      printf("\n-%s unloaded. (%lu bytes released)",VERSTRING,gMemSize);
      printf("\n-Server responded to %lu requests.\n",gNbrRqServed);
      }
    }
}


/****************************************************************************
*
*   Function to Determine If A Copy of the Server is Already in Memory
*
****************************************************************************/
WORD IsServerInstalled(void)
{
  BYTE   major,minor;

  if (FP_SEG(old_int_func) == 0xF000)
    return (FALSE);

  if (FP_SEG(old_int_func) + (FP_OFF(old_int_func) / 16) == 0)
    return (FALSE);

  GetServerVersion((ADSBYTE) &major, (ADSBYTE) &minor);
  if ((major == VERMAJOR) && (minor == VERMINOR))
    return (TRUE);
  else
    return (FALSE);
}


/****************************************************************************
*
*   Function to Determine If An Interrupt Is Already In Use
*
****************************************************************************/
WORD InterruptIsInUse(void)
{
  if (FP_SEG(old_int_func) + (FP_OFF(old_int_func) / 16) == 0)
    return (FALSE);

  if (FP_SEG(old_int_func) == 0xF000)
    return (FALSE);

  return (TRUE);
}


/****************************************************************************
*
*   Function to Determine If Keyboard is 101 Key AT-style
*
****************************************************************************/
WORD IsKeyboard101(void)
{
    if (* (ADSBYTE) MK_FP(0x0000,0x0496) & 0x10)
      return (TRUE);
    else
      return (FALSE);
}


/****************************************************************************
*
*   Function to Determine If BIOS Supports Extended Keyboard
*
****************************************************************************/
WORD IsBIOSNew(void)
{
    BYTE KeyAH,KeyAL;
    WORD IsSupported;

    IsSupported = FALSE;

    /******************/
    /* Flush Keyboard */
    /******************/
    while (BIOS_IsKeyReady() == TRUE)
      BIOS_ReadExtKey((ADSBYTE) &KeyAH, (ADSBYTE) &KeyAL);

    /**********************************************************/
    /* Try BIOS Kbd Buffer Stuff, available only on NEW BIOSs */
    /**********************************************************/
    if (BIOS_StuffExtKey(0xFF,0xFF) == TRUE)
      {
      /*********************/
      /* Read the Key back */
      /*********************/
      BIOS_ReadExtKey((ADSBYTE) &KeyAH, (ADSBYTE) &KeyAL);
      if ((KeyAH == 0xFF) && (KeyAL == 0xFF))
        IsSupported = TRUE;
      }

    return (IsSupported);
}


/****************************************************************************
*
*   Function to Handle Keyboard Interrupt
*
****************************************************************************/
void interrupt KeyboardTask(void)
{
    enable();

    if (old_int_9)
      (*old_int_9) ();

    if (gfKbdBusy == TRUE)
      return;

    gfKbdBusy    = TRUE;
    gScrOutTicks = 0;

    if (gfScreenOff == TRUE)
      {
      BIOS_VideoOn();
      gfScreenOff = FALSE;
      }

    if (gfDTOn == TRUE)
      DisplayShiftState();

    gfKbdBusy = FALSE;
}


/****************************************************************************
*
*   Function to Handle PrtSc Interrupt and Convert to Copy Key
*
****************************************************************************/

/**********************************************
void interrupt PrtScTask(void)
{
    if (BIOS_StuffExtKey(mdkALTC,0x00) != TRUE)
      Beep();
}
**********************************************/

void DisplayDateTime(void)
{
    WORD erc;
    WORD iCol;

    erc   = PutFrameChars(2,49,0,(ADSCHAR) "                              ",30);
    iCol  = 80 - strlen(gDTString);
    erc   = PutFrameChars(2,iCol,0,gDTString,strlen(gDTString));
    if (erc == erc) ;   /* DEAD CODE - Prevents Warning Message */
}


/****************************************************************************
*
*   Function to Handle Timer Interrupt
*
****************************************************************************/
void interrupt TimerTask(void)
{
    enable();

    gDTTicks++;             /* Date & Time      */
    gScrOutTicks++;         /* Screen Timeout   */

    if (old_int_1C)
      (*old_int_1C) ();

    if (gfScrOutOn == TRUE)
      {
      if (gScrOutTicks > gScrOutWait)
        {
        BIOS_VideoOff();
        gfScreenOff = TRUE;
        }
      }


    if (gfDTOn == TRUE)
      {
      if (gDTTicks > gDTWait)
        {
        gDTTicks = 0;
        if (*gInDOSPtr == 0)
          {
          GetDateTimeString();
          DisplayDateTime();
          }
        }
      }
}


/****************************************************************************
*
*   Function to Distribute API Interrupt Function Requests
*
*
*   Note: Far pointers on stack are in Offset - Segment format.
*
****************************************************************************/
#pragma argsused
void interrupt CTOSServer(bp, di, si, ds, es, dx, cx,
                          bx, ax, ip, cs, flags)
{
    int       i;
    struct    StackFrameStruct far * SF;
    WORD      SaveAX;

    enable();

    gNbrRqServed++ ;

    SaveAX = ax;
    SF = (struct StackFrameStruct far *) MK_FP(es,si);

    switch( SaveAX )
      {

      /***************************/
      /*  MS-DOS Misc TSR Calls  */
      /***************************/

      case fnGETVERNUM      :   ax = ercOK;
                                bx = (VERMAJOR << 8) + VERMINOR;
                                break;

      case fnGETSTATUS      :   ax = GetServerStatistics((ADSMEM) SF->Param1);
                                break;

      case fnSETDTOFF       :   gfDTOn = FALSE;
                                break;

      case fnTERMINATE      :
                                bx = gPspSeg;
                                ax = (gNbrRqServed  / 256L);
                                cx = (gNbrRqServed  % 256L);
                                ds = (gMemSize      / 256L);
                                dx = (gMemSize      % 256L);
                                setvect(0x09,   old_int_9);
                                setvect(0x1C,   old_int_1C);
                                setvect(gIntNum,old_int_func);
                                /****************************
                                if (gfPrtSc == FALSE)
                                  setvect(0x05,old_int_5);
                                ****************************/
#ifdef XMVER
                                XMMfree(gXHMap);
                                XMMfree(gXHHeap);
                                XMMfree(gXHScreen);
#endif
                                break;


      /*************************/
      /*  CTOS Function Calls  */
      /*************************/
      case fnBeep           :   ax = Beep();
                                * (ADSWORD) SF->Param1 = CW(ax);
                                break;

      case fnDelay          :   delay(CW(* (ADSWORD) SF->Param2) * 100);
                                ax = ercOK;
                                * (ADSWORD) SF->Param1 = CW(ax);
                                break;

      case  fnGetWSUserName :   ax = GetWSUserName(CW(* (ADSWORD) SF->Param2),
                                                        (ADSCHAR) SF->Param3,
                                                   CW(* (ADSWORD) SF->Param4));
                                * (ADSWORD) SF->Param1 = ax;
                                break;


      case  fnInitCharMap   :   ax = InitCharMap(  (ADSMEM)  SF->Param2,
                                              CW(* (ADSWORD) SF->Param3));
                                * (ADSWORD) SF->Param1 = ax;
                                break;

      case  fnInitVidFrame  :   ax = InitVidFrame(CW(* (ADSWORD) SF->Param2),
                                                  CW(* (ADSWORD) SF->Param3),
                                                  CW(* (ADSWORD) SF->Param4),
                                                  CW(* (ADSWORD) SF->Param5),
                                                  CW(* (ADSWORD) SF->Param6),
                                                     * (ADSBYTE) SF->Param7,
                                                     * (ADSBYTE) SF->Param8,
                                                     * (ADSBYTE) SF->Param9,
                                                  CW(* (ADSWORD) SF->Param10),
                                                  CW(* (ADSWORD) SF->Param11));
                                * (ADSWORD) SF->Param1 = CW(ax);
                                break;

      case fnLoadTask       :   ax = ercOK;
                                * (ADSWORD) SF->Param1 = CW(ax);
                                gfDTOn = TRUE;
                                GetDateTimeString();
                                DisplayDateTime();
                                break;

      case fnMuLogicalAND   :   ax = MuLogicalAND(* (ADSBYTE) SF->Param2,
                                                  * (ADSBYTE) SF->Param3);
                                * (ADSWORD) SF->Param1 = ax;
                                break;

      case fnMuLogicalOR    :   ax = MuLogicalOR(* (ADSBYTE) SF->Param2,
                                                 * (ADSBYTE) SF->Param3);
                                * (ADSWORD) SF->Param1 = ax;
                                break;

      case fnMuLogicalXOR   :   ax = MuLogicalXOR(* (ADSBYTE) SF->Param2,
                                                  * (ADSBYTE) SF->Param3);
                                * (ADSWORD) SF->Param1 = ax;
                                break;

      case fnMuLowerUpper   :   ax = MuLowerUpper(  (ADSCHAR) SF->Param2,
                                               CW(* (ADSWORD) SF->Param3));
                                * (ADSWORD) SF->Param1 = CW(ax);
                                break;

      case fnMuMemAvail     :   ax = MUARRAYSIZE - gHeapSP;
                                * (ADSWORD) SF->Param1 = ax;
                                break;

      case fnMuMoveRec      :   ax = ercOK;
                                memcopy(  (ADSCHAR) SF->Param3,
                                          (ADSCHAR) SF->Param2,
                                     CW(* (ADSWORD) SF->Param4));
                                * (ADSWORD) SF->Param1 = CW(ax);
                                break;

      case fnMuPrintTran    :   ax = MuPrintTran(  (ADSCHAR) SF->Param2,
                                              CW(* (ADSWORD) SF->Param3),
                                              CW(* (ADSWORD) SF->Param4));
                                * (ADSWORD) SF->Param1 = CW(ax);
                                break;

      case fnMuPurgeArray   :   ax = MuPurgeArray(CW(* (ADSWORD) SF->Param2));
                                * (ADSWORD) SF->Param1 = CW(ax);
                                break;

      case fnMuRestoreArray :   ax = MuRestoreArray(CW(* (ADSWORD) SF->Param2),
                                                         (ADSCHAR) SF->Param3);
                                * (ADSWORD) SF->Param1 = CW(ax);
                                break;


      case fnMuRestoreMap   :   ax = MuRestoreMap(     (ADSCHAR) SF->Param2,
                                                  CW(* (ADSWORD) SF->Param3));
                                * (ADSWORD) SF->Param1 = CW(ax);
                                break;

      case fnMuResume       :   ax = MuResume();
                                * (ADSWORD) SF->Param1 = CW(ax);
                                break;

      case fnMuSaveArray    :   ax = MuSaveArray(CW(* (ADSWORD) SF->Param2),
                                                      (ADSCHAR) SF->Param3);
                                * (ADSWORD) SF->Param1 = CW(ax);
                                break;

      case fnMuSaveMap      :   ax = MuSaveMap(     (ADSCHAR) SF->Param2,
                                               CW(* (ADSWORD) SF->Param3));
                                * (ADSWORD) SF->Param1 = CW(ax);
                                break;

      case fnMuSuspend      :   ax = MuSuspend();
                                * (ADSWORD) SF->Param1 = CW(ax);
                                break;

      case fnPosFrameCursor :   ax = PosFrameCursor(CW(* (ADSWORD) SF->Param2),
                                                    CW(* (ADSWORD) SF->Param3),
                                                    CW(* (ADSWORD) SF->Param4));
                                * (ADSWORD) SF->Param1 = CW(ax);
                                break;

      case fnProgramColorMapper:
                                ax = ProgramColorMapper(     (ADSMEM)  SF->Param2,
                                                        CW(* (ADSWORD) SF->Param3),
                                                             (ADSMEM)  SF->Param4,
                                                        CW(* (ADSWORD) SF->Param5),
                                                             (ADSMEM)  SF->Param6,
                                                        CW(* (ADSWORD) SF->Param7),
                                                             (ADSMEM)  SF->Param8,
                                                        CW(* (ADSWORD) SF->Param9));
                                * (ADSWORD) SF->Param1 = CW(ax);
                                break;

      case fnPutFrameAttrs  :   ax = PutFrameAttrs(CW(* (ADSWORD) SF->Param2),
                                                   CW(* (ADSWORD) SF->Param3),
                                                   CW(* (ADSWORD) SF->Param4),
                                                   CW(* (ADSWORD) SF->Param5),
                                                   CW(* (ADSWORD) SF->Param6));
                                * (ADSWORD) SF->Param1 = CW(ax);
                                break;

      case fnPutFrameChars  :   ax = PutFrameChars(CW(* (ADSWORD) SF->Param2),
                                                   CW(* (ADSWORD) SF->Param3),
                                                   CW(* (ADSWORD) SF->Param4),
                                                        (ADSCHAR) SF->Param5,
                                                   CW(* (ADSWORD) SF->Param6));
                                * (ADSWORD) SF->Param1 = CW(ax);
                                break;

      case fnQueryFrameAttrs :  ax = QueryFrameAttrs(CW(* (ADSWORD) SF->Param2),
                                                     CW(* (ADSWORD) SF->Param3),
                                                     CW(* (ADSWORD) SF->Param4),
                                                     CW(* (ADSWORD) SF->Param5),
                                                          (ADSCHAR) SF->Param6,
                                                          (ADSWORD) SF->Param7);
                                * (ADSWORD) SF->Param1 = CW(ax);
                                break;

      case fnQueryFrameString:  ax = QueryFrameString(CW(* (ADSWORD) SF->Param2),
                                                      CW(* (ADSWORD) SF->Param3),
                                                      CW(* (ADSWORD) SF->Param4),
                                                      CW(* (ADSWORD) SF->Param5),
                                                           (ADSCHAR) SF->Param6,
                                                           (ADSWORD) SF->Param7);
                                * (ADSWORD) SF->Param1 = CW(ax);
                                break;

      case fnQueryVideo     :
      case fnQueryVidHdw    :   ax = QueryVidHdw(     (ADSMEM)  SF->Param2,
                                                 CW(* (ADSWORD) SF->Param3));
                                * (ADSWORD) SF->Param1 = CW(ax);
                                break;

      case fnQueryWsNum     :   ax = QueryWsNum((ADSWORD) SF->Param2);
                                * (ADSWORD) SF->Param1 = CW(ax);
                                break;

      case fnReadKbd        :   ax = ReadKbdDirect(0,
                                                   (ADSBYTE) SF->Param2);
                                * (ADSWORD) SF->Param1 = CW(ax);
                                break;

      case fnReadKbdDirect  :   ax = ReadKbdDirect(CW(* (ADSWORD) SF->Param2),
                                                        (ADSBYTE) SF->Param3);
                                * (ADSWORD) SF->Param1 = CW(ax);
                                break;

      case fnResetFrame     :   ax = ResetFrame(CW(* (ADSWORD) SF->Param2));
                                * (ADSWORD) SF->Param1 = CW(ax);
                                break;

      case fnResetVideo     :   ax = ResetVideo(CW(* (ADSWORD) SF->Param2),
                                                CW(* (ADSWORD) SF->Param3),
                                                CW(* (ADSWORD) SF->Param4),
                                                CW(* (ADSWORD) SF->Param5),
                                                     (ADSWORD) SF->Param6);
                                * (ADSWORD) SF->Param1 = CW(ax);
                                break;

      case fnSetKbdLed      :   ax = SetKbdLed(CW(* (ADSWORD) SF->Param2),
                                                  * (ADSBYTE) SF->Param3);
                                * (ADSWORD) SF->Param1 = CW(ax);
                                break;

      case
      fnSetAlphaColorDefault:   ax = SetAlphaColorDefault(CW(* (ADSWORD) SF->Param2));
                                * (ADSWORD) SF->Param1 = CW(ax);
                                break;

      case fnSetScreenVidAttr:  ax = SetScreenVidAttr(CW(* (ADSWORD) SF->Param2),
                                                      CW(* (ADSWORD) SF->Param3));
                                * (ADSWORD) SF->Param1 = CW(ax);
                                break;

      case fnSetVideoTimeout:   ax = SetVideoTimeout(CW(* (ADSWORD) SF->Param2));

                                * (ADSWORD) SF->Param1 = CW(ax);
                                break;


      /***************************/
      /*  Invalid Function Call  */
      /***************************/
      default               :   ax = ercNoSuchRequestCode;
                                * (ADSWORD) SF->Param1 = CW(ax);
                                SayLocus("VKS: No such request code.");
                                break;
      }
}


/*****************************************************************************
*
*                               MAIN
*
*****************************************************************************/
void main(int argc, char * argv[])
{
  int           i;                /* Counter Variable                     */
  char          * lparam;         /* Command Line Parameter               */

  printf("\n%s, version %u.%u",VERSTRING,VERMAJOR,VERMINOR);
  printf("\n  Copyright 1992 by United Computer Sales & Service, Inc.\n");

  gIntNum = DEFINT;
  /*******************************/
  /* Get Command Line Parameters */
  /*******************************/
  for (i=1; i < argc; i++)
    {
    lparam = argv[i];
    if ((*lparam == '/') || (*lparam == '-'))
      lparam++ ;
    switch (*lparam)
      {
      /*************************/
      /* Bold Bit Is Always On */
      /*************************/
      case 'b':
      case 'B': gfBoldBitOn = TRUE;
                break;

      /******************************************/
      /* Initial CapsLock State on ResetVideo   */
      /******************************************/
      case 'c':
      case 'C': gfCapsOn = TRUE;
                break;

      /*****************************/
      /* Emulator Interrupt Number */
      /*****************************/
      case 'i':
      case 'I': lparam++ ;
                gIntNum = atoi(lparam);
                if ((gIntNum < 0) || (gIntNum > 255))
                  gIntNum = DEFINT;
                break;

      /***************************/
      /* Skip 101 Keyboard Check */
      /***************************/
      case 'k':
      case 'K': gfSkip101 = TRUE;
                break;

      /***********************************/
      /* Establish Video Mode to Invoke */
      /**********************************/
      case 'm':
      case 'M': lparam++ ;
                gVideoMode = atoi(lparam);
                break;

      /*********************************/
      /* Establish CTOS Station Number */
      /*********************************/
      case 'n':
      case 'N': lparam++ ;
                gStationNum = atoi(lparam);
                break;

      /********************************/
      /* Establish CTOS Station Name  */
      /********************************/
      case 'o':
      case 'O': lparam++ ;
                memcopy((ADSCHAR) lparam,
                        (ADSCHAR) gStationName,
                        stringlength((ADSCHAR) lparam) + 1);
                break;

      /************************************/
      /* DO NOT make PrtSc key a Copy Key */
      /************************************/

/******************************************
      case 'p':
      case 'P': gfPrtSc = TRUE;
                break;
******************************************/

     /**********************/
     /* Set Video Timeout  */
     /**********************/
      case 't':
      case 'T': lparam++ ;
                if (SetVideoTimeout(atoi(lparam))) ;
                break;

      /***********************/
      /* Terminate Server    */
      /***********************/
      case 'u':
      case 'U': gfRemove = TRUE;
                break;

      /***********************/
      /* Display Help        */
      /***********************/
      case '?':
      case 'h':
      case 'H': gfHelp = TRUE;
                break;

      default:  printf("\n*Note: ignored unknown parameter \"%s\"\n",lparam);
                break;
      }
    }
  
  old_int_func  = getvect(gIntNum);   /* Save Old Vector             */
/*  old_int_5     = getvect(0x05);         Save Old Print Scr Interrupt*/
  old_int_1C    = getvect(0x1C);      /* Save Old Timer Interrupt    */
  old_int_9     = getvect(0x09);      /* Save Old Keyboard Interrupt */
  gInDOSPtr     = DOS_GetInDOSPtr();


  /**************************/
  /* Show Help Screen       */
  /**************************/
  if (gfHelp == TRUE)
    {
    printf("\nOptional parameters are:");
    printf("\n\t/BoldBitOn      \tBold Bit in Attribute is Always On.");
    printf("\n\t/capslock       \tTurn CapsLock ON on entry to MuSoft.");
    printf("\n\t/i<intnum>      \tGive Emulator interrupt number (128-255).");
    printf("\n\t/k              \tSkip 101 Keyboard Checks.");
    printf("\n\t/m<video mode>  \tGive video mode to invoke (0-255).");
    printf("\n\t/n<station>     \tGive CTOS station number (1-65000).");
    printf("\n\t/o<station name>\tGive CTOS station name (12 chars max).");
/*****************
    printf("\n\t/p              \tDO NOT make PrtSc key a copy key.");
*****************/
    printf("\n\t/t<minutes>     \tSet video timeout in minutes (0-109).");
    printf("\n\t/u              \tUnload server from memory.");
    printf("\n");
    exit(EXIT_SUCCESS);
    }

  /**************************/
  /* Terminate Video Server */
  /**************************/
  if (gfRemove == TRUE)
    if (IsServerInstalled() == TRUE)
      {
      TerminateServer();
      exit(EXIT_SUCCESS);
      }
    else
      {
      printf("\n-Server is not installed!\n");
      exit(EXIT_FAILURE);
      }


  /*************************************/
  /* Check If Server Already Installed */
  /*************************************/
  if (IsServerInstalled() == TRUE)
    {
    printf("\n-Server is already installed!");
    printf("\n-Use parameter /u to terminate server, then install.\n");
    exit(EXIT_FAILURE);
    }


  /****************************************/
  /* Check If Interrupt Is Already In Use */
  /****************************************/
  if (InterruptIsInUse() == TRUE)
    {
    printf("\n-Interrupt #%u already in use!",gIntNum);
    printf("\n-Use parameter /i to specify another interrupt number.\n");
    exit(EXIT_FAILURE);
    }


  /*************************************/
  /* Check If Keyboard is Newer 101    */
  /*************************************/
  if (IsKeyboard101() == FALSE)
    {
    if (gfSkip101 == FALSE)
      {
      printf("\n-ERROR:");
      printf(" Keyboard is not AT 101-key.\n");
      exit(EXIT_FAILURE);
      }
    else
      {
      printf("\n-WARNING:");
      printf(" Keyboard is not AT 101-key.\n");
      }
    }

  /********************************************/
  /* Check If BIOS Supports Extended Keyboard */
  /********************************************/
  if (IsBIOSNew() == FALSE)
    {
    if (gfSkip101 == FALSE)
      {
      printf("\n-ERROR:");
      printf(" The ROM-BIOS will not support extended keyboard calls.\n");
      exit(EXIT_FAILURE);
      }
    else
      {
      printf("\n-WARNING:");
      printf(" The ROM-BIOS will not support extended keyboard calls.\n");
      }
    }

  /******************************/
  /* Allocate Extended Memory   */
  /******************************/
#ifdef XMVER
  if (XMMlibinit() != 0x00)
    {
    printf("\n-ERROR:");
    printf(" Could not initialize extended memory area!\n");
    exit(EXIT_FAILURE);
    }
  if (XMMcoreleft() < XMSIZE)
    {
    printf("\n-ERROR:");
    printf(" Not enough extended memory. Need %u bytes,",XMSIZE);
    printf(" %u bytes availble\n",XMMcoreleft());
    exit(EXIT_FAILURE);
    }
  gXHMap = XMMalloc((MAXVIDMAPS * VIDMAPSIZE) + (MAXVIDMAPS * CTOSATTRMAPSIZE));
  if (_XMMerror != 0)
    {
    printf("\n-ERROR:");
    printf(" Could not allocated extended memory.");
    exit(EXIT_FAILURE);
    }
  gXHHeap = XMMalloc(MUARRAYSIZE);
  if (_XMMerror != 0)
    {
    printf("\n-ERROR:");
    printf(" Could not allocated extended memory.");
    exit(EXIT_FAILURE);
    }
  gXHScreen = XMMalloc(FULLSCREENSIZE);
  if (_XMMerror != 0)
    {
    printf("\n-ERROR:");
    printf(" Could not allocated extended memory.");
    exit(EXIT_FAILURE);
    }
#endif

  /**************************/
  /* Install Video Server   */
  /**************************/
  setvect(gIntNum,  CTOSServer);    /* Point Interrupt to Our Routine */
  setvect(0x1C,     TimerTask);
  setvect(0x09,     KeyboardTask);

/*********************************
  if (gfPrtSc == FALSE)
    {
    setvect(0x05,PrtScTask);
    printf("\nVector set.");
    }
*********************************/

  gPspSeg  = _psp;
  gEnvSeg  = (ADSWORD) MK_FP(_psp,0x2C);
  gMemSize = (_SS + ((_SP + _stklen) / 16)) - _psp;

  printf("\n-Server Installed, using %lu bytes of ",(gMemSize * 16L));
  if (_psp > 40000) /* psp segment is greater than 640k */
    {
    gfLoadHigh = TRUE;
    printf("high memory");
    }
  else
    {
    gfLoadHigh = FALSE;
    printf("conventional memory");
    }
  printf(", interrupt #%u",gIntNum);
  printf("\n-This workstation known as CTOS user '%s',",gStationName);
  printf(" user #%u",gStationNum);
  printf("\n-Running under DOS v%u.%u",_osmajor,_osminor);
  printf("\n");

  if (DOSFreeSegment(*gEnvSeg) == ercOK)
    keep(0,gMemSize);
  else
    printf("\n-ERROR: Could not free the DOS environment space.\n");
}
