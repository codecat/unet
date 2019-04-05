#pragma once

#include <Unet_common.h>
#include <Unet/ServiceType.h>

namespace Unet
{
	struct ServiceID
	{
		ServiceType Service;
		uint64_t ID;
	};
}
