/// ImageConversion.cpp: определяет точку входа для консольного приложения.

#include "stdafx.h"
#include <windows.h>
#include <stdio.h>
#include <thread>
#include <pthread.h>
#pragma pack(2)
using namespace std;

//Размеры видео YUV
const int videoHorRes = 1280;
const int videoVerRes = 720;
//odd number of threads isn't recommended due to anomalies
//количество потоков приложения(для проверки быстродействия использовались картинки genie, genie_large и genie_extra; при нечётном числе потоков могут возникнуть аномалии в верхней половине изображения для варианта genie.bmp)
const int numberOfThreads = 8;

struct BMPH
{
	char signature[2];
	unsigned int fileSize;
	unsigned int reserved;
	unsigned int offset;
} BmpHeader;

struct BMPII
{
	unsigned int headerSize;
	unsigned int width;
	unsigned int height;
	unsigned short planeCount;
	unsigned short bitDepth;
	unsigned int compression;
	unsigned int compressedImageSize;
	unsigned int horizontalResolution;
	unsigned int verticalResolution;
	unsigned int numColors;
	unsigned int importantColors;

}BmpImageInfo;

struct RGB
{
	unsigned char blue;
	unsigned char green;
	unsigned char red;
}Rgb;
struct Params 
{
	int threadNumber; 
	BMPII info; unsigned char* rPtr0; 
	unsigned char* wPtr0; 
	unsigned char* uPtr; 
	unsigned char* vPtr;
};


void convertToYUV(unsigned char*, BMPII);
//image convertation by blocks 
void convertToYUVWThreads(unsigned char*, BMPII);

void *blockConvert(void* arg);
void projectOverVideo(unsigned char*, unsigned char*, unsigned char*, BMPII);


