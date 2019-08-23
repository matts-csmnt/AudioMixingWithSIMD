//////////////////////////////////////////////////////////////////////////
// Audio Mixing Prototype (Optimization Assignment)
//////////////////////////////////////////////////////////////////////////

#include "Config.h"
#include "WaveFile.h"
#include "Profiler.h"
#include <iostream>
#include <immintrin.h>
#include <zmmintrin.h>

constexpr uint32_t kNumAudioStreams = 4;
constexpr uint32_t kTestBlockSize = 4096; // samples, e.g. 2048 stereo samples.
constexpr uint32_t kNumBlocks = 3698; // total number of blocks to mix, input files must be long enough.

constexpr int kInSize = sizeof(WavAudio::WavAudioFileInput);
constexpr float kInCacheLines = kInSize / 16;

//////////////////////////////////////////////////////////////////////////
#define INT_16BIT_MIXING 0
#define USING_FMADD_INTRIN 1

#define USING_256_BIT_REGS 1
#define USING_512_BIT_REGS 0
//////////////////////////////////////////////////////////////////////////

#define ALIGN16 __declspec(align(16))
#define ALIGN32 __declspec(align(32))
#define ALIGN64 __declspec(align(64))

// Define our audio streams.
ALIGN16 WavAudio::WavAudioFileInput g_inputFiles[kNumAudioStreams];
ALIGN16 WavAudio::WavAudioFileOutput g_outputFile;

// Define some paths to files we want to load.
const char* const g_inputFilePaths[kNumAudioStreams] = {
	"audio_input_1.wav",
	"audio_input_2.wav",
	"audio_input_3.wav",
	"audio_input_4.wav"
};

const char* const g_outputFilePath = "audio_mix_out.wav";

// This array contains the mixing proportions for each input (gain factors).
//    (We have stereo inputs so each has 2 gain factors, Left and Right).
float g_gainFactors[kNumAudioStreams * 2] =
{
	// Left Right
	 0.5f, 0.5f,
	 0.3f, 0.5f,
	 0.5f, 0.3f,
	 0.3f, 0.7f
};

// OPens audio files for reading and writing.
void prepare_audio_files()
{
	// Load our input wave files.
	for (uint32_t i = 0; i < kNumAudioStreams; ++i)
	{
		std::cout << "Open input file " << g_inputFilePaths[i] << std::endl;
		g_inputFiles[i].open(g_inputFilePaths[i]);
		g_inputFiles[i].print_format_info(std::cout);
	}

	std::cout << "Open output file " << g_outputFilePath << std::endl;
	WavAudio::FmtChunk format = WavAudio::make_format(WavAudio::eAudioFormat::kFormat_16bitPCM, 2, 48000);
	g_outputFile.open(g_outputFilePath, format);
	g_outputFile.print_format_info(std::cout);
}

// Clears a buffer to zero.
void clear_buffer(float* out, uint32_t blockSize)
{
	for (uint32_t i = 0; i < blockSize; ++i)
	{
		out[i] = 0;
	}
}

void clear_buffer16(int16_t* out, uint32_t blockSize)
{
	for (uint32_t i = 0; i < blockSize; ++i)
	{
		out[i] = 0;
	}
}

