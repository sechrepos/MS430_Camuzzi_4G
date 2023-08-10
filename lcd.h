#include "def.h"

#define LINE_SIZE				14
#define LINE_SIZE_2				10

#define SEND_CMD                   0
#define SEND_CHR                   1

#define LCD_X_RES                  84
#define LCD_Y_RES                  48

#define PIXEL_OFF                  0
#define PIXEL_ON                   1
#define PIXEL_XOR                  2

#define LCD_CACHE_SIZE             ((LCD_X_RES * LCD_Y_RES) / 8)

#define GRAPH_START					8
#define GRAPH_END					8

#define Flag_LCD_State				Flags_LCD.Bit._0
#define Flag_LCD_Update				Flags_LCD.Bit._1
#define Flag_LCD_Blink 				Flags_LCD.Bit._2
#define Flag_LCD_Blink_On			Flags_LCD.Bit._3
#define Flag_LCD_Scroll_On			Flags_LCD.Bit._4

void LCDEnable(void);
void LCDDisable(void);
void LCDSleep(void);
void LCDWakeup(void);
//void LCDBarAxis(void);
//void LCDBar(char);
//void LCDClrAlarm(void);
//void LCDAlarm(char);
void LCDClear(void);
void LCDBlinkOn(char x, char y, char *dataPtr, char size );
void LCDBlinkOff(void);
void LCDScrollOn(char y, char *dataPtr );
void LCDScrollOff(void);
void LCDStr_2(char x, char y, char *dataPtr);
void LCDChrXY_2 (char x, char y, char ch );
void LCD_tick(void);

/****************************************************************************/
/*  Init LCD Controler                                                      */
/*  Function : LCDInit                                                      */
/*      Parameters                                                          */
/*          Input   :  Nothing                                              */
/*          Output  :  Nothing                                              */
/****************************************************************************/
void LCDInit(void);

/****************************************************************************/
/*  Send to LCD                                                             */
/*  Function : LCDSend                                                      */
/*      Parameters                                                          */
/*          Input   :  data and  SEND_CHR or SEND_CMD                       */
/*          Output  :  Nothing                                              */
/****************************************************************************/
void LCDSend(char data, char cd);

/****************************************************************************/
/*  Update LCD memory                                                       */
/*  Function : LCDUpdate                                                    */
/*      Parameters                                                          */
/*          Input   :  Nothing                                              */
/*          Output  :  Nothing                                              */
/****************************************************************************/
void LCDUpdate ( void );

/****************************************************************************/
/*  Clear LCD                                                               */
/*  Function : LCDClear                                                     */
/*      Parameters                                                          */
/*          Input   :  Nothing                                              */
/*          Output  :  Nothing                                              */
/****************************************************************************/
void LCDClearLines(unsigned short a, unsigned short b);

/****************************************************************************/
/*  Change LCD Pixel mode                                                   */
/*  Function : LcdContrast                                                  */
/*      Parameters                                                          */
/*          Input   :  contrast                                             */
/*          Output  :  Nothing                                              */
/****************************************************************************/
void LCDPixel (char x, char y, char mode );


/****************************************************************************/
/*  Change LCD Pixel mode                                                   */
/*  Function : LcdContrast                                                  */
/*      Parameters                                                          */
/*          Input   :  contrast                                             */
/*          Output  :  Nothing                                              */
/****************************************************************************/
void LCDChrXY (char x, char y, char ch );
void LCDNotChrXY (char x, char y, char ch );

/****************************************************************************/
/*  Set LCD Contrast                                                        */
/*  Function : LcdContrast                                                  */
/*      Parameters                                                          */
/*          Input   :  contrast                                             */
/*          Output  :  Nothing                                              */
/****************************************************************************/
void LCDContrast(char contrast);

/****************************************************************************/
/*  Send string to LCD                                                      */
/*  Function : LCDStr                                                       */
/*      Parameters                                                          */
/*          Input   :  row, text                                            */
/*          Output  :  Nothing                                              */
/****************************************************************************/
void LCDStr(char x, char y, char *dataPtr );


