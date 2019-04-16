#pragma once

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include <string>
#include <vector>
#include <queue>
#include <algorithm>
#include <memory>

#include <Unet/guid.hpp>

#include <Unet/json.hpp>
using json = nlohmann::json;

namespace Unet
{
	class Callbacks;
	class Context;
	class Lobby;
	class Service;
}
