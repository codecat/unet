#pragma once

#include <Unet_common.h>

namespace Unet
{
	class LobbyFile
	{
	public:
		std::string m_filename;
		uint64_t m_hash;

		uint8_t* m_buffer = nullptr;
		size_t m_size = 0;
		size_t m_availableSize = 0;

	public:
		LobbyFile(const std::string &filename);
		~LobbyFile();

		void Prepare(size_t size, uint64_t hash);

		void LoadFromFile(const std::string &filenameOnDisk);
		void Load(uint8_t* buffer, size_t size);

		bool IsValid();
	};
}