extern volatile tREG08 Flags_LCD;

/*
static const char BarTable [][2] =
{
	{0x80, 0x00},
	{0xC0, 0x00},
	{0xE0, 0x00},
	{0xF0, 0x00},
	{0xF8, 0x00},
	{0xFC, 0x00},
	{0xFE, 0x00},
	{0xFF, 0x00},
	{0xFF, 0x80},
	{0xFF, 0xC0},
	{0xFF, 0xE0},
	{0xFF, 0xF0},
	{0xFF, 0xF8},
	{0xFF, 0xFC},
	{0xFF, 0xFE},
	{0xFF, 0xFF},
};

static const char DotTable [][2] =
{
	{0x80, 0x00},
	{0x40, 0x00},
	{0x20, 0x00},
	{0x10, 0x00},
	{0x08, 0x00},
	{0x04, 0x00},
	{0x02, 0x00},
	{0x01, 0x00},
	{0x00, 0x80},
	{0x00, 0x40},
	{0x00, 0x20},
	{0x00, 0x10},
	{0x00, 0x08},
	{0x00, 0x04},
	{0x00, 0x02},
	{0x00, 0x01},
};
*/

static const char FontLookup [][5] =
{
    { 0x00, 0x00, 0x00, 0x00, 0x00 },  // sp
    { 0x00, 0x00, 0x2f, 0x00, 0x00 },   // !
    { 0x00, 0x07, 0x00, 0x07, 0x00 },   // "
    { 0x14, 0x7f, 0x14, 0x7f, 0x14 },   // #
    { 0x24, 0x2a, 0x7f, 0x2a, 0x12 },   // $
    { 0xc4, 0xc8, 0x10, 0x26, 0x46 },   // %
    { 0x36, 0x49, 0x55, 0x22, 0x50 },   // &
    { 0x00, 0x05, 0x03, 0x00, 0x00 },   // '
    { 0x00, 0x1c, 0x22, 0x41, 0x00 },   // (
    { 0x00, 0x41, 0x22, 0x1c, 0x00 },   // )
    { 0x14, 0x08, 0x3E, 0x08, 0x14 },   // *
    { 0x08, 0x08, 0x3E, 0x08, 0x08 },   // +
    { 0x00, 0x00, 0x50, 0x30, 0x00 },   // ,
    { 0x10, 0x10, 0x10, 0x10, 0x10 },   // -
    { 0x00, 0x60, 0x60, 0x00, 0x00 },   // .
    { 0x20, 0x10, 0x08, 0x04, 0x02 },   // /
    { 0x3E, 0x51, 0x49, 0x45, 0x3E },   // 0
    { 0x00, 0x42, 0x7F, 0x40, 0x00 },   // 1
    { 0x42, 0x61, 0x51, 0x49, 0x46 },   // 2
    { 0x21, 0x41, 0x45, 0x4B, 0x31 },   // 3
    { 0x18, 0x14, 0x12, 0x7F, 0x10 },   // 4
    { 0x27, 0x45, 0x45, 0x45, 0x39 },   // 5
    { 0x3C, 0x4A, 0x49, 0x49, 0x30 },   // 6
    { 0x01, 0x71, 0x09, 0x05, 0x03 },   // 7
    { 0x36, 0x49, 0x49, 0x49, 0x36 },   // 8
    { 0x06, 0x49, 0x49, 0x29, 0x1E },   // 9
    { 0x00, 0x36, 0x36, 0x00, 0x00 },   // :
    { 0x00, 0x56, 0x36, 0x00, 0x00 },   // ;
    { 0x08, 0x14, 0x22, 0x41, 0x00 },   // <
    { 0x14, 0x14, 0x14, 0x14, 0x14 },   // =
    { 0x00, 0x41, 0x22, 0x14, 0x08 },   // >
    { 0x02, 0x01, 0x51, 0x09, 0x06 },   // ?
    { 0x32, 0x49, 0x59, 0x51, 0x3E },   // @
    { 0x7E, 0x11, 0x11, 0x11, 0x7E },   // A
    { 0x7F, 0x49, 0x49, 0x49, 0x36 },   // B
    { 0x3E, 0x41, 0x41, 0x41, 0x22 },   // C
    { 0x7F, 0x41, 0x41, 0x22, 0x1C },   // D
    { 0x7F, 0x49, 0x49, 0x49, 0x41 },   // E
    { 0x7F, 0x09, 0x09, 0x09, 0x01 },   // F
    { 0x3E, 0x41, 0x49, 0x49, 0x7A },   // G
    { 0x7F, 0x08, 0x08, 0x08, 0x7F },   // H
    { 0x00, 0x41, 0x7F, 0x41, 0x00 },   // I
    { 0x20, 0x40, 0x41, 0x3F, 0x01 },   // J
    { 0x7F, 0x08, 0x14, 0x22, 0x41 },   // K
    { 0x7F, 0x40, 0x40, 0x40, 0x40 },   // L
    { 0x7F, 0x02, 0x0C, 0x02, 0x7F },   // M
    { 0x7F, 0x04, 0x08, 0x10, 0x7F },   // N
    { 0x3E, 0x41, 0x41, 0x41, 0x3E },   // O
    { 0x7F, 0x09, 0x09, 0x09, 0x06 },   // P
    { 0x3E, 0x41, 0x51, 0x21, 0x5E },   // Q
    { 0x7F, 0x09, 0x19, 0x29, 0x46 },   // R
    { 0x46, 0x49, 0x49, 0x49, 0x31 },   // S
    { 0x01, 0x01, 0x7F, 0x01, 0x01 },   // T
    { 0x3F, 0x40, 0x40, 0x40, 0x3F },   // U
    { 0x1F, 0x20, 0x40, 0x20, 0x1F },   // V
    { 0x3F, 0x40, 0x38, 0x40, 0x3F },   // W
    { 0x63, 0x14, 0x08, 0x14, 0x63 },   // X
    { 0x07, 0x08, 0x70, 0x08, 0x07 },   // Y
    { 0x61, 0x51, 0x49, 0x45, 0x43 },   // Z
    { 0x00, 0x7F, 0x41, 0x41, 0x00 },   // [
    { 0x55, 0x2A, 0x55, 0x2A, 0x55 },   // 55
    { 0x00, 0x41, 0x41, 0x7F, 0x00 },   // ]
    { 0x04, 0x02, 0x01, 0x02, 0x04 },   // ^
    { 0x40, 0x40, 0x40, 0x40, 0x40 },   // _
    { 0x00, 0x01, 0x02, 0x04, 0x00 },   // '
    { 0x20, 0x54, 0x54, 0x54, 0x78 },   // a
    { 0x7F, 0x48, 0x44, 0x44, 0x38 },   // b
    { 0x38, 0x44, 0x44, 0x44, 0x20 },   // c
    { 0x38, 0x44, 0x44, 0x48, 0x7F },   // d
    { 0x38, 0x54, 0x54, 0x54, 0x18 },   // e
    { 0x08, 0x7E, 0x09, 0x01, 0x02 },   // f
    { 0x0C, 0x52, 0x52, 0x52, 0x3E },   // g
    { 0x7F, 0x08, 0x04, 0x04, 0x78 },   // h
    { 0x00, 0x44, 0x7D, 0x40, 0x00 },   // i
    { 0x20, 0x40, 0x44, 0x3D, 0x00 },   // j
    { 0x7F, 0x10, 0x28, 0x44, 0x00 },   // k
    { 0x00, 0x41, 0x7F, 0x40, 0x00 },   // l
    { 0x7C, 0x04, 0x18, 0x04, 0x78 },   // m
    { 0x7C, 0x08, 0x04, 0x04, 0x78 },   // n
    { 0x38, 0x44, 0x44, 0x44, 0x38 },   // o
    { 0x7C, 0x14, 0x14, 0x14, 0x08 },   // p
    { 0x08, 0x14, 0x14, 0x18, 0x7C },   // q
    { 0x7C, 0x08, 0x04, 0x04, 0x08 },   // r
    { 0x48, 0x54, 0x54, 0x54, 0x20 },   // s
    { 0x04, 0x3F, 0x44, 0x40, 0x20 },   // t
    { 0x3C, 0x40, 0x40, 0x20, 0x7C },   // u
    { 0x1C, 0x20, 0x40, 0x20, 0x1C },   // v
    { 0x3C, 0x40, 0x30, 0x40, 0x3C },   // w
    { 0x44, 0x28, 0x10, 0x28, 0x44 },   // x
    { 0x0C, 0x50, 0x50, 0x50, 0x3C },   // y
    { 0x44, 0x64, 0x54, 0x4C, 0x44 },   // z
    { 0x08, 0x6C, 0x6A, 0x19, 0x08 },   // {
    { 0x30, 0x4E, 0x61, 0x4E, 0x30 },   // |
    { 0x7E, 0x7E, 0x7E, 0x7E, 0x7E }    // 
};


