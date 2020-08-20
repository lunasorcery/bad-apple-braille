#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <cstring>
#include <chrono>
#include <thread>
#include <assert.h>
#include <unistd.h>
#include "data_header.h"

using namespace std::chrono;

void readRLE(uint8_t* dst, FILE* fh, int dataLength)
{
	while (dataLength > 0)
	{
		uint8_t key;
		fread(&key, 1, 1, fh);

		int const count = key & 0x7f;
		dataLength -= count;

		if (key & 0x80)
		{
			fread(dst, 1, count, fh);
			dst += count;
		}
		else
		{
			uint8_t value;
			fread(&value, 1, 1, fh);
			for (int i = 0; i < count; ++i)
				*dst++ = value;
		}
	}
	assert(dataLength == 0);
}

void abortHandler(int signo)
{
	// show the cursor
	fputs("\033[?25h", stdout);
	exit(signo);
}

int main()
{
	FILE* fh = fopen("data.bin", "rb");
	if (!fh)
	{
		fprintf(stderr, "Failed to open frame data (data.bin)\n");
		return 1;
	}

	signal(SIGINT, abortHandler);

	data_header_t header;
	fread(&header, sizeof(header), 1, fh);

	// set up the framebuffer
	// 3 bytes per braille pixel,
	// plus one byte per newline
	// plus six bytes for the cursor reset
	// and one more for the null-terminator
	char* frameStringBuffer = new char[(header.width*3+1)*header.height+6+1];
	{
		char* fbWritePtr = frameStringBuffer;
		for (int y = 0; y < header.height; ++y)
		{
			for (int x = 0; x < header.width; ++x)
			{
				// magic constants for the empty braille pattern (U+2800)
				*fbWritePtr++ = 0xe2;
				*fbWritePtr++ = 0xa0;
				*fbWritePtr++ = 0x80;
			}
			*fbWritePtr++ = '\n';
		}
		//*fbWritePtr='\0';
		strcpy(fbWritePtr, "\033[1;1H");
	}

	int packedBufferSize = (header.width * header.height);
	uint8_t* packed_frame_buffer = new uint8_t[packedBufferSize];
	uint8_t* packed_delta_buffer = new uint8_t[packedBufferSize];
	memset(packed_frame_buffer, 0, packedBufferSize);

	// hide the cursor
	fputs("\033[?25l", stdout);

	// clear screen and reset cursor position
	fputs("\033[2J\033[1;1H", stdout);

	int lastRenderedFrame = -1;
	auto const timeStart = high_resolution_clock::now();
	while (1)
	{
		auto const timeNow = high_resolution_clock::now();
		auto const duration = duration_cast<milliseconds>(timeNow - timeStart);

		int64_t const currFrame = (duration.count() * header.framerate) / 1000;

		if (currFrame >= header.frameCount)
			break;

		bool frameChanged = false;
		while (currFrame > lastRenderedFrame)
		{
			readRLE(packed_delta_buffer, fh, packedBufferSize);
			for (int i = 0; i < packedBufferSize; ++i)
			{
				packed_frame_buffer[i] ^= packed_delta_buffer[i];
			}

			for (int y = 0; y < header.height; ++y)
			{
				for (int x = 0; x < header.width; ++x)
				{
					uint8_t const key = packed_frame_buffer[y*header.width+x];
					frameStringBuffer[y*(header.width*3+1)+(x*3)+1] = 0xa0 | (key>>6);
					frameStringBuffer[y*(header.width*3+1)+(x*3)+2] = 0x80 | (key&0x3f);
				}
			}
			frameChanged = true;

			lastRenderedFrame++;
		}

		if (frameChanged)
		{
			fputs(frameStringBuffer, stdout);
			fflush(stdout);

			// now I know what you're thinking
			// that should be 1,000,000 rather than 100,000
			// but I don't actually want to sleep by an entire frame's duration
			// I just want to sleep enough so the loop isn't constantly getting slammed
			// while hopefully not actually stalling the framerate performance
			std::this_thread::sleep_for(microseconds(100000u/header.framerate));
		}
	}

	// show the cursor
	fputs("\033[?25h", stdout);

	// clear screen and reset cursor position
	fputs("\033[2J\033[1;1H", stdout);
}
