#include"./JHLib.h"


typedef struct
{
	int32_t	sizeX;
	int32_t	sizeY;
	uint8_t	bitDepth;
	void*	data;
	//uint8_t	format;
	//uint32_t	bytesPerRow;
}bitmap_t;

bitmap_t *bmp_load(char *path);
void bmp_free(bitmap_t *bmp);

bool bmp_save(char *path, bitmap_t *bitmap);