// 12x8
static const unsigned int FontLookup_2 [][8] =
{
	{0,0,0,0,0,0,0,0},					//' '
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,32,32,248,248,32,32,0},			//'+'
	{0,0,0,0,0,0,0,0},
	{32,32,32,32,32,32,32,0},			//'-'
	{0,0,0,768,768,0,0,0},				//'.'
	{0,0,0,0,0,0,0,0},
	{508,1022,610,562,538,1022,508,0},	//'0'
	{0,516,516,1022,1022,512,512,0},	//'1'
	{780,910,706,610,562,798,780,0},	//'2'
	{260,774,546,546,546,1022,476,0},	//'3'
	{96,112,88,76,1022,1022,64,0},		//'4'
	{318,830,546,546,546,994,450,0},	//'5'
	{508,1022,546,546,546,998,452,0},	//'6'
	{6,6,994,1010,26,14,6,0},			//'7'
	{476,1022,546,546,546,1022,476,0},	//'8'
	{284,830,546,546,546,1022,508,0},	//'9'
	{0,0,0,0,408,408,0,0},				//':'
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{1016,1020,70,66,70,1020,1016,0},	//'A'
	{512,1022,1022,546,546,1022,476,0},	//'B'
	{0,0,0,0,0,0,0,0},
	{514,1022,1022,514,774,508,248,0},	//'D'
	{514,1022,1022,546,546,806,774,0},	//'E'
	{0,0,0,0,0,0,0,0},
	{508,1022,514,514,578,974,460,0},	//'G'
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{514,1022,1022,546,34,62,28,0},		//'P'
	{0,0,0,0,0,0,0,0},
	{514,1022,1022,98,226,958,796,0},	//'R'
	{268,798,562,546,610,966,388,0},	//'S'
	{0,6,514,1022,1022,514,6,0},		//'T'
	{0,0,0,0,0,0,0,0},
	{126,254,384,768,384,254,126,0},	//'V'
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{514,1022,1022,528,528,1008,480,0},		//'b'
	{0,0,0,0,0,0,0,0},
	{480,1008,528,530,1022,1022,512,0},		//'d'
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{1248,3568,2320,2320,2336,4080,2032,0}, //'g'
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{992,1008,48,224,48,1008,992,0},		//'m'
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
};

