/*
 * !! Make sure to build this with an old version of GCC so it doesn't !!
 * !! depend on new libgcc_s.so.1 features!                            !!
 *
 * => on debian 7 "wheezy" you could do: "gcc-4.7 -std=gnu99 -o YourGameWrapper wrapper.c -ldl"
 *
 * This is a simple wrapper for your Linux x86 or x86_64 application that
 * checks the versions of some system libs and compares it with the versions
 * of the same libs you supply, and sets LD_LIBRARY_PATH in such a way that
 * your version of the libs is used if it's newer than the one on the system.
 * (If an LD_LIBRARY_PATH is already set, the changes from the wrapper will
 *  be prepended to it)
 * Currently supported libs:
 * - libstdc++.so.6
 * - libgcc_so.so.1
 * - libSDL2.so.0
 * - libcurl.so.4 (only used if not available on the system at all)
 *
 * Directly under this comment are a few #defines you can tweak to your needs
 * to set your wrapped executable, the paths to your fallback libs and it
 * also lets you disable the check for each lib.
 *
 * When executing the wrapper and the environment variable WRAPPER_DEBUG
 * is set 1, some helpful messages about the detected versions and the used
 * LD_LIBRARY_PATH will be printed. This is helpful to debug problems,
 * especially when your users are having them.
 * e.g. $ WRAPPER_DEBUG=1 ./YourGameWrapper
 *
 * (C) 2017-2023 Daniel Gibson
 *
 * LICENSE
 *   This software is dual-licensed to the public domain and under the following
 *   license: you are granted a perpetual, irrevocable license to copy, modify,
 *   publish, and distribute this file as you see fit.
 *   No warranty implied; use at your own risk.
 *
 * So you can do whatever you want with this code, including copying it
 * (or parts of it) into your own source.
 * No need to mention me or this "license" in your code or docs, even though
 * it would be appreciated, of course.
 */

// path to the app you wanna launch, relative to the directory this wrapper is in
#define APP_EXECUTABLE "bin/YourGame"

// uncomment (and adjust) the following line to change the application name
// that will be shown in ps, top etc (if not set, APP_EXECUTABLE will be used)
//#define APP_NAME "YourGame"

// comment out the following line if you don't want to chdir() into the
// directory this wrapper lies in
#define CHANGE_TO_WRAPPER_DIR

// just comment out the ones you don't care about
#define CHECK_LIBSTDCPP
#define CHECK_LIBGCC
#define CHECK_LIBSDL2
#define CHECK_LIBCURL4

// change these defines to the directories you put your fallback libs in
// (relative to the directory this wrapper is in)
#define FALLBACK_DIR_STDCPP "libs/stdcpp"
#define FALLBACK_DIR_GCC    "libs/gcc"
#define FALLBACK_DIR_SDL2   "libs/sdl2"
#define FALLBACK_DIR_CURL   "libs/curl" // only used if no libcurl.so.4 is found on system at all


//
// usually, you won't have to change anything below this line, unless you want
// to support newer GCC versions or additional libs - please send a patch :) -
// or different architectures with different name mangling
//

#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>

enum { HAVE_64_BIT = (sizeof(void*) == 8) };

static int debugOutput = 0;
#define dprintf(...) do { if(debugOutput){ printf(__VA_ARGS__); } } while(0)

#define eprintf(...) fprintf(stderr, __VA_ARGS__)

#if defined(CHECK_LIBSTDCPP) || defined(CHECK_LIBGCC)
struct gcc_version_check
{
	const char* gcc_ver_name;
	const char* fn_version;
	const char* fn_name;
};

static int get_gcc_version(const char* libpath, const struct gcc_version_check checks[], const int num_checks)
{
	void* handle = dlopen(libpath, RTLD_LAZY);
	int i, ret = -1;
	if(handle == NULL)
	{
		eprintf("couldn't dlopen() %s : %s\n", libpath, dlerror());
		return ret;
	}

	for(i=0; i < num_checks; ++i)
	{
		const char* fn_name = checks[i].fn_name;
		if(fn_name != NULL)
		{
			void* fn = dlvsym(handle, fn_name, checks[i].fn_version);
			if(fn == NULL)
			{
				dlerror(); // clear it
				break;
			}
			ret = i;
		}
	}

	dlclose(handle);

	// if ret == num_checks-1, the real version could potentially be even newer
	return ret;
}

