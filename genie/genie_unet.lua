local DIR_UNET = (path.getabsolute('..') .. '/') .. 'UniversalNetworking/'

dofile('genie_common.lua')

function unet_project(options)
	options = unet_verify_options(options)

	project 'unet'
		kind 'StaticLib'

		includedirs {
			DIR_UNET .. 'include/',
		}

		pchheader 'Unet_common.h'
		pchsource(DIR_UNET .. 'src/Unet_common.cpp')

		unet_defines()
		unet_modules(options.modules, true)
		unet_guid()

		-- Files
		files {
			DIR_UNET .. 'src/*.cpp',
			DIR_UNET .. 'src/*.c',
			DIR_UNET .. 'src/Results/*.cpp',
			DIR_UNET .. 'include/**.hpp',
			DIR_UNET .. 'include/**.h',
		}

		-- System sources
		if os.get() == 'windows' then
			files {
				DIR_UNET .. 'src/System/SystemWindows.cpp',
			}
		elseif os.get() == 'linux' then
			files {
				DIR_UNET .. 'src/System/SystemLinux.cpp',
			}
		elseif os.get() == 'macosx' then
			files {
				DIR_UNET .. 'src/System/SystemMacOS.mm',
			}
		end
end
