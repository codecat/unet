local DIR_UNET = (path.getabsolute('..') .. '/') .. 'UniversalNetworking/'
local DIR_LIB = (path.getabsolute('..') .. '/') .. 'lib/'

configuration {}

-- Include dir
includedirs {
	DIR_LIB .. 'enet/include/',
}

-- Library dir
libdirs {
	DIR_LIB .. 'enet/lib/',
}

-- Link
if os.get() == 'windows' then
	configuration { 'Debug', 'x64' }
		links { 'enet64d' }
	configuration { 'Release', 'x64' }
		links { 'enet64' }

	configuration { 'Debug', 'x32' }
		links { 'enetd' }
	configuration { 'Release', 'x32' }
		links { 'enet' }
else
	configuration {}
		links { 'enet' }
end

-- Additional libraries
if os.get() == 'windows' then
	configuration {}
		links {
			'Ws2_32',
			'Winmm',
		}
end
