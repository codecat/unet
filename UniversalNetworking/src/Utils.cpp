#include <Unet_common.h>
#include <Unet/Utils.h>

std::vector<uint8_t> Unet::JsonPack(const json &js)
{
	return json::to_msgpack(js);
}

json Unet::JsonUnpack(const std::vector<uint8_t> &data)
{
	return json::from_msgpack(data);
}

json Unet::JsonUnpack(uint8_t* data, size_t size)
{
	return json::from_msgpack(data, size);
}
