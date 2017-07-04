#include "stdafx.h"
#include "bitstream.h"

// ************************************ //
// DECOMPRESSION						//
// ************************************ //
u32 Decompress(u8 *data, u8 *&dest)
{
	CBitStream b;

	b.Open(data);
	u32 size = b.Read_LE(16);
	dest = new u8[size];
	u8 *dd = dest;
	u8 window[256], wpos = 239;
	memset(window, 0x20, sizeof(window));

	for (u32 i = 0; i < size; )
	{
		// literal
		if (b.Read_LE(1))
		{
			u8 by = b.Read_LE(8);
			dest[i++] = by;
			if (i >= size) break;
			window[wpos++] = by;
		}
		// compressed
		else
		{
			u8 jmp = b.Read_LE(8);
			int len = b.Read_LE(4) + 2;

			for (int j = 0; j < len; j++)
			{
				dest[i++] = window[jmp];
				if (i >= size) break;
				window[wpos++] = window[jmp++];
			}
		}
	}

	int ssize = b.Get_read_count();

	return size;
}

// ************************************ //
// COMPRESSION							//
// ************************************ //
#define HAS_RINGBUF		1
#define LZ_MINLEN		2
#define LZ_JUMP			256
#define LZ_LENGTH		(15 + LZ_MINLEN)

typedef struct tagLzssData
{
	s16 LzByteStartTable[256];
	s16 LzByteEndTable[256];
	s16 LzNextOffsetTable[LZ_JUMP];
#if HAS_RINGBUF
	u8  LzWindow[LZ_JUMP];
#endif
	u32 WindowLen;
	u32 WindowPos;
	u32 InputSize;
	u32 InputPos;
} LZSS_DATA;

#if HAS_RINGBUF
static void LzssSlideByte(LZSS_DATA& Data, u8 InputByte);
#else
static void LzssSlideByte(LZSS_DATA& Data, u8 *Input, u8 InputPos);
#endif

static void LzssInit(LZSS_DATA& Data, u8* OutputBuffer, int Size)
{
	memset(Data.LzByteStartTable,	-1, sizeof(Data.LzByteStartTable));
	memset(Data.LzByteEndTable,		-1, sizeof(Data.LzByteEndTable));
	memset(Data.LzNextOffsetTable,	-1, sizeof(Data.LzNextOffsetTable));
	Data.WindowLen = 0;
	Data.WindowPos = 0;
	Data.InputSize = Size;
	Data.InputPos = 0;

	for(int i=0; i < 256; i++)
		LzssSlideByte(Data, 0x20);
	while (Data.WindowPos < 239)
		LzssSlideByte(Data, 0x20);
}

static void LzssSlideByte(LZSS_DATA& Data, u8 InputByte)
{
	int WindowInsertPos = 0;

	if (Data.WindowLen == LZ_JUMP)	// voll
	{
		// das byte was jetzt rausfliegt laden, dann den nächsten offset
		// für dieses byte als start setzen
		u8 OldByte = Data.LzWindow[Data.WindowPos];
		Data.LzByteStartTable[OldByte] = Data.LzNextOffsetTable[Data.LzByteStartTable[OldByte]];
		if (Data.LzByteStartTable[OldByte] == -1)	// gibts das byte jetzt nicht mehr?
			Data.LzByteEndTable[OldByte] = -1;		// dann ende zurücksetzen
	}
	WindowInsertPos = Data.WindowPos;

	// letzte vorkommnis des bytes laden
	int LastOffset = Data.LzByteEndTable[InputByte];
	 // byte kommt gar nicht vor
	if (LastOffset == -1) Data.LzByteStartTable[InputByte] = WindowInsertPos;		// die position als start schreiben
	else Data.LzNextOffsetTable[LastOffset] = WindowInsertPos;		// die position als next schreiben

	Data.LzByteEndTable[InputByte] = WindowInsertPos;		// ist jetzt das letzte vorkommnis des bytes
	Data.LzNextOffsetTable[WindowInsertPos] = -1;			// also kommt danach natElich auch nichts
	Data.LzWindow[Data.WindowPos] = InputByte;

	Data.WindowPos = (Data.WindowPos+1) % LZ_JUMP;
	// voll
	if (Data.WindowLen < LZ_JUMP)
		Data.WindowLen++;	// einfach eins weiter gehen
}

static void LzssAdvanceBytes(LZSS_DATA& Data, u8* Input, int num)
{
	for (int i = 0; i < num; i++)
		LzssSlideByte(Data, Input[Data.InputPos + i]);
	Data.InputPos += num;
}

bool RleSearchMatch(LZSS_DATA& Data, unsigned char* Input, int& ReturnSize)
{
	int InputPos = Data.InputPos;
	int MaxInput = Data.InputSize;

	int chain = 0;
	int maxLen = 0xFF + LZ_LENGTH + 1;
	while (InputPos+chain < MaxInput && chain < maxLen && Input[InputPos+chain] == Input[InputPos])
		chain++;

	if (chain < 2)
		return false;

	ReturnSize = chain;
	return true;
}

static bool LzssSearchMatch(LZSS_DATA& Data, u8* Input, int& ReturnPos, int& ReturnSize)
{
	int Offset = Data.LzByteStartTable[Input[Data.InputPos]];	// ersten offset dieses bytes laden
	int maxchain = 0;
	int maxpos = 0;
	int InputPos = Data.InputPos;
	int MaxInput = Data.InputSize;

	while (Offset != -1)
	{
		// position berechnen
		if (Offset == Data.WindowPos)
		{
			Offset = Data.LzNextOffsetTable[Offset];
			continue;
		}

		int searchPos = Offset;
		if (searchPos != Data.WindowPos)
		{
			int chain = 0;
			while (InputPos+chain < MaxInput)
			{
				if (Data.LzWindow[searchPos] !=  Input[InputPos+chain])
					break;

				searchPos = (searchPos+1) % LZ_JUMP;
				if (searchPos > (int)Data.WindowLen || searchPos == (int)Data.WindowPos)	// should handle the second...
					break;

				chain++;
				if (chain == LZ_LENGTH-1) break;
			}

			if (chain > maxchain)
			{
				maxchain = chain;
				maxpos = Offset;
				if (maxchain == LZ_LENGTH-1) break;
			}
		}

		Offset = Data.LzNextOffsetTable[Offset];	// nächsten offset dieses bytes laden
	}

	if (maxchain < LZ_MINLEN) return false;
	ReturnPos = maxpos;
	ReturnSize = maxchain;
	return true;
}

u32 Compress(u8* &out, u8* in, u32 src_size)
{
	int in_pos = 0;
	int len, jmp;

	LZSS_DATA CompressionData;
	CBitStream b;

	LzssInit(CompressionData, out, src_size);
	b.Reserve(src_size * 2);
	b.Write_LE(src_size, 16);

	for (; CompressionData.InputPos < src_size;)
	{
		// search back for data
		if (LzssSearchMatch(CompressionData, in, jmp, len))
		{
			b.Write_LE(0, 1);			// compression signal
			b.Write_LE(jmp /*+ 238*/, 8);		// jump
			b.Write_LE(len - LZ_MINLEN, 4);	// length
			LzssAdvanceBytes(CompressionData, in, len);
		}
		// no data found, write and increase length
		else
		{
			b.Write_LE(1, 1);			// literal signal
			b.Write_LE(in[CompressionData.InputPos], 8);	// out literal
			LzssAdvanceBytes(CompressionData, in, 1);
		}
	}
	b.Flush();

	return b.Copy(out);
}