int main()
{
	FILE *inFile, *outFile;
	const char* fileName = "genie_large.bmp";
	struct BMPH header;
	struct BMPII info;
	inFile = fopen(fileName, "rb"); //Открытие произвольного исходного изображения с названием, заданным в filename
	if (!inFile) {
		printf("Error opening file %s.\n", fileName);
		return -1;
	}

	if (fread(&header, 1, sizeof(BMPH), inFile) != sizeof(BMPH)) {
		printf("Error reading bmp header.\n");
		return -1;
	}

	if (fread(&info, 1, sizeof(BMPII), inFile) != sizeof(BMPII)) {
		printf("Error reading image info.\n");
		return -1;
	}
	if (info.height < 0 || info.height % 2 || info.width % 2)
	{
		printf("This type of bitmap is not supported.\n");
		getchar();
		return -1;
	}

	unsigned char *rgbData = (unsigned char*)malloc(sizeof(RGB)*info.height*info.width);
	int read, j;
	fread(rgbData, 1, sizeof(RGB)*info.height*info.width, inFile);
	//convertToYUV(rgbData,  info);
	convertToYUVWThreads(rgbData, info);
	printf("Done.\n");
	fclose(inFile);
	printf("\nBMP-Info:\n");
	printf("Width x Height: %i x %i\n", info.width, info.height);
	printf("Depth: %i\n", (int)info.bitDepth);
	getchar();
	return 0;
}
void convertToYUV(unsigned char* rgbData, BMPII info)
{
	clock_t initialTime = clock();
	int R, G, B;
	int U, V;
	unsigned char* yData = (unsigned char*)malloc(info.height*info.width);
	unsigned char* vData = (unsigned char*)malloc(info.height*info.width / 4);
	unsigned char* uData = (unsigned char*)malloc(info.height*info.width / 4);

	unsigned char* rPtr0 = rgbData;
	unsigned char* rPtr1 = rgbData + info.width * 3;

	unsigned char* wPtr0 = yData;
	unsigned char* wPtr1 = yData + info.width;

	unsigned char* uPtr = uData;
	unsigned char* vPtr = vData;

#pragma region converting
	for (int j = 0; j < info.height; j += 2) {
		for (int i = 0; i < info.width; i += 2) {
			//Two rows are calculated simultaneously: for both of then Y-component is taken two times; meanwhile, U- and V-components are calculated from block of 4 pixels, located in those rows.

			//Параллельно засчитываются значения для двух строк: в то время, как для Y-компоненты в массив заносится два значения для каждой строки, 
			//U- и V-компоненты пополняются единожды за цикл в среднем по 4 взятым из оригинала пикселам. 
			//Операции битового сдвига используются для более быстрого деления.
			B = *rPtr0++;
			G = *rPtr0++;
			R = *rPtr0++;

			*wPtr0++ = ((66 * R + 129 * G + 25 * B + 128) >> 8) + 16;
			U = (-38 * R - 74 * G + 112 * B + 128);
			V = (112 * R - 94 * G - 18 * B + 128);

			B = *rPtr0++;
			G = *rPtr0++;
			R = *rPtr0++;

			*wPtr0++ = ((66 * R + 129 * G + 25 * B + 128) >> 8) + 16;
			U += (-38 * R - 74 * G + 112 * B + 128);
			V += (112 * R - 94 * G - 18 * B + 128);

			B = *rPtr1++;
			G = *rPtr1++;
			R = *rPtr1++;

			*wPtr1++ = ((66 * R + 129 * G + 25 * B + 128) >> 8) + 16;
			U += (-38 * R - 74 * G + 112 * B + 128);
			V += (112 * R - 94 * G - 18 * B + 128);

			B = *rPtr1++;
			G = *rPtr1++;
			R = *rPtr1++;

			*wPtr1++ = ((66 * R + 129 * G + 25 * B + 128) >> 8) + 16;
			U += (-38 * R - 74 * G + 112 * B + 128);
			V += (112 * R - 94 * G - 18 * B + 128);

			U >>= 8;
			V >>= 8;
			U += 512;
			V += 512;

			*uPtr++ = U >> 2;
			*vPtr++ = V >> 2;
		}


		rPtr0 += info.width * 3;
		rPtr1 += info.width * 3;

		wPtr0 += info.width;
		wPtr1 += info.width;
	}
#pragma endregion

	unsigned int summaryTime = clock() - initialTime;
	printf("Converted in %d miliseconds\nPress any button to continue\n", summaryTime);
	getchar();
	//video resolution is stated in constantes at the beginning of the program
	//открытие видео размера, указанного в соответствующих константах;
	FILE *out = fopen("test.yuv", "wb"); 
	if (out == NULL) {
		fprintf(stderr, "Error open file %s\nPress ENTER to exit\n",
			"test.yuv");
		getchar();
		exit(-1);
	}

#pragma region mirrorring

	unsigned char* yData1 = (unsigned char*)malloc(info.height*info.width);
	unsigned char* vData1 = (unsigned char*)malloc(info.height*info.width / 4);
	unsigned char* uData1 = (unsigned char*)malloc(info.height*info.width / 4);
	wPtr0 = yData1;
	wPtr1 = yData;
	unsigned char* uPtr1 = uData1;
	uPtr = uData;
	unsigned char* vPtr1 = vData1;
	vPtr = vData;
	for (int j = 0; j < info.height; j++)
	{
		wPtr1 += info.width;
		for (int i = 0; i < info.width; i++)
		{
			wPtr1--;
			*wPtr0++ = *wPtr1;


		}
		wPtr1 += info.width;
	}
	for (int j = 0; j < info.height / 2; j++)
	{
		uPtr += info.width / 2;
		for (int i = 0; i < info.width / 2; i++)
		{
			uPtr--;
			*uPtr1++ = *uPtr;
		}
		uPtr += info.width / 2;
	}

	for (int j = 0; j < info.height / 2; j++)
	{
		vPtr += info.width / 2;
		for (int i = 0; i < info.width / 2; i++)
		{
			vPtr--;
			*vPtr1++ = *vPtr;
		}
		vPtr += info.width / 2;
	}
	free(yData);
	free(uData);
	free(vData);
#pragma endregion

#pragma region saving	
	wPtr1 = wPtr0;
	unsigned char *framePtr, *frame = (unsigned char*)malloc(info.height*info.width*1.5);
	framePtr = frame;
	for (int i = 0; i < info.width*info.height; i++)
	{
		*framePtr++ = *wPtr1--;

	}
	uPtr = uPtr1;
	for (int i = 0; i < info.width*info.height / 4; i++)
	{
		*framePtr++ = *uPtr--;

	}
	vPtr = vPtr1;
	for (int i = 0; i < info.width*info.height / 4; i++)
	{
		*framePtr++ = *vPtr--;

	}
	fwrite(frame, info.height*info.width*1.5, 1, out);
	free(frame);
	fclose(out);
#pragma endregion	

	if (info.width <= videoHorRes && info.height <= videoVerRes)
	{
		projectOverVideo(wPtr0, uPtr1, vPtr1, info);
	}
	else printf("Image can't be projected onto video");
}


