-- Defines
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
configuration {}
