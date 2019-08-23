//////////////////////////////////////////////////////////////////////////
// Audio Mixing Prototype (Optimization Assignment)
//////////////////////////////////////////////////////////////////////////

#include "WaveFile.h"
#include <iostream>

namespace WavAudio {

// Builds a RIFF chunk identifier.
constexpr uint32_t make_riff_fourcc(const char* str)
{
	return (str[3] << 24) | (str[2] << 16) | (str[1] << 8) | (str[0] << 0);
}

// Prints a RIFF chunk identifier to an ostream.
inline void print_fourcc(std::ostream& s, uint32_t id)
{
	const char* pfourcc(reinterpret_cast<const char*>(&id));
	s << pfourcc[0] << pfourcc[1] << pfourcc[2] << pfourcc[3];
}

// Define some common riff chunks.
namespace ChunkId{
enum eChunkId : uint32_t
{
	kRiff = make_riff_fourcc("RIFF")
	, kWave = make_riff_fourcc("WAVE")
	, kFmt = make_riff_fourcc("fmt ")
	, kData = make_riff_fourcc("data")
};
}

namespace WaveFormatCode {
enum eWaveFormatCode : uint16_t
{
	kFormatCode_PCM = 0x0001,
	kFormatCode_Float = 0x0003,
	kFormatCode_ALAW = 0x0006,
	kFormatCode_MULAW = 0x0007,
	kFormatCode_Extensible = 0xFFFE,
};
}


inline void decode_16bit_pcm_to_float(const uint8_t* inBuffer, float* outBuffer, uint32_t numSamples)
{
	constexpr uint32_t kMax = 1 << (16 - 1); // i.e. 2^(bitdepth-1)
	constexpr float kfCoef = 1.0f / kMax;

	const int16_t* pIn = reinterpret_cast<const int16_t*>(inBuffer);
	for (uint32_t i = 0; i < numSamples; i++)
	{
		outBuffer[i] = (float)pIn[i] * kfCoef;
	}
}

//NEW -- 16 bit passthrough
inline void decode_16bit_pcm_to_16bit(const uint8_t* inBuffer, int16_t* outBuffer, uint32_t numSamples)
{
	const int16_t* pIn = reinterpret_cast<const int16_t*>(inBuffer);
	for (uint32_t i = 0; i < numSamples; i++)
	{
		outBuffer[i] = pIn[i];
	}
}

inline void encode_float_to_16bit(const float* inBuffer, uint8_t* outBuffer, uint32_t numSamples)
{
	constexpr uint32_t kMax = 1 << (16 - 1); // i.e. 2^(bitdepth-1)
	constexpr float kfCoef = kMax;

	int16_t* pOut = reinterpret_cast<int16_t*>(outBuffer);
	for (uint32_t i = 0; i < numSamples; i++)
	{
		pOut[i] = (uint16_t)(inBuffer[i] * kfCoef);
	}
}

//NEW -- 16 bit passback
inline void encode_16bit_to_16bit(const int16_t* inBuffer, uint8_t* outBuffer, uint32_t numSamples)
{
	int16_t* pOut = reinterpret_cast<int16_t*>(outBuffer);
	for (uint32_t i = 0; i < numSamples; i++)
	{
		pOut[i] = inBuffer[i];
	}
}

inline void decode_24bit_pcm_to_float(const uint8_t* inBuffer, float* outBuffer, uint32_t numSamples)
{
	constexpr float kfDecodeFactor = 1.0f / 0x00EFffffu;

	const uint8_t* pIn = inBuffer;
	for (uint32_t i = 0; i < numSamples; i++)
	{
		int32_t sample = (pIn[2] << 16) | (pIn[1] << 8) | pIn[0];  // < would need to check the order the bytes are packed here. 
		outBuffer[i] = (float)sample * kfDecodeFactor;
		pIn += 3;
	}
}


WavAudioFile::WavAudioFile()
	: m_formatChunk{ 0 }
	, m_samples{ 0 }
{}

void WavAudioFile::print_format_info(std::ostream& streamOut) const
{
	streamOut
		<< "\n\tformatTag " << m_formatChunk.m_formatTag
		<< "\n\tchannels " << m_formatChunk.m_channels
		<< "\n\tsamplesPerSec " << m_formatChunk.m_samplesPerSec
		<< "\n\tavgBytesPerSec " << m_formatChunk.m_avgBytesPerSec
		<< "\n\tblockAlign " << m_formatChunk.m_blockAlign
		<< "\n\tbitsPerSample " << m_formatChunk.m_bitsPerSample
		<< "\n\tcbSize " << m_formatChunk.m_cbSize
		<< "\n\tvalidBitsPerSample " << m_formatChunk.m_validBitsPerSample
		<< "\n";
}

void WavAudioFile::allocate_scratch_memory(uint32_t size)
{
	// check current scratch size.
	// reallocate if we need to.
	// otherwise leave as is.
	if (size > m_scratchMemory.size())
	{
		m_scratchMemory.resize(size);
	}
}

WavAudioFileInput::WavAudioFileInput(const char* filename)
{
	open(filename);
}

void WavAudioFileInput::open(const char* filename)
{
	m_audioFile.open(filename, std::ios::binary);
	if (m_audioFile.good())
	{

		ChunkInfo riffChunk;
		m_audioFile.read(reinterpret_cast<char*>(&riffChunk), sizeof(ChunkInfo));
		if (riffChunk.m_id != ChunkId::kRiff)
		{
			throw WavAudioFileException("Could not find RIFF chunk.");
		}
		 
		WaveChunk waveChunk;
		m_audioFile.read(reinterpret_cast<char*>(&waveChunk), sizeof(WaveChunk));
		if (waveChunk.m_id != ChunkId::kWave)
		{
			throw WavAudioFileException("Could not find WAVE chunk.");
		}

		// Now read the RIFF file chunk by chunk
		// For 16/24bit audio you only need the '_fmt' chunk and the 'data' chunk 			

		ChunkInfo chunkInfo;
		m_audioFile.read(reinterpret_cast<char*>(&chunkInfo), sizeof(ChunkInfo));
		uint32_t offset = static_cast<uint32_t>(m_audioFile.tellg());

		while (m_audioFile)
		{
			switch (chunkInfo.m_id)
			{
			case ChunkId::kFmt: handle_format_chunk(m_audioFile, chunkInfo, offset); break;
			case ChunkId::kData: handle_data_chunk(m_audioFile, chunkInfo, offset); break;
			}

			// seek next and read.
			m_audioFile.seekg(offset + chunkInfo.m_size, std::ios_base::beg);
			m_audioFile.read(reinterpret_cast<char*>(&chunkInfo), sizeof(ChunkInfo));
			offset = static_cast<uint32_t>(m_audioFile.tellg());
		}

		// May have hit eof during the chunk info parsing.
		// clear stream state and continue.
		m_audioFile.clear();
		ASSERT(m_audioFile);

		// prepare for streaming
		// seek to the start of the data chunk.
		// calculate any remaining samples based on the format info.
		m_audioFile.seekg(m_dataStart, std::ios_base::beg);
		const uint32_t bytesPerSample = m_formatChunk.m_bitsPerSample / 8;

		m_samples = m_dataSize / bytesPerSample;
		m_readPosition = 0;

		ASSERT(m_audioFile);
	}
	else
	{
		throw WavAudioFileException("Bad audio file");
	}
}

void WavAudioFileInput::read(float* buffer, uint32_t numSamples)
{
	const uint32_t bytesToRead = numSamples * m_formatChunk.m_bitsPerSample / 8;
	allocate_scratch_memory(bytesToRead);

	m_audioFile.read((char*)scratch_memory(), bytesToRead);
	if(m_audioFile)
	{
		decode_16bit_pcm_to_float(scratch_memory(), buffer, numSamples);
		m_readPosition += numSamples;
	}
}

//NEW -- read and pass as 16 bit data
void WavAudioFileInput::read16(int16_t* buffer, uint32_t numSamples)
{
	const uint32_t bytesToRead = numSamples * m_formatChunk.m_bitsPerSample / 8;
	allocate_scratch_memory(bytesToRead);

	m_audioFile.read((char*)scratch_memory(), bytesToRead);
	if (m_audioFile)
	{
		decode_16bit_pcm_to_16bit(scratch_memory(), buffer, numSamples);
		m_readPosition += numSamples;
	}
}

void WavAudioFileInput::handle_format_chunk(std::ifstream& audioFile, const ChunkInfo& chunkInfo, uint32_t offset)
{
	UNUSED(offset);
	UNUSED(chunkInfo);
	audioFile.read(reinterpret_cast<char*>(&m_formatChunk), sizeof(FmtChunk));
}

void WavAudioFileInput::handle_data_chunk(std::ifstream& audioFile, const ChunkInfo& chunkInfo, uint32_t offset)
{
	UNUSED(audioFile);

	// can keep reading blocks up until the chunkInfo.m_size
	m_dataStart = offset;
	m_dataSize = chunkInfo.m_size;
}

WavAudioFileOutput::WavAudioFileOutput(const char* filename, FmtChunk format)
{
	open(filename, format);
}

WavAudioFileOutput::~WavAudioFileOutput()
{
	close();
}

void WavAudioFileOutput::open(const char* filename, FmtChunk format)
{
	std::cout << "Output file: " << filename << "\n";

	m_formatChunk = format;
	m_audioFile.open(filename, std::ios::binary);
	if (m_audioFile.good())
	{
		m_audioDataSize = 0;
		
		// write an invalid/dummy header so we can stream audio to the correct location on disk
		// before closing the file we seek back and re-write the header
		write_header(); 
	}
}

void WavAudioFileOutput::write(const float* buffer, uint32_t numSamples)
{
	const uint32_t bytesToWrite = numSamples * m_formatChunk.m_bitsPerSample / 8;
	allocate_scratch_memory(bytesToWrite);

	encode_float_to_16bit(buffer, scratch_memory(), numSamples);
	m_audioFile.write((const char*)scratch_memory(), bytesToWrite);
	m_audioDataSize += bytesToWrite;
	m_samples += numSamples;
}

//NEW -- write as 16 bit
void WavAudioFileOutput::write16(const int16_t* buffer, uint32_t numSamples)
{
	const uint32_t bytesToWrite = numSamples * m_formatChunk.m_bitsPerSample / 8;
	allocate_scratch_memory(bytesToWrite);

	encode_16bit_to_16bit(buffer, scratch_memory(), numSamples);
	m_audioFile.write((const char*)scratch_memory(), bytesToWrite);
	m_audioDataSize += bytesToWrite;
	m_samples += numSamples;
}

void WavAudioFileOutput::close()
{
	if (m_audioFile.good())
	{
		// seek back and re-write the header
		// write an valid header so we can stream audio to the correct location on disk
		m_audioFile.seekp(0, std::ios_base::beg);
		write_header();
	}
}

void WavAudioFileOutput::write_header()
{
	ChunkInfo riffChunk;
	riffChunk.m_id = ChunkId::kRiff;
	riffChunk.m_size = sizeof(WaveChunk)
						+ sizeof(ChunkInfo) + sizeof(FmtChunk) // fmt chunk info and fmt data
						+ sizeof(ChunkInfo) + m_audioDataSize; // data chunk info and audio data

	m_audioFile.write(reinterpret_cast<const char*>(&riffChunk), sizeof(ChunkInfo));

	WaveChunk waveChunk;
	waveChunk.m_id = ChunkId::kWave;
	m_audioFile.write(reinterpret_cast<const char*>(&waveChunk), sizeof(WaveChunk));

	ChunkInfo fmtChunkInfo;
	fmtChunkInfo.m_id = ChunkId::kFmt;
	fmtChunkInfo.m_size = sizeof(FmtChunk);
	m_audioFile.write(reinterpret_cast<const char*>(&fmtChunkInfo), sizeof(ChunkInfo));
	m_audioFile.write(reinterpret_cast<const char*>(&m_formatChunk), sizeof(FmtChunk));

	ChunkInfo dataChunk;
	dataChunk.m_id = ChunkId::kData;
	dataChunk.m_size = m_audioDataSize;
	m_audioFile.write(reinterpret_cast<const char*>(&dataChunk), sizeof(ChunkInfo));

}

WavAudio::FmtChunk make_format(eAudioFormat format, uint16_t channels, uint32_t samplerate)
{
	FmtChunk fmt = { 0 };
	fmt.m_channels = channels;
	fmt.m_samplesPerSec = samplerate;

	switch (format)
	{
	case eAudioFormat::kFormat_16bitPCM:
		fmt.m_formatTag = WaveFormatCode::kFormatCode_PCM;
		fmt.m_bitsPerSample = 16;
		fmt.m_avgBytesPerSec = samplerate * fmt.m_bitsPerSample / 8 * channels;
		fmt.m_blockAlign = fmt.m_bitsPerSample / 8 * channels;
		break;
	}
	return fmt;
}

} // namespace WavAudio