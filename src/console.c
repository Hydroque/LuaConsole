
#define PRIMARY_BUFFER_SIZE		(1024 + 1)
#define SECONDARY_BUFFER_SIZE	(1032 + 1)


#define DEFINES_INIT			4
#define DEFINES_EXPANSION		4

#define LIBRARIES_INIT			2
#define LIBRARIES_EXPANSION		2


#if defined(linux) || defined(__linux__) || defined(__linux)
#	include <unistd.h>
#	include <stdio.h>
#	include <stdlib.h>
#elif defined(unix) || defined(__unix__) || defined(__unix)
#	include <unistd.h>
#	include <stdio.h>
#	include <stdlib.h>
#elif defined(__APPLE__) || defined(__MACH__)
#	include <unistd.h>
#	include <stdio.h>
#	include <stdlib.h>
#elif defined(_WIN32) || defined(_WIN64)
#	include <windows.h>
#	include <stdio.h>
#	include <stdlib.h>
#	include <dirent.h>
#else
#	error "Not familiar. Set up headers accordingly, or -D__linux__ of -Dunix or -D__APPLE__ or -D_WIN32"
#endif

#include <string.h>

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#include "darr.h"

#if defined(LUACON_ADDITIONS)
#	include "additions.h"
#endif


#define LUA_CONSOLE_COPYRIGHT	"LuaConsole Copyright MIT (C) 2017 Hydroque\n"


// internal enums, represent lua error category
typedef enum LuaConsoleError {
	INTERNAL_ERROR = 0,
	SYNTAX_ERROR = 1,
	RUNTIME_ERROR = 2,
} LuaConsoleError;


// usage message
const char HELP_MESSAGE[] = 
	"Lua Console | Version: 1/2/2017\n"
	LUA_COPYRIGHT
	"\n"
	LUA_CONSOLE_COPYRIGHT
	"\n"
	"Supports Lua5.3, Lua5.2, Lua5.1\n"
	"5.2.x and 5.1.x assume that you have enough memory for initial functions.\n"
	"\n"
	"\t- Files executed by passing\n"
	"\t- Global variable defintions\n"
	"\t- Dynamic module loading\n"
	#if defined(LUACON_ADDITIONS)
		"\t- Working directory support\n"
		"\t- Built in stack-dump\n"
		"\t- Console clearing\n"
	#endif
	"\n"
	"Usage: lua.exe [FILE_PATH] [-v] [-e] [-s START_PATH] [-p] [-a] [-c] [-Dvar=val] [-Lfilepath.lua] [-b] [-?] [-n]{parameter1 ...} \n"
	"\n"
	"-v \t Prints the Lua version in use\n"
	"-e \t Prevents lua core libraries from loading\n"
	"-s \t Issues a new root path\n"
	"-p \t Has console post exist after script in line by line mode\n"
	#if defined(LUACON_ADDITIONS)
		"-a \t Disables the additions\n"
	#endif
	"-c \t No copyright on init\n"
	"-d \t Defines a global variable as value after '='\n"
	"-l \t Executes a module before specified script\n"
	"-b \t Load specified parameters by -n before -l modules execute\n"
	"-n \t Start of parameter section\n"
	"-? \t Displays this help message\n";



// one environment per process
static lua_State* L = NULL;


// necessary to put this outside of main, print doesn't work
static int no_libraries = 0;




// comprehensive error output
static void print_error(LuaConsoleError error) {
	switch(error) {
	case INTERNAL_ERROR:
		fprintf(stderr, " (Internal)");
		break;
	case SYNTAX_ERROR:
		fprintf(stderr, " (Syntax)");
		break;
	case RUNTIME_ERROR:
		fprintf(stderr, " (Runtime)");
		break;
	}
	const char* msg = lua_tostring(L, 1);
	size_t top = lua_gettop(L);
	fprintf(stderr, " | Stack Top: %zu | %s\n", top, msg);
	lua_pop(L, 1);
	#if defined(LUACON_ADDITIONS)
		if(top > 1)
			stack_dump(L);
	#endif
}

static int lua_print_error(lua_State* L) {
	const char* msg = lua_tostring(L, -1);
	luaL_traceback(L, L, "--", 1);
	size_t top = lua_gettop(L);
	const char* tb = lua_tostring(L, -1);
	fprintf(stderr, " (Runtime) | Stack Top: %zu | %s\n %s\n", top, msg, tb);
	lua_pop(L, 1);
	lua_pop(L, 1);
	top = lua_gettop(L);
	#if defined(LUACON_ADDITIONS)
		if(top > 1)
			stack_dump(L);
	#endif
	return 0;
}



