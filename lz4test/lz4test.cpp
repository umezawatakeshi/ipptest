// lz4test.cpp : このファイルには 'main' 関数が含まれています。プログラム実行の開始と終了がそこで行われます。
//

#include "pch.h"

#define LOOPS 10000
#define FROMFILE 1

const char* strbool(bool b)
{
	return b ? "true" : "false";
}

Ipp8u* ht;


#define NCOMP 3

int LZ4CompressOriginal(void* src, int srclen, void* dst, int dstlen)
{
	return LZ4_compress_default((char*)src, (char*)dst, srclen, dstlen);
}

int LZ4CompressIPP(void* src, int srclen, void* dst, int dstlen)
{
	int resultlen;
	ippsEncodeLZ4_8u((Ipp8u*)src, srclen, (Ipp8u*)dst, &resultlen, ht);
	return resultlen;
}

int LZ4CompressIPPSafe(void* src, int srclen, void* dst, int dstlen)
{
	ippsEncodeLZ4Safe_8u((Ipp8u*)src, &srclen, (Ipp8u*)dst, &dstlen, ht);
	return dstlen;
}

const std::pair<const char*, int(*)(void*, int, void*, int)> compressors[NCOMP] = {
	{"original", LZ4CompressOriginal},
	{"IPP",      LZ4CompressIPP},
	{"IPP safe", LZ4CompressIPPSafe},
};


#define NDECOMP 2

int LZ4DecompressOriginal(void* src, int srclen, void* dst, int dstlen)
{
	return LZ4_decompress_safe((char*)src, (char*)dst, srclen, dstlen);
}

int LZ4DecompressIPP(void* src, int srclen, void* dst, int dstlen)
{
	ippsDecodeLZ4_8u((Ipp8u*)src, srclen, (Ipp8u*)dst, &dstlen);
	return dstlen;
}

const std::pair<const char*, int(*)(void*, int, void*, int)> decompressors[NDECOMP] = {
	{"original", LZ4DecompressOriginal},
	{"IPP",      LZ4DecompressIPP},
};


int main(int argc, char** argv)
{
	LARGE_INTEGER liFreq, liBegin, liEnd;
	QueryPerformanceFrequency(&liFreq);

	auto ippst = ippInit();
	printf("ippInit returned %d\n", ippst);

#ifdef FROMFILE
	std::ifstream ifs(argv[1], std::ios::binary);
	ifs.seekg(0, std::ios::end);
	int bufsize = ifs.tellg();
	ifs.clear();
	ifs.seekg(0, std::ios::beg);
#else
	int bufsize = atoi(argv[1]);
#endif
	char* buf1 = (char*)malloc(bufsize); // compression src
	int enccap = LZ4_compressBound(bufsize);
	char* buf2[NCOMP]; // compression dst / decompression src
	for (int i = 0; i < NCOMP; ++i)
		buf2[i] = (char*)malloc(enccap);
	char* buf3 = (char*)malloc(bufsize); // decompression dst
#ifdef FROMFILE
	ifs.read(buf1, bufsize);
#else
	std::mt19937 engine;
	std::exponential_distribution<double> dist(0.1);

	for (int i = 0; i < bufsize; ++i) {
		double v = dist(engine);
		if (v >= 256.0)
			v = 0.0;
		buf1[i] = (unsigned char)v;
	}
#endif

	printf("srclen = %d\nenccap = %d\n", bufsize, enccap);

	int htsize;
	ippsEncodeLZ4HashTableGetSize_8u(&htsize);
	ht = (Ipp8u*)malloc(htsize);
	ippsEncodeLZ4HashTableInit_8u(ht, bufsize);


	printf("\nencode test\n");

	int encsize[NCOMP];
	for (int i = 0; i < NCOMP; ++i)
	{
		auto& comp = compressors[i];
		QueryPerformanceCounter(&liBegin);
		for (int k = 0; k < LOOPS; ++k) {
			encsize[i] = comp.second(buf1, bufsize, buf2[i], enccap);
		}
		QueryPerformanceCounter(&liEnd);
		printf("%-8s %fs / sz %d\n", comp.first, (liEnd.QuadPart - liBegin.QuadPart) / (double)liFreq.QuadPart, encsize[i]);
	}


	for (int i = 0; i < NCOMP; ++i)
	{
		printf("\ndecode test from %s\n", compressors[i].first);

		for (int j = 0; j < NDECOMP; ++j)
		{
			auto& decomp = decompressors[j];
			QueryPerformanceCounter(&liBegin);
			int decsize;
			for (int k = 0; k < LOOPS; ++k) {
				decsize = decomp.second(buf2[i], encsize[i], buf3, bufsize);
			}
			QueryPerformanceCounter(&liEnd);
			printf("%-8s %fs %s\n", decomp.first, (liEnd.QuadPart - liBegin.QuadPart) / (double)liFreq.QuadPart, strbool(decsize == bufsize && memcmp(buf1, buf3, bufsize) == 0));
		}
	}
}
