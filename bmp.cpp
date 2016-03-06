#include "bmp.h"

using namespace std;

#pragma pack(push, 2)
struct BMPHeader
{
	WORD    bfType;        // must be 'BM' 
	DWORD   bfSize;        // size of the whole .bmp file
	WORD    bfReserved1;   // must be 0
	WORD    bfReserved2;   // must be 0
	DWORD   bfOffBits;	   // position where actual data starts
};

struct BMPInfoHeader
{
	DWORD  biSize;            // size of the structure
	LONG   biWidth;           // image width
	LONG   biHeight;          // image height
	WORD   biPlanes;          // bitplanes
	WORD   biBitCount;        // resolution 
	DWORD  biCompression;     // compression
	DWORD  biSizeImage;       // size of the image
	LONG   biXPelsPerMeter;   // pixels per meter X
	LONG   biYPelsPerMeter;   // pixels per meter Y
	DWORD  biClrUsed;         // colors used
	DWORD  biClrImportant;    // important colors
};
#pragma pack(pop)

bool checkBMP(BMPHeader& header, BMPInfoHeader& info)
{
	return header.bfType == 'MB';
}

RGB* openConvert(const char *fileName, unsigned& height, unsigned& width, unsigned long long& size)
{
	cout << "\nOpening file....";
	if (fileName == nullptr)
	{
		cout << "Error: No input file name specified\n";
		return false;
	}
	ifstream in(fileName, ios::binary);
	if (!in)
	{
		cout << "Error: Cannot open file\n";
		return nullptr;
	}
	cout << "Done\n";
	char *data;
	BMPHeader header;
	BMPInfoHeader infoHeader;
	cout << "Fetching header...";
	in.read((char *)&header, sizeof(BMPHeader));
	in.read((char *)&infoHeader, sizeof(BMPInfoHeader));
	cout << "Done\n";
	cout << "Checking header...";
	if (!checkBMP(header, infoHeader))
	{
		in.close();
		cout << "Error: file is not BMP\n";
		return nullptr;
	}
	cout << "Done\n";
	width = infoHeader.biWidth;
	height = infoHeader.biHeight;
	in.seekg(header.bfOffBits, in.beg);
	unsigned dataSize = header.bfSize - header.bfOffBits;
	cout << "Preparing for reading file data...";
	data = new char[dataSize]();
	if (data == nullptr)
	{
		in.close();
		cout << "Error: Bad memory alloc\n";
		return nullptr;
	}
	cout << "Done\n";
	cout << "Checking header...";
	in.read(data, dataSize);
	in.close();
	cout << "Done\n";
	cout << "Preparing for converting to pure RGB...";
	unsigned padding = 0;
	unsigned scanlinebytes = width * 3;
	while ((scanlinebytes + padding) % 4 != 0)
		padding++;
	unsigned psw = scanlinebytes + padding;
	long bufpos = 0;
	long newpos = 0;
	BYTE *buffer = new BYTE[height * width * 3];
	if (buffer == nullptr)
	{
		cout << "Error: Bad memory alloc\n";
		return false;
	}
	cout << "Done\n";
	cout << "Converting to pure RGB...";
	for (unsigned y = 0; y < height; y++)
		for (unsigned x = 0; x < 3 * width; x += 3)
		{
			newpos = y * 3 * width + x;
			bufpos = (height - y - 1) * psw + x;

			buffer[newpos] = data[bufpos + 2];
			buffer[newpos + 1] = data[bufpos + 1];
			buffer[newpos + 2] = data[bufpos];
		}
	cout << "Done\n";
	size = height * width * 3 + sizeof(BMPHeader) + sizeof(BMPInfoHeader);
	cout << "Input file size: " << size << " bytes\n";
	delete[] data;
	return (RGB *)buffer;
}