static const char* get_gcc_version_name(const struct gcc_version_check checks[], int idx)
{
	if(idx < 0) return "Not found";
	return checks[idx].gcc_ver_name;
}
#endif // defined(CHECK_LIBSTDCPP) || defined(CHECK_LIBGCC)

#ifdef CHECK_LIBSTDCPP
enum stdcpp_gcc_version {
	STDCPP_GCC_OLDER = -1,
	// see https://gcc.gnu.org/onlinedocs/libstdc++/manual/abi.html#abi.versioning
	// Note that this does not list all released GCC versions, but only
	// the ones that bumped the libstdc++ ABI version (GLIBCXX_3.4.x)
	// BTW, the value of MY_GCC_* corresponds with x in GLIBCXX_3.4.x
	STDCPP_GCC_3_4_0 = 0, // GLIBCXX_3.4, CXXABI_1.3
	STDCPP_GCC_3_4_1, // GLIBCXX_3.4.1, CXXABI_1.3
	STDCPP_GCC_3_4_2, // GLIBCXX_3.4.2
	STDCPP_GCC_3_4_3, // GLIBCXX_3.4.3 - unfortunately there's no function with that version in libstdc++
	STDCPP_GCC_4_0_0, // GLIBCXX_3.4.4, CXXABI_1.3.1
	STDCPP_GCC_4_0_1, // GLIBCXX_3.4.5
	STDCPP_GCC_4_0_2, // GLIBCXX_3.4.6
	STDCPP_GCC_4_0_3, // GLIBCXX_3.4.7
	STDCPP_GCC_4_1_1, // GLIBCXX_3.4.8 - no function for x86?!
	STDCPP_GCC_4_2_0, // GLIBCXX_3.4.9,
	STDCPP_GCC_4_3_0, // GLIBCXX_3.4.10, CXXABI_1.3.2
	STDCPP_GCC_4_4_0, // GLIBCXX_3.4.11, CXXABI_1.3.3
	STDCPP_GCC_4_4_1, // GLIBCXX_3.4.12, CXXABI_1.3.3 - no function with that version
	STDCPP_GCC_4_4_2, // GLIBCXX_3.4.13, CXXABI_1.3.3
	STDCPP_GCC_4_5_0, // GLIBCXX_3.4.14, CXXABI_1.3.4
	STDCPP_GCC_4_6_0, // GLIBCXX_3.4.15, CXXABI_1.3.5
	STDCPP_GCC_4_6_1, // GLIBCXX_3.4.16, CXXABI_1.3.5
	STDCPP_GCC_4_7_0, // GLIBCXX_3.4.17, CXXABI_1.3.6
	STDCPP_GCC_4_8_0, // GLIBCXX_3.4.18, CXXABI_1.3.7
	STDCPP_GCC_4_8_3, // GLIBCXX_3.4.19, CXXABI_1.3.7
	STDCPP_GCC_4_9_0, // GLIBCXX_3.4.20, CXXABI_1.3.8
	STDCPP_GCC_5_1_0, // GLIBCXX_3.4.21, CXXABI_1.3.9
	STDCPP_GCC_6_1_0, // GLIBCXX_3.4.22, CXXABI_1.3.10
	STDCPP_GCC_7_1_0, // GLIBCXX_3.4.23, CXXABI_1.3.11
	STDCPP_GCC_7_2_0, // GLIBCXX_3.4.24, CXXABI_1.3.11 - no function for x86_64
	STDCPP_GCC_8_1_0, // GLIBCXX_3.4.25, CXXABI_1.3.11
	STDCPP_GCC_9_1_0, // GLIBCXX_3.4.26, CXXABI_1.3.12
	STDCPP_GCC_9_2_0, // GLIBCXX_3.4.27, CXXABI_1.3.12
	STDCPP_GCC_9_3_0, // GLIBCXX_3.4.28, CXXABI_1.3.12
	//STDCPP_GCC_10_1_0,// GLIBCXX_3.4.28, CXXABI_1.3.12 - same as 9.3.0 ?!
	STDCPP_GCC_11_1_0, // GLIBCXX_3.4.29, CXXABI_1.3.13
	STDCPP_GCC_12_1_0, // GLIBCXX_3.4.30, CXXABI_1.3.13
	STDCPP_GCC_13_1_0, // GLIBCXX_3.4.31, CXXABI_1.3.14
	STDCPP_GCC_13_2_0, // GLIBCXX_3.4.32, CXXABI_1.3.14
	// TODO: add newer versions, once available (also in libstdcpp_version_checks below)
	_NUM_STDCPP_GCC_VERSIONS
};