static int lua_protective_error_call(lua_State* L) {
	int base = lua_gettop(L);
	lua_pushcclosure(L, lua_print_error, 0);
	lua_insert(L, base);
	int status = lua_pcall(L, 0, 0, base);
	lua_remove(L, base);
	lua_pop(L, 1);
	return status;
}

// handles the execution of a file.lua
static int lua_main_dofile(lua_State* L) {
	const char* file = lua_tostring(L, 1);
	lua_pop(L, 1);
	if(luaL_loadfile(L, file) != 0) {
		print_error(SYNTAX_ERROR);
		return 0;
	}
	lua_protective_error_call(L);
	return 0;
}

void lua_load_parameters(char** parameters_argv, size_t param_len) {
	lua_createtable(L, param_len, 0);
	for(size_t i=0; i<param_len; i++) {
		lua_pushinteger(L, i+1);
		lua_pushlstring(L, parameters_argv[i], strlen(parameters_argv[i]));
		lua_settable(L, -3);
	}
	lua_setglobal(L, "args");
}


// all-in-one function to handle file and line by line interpretation
int start_protective_mode(lua_CFunction func, const char* file, char** parameters_argv, size_t param_len) {
	lua_pushcclosure(L, func, 0);
	lua_pushlstring(L, file, strlen(file));
	lua_protective_error_call(L);
	return EXIT_SUCCESS;
}

// returns a malloc'd string with each split item being seperated by \0
char* strsplit(const char* str1, const char lookout, size_t len, size_t max) {
	char* cpy = malloc(len);
	if(cpy == NULL) {
		fputs("Error: Out of memory.", stderr);
		return 0;
	}
	memcpy(cpy, str1, len);
	
	size_t temp_max = max;
	for(size_t i=0; i<len-1; i++) {
		if(str1[i] == lookout) {
			cpy[i] = '\0';
			max--;
		}
		if(max == 0)
			break;
	}
	if(temp_max == max) {
		free(cpy);
		return 0;
	}
	return cpy;
}

// counts the number of 'char c' occurances in a string
size_t strcnt(const char* str1, char c) {
	size_t count = 0;
	while(*str1 != '\0') {
		count += (*str1 == c ? 1 : 0);
		str1++;
	}
	return count;
}


