#ifndef BITWRITER_H
#define BITWRITER_H

#include <fstream>

#define CAPACITY 8
#define BYTE_MASK 0xFF

class BitWriter
{
	std::ofstream *_out;
	unsigned char _acc;
	unsigned char _len;

public:
	BitWriter(std::ofstream *out) : _len(0), _acc(0), _out(out) { }

	void addBits(unsigned bits, unsigned char numOfBits)
	{
		unsigned char difference = CAPACITY - _len;
		if (numOfBits <= difference)
		{
			_acc = (_acc << numOfBits) | (bits & ((1 << numOfBits) - 1));
			_len += numOfBits;
			if (_len == CAPACITY)
			{
				_len = 0;				
				_out->write((char *)&_acc, 1);
				_acc = 0;
			}
		}
		else
		{
			_acc = (_acc << difference) | ((bits >> (numOfBits - difference)) & ((1 << difference) - 1));
			numOfBits -= difference;
			_len = 0;
			_out->write((char *)&_acc, 1);
			_acc = 0;
			unsigned char temp;
			for (; numOfBits >= CAPACITY; numOfBits -= CAPACITY)
			{
				temp = ((bits >> (numOfBits - CAPACITY)) & BYTE_MASK);
				_out->write((char *)&temp, 1);
			}
			if (numOfBits > 0)
			{
				_len = numOfBits;
				_acc = (bits & ((1 << numOfBits) - 1));
			}
		}
	}
	void finish()
	{
		unsigned char difference = CAPACITY - _len;
		_acc = (_acc << difference);
		_out->write((char *)&_acc, 1);
		_acc = 0;
		_len = 0;
		_out->flush();
	}
};

#endif