// I found the entries for this table with: objdump -T /path/to/libstdc++.so.6 | grep " DF .text" | less
// (and then searched for GLIBCXX_3.4.5 or whatever)
// Note that readelf does truncate the function names in output and thus is not suitable for this!
static const struct gcc_version_check
libstdcpp_version_checks[_NUM_STDCPP_GCC_VERSIONS] =
{
	{ "3.4.0", "GLIBCXX_3.4",   "_ZNSt5ctypeIwED2Ev" },
	{ "3.4.1", "GLIBCXX_3.4.1", "_ZNSt12__basic_fileIcE4fileEv" },
	{ "3.4.2", "GLIBCXX_3.4.2", "_ZN9__gnu_cxx17__pool_alloc_base12_M_get_mutexEv" },
	{ "3.4.3", "GLIBCXX_3.4.3", NULL }, // FIXME: couldn't find a function with that version in libstdc++.so.6
	{ "4.0.0", "GLIBCXX_3.4.4", "_ZN9__gnu_cxx6__poolILb0EE10_M_destroyEv" },
	{ "4.0.1", "GLIBCXX_3.4.5", "_ZNKSs11_M_disjunctEPKc" },
	{ "4.0.2", "GLIBCXX_3.4.6", "_ZNSt6locale5facet13_S_get_c_nameEv" },
	// the following has a different signature for 32bit vs 64bit
	{ "4.0.3", "GLIBCXX_3.4.7", HAVE_64_BIT ? "_ZNSt6locale5_Impl16_M_install_cacheEPKNS_5facetEm"
	                                        : "_ZNSt6locale5_Impl16_M_install_cacheEPKNS_5facetEj" },
	{ "4.1.1", "GLIBCXX_3.4.8", HAVE_64_BIT ? "_ZSt17__copy_streambufsIcSt11char_traitsIcEElPSt15basic_streambufIT_T0_ES6_"
	                                        : NULL }, // FIXME: couldn't find a function for i686 ?!
	{ "4.2.0", "GLIBCXX_3.4.9",  "_ZNSo9_M_insertIdEERSoT_" },
	{ "4.3.0", "GLIBCXX_3.4.10", "_ZNKSt4hashISbIwSt11char_traitsIwESaIwEEEclES3_" },
	{ "4.4.0", "GLIBCXX_3.4.11", "_ZSt16generic_categoryv" },
	{ "4.4.1", "GLIBCXX_3.4.12", NULL }, // FIXME: couldn't find a function with that version in libstdc++.so.6
	{ "4.4.2", "GLIBCXX_3.4.13", "_ZNSt14basic_ofstreamIcSt11char_traitsIcEEC2ERKSsSt13_Ios_Openmode" },
	{ "4.5.0", "GLIBCXX_3.4.14", "_ZNSs13shrink_to_fitEv" },
	{ "4.6.0", "GLIBCXX_3.4.15", "_ZNSt17bad_function_callD2Ev" },
	{ "4.6.1", "GLIBCXX_3.4.16", HAVE_64_BIT ? "_ZNSs10_S_compareEmm" : "_ZNSs10_S_compareEjj" },
	{ "4.7.0", "GLIBCXX_3.4.17", "_ZNSt6thread20hardware_concurrencyEv" },
	{ "4.8.0", "GLIBCXX_3.4.18", "_ZNSt13random_device9_M_getvalEv" },
	{ "4.8.3", "GLIBCXX_3.4.19", "_ZNSt6chrono3_V212steady_clock3nowEv" },
	{ "4.9.0", "GLIBCXX_3.4.20", "_ZSt14get_unexpectedv" },
	{ "5.1.0", "GLIBCXX_3.4.21", "_ZNSt13runtime_errorC1EPKc" },
	{ "6.1.0", "GLIBCXX_3.4.22", "_ZNSt6thread6_StateD1Ev" },
	{ "7.1.0", "GLIBCXX_3.4.23", HAVE_64_BIT ? "_ZNSsC1ERKSsmRKSaIcE" : "_ZNSsC1ERKSsjRKSaIcE" },
	{ "7.2.0", "GLIBCXX_3.4.24", HAVE_64_BIT ? NULL // couldn't find a 64bit function with this version
	                                         : "_ZNSbIwSt11char_traitsIwESaIwEEC2ERKS2_jRKS1_" },
	{ "8.1.0", "GLIBCXX_3.4.25", "_ZNKSt13random_device13_M_getentropyEv" },
	{ "9.1.0", "GLIBCXX_3.4.26", "_ZNSs4dataEv" },
	{ "9.2.0", "GLIBCXX_3.4.27", "_ZNSt10filesystem28recursive_directory_iteratoraSERKS0_" },
	{ "9.3.0", "GLIBCXX_3.4.28", "_ZNSt3pmr15memory_resourceD0Ev" },
	// { "10.1.0" "GLIBCXX_3.4.28", "" }, - has same symbol version as 9.3.0
	{ "11.1.0", "GLIBCXX_3.4.29", "_ZNSs7reserveEv" },
	{ "12.1.0", "GLIBCXX_3.4.30", "_ZSt21__glibcxx_assert_failPKciS0_S0_" },
	{ "13.1.0", "GLIBCXX_3.4.31", "_ZSt8to_charsPcS_DF128_" },
	{ "13.2.0", "GLIBCXX_3.4.32", "_ZSt21ios_base_library_initv" },
	// TODO: add new versions once available
};

