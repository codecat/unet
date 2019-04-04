DIR_ROOT = (path.getabsolute('..') .. '/')

solution 'UniversalNetworking'
	language 'C++'
	location('Projects/' .. _ACTION)
	startproject 'unet_test'

	-- Configurations
	configurations {
		'Debug',
		'Release',
	}

	-- Platforms
	if os.get() ~= 'macosx' then
		platforms { 'x32' }
	end
	platforms { 'x64' }

	-- Flags
	flags {
		'NoRTTI',
		'NoEditAndContinue',
		'Cpp11',

		-- Generate a pdb on Windows
		'FullSymbols',
		'Symbols',
	}

	configuration 'Debug'
		targetdir(DIR_ROOT .. 'bin/debug/')
		removeflags { 'Optimize' }

	configuration 'Release'
		targetdir(DIR_ROOT .. 'bin/release/')
		flags { 'OptimizeSpeed' }

	-- Projects
	dofile('genie_unet.lua')
	dofile('genie_unet_test.lua')
