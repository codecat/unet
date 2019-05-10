#pragma once

#include <Unet_common.h>
#include <Unet/ServiceID.h>

namespace Unet
{
	class LobbyFile
	{
	public:
		std::string m_filename;
		uint64_t m_hash = 0;

		uint8_t* m_buffer = nullptr;
		size_t m_size = 0;
		size_t m_availableSize = 0;

	public:
		LobbyFile(const std::string &filename);
		~LobbyFile();

		void Prepare(size_t size, uint64_t hash);

		void LoadFromFile(const std::string &filenameOnDisk);
		void Load(uint8_t* buffer, size_t size);

		void AppendData(uint8_t* buffer, size_t size);

		// Checks whether the data captured in this file is complete and valid. Note
		// that this also computes and compares a hash for the entire buffer, so
		// consider that while working in performance-critical code.
		bool IsValid() const;

		double GetPercentage() const;
		double GetPercentage(const struct OutgoingFileTransfer &transfer) const;
	};

	struct OutgoingFileTransfer
	{
		//TODO: Change these to File, Member (or Receiver?), and CurrentPos

		//TODO: Don't put LobbyFile*, but use some ID (or even the hash) of the file
		LobbyFile* m_file = nullptr;
		//TODO: Don't use LobbyMember*, but use the UnetPeer of the member
		class LobbyMember* m_member = nullptr;
		size_t m_currentPos = 0;
	};
}