static enum stdcpp_gcc_version get_libstdcpp_version(const char* path)
{
	return get_gcc_version(path, libstdcpp_version_checks, _NUM_STDCPP_GCC_VERSIONS);
}
#endif // CHECK_LIBSTDCPP

#ifdef CHECK_LIBGCC
enum libgcc_version {
	LIBGCC_OLDER = -1,
	LIBGCC_3_0_0 = 0, // GCC_3.0
	LIBGCC_3_3_0, // GCC_3.3
	LIBGCC_3_3_1, // GCC_3.3.1
	LIBGCC_3_3_2, // GCC_3.3.2 - no function with that version
	LIBGCC_3_3_4, // GCC_3.3.4 - ditto
	LIBGCC_3_4_0, // GCC_3.4
	LIBGCC_3_4_2, // GCC_3.4.2
	LIBGCC_3_4_4, // GCC_3.4.4 - no x86 function with that version
	LIBGCC_4_0_0, // GCC_4.0.0
	LIBGCC_4_1_0, // GCC_4.1.0 - no function with that version
	LIBGCC_4_2_0, // GCC_4.2.0
	LIBGCC_4_3_0, // GCC_4.3.0
	LIBGCC_4_4_0, // GCC_4.4.0 - no x86_64 function with that version
	LIBGCC_4_5_0, // GCC_4.5.0 - ditto
	LIBGCC_4_6_0, // GCC_4.6.0 - no function with that version
	LIBGCC_4_7_0, // GCC_4.7.0
	LIBGCC_4_8_0, // GCC_4.8.0
	// apparently no new functions were added (in x86/amd64) until 7.0.0
	LIBGCC_7_0_0, // GCC_7.0.0
	// apparently no new functions were added (in x86/amd64) until 12.0.0
	LIBGCC_12_1_0, // GCC_12.0.0
	LIBGCC_13_1_0, // GCC_13.0.0
	// TODO: add new versions, if any
	_NUM_LIBGCC_VERSIONS
};

