#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "buffer.h"

#define RING_BUFFER_LEN 10485760
#define RING_BUFFER_START 1048576

int main(int argc, char *argv[])
{
	ringBuffer someBuffer;
	
	char* buffer;
	char* temp;
	int len, i, ret;
	int error = 0;
	
	len = 768 * 576 * 3;
	buffer = malloc (len);
	if (!buffer)
	{
		fprintf (stderr, "seems like there's no memory left ...\n");
		error = EXIT_FAILURE;
		goto theEnd;
	}
	
	for (i = 0; i < len; i++)
	{
		buffer[i] = i % 255;
	}
	
	someBuffer = createRingBuffer (RING_BUFFER_START, RING_BUFFER_LEN);
	
	//writeRingBuffer (someBuffer, buffer, len);
	
	for (i = 0; i < 20; i++)
	{
		ret = writeRingBuffer (someBuffer, buffer, len);
		printf ("writeRingBuffer: %d, .write: %d (.end: %d), .base at %p\n",
			ret, someBuffer->write - someBuffer->base, someBuffer->end - someBuffer->base, someBuffer->base);
		
		if (ret != len)
			error = 1;
		
		ret = readRingBuffer (someBuffer, (BYTE**) &temp, len);
		printf ("\treadRingBuffer: %d, .ahead: %d (.read: %d)\n",
			ret, someBuffer->ahead - someBuffer->base, someBuffer->read - someBuffer->base);
		
		if (ret != len)
			error = 1;
	
		if (strncmp (buffer, temp, ret) != 0)
			printf ("----- read data doesn't match the written!\n");
		
		freeRingBuffer (someBuffer);
		
		printf ("\n");
	}
	

theEnd:
	if (someBuffer)
		destroyRingBuffer (someBuffer);
	
	if (buffer)
		free (buffer);
	
	printf ("\n---------------------------------------------------------------\n");
	if (error == 0)
		printf ("everything all right.\n");
	else
		fprintf (stderr, "well, something went wrong.\n");
	
	
	system("PAUSE");	
	
	if (error)
		return error;
		
	return EXIT_SUCCESS;
}
