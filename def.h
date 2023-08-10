#ifndef DEF
#define DEF

typedef union
{
  char Byte;
  
  struct
  {
    char	_0	: 1;
    char	_1	: 1;
    char	_2	: 1;
    char	_3	: 1;
    char	_4	: 1;
    char	_5	: 1;
    char	_6	: 1;
    char	_7	: 1;
  } Bit;
}tREG08;

typedef union
{
  unsigned short Short;
  
  struct
  {
    char	_0	: 1;
    char	_1	: 1;
    char	_2	: 1;
    char	_3	: 1;
    char	_4	: 1;
    char	_5	: 1;
    char	_6	: 1;
    char	_7	: 1;
    char	_8	: 1;
    char	_9	: 1;
    char	_10	: 1;
    char	_11	: 1;
    char	_12	: 1;
    char	_13	: 1;
    char	_14	: 1;
    char	_15	: 1;
  } Bit;
}tREG16;


typedef union
{
	unsigned long Long;

	struct
	{ // Big endian
		char ll;
		char lh;
		char hl;
		char hh;
	} Byte;
}tLONG;

#endif