static const struct gcc_version_check
libgcc_version_checks[_NUM_LIBGCC_VERSIONS] =
{
	{ "3.0.0", "GCC_3.0", "__mulvsi3" },
	{ "3.3.0", "GCC_3.3", "_Unwind_Backtrace" },
	{ "3.3.1", "GCC_3.3.1", "__gcc_personality_v0" },
	{ "3.3.2", "GCC_3.3.2", NULL }, // no function with this version in i686 and amd64 libgcc
	{ "3.3.4", "GCC_3.3.4", NULL }, // ditto
	{ "3.4.0", "GCC_3.4",   "__paritydi2" },
	{ "3.4.2", "GCC_3.4.2", "__enable_execute_stack" },
	{ "3.4.4", "GCC_3.4.4", HAVE_64_BIT ? "__addvti3" : NULL }, // no x86 function with that version
	{ "4.0.0", "GCC_4.0.0", "__muldc3" },
	{ "4.1.0", "GCC_4.1.0", NULL }, // no function for this version
	{ "4.2.0", "GCC_4.2.0", "_Unwind_GetIPInfo" },
	{ "4.3.0", "GCC_4.3.0", "__bswapdi2" },
	{ "4.4.0", "GCC_4.4.0", HAVE_64_BIT ? NULL : "__letf2" }, // no 64bit function for this version
	{ "4.5.0", "GCC_4.5.0", HAVE_64_BIT ? NULL : "__extendxftf2" }, // ditto
	{ "4.6.0", "GCC_4.6.0", NULL }, // no function for that version at all
	{ "4.7.0", "GCC_4.7.0", "__clrsbdi2" },
	{ "4.8.0", "GCC_4.8.0", "__cpu_indicator_init" },
	{ "7.0.0", "GCC_7.0.0", HAVE_64_BIT ? "__divmodti4" : "__divmoddi4" },
	{ "12.1.0","GCC_12.0.0", "__nehf2" },
	{ "13.1.0","GCC_13.0.0", "__truncdfbf2" },
	// TODO: add new versions
};

static enum libgcc_version get_libgcc_version(const char* path)
{
	return get_gcc_version(path, libgcc_version_checks, _NUM_LIBGCC_VERSIONS);
}
#endif // CHECK_LIBGCC


#ifdef CHECK_LIBSDL2
typedef struct My_SDL2_version
{
    unsigned char major;
    unsigned char minor;
    unsigned char patch; // update version, e.g. 5 in 2.0.5
} My_SDL2_version;

static My_SDL2_version get_libsdl2_version(const char* path)
{
	My_SDL2_version ret = {0};
	void* handle = dlopen(path, RTLD_LAZY);
	if(handle == NULL)
	{
		dprintf("couldn't dlopen() %s : %s\n", path, dlerror());
		return ret;
	}

	void (*sdl_getversion)(My_SDL2_version* ver);

	sdl_getversion = dlsym(handle, "SDL_GetVersion");
	if(sdl_getversion == NULL)
	{
		eprintf("Couldn't find SDL_GetVersion() in %s !\n", path);
	}
	else
	{
		sdl_getversion(&ret);
	}

	dlclose(handle);
	return ret;
}
#endif // CHECK_LIBSDL2



// directory the wrapper executable is in, *without* trailing slash
static char wrapper_exe_dir[PATH_MAX] = {0};

// returns 1 after successfully setting wrapper_exe_dir to the directory this wrapper is in,
// otherwise it returns 0
static int set_wrapper_dir(void)
{
	{
		// taken from https://github.com/DanielGibson/Snippets/blob/master/DG_misc.h
	#if defined(__linux)
		// readlink() doesn't null-terminate!
		int len = readlink("/proc/self/exe", wrapper_exe_dir, PATH_MAX-1);
		if (len <= 0)
		{
			// an error occured, clear exe path
			wrapper_exe_dir[0] = '\0';
		}
		else
		{
			wrapper_exe_dir[len] = '\0';
		}
	#elif defined(__FreeBSD__) // TODO: really keep this?
		// the sysctl should also work when /proc/ is not mounted (which seems to
		// be common on FreeBSD), so use it..
		int name[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1};
		size_t len = PATH_MAX-1;
		int ret = sysctl(name, sizeof(name)/sizeof(name[0]), wrapper_exe_dir, &len, NULL, 0);
		if(ret != 0)
		{
			// an error occured, clear exe path
			wrapper_exe_dir[0] = '\0';
		}
	#else
		error "Unsupported Platform!" // not sure how useful this even is on other platforms
	#endif
	}

	if(wrapper_exe_dir[0] != '\0')
	{
		// find the last slash before the executable name and set it to '\0'
		// so wrapper_exe_dir contains the full path of the directory the executable is in
		// then change to that directory
		char* lastSlash = strrchr(wrapper_exe_dir, '/');
		if(lastSlash != NULL)
		{
			*lastSlash = '\0';

			return 1;
		}
	}
	return 0;
}

// returns 1 after successfully changing to the directory this executable is in,
// otherwise it returns 0
static int change_to_wrapper_dir(void)
{
	return chdir(wrapper_exe_dir) == 0;
}

