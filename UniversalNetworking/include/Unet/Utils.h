#pragma once

#include <Unet_common.h>

namespace Unet
{
	template<typename ... Args>
	std::string strPrintF(const std::string& format, Args ... args)
	{
		size_t size = snprintf(nullptr, 0, format.c_str(), args ...) + 1;
		std::unique_ptr<char[]> buf(new char[size]);
		snprintf(buf.get(), size, format.c_str(), args ...);
		return std::string(buf.get(), buf.get() + size - 1);
	}

	std::vector<uint8_t> JsonPack(const json &js);
	json JsonUnpack(const std::vector<uint8_t> &data);
	json JsonUnpack(uint8_t* data, size_t size);
}
