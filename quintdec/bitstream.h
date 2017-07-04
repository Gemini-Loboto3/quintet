#include <types.h>

typedef unsigned int UINT;
typedef unsigned char BYTE;

#define ER	1
#define OK	0

typedef struct tagBinBuffer
{
	u8* data;
	u8 byte;
	char end;
	short bit;
	u32 seek;
	u32 size;
} BIN_BUFFER;

void Buf_Bopen(BIN_BUFFER *buffer, u8 *data, int size);
void Buf_Bopen(BIN_BUFFER *buffer, BYTE *data);
void Buf_Bclose(BIN_BUFFER* buffer);
int Buf_Bread_I(u8 nb, BIN_BUFFER *buffer);
int Buf_Bread_M(u8 nb, BIN_BUFFER *buffer);
int Buf_Btell(BIN_BUFFER *buffer);
void Buf_Bwrite_I(UINT val, u8 nb, BIN_BUFFER *buffer);
void Buf_Bwrite_M(UINT val, u8 nb, BIN_BUFFER *buffer);
void Buf_Bflush(BIN_BUFFER *buffer);

class CBitStream
{
public:
	CBitStream()
	{
		memset(&buf, 0, sizeof(buf));
	}

	~CBitStream()
	{
		Buf_Bclose(&buf);
	}

	__inline void Open(u8 *data)
	{
		Buf_Bopen(&buf, data);
	}

	void Reserve(u32 size)
	{
		buf.data = new u8[size];
		buf.size = size;
	}

	u32 Copy(u8 *&copy)
	{
		copy = new u8[buf.seek];
		memcpy(copy, buf.data, buf.seek);
		return buf.seek;
	}

	__inline u32 Read_LE(int nbit)
	{
		return (u32)Buf_Bread_I(nbit, &buf);
	}

	__inline u32 Read_BE(int nbit)
	{
		return (u32)Buf_Bread_M(nbit, &buf);
	}

	__inline void Write_LE(u32 bit, int nbit)
	{
		Buf_Bwrite_I(bit, nbit, &buf);
	}

	u32 Get_read_count()
	{
		return buf.seek;
	}

	__inline void Flush()
	{
		Buf_Bflush(&buf);
	}

private:
	BIN_BUFFER buf;
};
