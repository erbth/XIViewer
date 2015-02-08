#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "buffer.h"

int main(int argc, char *argv[])
{
	ringBuffer someBuffer;
	char buffer[1024];
	BYTE* temp;
	size_t len;
	int ret, i;
	
	someBuffer = createRingBuffer (30, 1024);
	if (!someBuffer)
	{
		fprintf (stderr, "seems like there's no memory left ...\n");
		goto theEnd;
	}	
	
	strcpy (buffer, "this is all about ringBuffer");
	len = strlen (buffer);
	
	while ((ret = writeRingBuffer (someBuffer, buffer, len)) > 0)
	{
		printf ("writing %d bytes to ring buffer,\n\twrite: %d, end: %d\n",
			ret, someBuffer->write - someBuffer->base, someBuffer->end - someBuffer->base);
	}
	
	printf ("writeRingBuffer returned: %d\n", ret);
	
	for (i = 0; i < 2; i++)
	{
		ret = readRingBuffer (someBuffer, &temp, len);
		
		if (ret > 0)
		{
			buffer[ret] = 0;
			printf ("read %d bytes: %.*s\n", ret, ret, temp);
		}
		else
		{
			printf ("couldn't read any data from ring buffer.\n");
			goto theEnd;
		}
	}
	
	ret = writeRingBuffer (someBuffer, buffer, len);
	printf ("\ntried to write to ring buffer: %d\n", ret);
	
	if (ret != 0)
	{
		printf ("\twell, it should have returned 0.\n");
		goto theEnd;
	}
	
	printf ("calling freeRingBuffer ...\n");
	freeRingBuffer (someBuffer);
	
	for (i = 0; i < 2; i++)
	{
		ret = writeRingBuffer (someBuffer, buffer, len);
		printf ("wrote %d bytes to ring buffer,\n\twrite: %d, read: %d\n",
			ret, someBuffer->write - someBuffer->base, someBuffer->read - someBuffer->base);
	}
	
	ret = writeRingBuffer (someBuffer, buffer, len);
	printf ("tried to write to ring buffer: %d\n\n", ret);
	
	if (ret != 0)
	{
		printf ("\twell, it should have returned 0.\n");
		goto theEnd;
	}
		
	while ((ret = readRingBuffer (someBuffer, &temp, len)) > 0)
	{
		printf ("read %d bytes from ring buffer: %.*s\n\tahead: %d, read: %d\n",
			ret, ret, temp, someBuffer->ahead - someBuffer->base, someBuffer->read - someBuffer->base);
	}

	printf ("well, seems like everything's all right.\n");	
	
theEnd:
	if (someBuffer)
		destroyRingBuffer (someBuffer);
		
	system("PAUSE");	
	return 0;
}
