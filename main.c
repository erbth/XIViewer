/** Just another viewer for Magewell's Video Capture Cards using their SDK
 * you can get it from http://www.magewell.com/sdk?lang=en
 *
 * much stuff about creating windows and their message loops was taken from the
 * Dev-C++ 'Windows Application' templates.
 *
 * Please feel free to modify and copy my code!
 * Thomas Erbesdobler <t.erbesdobler@team103.com>
 */


#include <windows.h>
#include <stdio.h>
#include <string.h>

#include <LibXIStream/XIStream.h>
#include <LibXIProperty/XIProperty.h>
#include <LibXIProperty/XIPHDCapture.h>

#include "buffer.h"
#include "resources.h"

#define INTERNAL_BUFFER_LEN 16384
#define INPUT_BUFFER_SIZE 26214400	// 25MB


#define ERROR_RESET_RENDERER 0x100
#define ERROR_WRITE_TO_RING_BUFFER 0x101
#define ERROR_GET_CAPTURE_INFO 0x102
#define ERROR_OPEN_CAPTURE 0x103
#define ERROR_SET_CAPTURE_FORMAT 0x104
#define ERROR_START_CAPTURE 0x105
#define ERROR_SET_CALLBACK 0x106
#define ERROR_MALLOC 0x107

#define MESSAGE_PREVIEW_UPDATE 0x100
#define MESSAGE_EXIT 0x1

#define ID_STANDING_ON_THE_BREAK 0x101
#define ID_DEVICE 0x400
#define ID_PROPERTY 0x102
#define ID_RESOLUTION 0x200
#define ID_SET_ASPECT_RATIO 0x103

#define ID_TIMER 0x500

#define _DEBUG(x) MessageBoxA (NULL, "Debug: " x, "Debug", MB_OK | MB_ICONASTERISK);

/* well, time passed and this structure got something like a context */
typedef struct _inputStream inputStream;
struct _inputStream
{
	int numDevices;
	VIDEO_CAPTURE_INFO_EX devices[10];
	
	HANDLE videoCapture;
	HANDLE videoProperty;
	int width, height, duration;
	
	HANDLE videoRenderer;
	
	HWND outputWindow;
	GUID outputGUID;
	HMENU deviceMenu;
	
	ringBuffer inputBuffer;
	
	HANDLE messageAvailable;
	HANDLE mutex;
	int message;
	
	BYTE* outputBuffer;
	int outputBufferSize;
	
	HANDLE thread;
	
	HMENU resolutionMenu;
	int resolution;
};

struct frameHeader
{
	int stride;
	size_t len;
};


/* some variables ... */
inputStream input;

/* ... and all the functions ... */
int ssse3_YUV420ToRGB_8u_P3AC4R(BYTE **pSrc, int *srcStep, BYTE *pDst, int dstStep, int width, int height);

void videoCaptureCallback (const BYTE* pbyImage, int cbImageStride, void* pvParam, UINT64 u64TimeStamp);

/* ... window like things ... */
void setResolution (inputStream* input)
{
	switch (input->resolution)
	{
		case 0:
			input->width = 640;
			input->height = 480;
			break;
		
		case 1:
			input->width = 768;
			input->height = 576;
			break;
		
		case 2:
			input->width = 1024;
			input->height = 768;
			break;
		
		case 3:
			input->width = 1280;
			input->height = 1024;
			break;
		
		case 4:
			input->width = 1920;
			input->height = 1080;
			break;
			
		case 5:
			input->width = 1920;
			input->height = 1200;
			break;
		
		default:
			input->width = 768;
			input->height = 576;
			break;
	}
	return;
}