// handles arguments, cwd, loads necessary data, executes lua
int main(int argc, char* argv[])
{
	static int print_version = 0;
	static int change_start = 0;
	static int no_file = 0;
	static char* start = NULL;
	#if defined(LUACON_ADDITIONS)
		static int no_additions = 0;
	#endif
	static int copyright_squelch = 0;
	static int delay_parameters = 0;
	
	static size_t parameters = 0;
	static char** parameters_argv = NULL;
	
	static Array* globals = NULL;
	
	static Array* libraries = NULL;
	
	// handle arguments
	if(argc == 1) { // post-exist if !(arguments > 1)
		no_file = 1;
	} else {
		// don't try to execute file if it isn't first argument
		if(argv[1][0] == '-')
			no_file = 1;
		for(size_t i=1; i<(size_t)argc; i++) {
			// if we have args around, break
			if(parameters_argv != 0)
				break;
			// don't parse non-switches
			switch(argv[i][0]) {
			case '/':
			case '-':
				break;
			default:
				continue;
			}
			// set variables up for later parsing
			switch(argv[i][1]) {
			case 'v': case 'V':
				print_version = 1;
				break;
			case 'e': case 'E':
				no_libraries = 1;
				break;
			case 's': case 'S':
				change_start = 1;
				start = argv[i+1];
				break;
			#if defined(LUACON_ADDITIONS)
				case 'a': case 'A':
					no_additions = 1;
					break;
			#endif
			case 'c': case 'C':
				copyright_squelch = 1;
				break;
			case 'd': case 'D':
				if(globals == NULL)
					globals = array_new(DEFINES_INIT, DEFINES_EXPANSION, sizeof(char*));
				array_push(globals, argv[i]);
				break;
			case 'l': case 'L':
				if(libraries == NULL)
					libraries = array_new(LIBRARIES_INIT, LIBRARIES_EXPANSION, sizeof(char*));
				array_push(libraries, argv[i]);
				break;
			case 'b': case 'B':
				delay_parameters = 1;
				break;
			case 'n': case 'N':
				parameters = (argc - i) - 1;
				parameters_argv = &(argv[i+1]);
				break;
			case '?':
				fputs(HELP_MESSAGE, stdout);
				return EXIT_SUCCESS;
			}
		}
	}
	
	
	// make sure to start in the requested directory, if any
	if(change_start == 1 && chdir(start) == -1) {
		fputs("Error: Invalid start directory supplied.", stderr);
		fputs(HELP_MESSAGE, stdout);
		return EXIT_FAILURE;
	}
	
	
	// initiate lua
	L = luaL_newstate();
	if(L == NULL) {
		fputs("Lua Allocation Error: Out of Memory.", stderr);
		return EXIT_FAILURE;
	}
	
	
	// initiate global variables set up
	if(globals != NULL) {
		for(size_t i=0; i<globals->size; i++) {
			char* str = (char*) array_get(globals, i) + 2;
			
			char* m_args = strsplit(str, '=', strlen(str) + 1, 2);
			if(m_args == 0) {
				fputs("Error: Incorrect -D specified. Use format 'name=value'.", stderr);
				return EXIT_FAILURE;
			}
			char* arg1 = m_args;
			char* arg2 = arg1 + (strlen(arg1) + 1);
			
			size_t dot_count = strcnt(arg1, '.');
			if(dot_count > 0) {
				dot_count++;
				Array* args = array_new(dot_count, 4, sizeof(char*));
				if(args == 0) {
					fputs("Error: Out of memory.", stderr);
					return EXIT_FAILURE;
				}
				char* d_args = strsplit(arg1, '.', strlen(arg1) + 1, -1);
				if(d_args == 0) {
					fputs("Error: Incorrect -D specified. Use format 'name.sub=value'.", stderr);
					return EXIT_FAILURE;
				}
				
				size_t offset = 0;
				for(size_t l=0; l<dot_count; l++) {
					array_push(args, d_args + offset);
					offset += strlen(d_args + offset) + 1;
				}
				
				lua_getglobal(L, (char*) array_get(args, 0));
				int istab = lua_istable(L, -1);
				if(istab == 0) {
					lua_pop(L, 1);
					lua_newtable(L);
				}
				for(size_t l=1; l<dot_count - 1; l++) {
					char* argsl = (char*) array_get(args, l);
					lua_getfield(L, -1, argsl);
					if(lua_istable(L, -1) == 0) {
						lua_pop(L, 1);
						lua_newtable(L);
						lua_setfield(L, -2, argsl);
						lua_getfield(L, -1, argsl);
					}
				}
				lua_pushlstring(L, arg2, strlen(arg2));
				lua_setfield(L, -2, (char*) array_get(args, dot_count - 1));
				lua_pop(L, dot_count-2);
				if(istab == 0)
					lua_setglobal(L, (char*)array_get(args, 0));
				array_free(args);
				free(d_args);
				free(m_args);
				continue;
			}
			lua_pushlstring(L, arg2, strlen(arg2));
			lua_setglobal(L, arg1);
			
			free(m_args);
		}
		array_free(globals);
	}
	
	
	// initiate the libraries
	if(no_libraries == 0)
		luaL_openlibs(L);
	
	
	// print out the version, new state because no_libraries can be 1
	if(print_version == 1) {
		lua_State* gL = luaL_newstate();
		if(gL == NULL) {
			fputs("Error: Out of memory.", stderr);
			return EXIT_FAILURE;
		}
		
		luaL_openlibs(gL);
		
		// print lua version
		lua_getglobal(gL, "_VERSION");
		fprintf(stdout, "%s\n", lua_tostring(gL, 1));
		lua_pop(gL, 1);
		lua_close(gL);
	}
	
	
	// copyright
	if(copyright_squelch == 0) {
		fputs(LUA_COPYRIGHT "\n", stdout);
		fputs(LUA_CONSOLE_COPYRIGHT, stdout);
	}
	
	
	#if defined(LUACON_ADDITIONS)
		// add additions
		if(no_additions == 0)
			luaopen_additionsdll(L);
	#endif
	
	
	// load parameters early
	if(delay_parameters == 1)
		lua_load_parameters(parameters_argv, parameters);
	
	
	// do passed libraries/modules
	if(libraries != NULL) {
		for(size_t i=0; i<libraries->size; i++) {
			const char* str = (char*) array_get(libraries, i) + 2;
			if(luaL_loadfile(L, str) != 0) {
				print_error(SYNTAX_ERROR);
				continue;
			}
			lua_protective_error_call(L);
		}
		array_free(libraries);
	}
	
	
	// if there is nothing to do, then exit, as there is nothing left to do
	if(no_file == 1) {
		lua_close(L);
		return EXIT_SUCCESS;
	}
	
	
	// load function into protected mode (pcall)
	// load parameters late
	if(delay_parameters == 0)
		lua_load_parameters(parameters_argv, parameters);
	int status = start_protective_mode(&lua_main_dofile, argv[1], parameters_argv, parameters);
	
	
	// free resources
	lua_close(L);
	
	return status;
}