struct fallback_lib {
	const char* name;
	const char* dir;
	int use; // set by check_fallback_libs()
};

static struct fallback_lib fallback_libs[] = {
#ifdef CHECK_LIBSTDCPP
	{ "libstdc++.so.6", FALLBACK_DIR_STDCPP, 0 },
#endif
#ifdef CHECK_LIBGCC
	{ "libgcc_s.so.1", FALLBACK_DIR_GCC, 0 },
#endif
#ifdef CHECK_LIBSDL2
	{ "libSDL2-2.0.so.0", FALLBACK_DIR_SDL2, 0 },
#endif
#ifdef CHECK_LIBCURL4
	{ "libcurl.so.4", FALLBACK_DIR_CURL, 0 },
#endif
};

static const int NUM_FALLBACK_LIBS = sizeof(fallback_libs)/sizeof(fallback_libs[0]);

static int check_fallback_libs(void)
{
	int sys_ver, our_ver;
	char local_path[PATH_MAX];

	int fb_lib_idx = 0;

#ifdef CHECK_LIBSTDCPP
	sys_ver = get_libstdcpp_version(fallback_libs[fb_lib_idx].name);
	snprintf(local_path, PATH_MAX, "%s/%s", fallback_libs[fb_lib_idx].dir, fallback_libs[fb_lib_idx].name);
	our_ver = get_libstdcpp_version(local_path);
	dprintf("System libstdc++ version: %s ours: %s\n", get_gcc_version_name(libstdcpp_version_checks, sys_ver),
	                                                   get_gcc_version_name(libstdcpp_version_checks, our_ver));
	if(our_ver > sys_ver)
	{
		dprintf("Overwriting System libstdc++\n");
		fallback_libs[fb_lib_idx].use = 1;
	}

	++fb_lib_idx;
#endif

#ifdef CHECK_LIBGCC
	sys_ver = get_libgcc_version(fallback_libs[fb_lib_idx].name);
	snprintf(local_path, PATH_MAX, "%s/%s", fallback_libs[fb_lib_idx].dir, fallback_libs[fb_lib_idx].name);
	our_ver = get_libgcc_version(local_path);
	dprintf("System libgcc version: %s ours: %s\n", get_gcc_version_name(libgcc_version_checks, sys_ver),
	                                                get_gcc_version_name(libgcc_version_checks, our_ver));
	if(our_ver > sys_ver)
	{
		dprintf("Overwriting System libgcc\n");
		fallback_libs[fb_lib_idx].use = 1;
	}

	++fb_lib_idx;
#endif

#ifdef CHECK_LIBSDL2
	My_SDL2_version sdl_sys_ver = get_libsdl2_version(fallback_libs[fb_lib_idx].name);
	snprintf(local_path, PATH_MAX, "%s/%s", fallback_libs[fb_lib_idx].dir, fallback_libs[fb_lib_idx].name);
	My_SDL2_version sdl_our_ver = get_libsdl2_version(local_path);
	dprintf("System SDL2 version: %d.%d.%d ours: %d.%d.%d\n", (int)sdl_sys_ver.major, (int)sdl_sys_ver.minor, (int)sdl_sys_ver.patch,
	                                                          (int)sdl_our_ver.major, (int)sdl_our_ver.minor, (int)sdl_our_ver.patch);
	if( sdl_our_ver.major != 0 // otherwise it hasn't been found
	   && (sdl_sys_ver.major != sdl_our_ver.major // changes to the major version break the API/ABI
	       // minor versions don't break API/ABI (has been decided with 2.24.0 which followed 2.0.22)
	       || sdl_sys_ver.minor < sdl_our_ver.minor
	       || (sdl_sys_ver.minor == sdl_our_ver.minor && sdl_sys_ver.patch < sdl_our_ver.patch) ) )
	{
		dprintf("Overwriting System libSDL2\n");
		fallback_libs[fb_lib_idx].use = 1;
	}

	++fb_lib_idx;
#endif

#ifdef CHECK_LIBCURL4
	{
		// only use the bundled libcurl.so.4 if none is available on the system
		// there should be no backwards compat problems as long as it's libcurl.so.4
		// (and you linked against a libcurl without versioned symbols)
		// This way a (hopefully) security patched version supplied
		// by the user's Linux distribution is used, if available
		void* curlHandle = dlopen(fallback_libs[fb_lib_idx].name, RTLD_LAZY);
		if(curlHandle == NULL)
		{
			dprintf("Couldn't find libcurl.so.4 on System, will use bundled version\n");
			fallback_libs[fb_lib_idx].use = 1;
		}
		else
		{
			dprintf("Will use System's libcurl.so.4\n");
			dlclose(curlHandle);
		}
	}

	++fb_lib_idx;
#endif

	return 1;
}

