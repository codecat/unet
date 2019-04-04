local DIR_UNET = (path.getabsolute('..') .. '/') .. 'UniversalNetworking/'

project 'unet'
	kind 'StaticLib'

	dofile('genie_common.lua')
	dofile('genie_services.lua')

	-- Guid defines
	if os.get() == 'windows' then
		defines { 'GUID_WINDOWS' }
	elseif os.get() == 'linux' then
		defines { 'GUID_LIBUUID' }
	elseif os.get() == 'macosx' then
		defines { 'GUID_CFUUID' }
	end

	-- Linux links
	if os.get() == 'linux' then
		links { 'uuid' }
	end

	-- Files
	files {
		DIR_UNET .. 'src/**.cpp',
		DIR_UNET .. 'src/**.c',
		DIR_UNET .. 'include/**.hpp',
		DIR_UNET .. 'include/**.h',
	}