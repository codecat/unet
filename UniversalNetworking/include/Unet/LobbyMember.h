#pragma once

#include <Unet_common.h>
#include <Unet/ServiceID.h>
#include <Unet/LobbyData.h>
#include <Unet/LobbyFile.h>

namespace Unet
{
	class LobbyMember : public LobbyDataContainer
	{
		friend class ::Unet::Internal::Context;

	private:
		Internal::Context* m_ctx;

	public:
		// Only true if all fundamental user data has been received after joining the lobby (should only be a concern on the host)
		bool Valid = true;

		xg::Guid UnetGuid;
		int UnetPeer = -1;

		// The primary service this member uses to communicate (this is decided by which service the Hello packet is sent through)
		ServiceType UnetPrimaryService = ServiceType::None;

		std::string Name;
		std::vector<ServiceID> IDs;
		std::vector<LobbyFile*> Files;

	public:
		LobbyMember(Internal::Context* ctx);
		~LobbyMember();

		ServiceID GetServiceID(ServiceType type) const;
		ServiceID GetDataServiceID() const;
		ServiceID GetPrimaryServiceID() const;

		json Serialize() const;
		void Deserialize(const json &js);

		virtual void SetData(const std::string &name, const std::string &value) override;
		virtual void RemoveData(const std::string &name) override;

	private:
		void AddFile(const std::string &filename, const std::string &filenameOnDisk);
		void AddFile(const std::string &filename, uint8_t* buffer, size_t size);
		void AddFile(LobbyFile* file);
		void RemoveFile(const std::string &filename);
	};
}