static int set_ld_library_path(void)
{
	char* old_val = getenv("LD_LIBRARY_PATH");
	size_t len = 1; // 1 for terminating \0,
	if(old_val != NULL)
	{
		if(old_val[0] != '\0')  len += strlen(old_val) + 1; // +1 for ":"
		else old_val = NULL;
	}

	// Note: using absolute paths (=> prepending wrapper_exe_dir)

	// +1 for the '/' that will be added after wrapper_exe_dir before APP_EXECUTABLE
	const int wrapper_dir_len = strlen(wrapper_exe_dir) + 1;

	int num_used_overrides = 0;
	for(int i=0; i < NUM_FALLBACK_LIBS; ++i)
	{
		if(fallback_libs[i].use)
		{
			// + 1 for  separating ":" between entries
			len += wrapper_dir_len + strlen(fallback_libs[i].dir) + 1;
			++num_used_overrides;
		}
	}

	if(num_used_overrides == 0)
	{
		dprintf("no need to modify LD_LIBRARY_PATH\n");
		return 1; // nothing to do
	}

	char* new_val = malloc(len);
	if(new_val == NULL)  return 0; // malloc failed (unlikely)

	new_val[0] = '\0';

	for(int i=0; i < NUM_FALLBACK_LIBS; ++i)
	{
		if(fallback_libs[i].use)
		{
			// using strcat() here is safe because we checked the lengths above
			// and set len accordingly
			strcat(new_val, wrapper_exe_dir);
			strcat(new_val, "/");
			strcat(new_val, fallback_libs[i].dir);
			strcat(new_val, ":");
		}
	}

	if(old_val != NULL)
	{
		strcat(new_val, old_val);
	}
	else // remove the last added ":"
	{
		new_val[strlen(new_val) - 1] = '\0';
	}

	int ret = (setenv("LD_LIBRARY_PATH", new_val, 1) == 0);

	if(ret) {
		dprintf("Set LD_LIBRARY_PATH to '%s'\n", new_val);
	} else {
		int e = errno;
		eprintf("Failed to set LD_LIBRARY_PATH to '%s' : errnno %d (%s)\n", new_val, e, strerror(e));
	}

	free(new_val);
	return ret;
}

static void run_executable(char** argv)
{
#ifdef APP_NAME
	if(APP_NAME[0] != '\0')
	{
		argv[0] = APP_NAME; // override argv[0] so it contains your configured app name
	}
	else
#endif
	{
		argv[0] = APP_EXECUTABLE; // override argv[0] so it contains the name of the launched executable instead of this wrapper
	}

	char full_exe_path[PATH_MAX];
	int len = snprintf(full_exe_path, sizeof(full_exe_path), "%s/%s", wrapper_exe_dir, APP_EXECUTABLE);

	if(len <= 0 || len >= sizeof(full_exe_path))
	{
		eprintf("ERROR: Couldn't create full path to executable, snprintf() returned %d\n", len);
	}
	else if(execv(full_exe_path, argv) < 0)
	{
		int e = errno;
		eprintf("Executing %s failed: errno %d (%s)\n", APP_EXECUTABLE, e, strerror(e));
	}
	// else, if execv() was successful, this function never returns
}

int main(int argc, char** argv)
{
	char* debugVar = getenv("WRAPPER_DEBUG");
	if(debugVar != NULL && atoi(debugVar) != 0)
	{
		debugOutput = 1;
	}

	if(!set_wrapper_dir())
	{
		eprintf("Couldn't figure out the wrapper directory!\n");
		return 1;
	}

#ifdef CHANGE_TO_WRAPPER_DIR
	if(!change_to_wrapper_dir())
	{
		eprintf("Couldn't change to wrapper directory!\n");
		return 1;
	}
#endif

	if(check_fallback_libs() && set_ld_library_path())
	{
		run_executable(argv); // if it succeeds, it doesn't return.
	}
	return 1;
}