void convertToYUVWThreads(unsigned char* rgbData, BMPII info)
{
	clock_t initialTime = clock();

	unsigned char* yData = (unsigned char*)malloc(info.height*info.width);
	unsigned char* vData = (unsigned char*)malloc(info.height*info.width / 4);
	unsigned char* uData = (unsigned char*)malloc(info.height*info.width / 4);
	pthread_t threads[numberOfThreads];
	
	int rc[numberOfThreads];
	for (int i = 0; i<numberOfThreads; i++)
	{
		Params *params;
		params = (Params*)malloc(sizeof(Params));
		params->info = info;
		params->threadNumber = i+1;
		params->rPtr0 = rgbData + i * info.width * 3 * (info.height / numberOfThreads- info.height / numberOfThreads % 4);
		params->wPtr0 = yData + i * info.width * (info.height / numberOfThreads - info.height / numberOfThreads % 4);
		params->uPtr = uData + i * info.width * (info.height / numberOfThreads - info.height / numberOfThreads % 4) / 4;
		params->vPtr = vData + i * info.width * (info.height / numberOfThreads - info.height / numberOfThreads % 4) / 4;
		rc[i]= pthread_create(&threads[i], NULL, blockConvert, (void *)params);
	}
	for (int i = 0; i < numberOfThreads; i++)
	{
		rc[i]= pthread_join(threads[i], NULL);
	}



	unsigned int summaryTime = clock() - initialTime;
	printf("Converted in %d miliseconds\nPress any button to continue\n", summaryTime);
	getchar();
	FILE *out = fopen("test.yuv", "wb"); //открытие видео размера, указанного в соответствующих константах;
	if (out == NULL) {
		fprintf(stderr, "Error open file %s\nPress ENTER to exit\n",
			"test.yuv");
		getchar();
		exit(-1);
	}

#pragma region mirrorring

	unsigned char* yData1 = (unsigned char*)malloc(info.height*info.width);
	unsigned char* vData1 = (unsigned char*)malloc(info.height*info.width / 4);
	unsigned char* uData1 = (unsigned char*)malloc(info.height*info.width / 4);
	unsigned char *wPtr0, *wPtr1, *uPtr, *vPtr;
	wPtr0 = yData1;
	wPtr1 = yData;
	unsigned char* uPtr1 = uData1;
	uPtr = uData;
	unsigned char* vPtr1 = vData1;
	vPtr = vData;
	for (int j = 0; j < info.height; j++)
	{
		wPtr1 += info.width;
		for (int i = 0; i < info.width; i++)
		{
			wPtr1--;
			*wPtr0++ = *wPtr1;


		}
		wPtr1 += info.width;
	}
	for (int j = 0; j < info.height / 2; j++)
	{
		uPtr += info.width / 2;
		for (int i = 0; i < info.width / 2; i++)
		{
			uPtr--;
			*uPtr1++ = *uPtr;
		}
		uPtr += info.width / 2;
	}

	for (int j = 0; j < info.height / 2; j++)
	{
		vPtr += info.width / 2;
		for (int i = 0; i < info.width / 2; i++)
		{
			vPtr--;
			*vPtr1++ = *vPtr;
		}
		vPtr += info.width / 2;
	}
	free(yData);
	free(uData);
	free(vData);
#pragma endregion

#pragma region saving	
	wPtr1 = wPtr0;
	unsigned char *framePtr, *frame = (unsigned char*)malloc(info.height*info.width*1.5);
	framePtr = frame;
	for (int i = 0; i < info.width*info.height; i++)
	{
		*framePtr++ = *wPtr1--;

	}
	uPtr = uPtr1;
	for (int i = 0; i < info.width*info.height / 4; i++)
	{
		*framePtr++ = *uPtr--;

	}
	vPtr = vPtr1;
	for (int i = 0; i < info.width*info.height / 4; i++)
	{
		*framePtr++ = *vPtr--;

	}
	fwrite(frame, info.height*info.width*1.5, 1, out);
	free(frame);
	fclose(out);
#pragma endregion	

	if (info.width <= videoHorRes || info.height <= videoVerRes)
	{
		projectOverVideo(wPtr0, uPtr1, vPtr1, info);
	}
	else printf("Image can't be projected onto video");
}