/*
{0,0,0,0,0,0,0,0,0,0,0,0                                 }, //' ' 32
{0,24,60,60,60,24,24,0,24,24,0,0                         }, //'!' 33
{54,54,54,20,0,0,0,0,0,0,0,0                             }, //'"' 34
{0,108,108,108,254,108,108,254,108,108,0,0               }, //'#' 35
{24,24,124,198,192,120,60,6,198,124,24,24                }, //'$' 36
{0,0,0,98,102,12,24,48,102,198,0,0                       }, //'%' 37
{0,56,108,56,56,118,246,206,204,118,0,0                  }, //'&' 38
{12,12,12,24,0,0,0,0,0,0,0,0                             }, //''' 39
{0,12,24,48,48,48,48,48,24,12,0,0                        }, //'(' 40
{0,48,24,12,12,12,12,12,24,48,0,0                        }, //')' 41
{0,0,0,108,56,254,56,108,0,0,0,0                         }, //'*' 42
{0,0,0,24,24,126,24,24,0,0,0,0                           }, //'+' 43
{0,0,0,0,0,0,0,12,12,12,24,0                             }, //',' 44
{0,0,0,0,0,254,0,0,0,0,0,0                               }, //'-' 45
{0,0,0,0,0,0,0,0,24,24,0,0                               }, //'.' 46
{0,0,2,6,12,24,48,96,192,128,0,0                         }, //'/' 47
{0,124,198,206,222,246,230,198,198,124,0,0               }, //'0' 48
{0,24,120,24,24,24,24,24,24,126,0,0                      }, //'1' 49
{0,124,198,198,12,24,48,96,198,254,0,0                   }, //'2' 50
{0,124,198,6,6,60,6,6,198,124,0,0                        }, //'3' 51
{0,12,28,60,108,204,254,12,12,12,0,0                     }, //'4' 52
{0,254,192,192,192,252,6,6,198,124,0,0                   }, //'5' 53
{0,124,198,192,192,252,198,198,198,124,0,0               }, //'6' 54
{0,254,198,12,24,48,48,48,48,48,0,0                      }, //'7' 55
{0,124,198,198,198,124,198,198,198,124,0,0               }, //'8' 56
{0,124,198,198,198,126,6,6,198,124,0,0                   }, //'9' 57
{0,0,0,12,12,0,0,12,12,0,0,0                             }, //':' 58
{0,0,0,12,12,0,0,12,12,12,24,0                           }, //';' 59
{0,12,24,48,96,192,96,48,24,12,0,0                       }, //'<' 60
{0,0,0,0,254,0,254,0,0,0,0,0                             }, //'=' 61
{0,96,48,24,12,6,12,24,48,96,0,0                         }, //'>' 62
{0,124,198,198,12,24,24,0,24,24,0,0                      }, //'?' 63
{0,124,198,198,222,222,222,220,192,126,0,0               }, //'@' 64
{0,56,108,198,198,198,254,198,198,198,0,0                }, //'A' 65
{0,252,102,102,102,124,102,102,102,252,0,0               }, //'B' 66
{0,60,102,192,192,192,192,192,102,60,0,0                 }, //'C' 67
{0,248,108,102,102,102,102,102,108,248,0,0               }, //'D' 68
{0,254,102,96,96,124,96,96,102,254,0,0                   }, //'E' 69
{0,254,102,96,96,124,96,96,96,240,0,0                    }, //'F' 70
{0,124,198,198,192,192,206,198,198,124,0,0               }, //'G' 71
{0,198,198,198,198,254,198,198,198,198,0,0               }, //'H' 72
{0,60,24,24,24,24,24,24,24,60,0,0                        }, //'I' 73
{0,60,24,24,24,24,24,216,216,112,0,0                     }, //'J' 74
{0,198,204,216,240,240,216,204,198,198,0,0               }, //'K' 75
{0,240,96,96,96,96,96,98,102,254,0,0                     }, //'L' 76
{0,198,198,238,254,214,214,214,198,198,0,0               }, //'M' 77
{0,198,198,230,230,246,222,206,206,198,0,0               }, //'N' 78
{0,124,198,198,198,198,198,198,198,124,0,0               }, //'O' 79
{0,252,102,102,102,124,96,96,96,240,0,0                  }, //'P' 80
{0,124,198,198,198,198,198,198,214,124,6,0               }, //'Q' 81
{0,252,102,102,102,124,120,108,102,230,0,0               }, //'R' 82
{0,124,198,192,96,56,12,6,198,124,0,0                    }, //'S' 83
{0,126,90,24,24,24,24,24,24,60,0,0                       }, //'T' 84
{0,198,198,198,198,198,198,198,198,124,0,0               }, //'U' 85
{0,198,198,198,198,198,198,108,56,16,0,0                 }, //'V' 86
{0,198,198,214,214,214,254,238,198,198,0,0               }, //'W' 87
{0,198,198,108,56,56,56,108,198,198,0,0                  }, //'X' 88
{0,102,102,102,102,60,24,24,24,60,0,0                    }, //'Y' 89
{0,254,198,140,24,48,96,194,198,254,0,0                  }, //'Z' 90
{0,124,96,96,96,96,96,96,96,124,0,0                      }, //'[' 91
{0,0,128,192,96,48,24,12,6,2,0,0                         }, //'\' 92
{0,124,12,12,12,12,12,12,12,124,0,0                      }, //']' 93
{16,56,108,198,0,0,0,0,0,0,0,0                           }, //'^' 94
{0,0,0,0,0,0,0,0,0,0,0,255                               }, //'_' 95
{24,24,24,12,0,0,0,0,0,0,0,0                             }, //'`' 96
{0,0,0,0,120,12,124,204,220,118,0,0                      }, //'a' 97
{0,224,96,96,124,102,102,102,102,252,0,0                 }, //'b' 98
{0,0,0,0,124,198,192,192,198,124,0,0                     }, //'c' 99
{0,28,12,12,124,204,204,204,204,126,0,0                  }, //'d' 100
{0,0,0,0,124,198,254,192,198,124,0,0                     }, //'e' 101
{0,28,54,48,48,252,48,48,48,120,0,0                      }, //'f' 102
{0,0,0,0,118,206,198,198,126,6,198,124                   }, //'g' 103
{0,224,96,96,108,118,102,102,102,230,0,0                 }, //'h' 104
{0,24,24,0,56,24,24,24,24,60,0,0                         }, //'i' 105
{0,12,12,0,28,12,12,12,12,204,204,120                    }, //'j' 106
{0,224,96,96,102,108,120,108,102,230,0,0                 }, //'k' 107
{0,56,24,24,24,24,24,24,24,60,0,0                        }, //'l' 108
{0,0,0,0,108,254,214,214,198,198,0,0                     }, //'m' 109
{0,0,0,0,220,102,102,102,102,102,0,0                     }, //'n' 110
{0,0,0,0,124,198,198,198,198,124,0,0                     }, //'o' 111
{0,0,0,0,220,102,102,102,124,96,96,240                   }, //'p' 112
{0,0,0,0,118,204,204,204,124,12,12,30                    }, //'q' 113
{0,0,0,0,220,102,96,96,96,240,0,0                        }, //'r' 114
{0,0,0,0,124,198,112,28,198,124,0,0                      }, //'s' 115
{0,48,48,48,252,48,48,48,54,28,0,0                       }, //'t' 116
{0,0,0,0,204,204,204,204,204,118,0,0                     }, //'u' 117
{0,0,0,0,198,198,198,108,56,16,0,0                       }, //'v' 118
{0,0,0,0,198,198,214,214,254,108,0,0                     }, //'w' 119
{0,0,0,0,198,108,56,56,108,198,0,0                       }, //'x' 120
{0,0,0,0,198,198,198,206,118,6,198,124                   }, //'y' 121
{0,0,0,0,254,140,24,48,98,254,0,0                        }, //'z' 122
{0,14,24,24,24,112,24,24,24,14,0,0                       }, //'{' 123
{0,24,24,24,24,0,24,24,24,24,0,0                         }, //'|' 124
{0,112,24,24,24,14,24,24,24,112,0,0                      }, //'}' 125
{0,118,220,0,0,0,0,0,0,0,0,0                             }, //'~' 126
{0,0,0,16,56,56,108,108,254,0,0,0                        }, //' ' 127
*/



/*
static const char BitmapSmall[][18]  = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0,
  0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0,
  0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0,
  0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1, 0, 0,
  0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0,
  0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0,
  0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0,
  0, 0, 0, 0, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};
*/