/*  This function is called by the Windows function DispatchMessage()  */
LRESULT CALLBACK WindowProcedure (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)                  /* handle the messages */
    {
	/*	case WM_TIMER:
			if (wParam == ID_TIMER)
			{
				if (input.videoProperty)
				{
					if (XIPHD_IsSignalChanged (input.videoProperty) == S_OK)
						MessageBoxA (input.outputWindow, "HD signal changed.", "info", MB_OK | MB_ICONINFORMATION);
					
					if (XIPCVBS_IsSignalChanged (input.videoProperty) == S_OK)
						MessageBoxA (input.outputWindow, "CVBS signal changed.", "info", MB_OK | MB_ICONINFORMATION);
				}
			}
			
			break;*/
		
		case WM_COMMAND:
			if ((LOWORD (wParam) >= ID_RESOLUTION) && (LOWORD (wParam) <= ID_RESOLUTION + 100))
			{
				GUID monGUID;
				RECT rectangle;
				
				input.resolution = LOWORD (wParam) - ID_RESOLUTION;
				
				if (input.videoCapture && XIS_IsVideoCaptureStarted (input.videoCapture))
					XIS_StopVideoCapture (input.videoCapture);
				
				setResolution (&input);
				
				if (input.videoCapture)
				{
					if (!XIS_SetVideoCaptureFormat (input.videoCapture, XI_COLOR_I420, input.width, input.height, input.duration))
					{
						PostQuitMessage (ERROR_SET_CAPTURE_FORMAT);
						break;
					}
					
					/* yeah, we've got a video renderer ... */
					if (!XIS_GetMonitorGUIDFromWindow (hwnd, &monGUID))
					{
						PostQuitMessage (ERROR_RESET_RENDERER);
						break;
					}
					
					if (!XIS_ResetVideoRenderer (input.videoRenderer, &monGUID, hwnd,
						XI_COLOR_RGB32, input.width, input.height, FALSE, TRUE))
					{
						PostQuitMessage (ERROR_RESET_RENDERER);
						break;
					}
					
					GetClientRect (hwnd, &rectangle);
					XIS_SetVideoRendererPosition (input.videoRenderer, NULL, &rectangle);
					
					/* start again */
					if (!XIS_StartVideoCapture (input.videoCapture))
					{
						PostQuitMessage (ERROR_START_CAPTURE);
						break;
					}
					
					/* set aspect ratio */
					PostMessage (hwnd, WM_COMMAND, ID_SET_ASPECT_RATIO, 0);
					
				}
				
				break;
			}
			
			/* all about device selection ... */
			if (LOWORD (wParam) >= ID_DEVICE)
			{
				int duration;
				RECT rectangle;
				
				GUID monGUID;
				char buffer[100];
				
				if (input.videoCapture)
					break;
					
				input.videoCapture = XIS_OpenVideoCapture (input.devices[LOWORD (wParam) - ID_DEVICE].szDShowID);
				if (!input.videoCapture)
				{
					PostQuitMessage (ERROR_OPEN_CAPTURE);
					break;
				}
				
				/*input.videoProperty = XIS_OpenVideoCapturePropertyHandle (input.videoCapture);
				if (!input.videoProperty)
				{
					PostQuitMessage (ERROR_OPEN_CAPTURE);
					break;
				}*/
				
				setResolution (&input);
				
				if (input.outputBufferSize < input.width * input.height * 4)
				{					
					free (input.outputBuffer);
					
					input.outputBufferSize = input.width * input.height * 4;
					
					input.outputBuffer = malloc (input.outputBufferSize);
					if (!input.outputBuffer)
					{
						PostQuitMessage (ERROR_MALLOC);
						break;
					}
				}
				
				if (!XIS_SetVideoCaptureFormat (input.videoCapture, XI_COLOR_I420, input.width, input.height, input.duration))
				{
					PostQuitMessage (ERROR_SET_CAPTURE_FORMAT);
					break;
				}
				
				if (!XIS_SetVideoCaptureCallbackEx (input.videoCapture, &videoCaptureCallback, &input, TRUE))
				{
					PostQuitMessage (ERROR_SET_CALLBACK);
					break;
				}
				
				
				if (!XIS_GetMonitorGUIDFromWindow (hwnd, &monGUID))
				{
					PostQuitMessage (ERROR_RESET_RENDERER);
					break;
				}
				
				if (!XIS_ResetVideoRenderer (input.videoRenderer, &monGUID, hwnd,
					XI_COLOR_RGB32, input.width, input.height, FALSE, TRUE))
				{
					PostQuitMessage (ERROR_RESET_RENDERER);
					break;
				}
				
				
				GetClientRect (hwnd, &rectangle);
				XIS_SetVideoRendererPosition (input.videoRenderer, NULL, &rectangle);
				
				if (!XIS_StartVideoCapture (input.videoCapture))
				{
					PostQuitMessage (ERROR_START_CAPTURE);
					break;
				}
				
				/* set aspect ratio */
				PostMessage (hwnd, WM_COMMAND, ID_SET_ASPECT_RATIO, 0);
				
				break;
			}
			
			switch (LOWORD (wParam))
			{
				case ID_STANDING_ON_THE_BREAK:
					if (input.videoCapture)
					{
						if (XIS_IsVideoCaptureStarted (input.videoCapture))
							XIS_StopVideoCapture (input.videoCapture);
						
						XIS_CloseVideoCapture (input.videoCapture);
						
						input.videoCapture = NULL;
					}
					
					if (input.videoProperty)
					{
						XIP_ClosePropertyHandle (input.videoProperty);
						input.videoProperty = NULL;
					}
					break;
				
				case ID_PROPERTY:
					if (input.videoCapture)
						XIS_ShowVideoCapturePropertyDialog (input.videoCapture, input.outputWindow);
					break;
				
				case ID_SET_ASPECT_RATIO:
				{
					RECT client, window;
					char buffer[100];
					
					GetWindowRect (hwnd, &window);
					GetClientRect (hwnd, &client);
					
					window.right -= window.left;
					window.bottom -= window.top;
					
					client.right -= client.left;
					client.bottom -= client.top;
					
					window.right -= client.right;
					window.bottom -= client.bottom;
					
					client.bottom = client.right * input.height / input.width;
					
					client.right += window.right;
					client.bottom += window.bottom;
					
					SetWindowPos (hwnd, 0, 0, 0, client.right, client.bottom,
						SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
					break;
				}
			}
			
			break;
		
		case WM_PAINT:
		{
			RECT rectangle;
			
			GetClientRect (hwnd, &rectangle);
			XIS_VideoRendererRepaintRect (input.videoRenderer, &rectangle);
				
			return DefWindowProc (hwnd, message, wParam, lParam);
		}
			
		case WM_MOVE:
		case WM_SIZE:
		{
			RECT rectangle;
			
			GetClientRect (hwnd, &rectangle);
			XIS_SetVideoRendererPosition (input.videoRenderer, NULL, &rectangle);
			
			return DefWindowProc (hwnd, message, wParam, lParam);
		}
		
		case WM_CREATE:
		{
			HMENU menu, deviceMenu, resolutionMenu;
			int num, i;
			
			/* create the menu */
			menu = CreateMenu ();
			
			deviceMenu = CreatePopupMenu ();
			
			/* append devices to our menu */
			num = XIS_GetVideoCaptureCount ();
			if (num > 0)
			{
				if (num > 10)
					num = 10;
				
				for (i = 0; i < num; i++)
				{
					if (!XIS_GetVideoCaptureInfoEx (i, &input.devices[i]))
						PostQuitMessage (ERROR_GET_CAPTURE_INFO);
					
					AppendMenu (deviceMenu, MF_STRING, ID_DEVICE + i, input.devices[i].szName);
				}
			}
			
			AppendMenu (deviceMenu, MF_STRING, ID_STANDING_ON_THE_BREAK, "standing on the break ...");
			
			AppendMenu (menu, MF_STRING | MF_POPUP, (UINT) deviceMenu, "device");
			input.deviceMenu = deviceMenu;
			
			/* different capture resolutions ... */
			resolutionMenu = CreatePopupMenu ();
			
			AppendMenu (resolutionMenu, MF_STRING, ID_RESOLUTION + 0, "640x480");
			AppendMenu (resolutionMenu, MF_STRING, ID_RESOLUTION + 1, "768x576");
			AppendMenu (resolutionMenu, MF_STRING, ID_RESOLUTION + 2, "1024x768");
			AppendMenu (resolutionMenu, MF_STRING, ID_RESOLUTION + 3, "1280x1024");
			AppendMenu (resolutionMenu, MF_STRING, ID_RESOLUTION + 4, "1920x1080");
			AppendMenu (resolutionMenu, MF_STRING, ID_RESOLUTION + 5, "1920x1200");
			
			AppendMenu (menu, MF_STRING | MF_POPUP, (UINT) resolutionMenu, "capture resolution");
			input.resolutionMenu = resolutionMenu;
			
			AppendMenu (menu, MF_STRING, ID_PROPERTY, "properties");
			
			AppendMenu (menu, MF_STRING, ID_SET_ASPECT_RATIO, "set aspect ratio");
			
			SetMenu (hwnd, menu);
			
			return DefWindowProc (hwnd, message, wParam, lParam);
		}
			
        case WM_DESTROY:
			//KillTimer (hwnd, ID_TIMER);
			
            PostQuitMessage (0);       /* send a WM_QUIT to the message queue */
            break;
            
        default:                      /* for messages that we don't deal with */
            return DefWindowProc (hwnd, message, wParam, lParam);
    }

    return 0;
}