void *blockConvert(void* arg)
{
	Params *p = (Params*)arg;
	int offset = 0;
	if (p->threadNumber == numberOfThreads) offset = (p->info.height / numberOfThreads % 4)*(numberOfThreads-1);
	int R, G, B;
	int U, V;
	unsigned char* rPtr1 = p->rPtr0 + p->info.width * 3;
	unsigned char* wPtr1 = p->wPtr0 + p->info.width;


#pragma region converting
	for (int j = 0; j < p->info.height / numberOfThreads - p->info.height / numberOfThreads % 4 + offset; j += 2) {
		for (int i = 0; i < p->info.width; i += 2) {
			//Two rows are calculated simultaneously: for both of then Y-component is taken two times; meanwhile, U- and V-components are calculated from block of 4 pixels, located in those rows.

			//Параллельно засчитываются значения для двух строк: в то время, как для Y-компоненты в массив заносится два значения для каждой строки, 
			//U- и V-компоненты пополняются единожды за цикл в среднем по 4 взятым из оригинала пикселам. 
			//Операции битового сдвига используются для более быстрого деления.
			B = *p->rPtr0++;
			G = *p->rPtr0++;
			R = *p->rPtr0++;

			*p->wPtr0++ = ((66 * R + 129 * G + 25 * B + 128) >> 8) + 16;
			U = (-38 * R - 74 * G + 112 * B + 128);
			V = (112 * R - 94 * G - 18 * B + 128);

			B = *p->rPtr0++;
			G = *p->rPtr0++;
			R = *p->rPtr0++;

			*p->wPtr0++ = ((66 * R + 129 * G + 25 * B + 128) >> 8) + 16;
			U += (-38 * R - 74 * G + 112 * B + 128);
			V += (112 * R - 94 * G - 18 * B + 128);

			B = *rPtr1++;
			G = *rPtr1++;
			R = *rPtr1++;

			*wPtr1++ = ((66 * R + 129 * G + 25 * B + 128) >> 8) + 16;
			U += (-38 * R - 74 * G + 112 * B + 128);
			V += (112 * R - 94 * G - 18 * B + 128);

			B = *rPtr1++;
			G = *rPtr1++;
			R = *rPtr1++;

			*wPtr1++ = ((66 * R + 129 * G + 25 * B + 128) >> 8) + 16;
			U += (-38 * R - 74 * G + 112 * B + 128);
			V += (112 * R - 94 * G - 18 * B + 128);

			U >>= 8;
			V >>= 8;
			U += 512;
			V += 512;

			*p->uPtr++ = U >> 2;
			*p->vPtr++ = V >> 2;
		}


		p->rPtr0 += p->info.width * 3;
		rPtr1 += p->info.width * 3;

		p->wPtr0 += p->info.width;
		wPtr1 += p->info.width;
	}
#pragma endregion
}


void projectOverVideo(unsigned char* initialWPtr, unsigned char* initialUPtr, unsigned char* initialVPtr, BMPII info)
{
	FILE *source, *changed;
	source = fopen("testVideo.yuv", "rb");
	unsigned char *wPtr, *uPtr, *vPtr, *buf = (unsigned char*)malloc(videoHorRes*videoVerRes*1.5);

	if (source == NULL) {
		fprintf(stderr, "Error open file %s\nPress ENTER to exit\n",
			"testVideo.yuv");
		getchar();
		exit(-1);
	}
	changed = fopen("testVideoC.yuv", "wb");
	if (changed == NULL) {
		fprintf(stderr, "Error open file %s\nPress ENTER to exit\n",
			"testVideo.yuv");
		getchar();
		exit(-1);
	}

	for (int frame = 0; frame < 300; frame++)
	{
		wPtr = initialWPtr;
		uPtr = initialUPtr;
		vPtr = initialVPtr;


		fread(buf, videoHorRes*videoVerRes*1.5, 1, source);
		int str = 0;
		for (int i = 0; i < videoVerRes; i++)
		{

			for (int j = 0; j < info.width; j++)
			{
				if (i<info.height) buf[j + str] = *wPtr--;

			}
			str += videoHorRes;
		}


		for (int i = 0; i < videoVerRes / 2; i++)
		{
			for (int j = 0; j < info.width / 2; j++)
			{
				if (i<info.height / 2) buf[j + str] = *uPtr--;
			}
			str += videoHorRes / 2;
		}

		for (int i = 0; i < videoVerRes / 2; i++)
		{
			for (int j = 0; j < info.width / 2; j++)
			{
				if (i<info.height / 2) buf[j + str] = *vPtr--;
			}
			str += videoHorRes / 2;
		}
		fwrite(buf, videoHorRes*videoVerRes*1.5, 1, changed);
		printf("Completed frame %d\n", frame + 1);

	}
	fclose(source);
	fclose(changed);
}

