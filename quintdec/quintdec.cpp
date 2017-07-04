// quintdec.cpp : Defines the entry point for the console application.
//
#include "stdafx.h"
#include "lz.h"

void write_u24(u8 *rom, u32 w)
{
	rom[0] = w & 0xFF;
	rom[1] = (w & 0xFF00) >> 8;
	rom[2] = w >> 16;
}

int main()
{
	CBufferFile file(_T("..\\sb.sfc"));
	u8 *rom = (u8*)file.GetBuffer(),
		*dst, *ddst;

	u32 size = Decompress(&rom[/*0xBA320*/0xB91B8], dst);
	FlushFile(_T("font.bin"), dst, size);

	u32 ssze = Compress(ddst, dst, size);
	FlushFile(_T("font.lzs"), ddst, ssze);

	write_u24(&rom[0x2834B], 0x100000);

	MEM_STREAM str;
	MemStreamOpen(&str, rom, file.GetSize());
	// expand rom
	MemStreamSeek(&str, 0x108000-1, SEEK_SET);
	MemStreamWriteByte(&str, 0);
	// write new data
	MemStreamSeek(&str, 0x100000, SEEK_SET);
	MemStreamWrite(&str, ddst, ssze);

	MemStreamFlush(&str, _T("..\\test.smc"));

	delete[] dst;
	delete[] ddst;

    return 0;
}