int registerXIViewerWindowClass (HANDLE hThisInstance)
{
	WNDCLASSEX wincl;
	
	/* this code was taken from the Dev-C++ 'Windows Application' template */
	
	/* The Window structure */
    wincl.hInstance = hThisInstance;
    wincl.lpszClassName = "XIViewerWindow";
    wincl.lpfnWndProc = WindowProcedure;      /* This function is called by windows */
    wincl.style = CS_DBLCLKS;                 /* Catch double-clicks */
    wincl.cbSize = sizeof (WNDCLASSEX);

    /* Use default icon and mouse-pointer */
    wincl.hIcon = LoadIcon (NULL, IDI_APPLICATION);
    wincl.hIconSm = LoadIcon (NULL, IDI_APPLICATION);
    wincl.hCursor = LoadCursor (NULL, IDC_ARROW);
    wincl.lpszMenuName = NULL;				   /* set it up manually */
    wincl.cbClsExtra = 0;                      /* No extra bytes after the window class */
    wincl.cbWndExtra = 0;                      /* structure or the window instance */
    /* Use Windows's default color as the background of the window */
    wincl.hbrBackground = (HBRUSH) COLOR_BACKGROUND;

    /* Register the window class, and if it fails quit the program */
    if (!RegisterClassEx (&wincl))
        return 1;
    
    return 0;
}


