DIR_ROOT = (path.getabsolute('..') .. '/')

dofile('genie_unet.lua')
dofile('genie_unet_cli.lua')

solution 'Unet'
	language 'C++'
	location('Projects/' .. _ACTION)
	startproject 'unet_cli'

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
	options = {
		modules = {
			steam = { link = true },
			galaxy = { link = true },
			enet = { link = true },
		}
	}
	unet_project(options)
	unet_cli_project(options)
