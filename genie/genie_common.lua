-- Verifies and returns a valid options object
function unet_verify_options(options)
	if not options then options = {} end
	if not options.modules then options.modules = { 'enet' } end
	return options
end

-- Include the standard defines for Unet.
function unet_defines()
	if os.get() == 'windows' then
		defines {
			'PLATFORM_WINDOWS',
			'_CRT_SECURE_NO_WARNINGS',
			'NOMINMAX',
		}
	elseif os.get() == 'linux' then
		defines { 'PLATFORM_LINUX' }
	elseif os.get() == 'macosx' then
		defines { 'PLATFORM_MACOS' }
	end

	configuration 'Debug'
		defines { 'DEBUG' }
	configuration 'Release'
		defines {
			'NDEBUG',
			'RELEASE',
		}
	configuration {}
end

-- Include all modules from the given list
function unet_services(modules)
	for _, v in pairs(modules) do
		dofile('genie_service_' .. v .. '.lua')
	end
end