/* ... some capture stuff ... */
void dumpFrame (const BYTE* pbyImage, int cbStride, inputStream* input)
{
	FILE* theFile;
	BYTE* inputData = (BYTE*) pbyImage;
	
	char buffer[20];
	static int num = 0;
	int i, j;
	
	snprintf (buffer, 20, "log\\frame%d.ppm", num ++);
	theFile = fopen (buffer, "wb");
	if (!theFile)
	{
		MessageBoxA (input->outputWindow, "couldn't create frame<num>.ppm!", "error", MB_OK | MB_ICONASTERISK);
		return;
	}
	
	fprintf (theFile, "P6\n%d %d\n255\n", input->width, input->height);
	
	for (i = 0; i < input->height; i ++)
	{
		for (j = 0; j < input->width; j ++)
		{
			inputData++;
			fwrite (inputData++, 1, 1, theFile);
			fwrite (inputData++, 1, 1, theFile);
			fwrite (inputData++, 1, 1, theFile);
		}
		
		inputData += cbStride - input->width * 4;
	}
	
	fclose (theFile);
	
	return;
}

DWORD WINAPI commitPreviewFrames (LPVOID lpParameter)
{
	inputStream* input = lpParameter;
	
	struct frameHeader* header;
	BYTE* pbImage;
	int message;
	
	static BYTE* pSrc[3] = {NULL, NULL, NULL};
	int srcStep[3];
	
	while (1)
	{
		WaitForSingleObject (input->messageAvailable, INFINITE);
	
		WaitForSingleObject (input->mutex, INFINITE);
		message = input->message;
		ReleaseMutex (input->mutex);
		
		if (message == MESSAGE_PREVIEW_UPDATE)
		{
			if (readRingBuffer (input->inputBuffer, (BYTE**) &header, sizeof (struct frameHeader)) <= 0)
				continue;
			
			if (header->len == 0)
				continue;
			
			if (readRingBuffer (input->inputBuffer, &pbImage, header->len) <= 0)
				continue;
			
			pSrc[0] = pbImage;
			pSrc[1] = pbImage + input->width * input->height;
			pSrc[2] = pSrc[1] + input->width * input->height / 4;
			
			srcStep[0] = input->width;
			srcStep[1] = input->width / 2;
			srcStep[2] = srcStep[1];
			
			ssse3_YUV420ToRGB_8u_P3AC4R(pSrc, srcStep, input->outputBuffer, input->width * 4,
				input->width, input->height);
					
			XIS_VideoRendererDrawImage (input->videoRenderer, input->outputBuffer, input->width * 4, TRUE);
			//dumpFrame (input->outputBuffer, input->width * 4, input);
			
			freeRingBuffer (input->inputBuffer);
		}
		else if (message == MESSAGE_EXIT)
		{
			break;
		}
	}
	
	return 0;
}	


