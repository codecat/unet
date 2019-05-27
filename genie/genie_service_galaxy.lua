local DIR_UNET = (path.getabsolute('..') .. '/') .. 'UniversalNetworking/'
local DIR_LIB = (path.getabsolute('..') .. '/') .. 'lib/'

configuration {}

-- Include dir
includedirs {
	DIR_LIB .. 'galaxy/Include/',
}

-- Lib dir
libdirs {
	DIR_LIB .. 'galaxy/Libraries/',
}

-- Link
if os.get() == 'linux' then
	configuration 'x64'
		links { 'Galaxy64' }
	configuration 'x32'
		links { 'Galaxy' }
else
	configuration {}
		links { 'Galaxy' }
end
