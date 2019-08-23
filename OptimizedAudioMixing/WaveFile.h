#pragma once
//////////////////////////////////////////////////////////////////////////
// Audio Mixing Prototype (Optimization Assignment)
//////////////////////////////////////////////////////////////////////////

#include "Config.h"
#include <fstream>
#include <vector>

namespace WavAudio {
//////////////////////////////////////////////////////////////////////////////
// Useful information on the Wav file format.
// http://www-mmsp.ece.mcgill.ca/Documents/AudioFormats/WAVE/WAVE.html
//////////////////////////////////////////////////////////////////////////////

#pragma pack(push,1)
struct ChunkInfo
{
	uint32_t m_id;
	uint32_t m_size;
};
#pragma pack(pop)

#pragma pack(push,1)
struct WaveChunk
{
	uint32_t	m_id;	// always 'WAVE'
};
#pragma pack(pop)

#pragma pack(push,1)
struct FmtChunk
{
	uint16_t m_formatTag; // always _fmt
	uint16_t m_channels;
	uint32_t m_samplesPerSec;
	uint32_t m_avgBytesPerSec;
	uint16_t m_blockAlign;
	uint16_t m_bitsPerSample;
	uint16_t m_cbSize;
	uint16_t m_validBitsPerSample;
	uint32_t m_channelMask;
	uint8_t  m_subFormatGuid[16];
};
#pragma pack(pop)

enum class eAudioFormat
{
	kFormat_16bitPCM
};

FmtChunk make_format(eAudioFormat format, uint16_t channels, uint32_t samplerate);

// Define an exception type that we throw if parsing the wave file fails.
class WavAudioFileException : public std::exception
{
public:
	WavAudioFileException(const char* msg) : m_msg(msg) {}
	virtual char const* what() const override { return m_msg; }
private:
	const char* m_msg;
};


// Audio file reading and writing class.
// Limit support for .wav file format :
//		Reads : 16bit, mono, PCM
//		Writes : 16bit, mono, PCM
class WavAudioFile
{
public:
	WavAudioFile();

	WavAudioFile(const WavAudioFile&) = delete;
	WavAudioFile& operator = (const WavAudioFile&) = delete;

	FmtChunk get_format() const { return m_formatChunk; }
	uint32_t get_samples() const { return m_samples; }
	uint32_t get_channels() const { return m_formatChunk.m_channels; }

	void print_format_info(std::ostream& streamOut) const;

	void allocate_scratch_memory(uint32_t size);
	uint8_t* scratch_memory() { return &m_scratchMemory[0]; }

protected:
	std::vector<uint8_t> m_scratchMemory; // decoder memory block, lazy allocation as we need it.
	FmtChunk m_formatChunk; // hold the format information
	uint32_t m_samples;		// number of samples in the data block.
};


class WavAudioFileInput : public WavAudioFile
{
public:
	// inherit default constructor
	WavAudioFileInput(){}

	// construct and open for reading
	WavAudioFileInput(const char* filename);

	void open(const char* filename);

	// Read samples, samples are converted to floating point but the channel data remains interleaved.
	void read(float* buffer, uint32_t numSamples);

	uint32_t samples_remaining() const { return m_samples - m_readPosition; }

	//custom 16 bit functions
	void read16(int16_t * buffer, uint32_t numSamples);

private:

	void handle_format_chunk(std::ifstream& audioFile, const ChunkInfo& chunkInfo, uint32_t offset);

	void handle_data_chunk(std::ifstream& audioFile, const ChunkInfo& chunkInfo, uint32_t offset);

private:
	std::ifstream m_audioFile; // file stream
	uint32_t m_dataStart; // start position of audio data in bytes
	uint32_t m_dataSize; // size of audio data in bytes
	uint32_t m_readPosition; // read position in samples
};



class WavAudioFileOutput : public WavAudioFile
{
public:
	WavAudioFileOutput() {}

	// construct and open for writing with specified format.
	WavAudioFileOutput(const char* filename, FmtChunk format);

	~WavAudioFileOutput();

	void open(const char* filename, FmtChunk format);

	// Read samples, samples are converted to floating point but the channel data is interleaved.
	void write(const float* buffer, uint32_t numSamples);

	void close();

	//custom 16 bit functions
	void write16(const int16_t * buffer, uint32_t numSamples);

private:

	void write_header();

	std::ofstream m_audioFile; // file stream
	uint32_t m_audioDataSize; // size of audio data in bytes
};

} // namespace WavAudio