/* capture callback function */
void videoCaptureCallback (const BYTE* pbyImage, int cbImageStride, void* pvParam, UINT64 u64TimeStamp)
{
	inputStream* input = pvParam;
	struct frameHeader header;
	
	header.stride = cbImageStride;
	header.len = cbImageStride * input->height + cbImageStride / 2 * input->height;
	
	//MessageBoxA (input->outputWindow, "test", "test", MB_OK);
	
	if (writeRingBuffer (input->inputBuffer, (BYTE*) &header, sizeof (header)) < 0)
	{
		PostMessageA (input->outputWindow, WM_QUIT, ERROR_WRITE_TO_RING_BUFFER, 0);
		return;
	}
	
	if (writeRingBuffer (input->inputBuffer, (BYTE*) pbyImage, header.len) < 0)
	{
		PostMessageA (input->outputWindow, WM_QUIT, ERROR_WRITE_TO_RING_BUFFER, 0);
		return;
	}
	
	/* send message to heaven ... */
	WaitForSingleObject (input->mutex, INFINITE);
	input->message = MESSAGE_PREVIEW_UPDATE;
	
	ReleaseMutex (input->mutex);
	SetEvent (input->messageAvailable);
	
	return;
}


/* the main function. */
int WINAPI WinMain (HINSTANCE hThisInstance,
                    HINSTANCE hPrevInstance,
                    LPSTR lpszArgument,
                    int nFunsterStil)

