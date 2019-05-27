local DIR_UNET = (path.getabsolute('..') .. '/') .. 'UniversalNetworking/'
local DIR_LIB = (path.getabsolute('..') .. '/') .. 'lib/'

return function(options, core)
	if not options then options = {} end
	if not options.dir then options.dir = DIR_LIB .. 'galaxy/' end

	configuration {}

	if core then
		-- Files
		files {
			DIR_UNET .. 'include/Unet/Services/ServiceGalaxy.h',
			DIR_UNET .. 'src/Services/ServiceGalaxy.cpp',
		}
	end

	-- Include dir
	includedirs {
		options.dir .. 'Include/',
	}

	if options.link then
		-- Lib dir
		libdirs {
			options.dir .. 'Libraries/',
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
	end
end
