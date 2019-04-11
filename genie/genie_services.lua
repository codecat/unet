local DIR_UNET = (path.getabsolute('..') .. '/') .. 'UniversalNetworking/'
local DIR_LIB = (path.getabsolute('..') .. '/') .. 'lib/'

configuration {}

-- Include dirs
includedirs {
	DIR_UNET .. 'include/',
	DIR_LIB .. 'steamworks/public/',
	DIR_LIB .. 'galaxy/Include/',
	DIR_LIB .. 'enet/include/',
}

-- Library dirs
libdirs {
	DIR_LIB .. 'galaxy/Libraries/',
	DIR_LIB .. 'enet/lib/',
}

-- Steam dir
if os.get() == 'windows' then
	configuration 'x64'
		libdirs {
			DIR_LIB .. 'steamworks/redistributable_bin/win64/',
		}
	configuration 'x32'
		libdirs {
			DIR_LIB .. 'steamworks/redistributable_bin/',
		}
elseif os.get() == 'linux' then
	configuration 'x64'
		libdirs {
			DIR_LIB .. 'steamworks/redistributable_bin/linux64/',
		}
	configuration 'x32'
		libdirs {
			DIR_LIB .. 'steamworks/redistributable_bin/linux32/',
		}
elseif os.get() == 'macosx' then
	libdirs {
		DIR_LIB .. 'steamworks/redistributable_bin/osx32/',
	}
end

-- Link to Steam
if os.get() == 'windows' then
	configuration 'x64'
		links { 'steam_api64' }
	configuration 'x32'
		links { 'steam_api' }
else
	configuration {}
		links { 'steam_api' }
end

-- Link to Galaxy
if os.get() == 'linux' then
	configuration 'x64'
		links { 'Galaxy64' }
	configuration 'x32'
		links { 'Galaxy' }
else
	configuration {}
		links { 'Galaxy' }
end

-- Link to enet
if os.get() == 'windows' then
	configuration 'x64'
		links { 'enet64' }
	configuration 'x32'
		links { 'enet' }
else
	configuration {}
		links { 'enet' }
end

if os.get() == 'windows' then
	configuration {}
		links {
			'Ws2_32',
			'Winmm',
		}
end

configuration {}
