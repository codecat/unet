local DIR_UNET = (path.getabsolute('..') .. '/') .. 'UniversalNetworking/'
local DIR_LIB = (path.getabsolute('..') .. '/') .. 'lib/'

configuration {}

-- Include dirs
includedirs {
	DIR_UNET .. 'include/',
	DIR_LIB .. 'steamworks/public/',
	DIR_LIB .. 'galaxy/Include/',
}

-- Galaxy dir
libdirs {
	DIR_LIB .. 'galaxy/Libraries/',
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

configuration 'x64'
	links { 'Galaxy64' }

configuration 'x32'
	links { 'Galaxy' }

configuration {}
