#include <string.h>
#include "bitstream.h"

/****************************************************************************************
* Funzioni di decompressione e compressione basate su buffer							*
****************************************************************************************/
void Buf_Bopen(BIN_BUFFER *buffer, BYTE *data, int size)
{
	buffer->data=data;
	buffer->end=OK;
	buffer->bit=0;
	buffer->byte=0;
	buffer->seek=0;
	buffer->size=size;
}

void Buf_Bopen(BIN_BUFFER *buffer, BYTE *data)
{
	buffer->data=data;
	buffer->end=OK;
	buffer->bit=0;
	buffer->byte=0;
	buffer->seek=0;
	buffer->size=-1;
}

void Buf_Bclose(BIN_BUFFER* buffer)
{
	if(buffer->size != (u32)-1 && buffer->size)
		delete[] buffer->data;
	memset(buffer,0,sizeof(BIN_BUFFER));
}

int Buf_Bread_I(BYTE nb, BIN_BUFFER *buffer)
{
	int val=0;
	char c=0;

	// verifica la validità dei parametri
	if(buffer->data && nb>0 && nb<=32)
	{
		while(nb>=8 && buffer->end==OK)
		{
			val+=Buf_Bread_M(8,buffer)<<(8*c);
			c++;
			nb-=8;
		}
		val+=Buf_Bread_M(nb,buffer)<<(8*c);
	}
	return val;
}

int Buf_Bread_M(BYTE nb, BIN_BUFFER *buffer)
{
	int val=0;
	char lus=0;

	// verifica la validità dei parametri
	if(buffer->data && nb>0 && nb<=32)
	{
		for (lus=0;lus<nb && buffer->end==OK;lus++)
		{
			buffer->bit--;
			if(buffer->bit<0)
			{
				buffer->byte=buffer->data[buffer->seek];
				buffer->seek++;
				// se è andato oltre le dimensioni del buffer
				if (buffer->seek>=buffer->size) buffer->end=ER;
				buffer->bit=val%256;
				buffer->bit=7;
			}
			// ottiene un bit
			val=val*2+((buffer->byte>>buffer->bit)%2);
		}
	}
	return(val);
}

int Buf_Btell(BIN_BUFFER *buffer)
{
	return(buffer->seek);
}

void Buf_Bwrite_I(UINT val, BYTE nb, BIN_BUFFER *buffer)
{
	char c=0;

	// verifica la validità dei parametri
	if(buffer->data && nb>0 && nb<=32)
	{
		while(nb>=8 && buffer->end==OK)
		{
			Buf_Bwrite_M((val>>(8*c)%256),8,buffer);
			c++;
			nb-=8;
		}
		Buf_Bwrite_M((val>>(8*c)%256),nb,buffer);
	}
}

void Buf_Bwrite_M(UINT val, BYTE nb, BIN_BUFFER *buffer)
{
	char ecr=0;

	// verifica la validità dei parametri
	if(buffer->data && nb>0 && nb<=32)
	{
		for (ecr=nb-1;ecr>=0;ecr--)
		{
			if(buffer->bit>7)
			{
				buffer->data[buffer->seek++]=buffer->byte;
				buffer->byte=0;
				buffer->bit=0;
			}
			// scrittura di un bit
			buffer->byte=buffer->byte*2+((val>>ecr)%2);
			// aggiorna il contatore di bit
			buffer->bit++;
		}
	}
}

void Buf_Bflush(BIN_BUFFER *buffer)
{
	// caso della fine di un byte
	if(buffer->bit>7)
	{
		buffer->data[buffer->seek++]=buffer->byte;
		buffer->byte=0;
		buffer->bit=0;
	}
	// scrittura del pezzo di byte mancante
	else
	{
		Buf_Bwrite_M(0,8-buffer->bit,buffer);
		Buf_Bflush(buffer);
	}
}

