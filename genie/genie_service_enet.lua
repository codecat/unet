local DIR_UNET = (path.getabsolute('..') .. '/') .. 'UniversalNetworking/'
local DIR_LIB = (path.getabsolute('..') .. '/') .. 'lib/'

return function(options, core)
	if not options then options = {} end
	if not options.dir then options.dir = DIR_LIB .. 'enet/' end
	if options.link == nil then options.link = true end

	configuration {}

	if core then
		-- Files
		files {
			DIR_UNET .. 'include/Unet/Services/ServiceEnet.h',
			DIR_UNET .. 'src/Services/ServiceEnet.cpp',
		}
	end

	-- Include dir
	includedirs {
		options.dir .. 'include/',
	}

	if options.link then
		-- Library dir
		libdirs {
			options.dir .. 'lib/',
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
	end
end
