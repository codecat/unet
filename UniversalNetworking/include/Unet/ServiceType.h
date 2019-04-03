#pragma once

namespace Unet
{
	enum class ServiceType
	{
		None,
		Steam,
		Galaxy,
	};

	ServiceType GetServiceTypeByName(const char* str);
	const char* GetServiceNameByType(ServiceType type);
}
