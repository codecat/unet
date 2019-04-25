#include <Unet_common.h>
#include <Unet/Reassembly.h>
#include <Unet/Context.h>
#include <Unet/xxhash.h>

Unet::Reassembly::Reassembly(Context* ctx)
{
	m_ctx = ctx;
}

Unet::Reassembly::~Reassembly()
{
	Clear();
}

void Unet::Reassembly::HandleMessage(ServiceID peer, int channel, uint8_t* msgData, size_t packetSize)
{
	uint8_t sequenceId = *(msgData++);
	packetSize--;

	auto existingMsg = std::find_if(m_staging.begin(), m_staging.end(), [sequenceId](NetworkMessage * msg) {
		return msg->m_sequenceId == sequenceId;
	});

	if (existingMsg != m_staging.end()) {
		auto msg = *existingMsg;
		assert(msg->m_packetsLeft > 0);

		msg->m_packetsLeft--;
		msg->Append(msgData, packetSize);
		if (msg->m_packetsLeft == 0) {
			uint32_t finalHash = XXH32(msg->m_data, msg->m_size, 0);
			if (finalHash != msg->m_sequenceHash) {
				m_ctx->GetCallbacks()->OnLogError(strPrintF("Sequence hash for fragmented packet does not match! Packet size: %d", (int)msg->m_size));
			}

			m_staging.erase(existingMsg);
			m_ready.push(msg);
		}
		return;
	}

	uint8_t packetCount = *(msgData++);
	packetSize--;

	if (packetCount > 0) {
		uint32_t packetHash = *(uint32_t*)msgData;
		msgData += 4;
		packetSize -= 4;

		auto newMessage = new NetworkMessage(msgData, packetSize);
		newMessage->m_sequenceId = sequenceId;
		newMessage->m_packetsLeft = packetCount;
		newMessage->m_sequenceHash = packetHash;
		newMessage->m_channel = channel;
		newMessage->m_peer = peer;
		m_staging.emplace_back(newMessage);
		return;
	}

	auto newMessage = new NetworkMessage(msgData, packetSize);
	newMessage->m_channel = channel;
	newMessage->m_peer = peer;
	m_ready.push(newMessage);
}

Unet::NetworkMessage* Unet::Reassembly::PopReady()
{
	if (m_ready.size() == 0) {
		return nullptr;
	}

	auto ret = m_ready.front();
	m_ready.pop();
	return ret;
}

void Unet::Reassembly::Clear()
{
	for (auto msg : m_staging) {
		delete msg;
	}
	m_staging.clear();

	while (m_ready.size() > 0) {
		delete m_ready.front();
		m_ready.pop();
	}
}

void Unet::Reassembly::SplitMessage(uint8_t* data, size_t size, PacketType type, size_t sizeLimit, const std::function<void(uint8_t*, size_t)> &callback)
{
	m_sequenceId++;

	if (type == PacketType::Unreliable) {
		m_tempBuffer.resize(size + 1);
		m_tempBuffer[0] = m_sequenceId;
		m_tempBuffer[1] = 0;
		memcpy(m_tempBuffer.data() + 1, data, size);
		callback(m_tempBuffer.data(), m_tempBuffer.size());
		return;
	}

	// Subtract 3 to ensure that, in the case of these being relay packets, we can still send them
	sizeLimit -= 3; //TODO: Do this selectively, only if relay is actually required?

	size_t numPackets = (size / sizeLimit) + 1;
	uint8_t* ptr = data;

	assert(numPackets <= 256); // It's currently not possible to send more than 256 partial packets at a time

	XXH32_hash_t hashData = 0;
	if (numPackets > 1) {
		hashData = XXH32(data, size, 0);
	}

	for (size_t i = 0; i < numPackets; i++) {
		size_t bytesLeft = size - (ptr - data);
		size_t dataSize = 0;

		if (i == 0) {
			size_t extraData = 2;
			if (numPackets > 1) {
				extraData += 4; // 4 bytes for the full packet hash
			}

			dataSize = std::min(bytesLeft, sizeLimit - extraData);

			m_tempBuffer.resize(dataSize + extraData);
			m_tempBuffer[0] = m_sequenceId;
			//TODO: Don't send number of packets, but final packet size (we should be able to get rid of the numPackets variable then)
			m_tempBuffer[1] = (uint8_t)numPackets - 1;
			if (numPackets > 1) {
				memcpy(m_tempBuffer.data() + 2, &hashData, 4);
			}
			memcpy(m_tempBuffer.data() + extraData, ptr, dataSize);

			callback(m_tempBuffer.data(), m_tempBuffer.size());

		} else {
			size_t extraData = 1;

			dataSize = std::min(bytesLeft, sizeLimit - extraData);

			m_tempBuffer.resize(dataSize + extraData);
			m_tempBuffer[0] = m_sequenceId;
			memcpy(m_tempBuffer.data() + extraData, ptr, dataSize);

			callback(m_tempBuffer.data(), m_tempBuffer.size());
		}

		ptr += dataSize;
	}
}
