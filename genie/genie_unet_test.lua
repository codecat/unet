local DIR_ROOT = (path.getabsolute('..') .. '/')
local DIR_UNET = DIR_ROOT .. 'UniversalNetworking/'
local DIR_UNET_TEST = DIR_ROOT .. 'UniversalNetworkingTest/'

project 'unet_test'
	kind('ConsoleApp')

	configuration 'Debug'
		debugdir(DIR_ROOT .. 'bin/debug/')

	configuration 'Release'
		debugdir(DIR_ROOT .. 'bin/release/')

	configuration {}

	dofile('genie_common.lua')
	dofile('genie_services.lua')

	-- Files
	files {
		DIR_UNET_TEST .. '**.cpp',
		DIR_UNET_TEST .. '**.c',
		DIR_UNET_TEST .. '**.hpp',
		DIR_UNET_TEST .. '**.h',
	}

	-- Includes
	includedirs {
		DIR_UNET_TEST,
		DIR_UNET .. 'include/',
	}

	-- Links
	links {
		'unet',
	}

	-- Guid links
	if os.get() == 'linux' then
		links { 'uuid' }
	elseif os.get() == 'macosx' then
		links { 'CoreFoundation.framework' }
	end

	-- MacOS rpath
	if os.get() == 'macosx' then
		linkoptions { '-Wl,-rpath,"@loader_path",-rpath,.' }
	end
