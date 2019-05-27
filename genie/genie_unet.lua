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
		unet_services(options.modules)

		-- Guid defines
		if os.get() == 'windows' then
			defines { 'GUID_WINDOWS' }
		elseif os.get() == 'linux' then
			defines { 'GUID_LIBUUID' }
		elseif os.get() == 'macosx' then
			defines { 'GUID_CFUUID' }
		end

		-- Guid links
		if os.get() == 'linux' then
			links { 'uuid' }
		elseif os.get() == 'macosx' then
			links { 'CoreFoundation.framework' }
		end

		-- Files
		files {
			DIR_UNET .. 'src/**.cpp',
			DIR_UNET .. 'src/**.c',
			DIR_UNET .. 'include/**.hpp',
			DIR_UNET .. 'include/**.h',
		}

		-- ObjC++ Files on Mac
		if os.get() == 'macosx' then
			files { DIR_UNET .. 'src/**.mm' }
		end

		-- System sources
		if os.get() == 'windows' then
			removefiles {
				DIR_UNET .. 'src/System/SystemLinux.cpp',
				DIR_UNET .. 'src/System/SystemMacOS.mm',
			}
		elseif os.get() == 'linux' then
			removefiles {
				DIR_UNET .. 'src/System/SystemWindows.cpp',
				DIR_UNET .. 'src/System/SystemMacOS.mm',
			}
		elseif os.get() == 'macosx' then
			removefiles {
				DIR_UNET .. 'src/System/SystemWindows.cpp',
				DIR_UNET .. 'src/System/SystemLinux.cpp',
			}
		end
end
