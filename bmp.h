#ifndef BMP_H
#define BMP_H

#include <fstream>
#include <iostream>

typedef unsigned __int16 WORD;
typedef unsigned __int32 DWORD;
typedef unsigned char BYTE;
typedef long LONG;

#pragma pack(push, 1)
struct RGB
{
	BYTE red;
	BYTE green;
	BYTE blue;
};
#pragma pack(pop)

RGB* openConvert(const char *fileName, unsigned& height, unsigned& width, unsigned long long& size);
#endif