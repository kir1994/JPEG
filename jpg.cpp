#define _USE_MATH_DEFINES
#include <math.h>
#include <cstring>
#include <cstdlib>
#include <vector>
#include "jpg.h"
#include <algorithm>

using namespace std;
//JPEG headers
#pragma pack(push, 1)
struct JPEGFrameHeader
{
	const WORD SOF0 = 0xFFC0;
	const WORD Lf = 17;//8 + 3 * Nf;
	const BYTE P = 0x08;
	WORD Y;
	WORD X;
	const BYTE Nf = 0x03;
	const BYTE C1 = 0x01; // Luminance
	const BYTE HV1 = 0x11;
	const BYTE Tq1 = 0x00;
	const BYTE C2 = 0x02; // Chrominance Cb
	const BYTE HV2 = 0x11;
	const BYTE Tq2 = 0x01;
	const BYTE C3 = 0x03; // Chrominance Cr
	const BYTE HV3 = 0x11;
	const BYTE Tq3 = 0x01;
	JPEGFrameHeader(WORD y, WORD x) : X(x), Y(y) {}
};
struct JPEGScanHeader
{
	const WORD SOS = 0xFFDA;
	const WORD Lf = 0x000C;//6 + 2 * Ns;
	const BYTE Ns = 0x03;
	const BYTE Cs1 = 0x01;
	const BYTE Tda1 = 0x00;
	const BYTE Cs2 = 0x02;
	const BYTE Tda2 = 0x11;
	const BYTE Cs3 = 0x03;
	const BYTE Tda3 = 0x11;
	const BYTE Ss = 0x00;
	const BYTE Se = 0x3F;
	const BYTE Ahl = 0x00;
};
#pragma pack(pop)
// JPEG Markers
const WORD SOI = 0xFFD8;
const WORD DQT = 0xFFDB;
const WORD DHT = 0xFFC4;
const WORD EOI = 0xFFD9;
// Huffman tables data for writing to image
BYTE huffData[huffLen];
// Quantization tables
// Luminance
unsigned char qTableY[] =
{ 
	16, 11, 10, 16, 24, 40, 51, 61,
	12, 12, 14, 19, 26, 58, 60, 55,
	14, 13, 16, 24, 40, 57, 69, 56,
	14, 17, 22, 29, 51, 87, 80, 62,
	18, 22, 37, 56, 68, 109, 103, 77,
	24, 36, 55, 64, 81, 104, 113, 92,
	49, 64, 78, 87, 103, 121, 120, 101,
	72, 92, 95, 98, 112, 100, 103, 99 
};
//Chrominance
unsigned char qTableC[] =
{
	17, 18, 24, 47, 99, 99, 99, 99,
	18, 21, 26, 66, 99, 99, 99, 99,
	24, 26, 56, 99, 99, 99, 99, 99,
	47, 66, 99, 99, 99, 99, 99, 99,
	99, 99, 99, 99, 99, 99, 99, 99,
	99, 99, 99, 99, 99, 99, 99, 99,
	99, 99, 99, 99, 99, 99, 99, 99,
	99, 99, 99, 99, 99, 99, 99, 99,
};
// Huffman codes' tables
// DC values (upper-left corner value in each 8*8 block)
HuffEntry huffTable_DC_0[] = {
	{ 2, 0x0 },
	{ 3, 0x2 },
	{ 3, 0x3 },
	{ 3, 0x4 },
	{ 3, 0x5 },
	{ 3, 0x6 },
	{ 4, 0xE },
	{ 5, 0x1E },
	{ 6, 0x3E },
	{ 7, 0x7E },
	{ 8, 0xFE },
	{ 9, 0x1FE },
};//Luminance
HuffEntry huffTable_DC_1[] = {
	{ 2, 0x0 },
	{ 2, 0x1 },
	{ 2, 0x2 },
	{ 3, 0x6 },
	{ 4, 0xE },
	{ 5, 0x1E },
	{ 6, 0x3E },
	{ 7, 0x7E },
	{ 8, 0xFE },
	{ 9, 0x1FE },
	{ 10, 0x3FE },
	{ 11, 0x7FE },
};//Chrominance
// AC values (all 63 remaining coeffs of 8*8 block)
HuffEntry huffTable_AC_0[255] = {};//Luminance
HuffEntry huffTable_AC_1[255] = {};//Chrominance
// Table of coeffs for discrete cosine transform
float DCT_Table[64];
// temp array for DCT
float DCT_temp[64];
// array where DCT result will be placed
float DCT_res[64] = {};
// Functions for writing data to image in appropriate way
void writeWord(char *bytes, std::ofstream& out)
{
	std::vector<char> temp(bytes, bytes + sizeof(WORD));
	std::reverse(temp.begin(), temp.end());
	out.write(temp.data(), sizeof(WORD));
}
void writeFrameHeader(JPEGFrameHeader& header, std::ofstream& out)
{
	writeWord((char *)&(header.SOF0), out);
	writeWord((char *)&(header.Lf), out);
	out.write((char *)&(header.P), 1);
	writeWord((char *)&(header.Y), out);
	writeWord((char *)&(header.X), out);
	out.write((char *)&header + (4 * sizeof(WORD)+1), sizeof(JPEGFrameHeader)-(4 * sizeof(WORD)+1));
}
void writeScanHeader(JPEGScanHeader& header, std::ofstream& out)
{
	writeWord((char *)&(header.SOS), out);
	writeWord((char *)&(header.Lf), out);
	out.write((char *)&header + (2 * sizeof(WORD)), sizeof(JPEGScanHeader)-(2 * sizeof(WORD)));
}
// Computing DCT coeffs and putting them into table
void prepareDCT()
{
	unsigned i, j;
	for (i = 0; i < 8; ++i)
		DCT_Table[i] = sqrtf(2.) / 4;
	for (i = 1; i < 8; ++i)
		for (j = 0; j < 8; ++j)
			DCT_Table[i * 8 + j] = cos(M_PI * (i + 2 * i * j) / 16) / 2.;
}
// Discrete cosine transform function. DCT(F) = U*F*U^T, where F is 8*8 image data block and U is matrix(table) of DCT coeffs
void DCT(int *data, const unsigned& width)
{
	memset(DCT_res, 0, sizeof(float) * 64);
	memset(DCT_temp, 0, 64 * sizeof(float));
	unsigned i, j, k;
	for (i = 0; i < 8; ++i)
		for (j = 0; j < 8; ++j)
			for (k = 0; k < 8; ++k)
				DCT_temp[i * 8 + j] += DCT_Table[i * 8 + k] * data[k * width + j];
	for (i = 0; i < 8; ++i)
		for (j = 0; j < 8; ++j)
			for (k = 0; k < 8; ++k)
				DCT_res[i * 8 + j] += DCT_temp[i * 8 + k] * DCT_Table[j * 8 + k];
}
// Making DCT and quantization steps of JPEG encoding
void DCT_Quantize(int *data, const unsigned& width, const unsigned char table[64])
{
	unsigned i, j;
	DCT(data, width);
	for (i = 0; i < 8; ++i)
		for (j = 0; j < 8; ++j)
			data[i * width + j] = lround(DCT_res[i * 8 + j] / table[i * 8 + j]);
}
// Placing values from 8*8 matrix into zig-zag way
template<class T>
void zigzag(T *data, const unsigned& width, T seq[64])
{
	int i = 0;
	int j = 0;
	unsigned cnt = 0;
	//vec.push_back(data[i * width + j]);
	seq[cnt++] = data[i * width + j];
	while (i != 7 || j != 7)
	{
		if (j != 7)
		{
			++j;
			//vec.push_back(data[i * width + j]);
			seq[cnt++] = data[i * width + j];
		}
		else
		{
			++i;
			//vec.push_back(data[i * width + j]);
			seq[cnt++] = data[i * width + j];
		}
		while (i < 7 && j > 0)
		{
			++i;
			--j;
			//vec.push_back(data[i * width + j]);
			seq[cnt++] = data[i * width + j];
		}
		if (i != 7)
		{
			++i;
			//vec.push_back(data[i * width + j]);
			seq[cnt++] = data[i * width + j];
		}
		else
		{
			++j;
			//vec.push_back(data[i * width + j]);
			seq[cnt++] = data[i * width + j];
		}
		while (i > 0 && j < 7)
		{
			--i;
			++j;
			//vec.push_back(data[i * width + j]);
			seq[cnt++] = data[i * width + j];
		}
	}
}
// Data for bit-writing into image
unsigned char _acc = 0;
unsigned char _len = 0;
// Write some bits into file specified by ofstream. Adding stuff byte is included
void addBits(unsigned bits, unsigned char numOfBits, ofstream &out)
{
	unsigned char difference = CAPACITY - _len;
	if (numOfBits <= difference)
	{
		_acc = (_acc << numOfBits) | (bits & ((1 << numOfBits) - 1));
		_len += numOfBits;
		if (_len == CAPACITY)
		{
			_len = 0;
			out.write((char *)&_acc, 1);
			if (_acc == BYTE_MASK)
				out.write((char *)&_len, 1);
			_acc = 0;
		}
	}
	else
	{
		_acc = (_acc << difference) | ((bits >> (numOfBits - difference)) & ((1 << difference) - 1));
		numOfBits -= difference;
		_len = 0;
		out.write((char *)&_acc, 1);
		if (_acc == BYTE_MASK)
			out.write((char *)&_len, 1);
		_acc = 0;
		unsigned char temp;
		for (; numOfBits >= CAPACITY; numOfBits -= CAPACITY)
		{
			temp = ((bits >> (numOfBits - CAPACITY)) & BYTE_MASK);
			out.write((char *)&temp, 1);
			if (temp == BYTE_MASK)
				out.write((char *)&_len, 1);
		}
		if (numOfBits > 0)
		{
			_len = numOfBits;
			_acc = (bits & ((1 << numOfBits) - 1));
		}
	}
}
// Finish writing, write remaining bits and align to byte by adding zeros;
void finish(ofstream& out)
{
	if (_len)
	{
		unsigned char difference = CAPACITY - _len;
		_acc = (_acc << difference);
		_len = 0;
		out.write((char *)&_acc, 1);
		_acc = 0;
	}
	out.flush();
}
// Encode block of 64 values with Huffman entropy coding algorithm
void encodeBlock(int& prev, int seq[64], HuffEntry DC[12], HuffEntry AC[255], ofstream& out)
{
	// AC		
	unsigned cnt;
	BYTE zeroCount, tempCount;
	int difference = seq[0] - prev;
	unsigned char group = log2(abs(difference)) + 1;
	HuffEntry tmp = DC[group];
	// Huff code writing into file + additional bits
	addBits(tmp.code, tmp.len, out);
	if (difference > 0)
		addBits(difference, group, out);
	else if (difference < 0)
		addBits(~abs(difference), group, out);
	prev = seq[0];
	// AC
	for (cnt = 1; cnt < 64;)
	{
		zeroCount = 0;
		while (cnt + zeroCount < 64 && seq[cnt + zeroCount] == 0) zeroCount++;
		if (cnt + zeroCount >= 64)
		{
			// Write 0x00 codeword
			tmp = AC[0x00];
			addBits(tmp.code, tmp.len, out);
			break;
		}else
		{
			tempCount = zeroCount;
			while (tempCount > 0xF)
			{
				//Writing 0xF0 codeword
				tmp = AC[0xF0];
				addBits(tmp.code, tmp.len, out);
				tempCount -= 0x10;
			}
			group = log2(abs(seq[cnt + zeroCount])) + 1;
			tmp = AC[(tempCount << 4) | group];
			// Huff code writing into file + additional bits
			addBits(tmp.code, tmp.len, out);
			if (seq[cnt + zeroCount] > 0)
				addBits(seq[cnt + zeroCount], group, out);
			else
				addBits(~abs(seq[cnt + zeroCount]), group, out);
			cnt += (zeroCount + 1);
		}
	}
}
// Converts into YCbCr color space from RGB and encodes data
void convertFromRGB(RGB *data, unsigned height, unsigned width, std::ofstream& out)
{
#ifdef	ALLOW_UNALIGNED
	unsigned temp_width = width % 8 ? width / 8 * 8 + 8 : width,
			temp_height = height % 8 ? height / 8 * 8 + 8 : height;
#endif
	cout << "Preparing for jpeg encoding...";
#ifndef	ALLOW_UNALIGNED
	int *y = new int[width * height]();
	int *Cb = new int[width * height]();
	int *Cr = new int[width * height]();
#else
	int *y = new int[temp_width * temp_height]();
	int *Cb = new int[temp_width * temp_height]();
	int *Cr = new int[temp_width * temp_height]();
#endif
	if (y == nullptr || Cb == nullptr || Cr == nullptr)
	{
		cerr << "Error: Bad memory alloc\n";
		return;
	}
	cout << "Done\n";
	cout << "Converting from RGB to YCbCr...";
	double r, g, b;
	for (unsigned i = 0; i < height; ++i)
	{
		for (unsigned j = 0; j < width; ++j)
		{
			r = data[i * width + j].red;
			g = data[i * width + j].green;
			b = data[i * width + j].blue;
#ifndef	ALLOW_UNALIGNED				
			y[i * width + j] = lround(.299 * r + .587 * g + .114 * b);
			Cb[i * width + j] = lround((b - y[i * width + j]) / 1.772);
			Cr[i * width + j] = lround((r - y[i * width + j]) / 1.402);
			y[i * width + j] -= 128;
#else
			y[i * temp_width + j] = lround(.299 * r + .587 * g + .114 * b);
			Cb[i * temp_width + j] = lround((b - y[i * temp_width + j]) / 1.772);
			Cr[i * temp_width + j] = lround((r - y[i * temp_width + j]) / 1.402);
			y[i * temp_width + j] -= 128;
#endif
		}
	}
#ifdef	ALLOW_UNALIGNED	
	width = temp_width;
	height = temp_height;
#endif
	cout << "Done\n";
	cout << "Convertion start...";
	int prevY = 0, prevCb = 0, prevCr = 0;
	int seqArray[64];
	unsigned long long cnt = 0;
	unsigned long long size = (width * height) / 64;
	unsigned long outWidth = log10(size) + 1;
	unsigned blocksPerLine = width / 8;
	unsigned char outCnt = 0;
	unsigned magicNum = 30;
	cout << "Done\n";
	//perform DCT & quantize every 8*8 block(assume width and height divide by 8)
	for (unsigned i = 0; i < height; i += 8)
	{
#ifndef TEST_MODE
		if (outCnt == 0)
		{
			cout << "\rConversion: " << setw(outWidth) << cnt << setw(1) << "/" << setw(outWidth) << size << " blocks handled...";
			cout.flush();
		}
#endif
		for (unsigned j = 0; j < width; j += 8)
		{
			DCT_Quantize(y + i * width + j, width, qTableY);
			zigzag(y + i * width + j, width, seqArray);
			encodeBlock(prevY, seqArray, huffTable_DC_0, huffTable_AC_0, out);
			DCT_Quantize(Cb + i * width + j, width, qTableC);
			zigzag(Cb + i * width + j, width, seqArray);
			encodeBlock(prevCb, seqArray, huffTable_DC_1, huffTable_AC_1, out);
			DCT_Quantize(Cr + i * width + j, width, qTableC);
			zigzag(Cr + i * width + j, width, seqArray);
			encodeBlock(prevCr, seqArray, huffTable_DC_1, huffTable_AC_1, out);
		}
#ifndef TEST_MODE
		cnt += blocksPerLine;
		outCnt++;
		if (outCnt == magicNum)
			outCnt = 0;
#endif
	}
	cout << "\rConversion: " << setw(outWidth) << cnt << setw(1) << "/" << setw(outWidth) << size << " blocks handled...";
	cout.flush();
	finish(out);
	delete[] y;
	delete[] Cb;
	delete[] Cr;
	cout << "Done\n";
}
// Set JPEG quality by multiplying quantization table by quality-specified value (some strange 'heuristic' used)
void setQuality(const char& quality)
{
	float tmp;
	float temp_res;
	if (quality != 100 && quality != 50)
	{
		if (quality < 50)
		{
			tmp = -254. / 49 * quality + 12749. / 49;
			//float(quality) / 25 - 1;
			for (unsigned i = 0; i < 8; ++i)
				for (unsigned j = 0; j < 8; ++j)
				{
					temp_res = float(qTableY[i * 8 + j]) * tmp;
					if (temp_res > BYTE_MASK)
						qTableY[i * 8 + j] = BYTE_MASK;
					else
						qTableY[i * 8 + j] = temp_res;
					temp_res = float(qTableC[i * 8 + j]) * tmp;
					if (temp_res > BYTE_MASK)
						qTableC[i * 8 + j] = BYTE_MASK;
					else
						qTableC[i * 8 + j] = temp_res;
				}
		}
		else
		{
			tmp = 99. / 50 * quality - 98;
			//quality - float(quality) / 5;
			for (unsigned i = 0; i < 8; ++i)
				for (unsigned j = 0; j < 8; ++j)
				{
					temp_res = float(qTableY[i * 8 + j]) / tmp;
					if (lround(temp_res) > 0)
						qTableY[i * 8 + j] = temp_res;
					else
						qTableY[i * 8 + j] = 1;
					temp_res = float(qTableC[i * 8 + j]) / tmp;
					if (lround(temp_res) > 0)
						qTableC[i * 8 + j] = temp_res;
					else
						qTableC[i * 8 + j] = 1;
				}
		}
	}
	else if (quality == 100)
	{
		for (unsigned i = 0; i < 8; ++i)
			for (unsigned j = 0; j < 8; ++j)
			{
				qTableY[i * 8 + j] = 1;
				qTableC[i * 8 + j] = 1;
			}
	}
}
// Main function. Opens file stream, writes additional information and starts convertation
bool convertWrite(const char *fileName, unsigned height, unsigned width, RGB *data, unsigned long long& size, const char& quality)
{
	cout << "\nOpening jpeg file...";
	if (fileName == nullptr)
	{
		std::cerr << "Error: No output file name specified\n";
		return false;
	}
	std::ofstream out(fileName, std::ios::binary);
	if (!out)
	{
		std::cerr << "Error: Cannot create output file\n";
		return false;
	}
	cout << "Done\n";
	setQuality(quality);
	cout << "Writing SOI marker and quantization tables data...";
	//SOI 
	writeWord((char *)&SOI, out);
	//Quant Table write
	writeWord((char *)&DQT, out);
	WORD Lq = 132;
	writeWord((char *)&Lq, out);
	BYTE PT = 0x00;
	out.write((char *)&PT, sizeof(BYTE));
	unsigned char tmp[64];
	zigzag(qTableY, 8, tmp);
	out.write((char *)tmp, 64);
	PT = 0x01;
	out.write((char *)&PT, sizeof(BYTE));
	zigzag(qTableC, 8, tmp);
	out.write((char *)tmp, 64);
	//end of Quant Table
	cout << "Done\n";
	cout << "Writing frame header data...";
	// Frame header writing

//#ifdef	ALLOW_UNALIGNED
//	unsigned temp_width = width % 8 ? width / 8 * 8 + 8 : width,
//		temp_height = height % 8 ? height / 8 * 8 + 8 : height;
//	//JPEGFrameHeader fHeader = JPEGFrameHeader(temp_height, temp_width);
//#endif
	JPEGFrameHeader fHeader = JPEGFrameHeader(height, width);

	writeFrameHeader(fHeader, out);
	cout << "Done\n";
	cout << "Writing huffman tables data...";
	// Huffman table writing
	writeWord((char *)&DHT, out);
	out.write((char *)huffData, huffLen);
	cout << "Done\n";
	cout << "Writing scan header data...";
	// Scan header write
	JPEGScanHeader fScanHeader;
	writeScanHeader(fScanHeader, out);
	cout << "Done\n";
	convertFromRGB(data, height, width, out);
	cout << "Finishing...";
	writeWord((char *)&EOI, out);
	size = out.tellp();
	out.close();
	cout << "Done\n";
	cout << "Output file size: " << size << " bytes\n";
	return true;
}