{
	int error = EXIT_SUCCESS;
    MSG messages;            /* Here messages to the application are saved */
    HANDLE hwnd;    
    
    /* init some buffers ... */
    /* clear the global inputStream */
    memset (&input, 0, sizeof (inputStream));
    
    input.duration = 333333;
	input.resolution = 5;
	
	setResolution (&input);
    
    input.inputBuffer = createRingBuffer (500000 * 20, INPUT_BUFFER_SIZE);
    if (!input.inputBuffer)
    {
		MessageBoxA (NULL, "seems like there's no memory left ...", "error", MB_OK | MB_ICONWARNING);
		error = 1;
		goto theEnd;
	}
	
	input.messageAvailable = CreateEvent (NULL, FALSE, FALSE, NULL);
	input.mutex = CreateMutex (NULL, FALSE, NULL);
	
	if (!input.messageAvailable || !input.mutex)
	{
		MessageBoxA (NULL, "couldn't create mutex.", "error", MB_OK | MB_ICONWARNING);
		error = 1;
		goto theEnd;
	}
    
    /* init LibXIStream */
    if (!XIS_Initialize ())
    {
		MessageBoxA (NULL, "no capture devices found ...", "...", MB_OK | MB_ICONINFORMATION);
		goto theEnd;
	}
    
    /* all the window like stuff */
	if (registerXIViewerWindowClass (hThisInstance))
	{
		MessageBoxA (NULL, "couldn't register a new window class ...\n", "interesting", MB_OK | MB_ICONQUESTION);
		error = 1;
		goto theEnd;
	}

    /* The class is registered, let's create the program */
	hwnd = CreateWindowEx (
           0,                   /* Extended possibilites for variation */
           "XIViewerWindow",    /* Classname */
           "XIViewer",          /* Title Text */
           WS_OVERLAPPEDWINDOW, /* default window */
           CW_USEDEFAULT,       /* Windows decides the position */
           CW_USEDEFAULT,       /* where the window ends up on the screen */
           640,                 /* The programs width */
           480,                 /* and height in pixels */
           HWND_DESKTOP,        /* The window is a child-window to desktop */
           NULL,                /* No menu */
           hThisInstance,       /* Program Instance handler */
           NULL                 /* No Window Creation data */
           );

    /* Make the window visible on the screen */
    ShowWindow (hwnd, nFunsterStil);
    
    
    /* create a XIStrem video renderer */
    if (!XIS_GetMonitorGUIDFromWindow (hwnd, &input.outputGUID))
    {
		MessageBoxA (NULL, "couldn't get kind of DirectShow GUID from our Window!", "error", MB_OK);
		goto theEnd;
	}
	
    input.videoRenderer = XIS_CreateVideoRenderer (&input.outputGUID, hwnd, XI_COLOR_RGB32, 768, 576, FALSE, TRUE);
    if (!input.videoRenderer)
    {
		MessageBoxA (hwnd, "could not create XIStream VideoRenderer.", "error", MB_OK | MB_ICONEXCLAMATION);
		goto theEnd;
	}
	
	RECT rectangle;
	GetClientRect (hwnd, &rectangle);
	XIS_SetVideoRendererPosition (input.videoRenderer, NULL, &rectangle);
	
	
	input.thread = CreateThread (NULL, 0, &commitPreviewFrames, &input, 0, NULL);
	if (!input.thread)
	{
		MessageBoxA (NULL, "could not create another thread for committing frames to preview.",
			"error", MB_OK | MB_ICONWARNING);
		error = 1;
		goto theEnd;
	};
	
	
	/* set up a timer to get informed about signal input ... */
	// SetTimer (hwnd, ID_TIMER, 500, NULL);
	// Well, XIS_OpenVideoCapturePropertyHandle returns always 0 ...
	
	/* Run the message loop. It will run until GetMessage() returns 0 */
    while (GetMessage (&messages, NULL, 0, 0))
    {
        /* Translate virtual-key messages into character messages */
        TranslateMessage(&messages);
        /* Send message to WindowProcedure */
        DispatchMessage(&messages);
    }
    
    error = messages.wParam; /* return value PostQuitMessage () gave */

	/* wait for the preview thread to exit */
    WaitForSingleObject (input.mutex, INFINITE);
    input.message = MESSAGE_EXIT;
    
    ReleaseMutex (input.mutex);
    SetEvent (input.messageAvailable);

    WaitForSingleObject (input.thread, INFINITE);
    
    
theEnd:
	/* cleanup everything */
	if (input.videoCapture && XIS_IsVideoCaptureStarted (input.videoCapture))
		XIS_StopVideoCapture (input.videoCapture);
	
	if (input.videoCapture)
		XIS_CloseVideoCapture (input.videoCapture);
	
	if (input.videoProperty)
		XIP_ClosePropertyHandle (input.videoProperty);
	
	if (input.videoRenderer)
		XIS_DestroyVideoRenderer (input.videoRenderer);
	
	XIS_Uninitialize ();
	
	if (input.mutex)
		CloseHandle (input.mutex);
	
	if (input.messageAvailable)
		CloseHandle (input.messageAvailable);
	
	if (input.inputBuffer)
		destroyRingBuffer (input.inputBuffer);
	
	
	switch (error)
	{
		case ERROR_WRITE_TO_RING_BUFFER:
			MessageBoxA (NULL, "error while writing to ring buffer. COOL!", "error ...", MB_OK | MB_ICONWARNING);
			break;
		
		case ERROR_GET_CAPTURE_INFO:
			MessageBoxA (NULL, "error while calling XIS_GetCaptureInfo.", "error", MB_OK | MB_ICONWARNING);
			break;
		
		case ERROR_OPEN_CAPTURE:
			MessageBoxA (NULL, "could not open capture device.", "error", MB_OK | MB_ICONWARNING);
			break;
		
		case ERROR_SET_CALLBACK:
			MessageBoxA (NULL, "unable to set capture callback", "error", MB_OK | MB_ICONWARNING);
		
		case ERROR_SET_CAPTURE_FORMAT:
			MessageBoxA (NULL, "couldn't set capture format", "error", MB_OK | MB_ICONWARNING);
			break;
		
		case ERROR_START_CAPTURE:
			MessageBoxA (NULL, "could not start capturing!", "???", MB_OK | MB_ICONWARNING);
			break;
		
		case ERROR_RESET_RENDERER:
			MessageBoxA (NULL, "couldn't reset video renderer", "error", MB_OK | MB_ICONWARNING);
	
		default:
			break;
	}
	
	return error;
}
