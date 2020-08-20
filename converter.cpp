#include <cstdint>
#include <vector>
#include "data_header.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

int const kWidth = WIDTH;
int const kHeight = HEIGHT;
int const kFramerate = FRAMERATE;

static char ditherTable[64] = {
	 0,48,12,60, 3,51,15,63,
	32,16,44,28,35,19,47,31,
	 8,56, 4,52,11,59, 7,55,
	40,24,36,20,43,27,39,23,
	 2,50,14,62, 1,49,13,61,
	34,18,46,30,33,17,45,29,
	10,58, 6,54, 9,57, 5,53,
	42,26,38,22,41,25,37,21
};

struct frame_t {
	uint8_t pixels[kWidth * kHeight];
};

frame_t calculateDeltaFrame(frame_t before, frame_t after)
{
	frame_t diff;
	for (int i = 0; i < sizeof(diff.pixels); ++i)
	{
		diff.pixels[i] = before.pixels[i]^after.pixels[i];
	}
	return diff;
}

void writeRLE(std::vector<uint8_t> const& data, FILE* fh)
{
	uint8_t const bitRepeat = 0x00;
	uint8_t const bitLiteral = 0x80;

	size_t offset = 0;
	while (offset < data.size())
	{
		if (data.size() - offset <= 2)
		{
			uint8_t const key = bitLiteral | (data.size() - offset);
			fwrite(&key, 1, 1, fh);
			fwrite(&data[offset], 1, data.size() - offset, fh);
			break;
		}

		bool findRepeats = data[offset+0] == data[offset+1] && data[offset+0] == data[offset+2];
		if (findRepeats)
		{
			int count = 3;
			for (size_t i = 3; offset+i < data.size(); ++i)
			{
				if (data[offset+i] == data[offset+0])
					count++;
				else
					break;
			}
			if (count > 127)
			{
				count = 127;
			}
			uint8_t const key = bitRepeat | count;
			fwrite(&key, 1, 1, fh);
			fwrite(&data[offset], 1, 1, fh);
			offset += count;
		}
		else
		{
			int count = 3;
			uint8_t last = data[offset+2];
			for (size_t i = 0; offset+i+3 < data.size(); ++i)
			{
				if (data[offset+i+3] == last && data[offset+i+4] == last)
				{
					count--;
					break;
				}
				else
				{
					count++;
					last = data[offset+i+3];
				}
			}
			if (count > 127)
			{
				count = 127;
			}
			uint8_t const key = bitLiteral | count;
			fwrite(&key, 1, 1, fh);
			fwrite(&data[offset], 1, count, fh);
			offset += count;
		}
	}
}

int main()
{
	std::vector<frame_t> frames;

	for (int frameIndex = 1; ; ++frameIndex)
	{
		char filename[32];
		sprintf(filename, "frames/%04d.png", frameIndex);

		int w,h,n;
		uint8_t const* data = stbi_load(filename, &w, &h, &n, 1);
		if (!data)
			break;

		if (w != kWidth*2 || h != kHeight*4)
			break;

		printf("Converting frame %d\n", frameIndex);

		frame_t currentFrame;
		for (int y = 0; y < kHeight; ++y)
		{
			for (int x = 0; x < kWidth; ++x)
			{
				uint8_t mask = 0;
				int offsets[]={
					0,0,
					0,1,
					0,2,
					1,0,
					1,1,
					1,2,
					0,3,
					1,3,
				};

				for (int j=0;j<8;++j) {
					int xx = x*2+offsets[j*2];
					int yy = y*4+offsets[j*2+1];
					uint8_t pixel = data[yy*(kWidth*2)+xx];

					int it = ditherTable[(yy%8)*8+(xx%8)];
					float threshold = (it+0.5f)/64.0f;
					float value = pixel/255.0f;

					// hack to force the top and bottom to be pure colors
					value = value*1.04f-0.02f;

					if (value > threshold) {
						mask |= (1<<j);
					}
				}

				currentFrame.pixels[y*kWidth+x] = mask;
			}
		}
		frames.push_back(currentFrame);

		stbi_image_free((void*)data);
	}

	printf("Delta-encoding frames...\n");
	std::vector<frame_t> deltaFrames;
	deltaFrames.push_back(frames[0]);
	for (size_t i = 1; i < frames.size(); ++i)
	{
		deltaFrames.push_back(calculateDeltaFrame(frames[i-1], frames[i]));
	}

	FILE* fh = fopen("data.bin", "wb");
	data_header_t header;
	header.framerate = kFramerate;
	header.width = kWidth;
	header.height = kHeight;
	header.frameCount = frames.size();
	fwrite(&header, sizeof(header), 1, fh);
	printf("Compressing and saving frames...\n");
	for (const auto& frame : deltaFrames)
	{
		writeRLE(std::vector<uint8_t>(frame.pixels, frame.pixels+sizeof(frame.pixels)), fh);
	}

	fclose(fh);
}
