#include <windows.h>
#include "buffer.h"

#define _DEBUG(x) MessageBoxA (NULL, "Debug: " x, "Debug", MB_OK | MB_ICONASTERISK);

int writeRingBuffer (ringBuffer buffer, BYTE* data, size_t size)
{
	BYTE* temp;
	size_t len;
	
	char tmp[20];
	
	if (size == 0)
		return 0;
	
	if (!data)
		return -1;
	
	WaitForSingleObject (buffer->mutex, INFINITE);
	
	/* if necessary, try to enlarge */
	temp = buffer->write + size;
	
	/* well, the 'is equal' case would be allowed, too, but this makes reading
	 * a lot easier, 'cause we don't have to worry what happens if read == write. */
	if (buffer->write >= buffer->read)
	{
		if (temp > buffer->end)
		{		
			while (temp + (buffer->base - buffer->end) >= buffer->read)
			{
				len = buffer->end - buffer->base;
				if (len == buffer->maxSize)
				{
					ReleaseMutex (buffer->mutex);
					return 0;
				}
				
				len *= 2;
				if (len > buffer->maxSize)
					len = buffer->maxSize;
				
				temp = realloc (buffer->base, len);
				if (!temp)
				{
					ReleaseMutex (buffer->mutex);
					return -1;
				}
				
				//memcpy (temp, buffer->base, buffer->write - buffer->base);
				
				if (temp != buffer->base)
				{
					buffer->read += temp - buffer->base;
					buffer->ahead += temp - buffer->base;
					buffer->write += temp - buffer->base;
					
					buffer->base = temp;
				}
				buffer->end = temp + len;
				
				temp = buffer->write + size;
			}
		}
	
		/* without overflow ... */
		if (temp <= buffer->end)
		{
			memcpy (buffer->write, data, size);
			buffer->write = temp;
			
			ReleaseMutex (buffer->mutex);
			
			return size;
		}	
	
		/* ... and with */
		memcpy (buffer->write, data, buffer->end - buffer->write);
		data += buffer->end - buffer->write;
		
		len = size - (buffer->end - buffer->write);
		
		memcpy (buffer->base, data, len);
		buffer->write = buffer->base + len;
	
		ReleaseMutex (buffer->mutex);
		return size;
	}
	else
	{
		if (temp < buffer->read)
		{
			memcpy (buffer->write, data, size);
			buffer->write = temp;
			
			ReleaseMutex (buffer->mutex);
			
			return size;
		}
	}
	
	ReleaseMutex (buffer->mutex);
	return 0;
}

int readRingBuffer (ringBuffer buffer, BYTE** pData, size_t size)
{
	BYTE* temp;
	size_t len;
	
	if (size == 0)
		return 0;
	
	if (!buffer)
		return -1;
	
	WaitForSingleObject (buffer->mutex, INFINITE);
	
	if (buffer->write > buffer->ahead)
	{
		if (buffer->ahead + size <= buffer->write)
		{
			*pData = buffer->ahead;
			buffer->ahead += size;
			
			ReleaseMutex (buffer->mutex);
			return size;
		}
		
		ReleaseMutex (buffer->mutex);
		return 0;
	}
	
	if (buffer->write < buffer->ahead)
	{
		if (buffer->ahead + size <= buffer->end)
		{
			*pData = buffer->ahead;
			buffer->ahead += size;
			
			ReleaseMutex (buffer->mutex);
			return size;
		}
		
		/* But I think reading to .write is no problem, 'cause writing always goes ahead,
		 * but we wouldn't know wether we have to read ahead or there's no data right know.
		 * We'd better treet it as no data there, and one 'round' in buffer would get lost. */
		if (buffer->ahead + size - buffer->end + buffer->base <= buffer->write)
		{
			if (buffer->bufferSize < size)
			{
				free (buffer->buffer);
				
				buffer->buffer = malloc (size);
				if (!buffer->buffer)
				{
					ReleaseMutex (buffer->mutex);
					return -1;
				}
				
				buffer->bufferSize = size;
			}
			
			temp = buffer->buffer;
			
			memcpy (temp, buffer->ahead, buffer->end - buffer->ahead);
			temp += buffer->end - buffer->ahead;
			
			len = size - (buffer->end - buffer->ahead);
			
			memcpy (temp, buffer->base, len);
			buffer->ahead = buffer->base + len;
			
			*pData = buffer->buffer;
			
			ReleaseMutex (buffer->mutex);
			return size;
		}
	}
	
	ReleaseMutex (buffer->mutex);
	return 0;
}

	/* Well, the name could be better, but this is used for making the read space
	 * available for the buffer again. */
int freeRingBuffer (ringBuffer buffer)
{
	if (!buffer)
		return -1;
		
	WaitForSingleObject (buffer->mutex, INFINITE);
	
	buffer->read = buffer->ahead;
	
	ReleaseMutex (buffer->mutex);
	return 0;
}


ringBuffer createRingBuffer (size_t initSize, size_t maxSize)
{
	ringBuffer buffer;	
	
	/* resizing doesn't work properly yet, cause base pointer always moves ... */
	initSize = maxSize;
	
	buffer = malloc (sizeof (struct _ringBuffer));
	if (!buffer)
	{
		CloseHandle (buffer->mutex);
		return NULL;
	}
		
	memset (buffer, 0, sizeof (struct _ringBuffer));
	
	
	buffer->mutex = CreateMutex (NULL, TRUE, NULL);
	if (!buffer->mutex)
	{
		free (buffer);
		return NULL;
	}
	
	
	buffer->base = malloc (initSize);
	if (!buffer->base)
	{
		CloseHandle (buffer->mutex);
		
		free (buffer);
		return NULL;
	}
		
	buffer->end = buffer->base + initSize;
	
	buffer->read = buffer->base;
	buffer->ahead = buffer->read;
	buffer->write = buffer->base;
	
	buffer->maxSize = maxSize;
	
	
	buffer->bufferSize = initSize / 8;
	
	buffer->buffer = malloc (buffer->bufferSize);
	if (!buffer->buffer)
	{
		CloseHandle (buffer->mutex);
		
		free (buffer->base);
		free (buffer);
		return NULL;
	}
	
	ReleaseMutex (buffer->mutex);
	return buffer;
}

void destroyRingBuffer (ringBuffer buffer)
{	
	WaitForSingleObject (buffer->mutex, INFINITE);
	
	free (buffer->buffer);
	free (buffer->base);
	
	CloseHandle (buffer->mutex);
	
	free (buffer);
	
	return;
}

