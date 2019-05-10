#include <Unet_common.h>
#include <Unet/LobbyFile.h>
#include <Unet/xxhash.h>

Unet::LobbyFile::LobbyFile(const std::string &filename)
{
	m_filename = filename;
}

Unet::LobbyFile::~LobbyFile()
{
	if (m_buffer != nullptr) {
		free(m_buffer);
	}
}

void Unet::LobbyFile::Prepare(size_t size, uint64_t hash)
{
	if (m_buffer != nullptr) {
		free(m_buffer);
	}

	m_buffer = (uint8_t*)malloc(size);
	m_size = size;
	m_availableSize = 0;

	m_hash = hash;

	//TODO: Load from cache if we can find it
}

void Unet::LobbyFile::LoadFromFile(const std::string &filenameOnDisk)
{
	//TODO: Don't keep buffer in memory and just read from file when needed

	if (m_buffer != nullptr) {
		free(m_buffer);
		m_buffer = nullptr;
	}

	FILE* fh = fopen(filenameOnDisk.c_str(), "rb");
	if (fh == nullptr) {
		// File does not exist!
		assert(false);
		return;
	}

	fseek(fh, 0, SEEK_END);
	m_size = ftell(fh);
	m_availableSize = m_size;
	m_buffer = (uint8_t*)malloc(m_size);
	fseek(fh, 0, SEEK_SET);

	fread(m_buffer, 1, m_size, fh);
	fclose(fh);

	m_hash = XXH64(m_buffer, m_size, 0);
}

void Unet::LobbyFile::Load(uint8_t* buffer, size_t size)
{
	if (m_buffer != nullptr) {
		free(m_buffer);
	}

	m_size = size;
	m_availableSize = size;
	m_buffer = (uint8_t*)malloc(size);
	memcpy(m_buffer, buffer, size);

	m_hash = XXH64(m_buffer, m_size, 0);
}

void Unet::LobbyFile::AppendData(uint8_t* buffer, size_t size)
{
	assert(m_availableSize + size <= m_size);

	memcpy(m_buffer + m_availableSize, buffer, size);
	m_availableSize += size;
}

bool Unet::LobbyFile::IsValid()
{
	if (m_buffer == nullptr) {
		return false;
	}

	if (m_availableSize != m_size) {
		return false;
	}

	uint64_t hash = XXH64(m_buffer, m_size, 0);
	if (hash != m_hash) {
		// If the hash doesn't match, something went wrong!
		//TODO: How do we handle this gracefully? Some file transfer failure callback?
		assert(false);
		return false;
	}

	return true;
}

double Unet::LobbyFile::GetPercentage()
{
	return m_availableSize / (double)m_size;
}
