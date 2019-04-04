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
	platforms {
		'x32',
		'x64',
	}

	-- Flags
	flags {
		'NoRTTI',
		--'Cpp17',

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
