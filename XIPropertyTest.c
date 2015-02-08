#define _UNICODE
#define UNICODE

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <LibXIStream/XIStream.h>
#include <LibXIProperty/XIProperty.h>

int main(int argc, char *argv[])
{
	int error = EXIT_SUCCESS;
	VIDEO_CAPTURE_INFO captureInfo;
	HANDLE videoCapture = NULL, videoProperty = NULL;
	HANDLE interfaceHand;
	
	BYTE buffer[4096], origin[2048];
	BYTE* temp;
	BYTE* output;
	int i;
	
	if (!XIS_Initialize ())
	{
		printf ("no capture device found.\n");
		goto theEnd;
	}
	
	if (!XIS_GetVideoCaptureInfo (0, &captureInfo))
	{
		fprintf (stderr, "could not retrieve information about our device.\n");
		error = EXIT_FAILURE;
		goto theEnd;
	}

	
	videoCapture = XIS_OpenVideoCapture (captureInfo.szDShowID);
	if (!videoCapture)
	{
		fprintf (stderr, "could not open capture device.\n");
		error = EXIT_FAILURE;
		goto theEnd;
	}

	videoProperty = XIS_OpenVideoCapturePropertyHandle (videoCapture);
	if (!videoProperty)
	{
		fprintf (stderr, "could not open property HANDLE.\n");
		error = EXIT_FAILURE;
		goto theEnd;
	}
	
	printf ("property HANDLE opened.\n");
	
	
theEnd:
	if (videoProperty)
		XIP_ClosePropertyHandle (videoProperty);
	
	if (videoCapture)
		XIS_CloseVideoCapture (videoCapture);
	
	XIS_Uninitialize ();
	
	if (error != EXIT_SUCCESS)
		fprintf (stderr, "------------------------------------------------------\n"
			"something went wrong ...\n");
	
	system("PAUSE");
	return error;
}
