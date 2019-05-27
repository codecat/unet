local DIR_ROOT = (path.getabsolute('..') .. '/')
local DIR_UNET = DIR_ROOT .. 'UniversalNetworking/'
local DIR_UNET_TEST = DIR_ROOT .. 'UniversalNetworkingTest/'

dofile('genie_common.lua')

function unet_test_project(options)
	options = unet_verify_options(options)

	project 'unet_test'
		kind('ConsoleApp')

		configuration 'Debug'
			debugdir(DIR_ROOT .. 'bin/debug/')

		configuration 'Release'
			debugdir(DIR_ROOT .. 'bin/release/')

		configuration {}

		unet_defines()
		unet_modules(options.modules)
		unet_guid()

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

		-- Specify rpath
		if os.get() == 'linux' then
			linkoptions { '-Wl,-rpath,.' }
		elseif os.get() == 'macosx' then
			linkoptions { '-Wl,-rpath,"@loader_path",-rpath,.' }
		end
end