// Mixes a stereo audio signal contained in a block sized buffer
// The output buffer is the accumulation of several inputs.
void mix_buffer(const float* in, float* out, float leftGain, float rightGain, uint32_t blockSize)
{
	TIMER_SCOPED("mix_buffer loop");

#if USING_FMADD_INTRIN == 1
	#if USING_512_BIT_REGS == 0
		#if USING_256_BIT_REGS ==0
			
			//default 128bit
			ALIGN16 __m128 outputs_128;
			ALIGN16 __m128 inputs_128;
			ALIGN16 __m128 gains_128;
			ALIGN16 float gainsTemp[4];

			for (uint32_t i = 0; i < (blockSize / 2); i+=2)
			{
		#else
			//256bit
			ALIGN16 __m256 outputs_256;
			ALIGN16 __m256 inputs_256;
			ALIGN16 __m256 gainsl_256;
			ALIGN16 __m256 gainsr_256;

			const int gainsMask = 0xAA;							//mask of 01010101
			gainsl_256 = _mm256_set1_ps(leftGain);				//splat left gain to all elements
			gainsr_256 = _mm256_set1_ps(rightGain);				//splat right gain to all elements
			gainsl_256 = _mm256_blend_ps(gainsl_256, gainsr_256, gainsMask);	//load alternating values using the mask

			for (uint32_t i = 0; i < (blockSize / 2); i += 4)
			{
		#endif
	#else
		//512bit
		ALIGN64 __m512 outputs_512;
		ALIGN64 __m512 inputs_512;
		ALIGN64 __m512 gainsl_512;
		ALIGN64 __m512 gainsr_512;

		__mmask16 gainsMask = 0xAAAA;						//mask of 0101010101010101
		gainsl_512 = _mm512_set1_ps(leftGain);				//splat left gain to all elements
		gainsr_512 = _mm512_set1_ps(rightGain);				//splat right gain to all elements
		gainsl_512 = _mm512_mask_expand_ps(gainsl_512, gainsMask, gainsr_512);	//load alternating values using the mask

		for (uint32_t i = 0; i < (blockSize / 2); i += 8)
		{
	#endif
#else
	//NO INTRINSIC VECTOR MATHS
	for (uint32_t i = 0; i < (blockSize / 2); ++i)
	{
#endif
		uint32_t leftIndex = i * 2;
		uint32_t rightIndex = i * 2 + 1;

#if USING_FMADD_INTRIN == 0

		out[leftIndex] += in[leftIndex] * leftGain;
		out[rightIndex] += in[rightIndex] * rightGain;

#else
	#if USING_512_BIT_REGS == 0
		#if USING_256_BIT_REGS == 0
			//DEFAULT 128bit
			gainsTemp[0] = leftGain;
			gainsTemp[1] = rightGain;
			gainsTemp[2] = leftGain;
			gainsTemp[3] = rightGain;

			//fused multiply add intrinsic - 2 samples in one pass - 
			outputs_128	 = _mm_setzero_ps();					//clear outputs
			inputs_128	 = _mm_load_ps(&in[leftIndex]);			//Load 128-bits - in1[l] in1[r] & in2[l] in2[r]
			gains_128	 = _mm_load_ps(gainsTemp);				//load 4 32bit floats in reverse order to the gains memory

			//mul - multioly and accumulate into outputs_128
			outputs_128	 = _mm_fmadd_ps(inputs_128, gains_128, outputs_128);

			//output to arrays
			out[leftIndex] += outputs_128.m128_f32[0];
			out[rightIndex] += outputs_128.m128_f32[1];
			out[leftIndex +2] += outputs_128.m128_f32[2];
			out[rightIndex +2] += outputs_128.m128_f32[3];
		#else
			//256bit
			//fused multiply add intrinsic - 2 samples in one pass - 
			outputs_256 = _mm256_setzero_ps();					//clear outputs
			inputs_256 = _mm256_load_ps(&in[leftIndex]);		//Load 512-bits - in1[l] in1[r] ... in16[l] in16[r]

			//mul - multioly and accumulate into outputs_512
			outputs_256 = _mm256_fmadd_ps(inputs_256, gainsl_256, outputs_256);

			//output to arrays
			for (size_t j(0); j < 8; j += 2)
			{
				out[leftIndex + j] += outputs_256.m256_f32[0 + j];
				out[rightIndex + j] += outputs_256.m256_f32[0 + j + 1];
			}
		#endif
	#else
		//512bit
		//fused multiply add intrinsic - 2 samples in one pass - 
		outputs_512 = _mm512_setzero_ps();					//clear outputs
		inputs_512 = _mm512_load_ps(&in[leftIndex]);		//Load 512-bits - in1[l] in1[r] ... in16[l] in16[r]

		//mul - multioly and accumulate into outputs_512
		outputs_512 = _mm512_fmadd_ps(inputs_512, gainsl_512, outputs_512);

		//output to arrays
		for (size_t j(0); j < 16; j += 2)
		{
			out[leftIndex + j] += outputs_512.m512_f32[0 + j];
			out[rightIndex + j] += outputs_512.m512_f32[0 + j + 1];
		}

	#endif
#endif
	}
}

void mix_buffer16(const int16_t* in, int16_t* out, float leftGain, float rightGain, uint32_t blockSize)
{
	TIMER_SCOPED("mix_buffer loop");

	for (uint32_t i = 0; i < (blockSize / 2); ++i)
	{
		uint32_t leftIndex = i * 2;
		uint32_t rightIndex = i * 2 + 1;

		out[leftIndex] += in[leftIndex] * leftGain;
		out[rightIndex] += in[rightIndex] * rightGain;
	}
}

//////////////////////////////////////////////////////////////////////////
// Performs the audio mixing algorithm.
// Focus your instrumentation, analysis and optimization on this function.
// Any functions that it calls are potential bottlenecks.
// Feel free to completely rewrite any of this function.
//////////////////////////////////////////////////////////////////////////
void mix_audio_block(uint32_t blockSize)
{
	TIMER_SCOPED("mix_audio_block scope");

#if INT_16BIT_MIXING == 0
	// Prepare to mix this block
	// Allocate some memory to load samples.
	ALIGN16 float* inputs = new float[blockSize];
	// And the output.
	ALIGN16 float* output = new float[blockSize];

	// Clear output ready to accumulate
	clear_buffer(output, blockSize);
#else
	//16 bit
	ALIGN16 int16_t* inputs = new int16_t[blockSize];
	// And the output.
	ALIGN16 int16_t* output = new int16_t[blockSize];

	// Clear output ready to accumulate
	clear_buffer16(output, blockSize);
#endif

	for (uint32_t i = 0; i < kNumAudioStreams; ++i)
	{
		// Mix out inputs.
		uint32_t leftIndex = i * 2;
		uint32_t rightIndex = i * 2 + 1;

#if INT_16BIT_MIXING == 0
		g_inputFiles[i].read(inputs, blockSize);
		mix_buffer(inputs, output, g_gainFactors[leftIndex], g_gainFactors[rightIndex], blockSize);
#else
		//read 16
		g_inputFiles[i].read16(inputs, blockSize);	
		mix_buffer16(inputs, output, g_gainFactors[leftIndex], g_gainFactors[rightIndex], blockSize);
#endif
	}

#if INT_16BIT_MIXING == 0
	// Write to output file
	g_outputFile.write(output, blockSize);
#else
	// Write 16 bit
	g_outputFile.write16(output, blockSize);
#endif

	// Clean up memory.	
	delete[] inputs;
	delete[] output;
}

// Main entry point function.
int main()
{
	prepare_audio_files();

	TIMER_START("main() mix loop");

	for (uint32_t i = 0; i < kNumBlocks; ++i)
	{
		mix_audio_block(kTestBlockSize);
	}

	TIMER_END;

	std::cout << "Finished: Output audio in " << g_outputFilePath << std::endl;

	TIMER_OUTALL_ATEXIT;
}
