#pragma once

#include <Unet_common.h>
#include <Unet/ServiceType.h>

namespace Unet
{
	struct ServiceEntryPoint
	{
		ServiceType Service;
		uint64_t ID;
	};
}
