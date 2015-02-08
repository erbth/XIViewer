#include <windows.h>

#ifndef _BUFFER_H_
#define _BUFFER_H_

typedef struct _ringBuffer* ringBuffer;
struct _ringBuffer
{
	BYTE* base;
	BYTE* end;
	
	BYTE* read;
	BYTE* ahead;
	BYTE* write;
	
	size_t maxSize;
	
	BYTE* buffer;
	size_t bufferSize;
	
	HANDLE mutex;
};


int writeRingBuffer (ringBuffer buffer, BYTE* data, size_t size);
int readRingBuffer (ringBuffer buffer, BYTE** pData, size_t size);
int freeRingBuffer (ringBuffer buffer);

ringBuffer createRingBuffer (size_t initSize, size_t maxSize);
void destroyRingBuffer (ringBuffer buffer);

#endif
