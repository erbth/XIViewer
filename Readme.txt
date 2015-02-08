Well, this is just another viewer application for Magewell's Video Capture Cards.

It's some kind of minimal implementation using their SDK available on:
	http://www.magewell.com/sdk?lang=en

It's written in C and compiled with MinGW 4.8.1 using Bloodshed Dev-C++ as IDE on Windows,
but it should work with any other MinGW version, too. The only important thing is
it has to support at least Intel SSSE3 intrinsics, because there's some special
SIMD code for converting I420 video data to XRGB. Everything else is just what has
to be done to get a picture out of the device.
Another thing, I have to tell you the SDK works really well, and there's good documentation
shipped with it.

And please feel free to do copy/modify/redistribute my code, just have fun!
