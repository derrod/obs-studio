#include <windows.h>
#include <obs-module.h>
#include <util/dstr.h>
#include <util/windows/win-version.h>
#include <util/platform.h>
#include <file-updater/file-updater.h>

#include "compat-format-version.h"
#include "lookup-config.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("win-capture", "en-US")

#define WIN_CAPTURE_LOG_STRING "[win-capture plugin] "
#define WIN_CAPTURE_VER_STRING "win-capture plugin (libobs " OBS_VERSION ")"

extern struct obs_source_info duplicator_capture_info;
extern struct obs_source_info monitor_capture_info;
extern struct obs_source_info window_capture_info;
extern struct obs_source_info game_capture_info;

static HANDLE init_hooks_thread = NULL;
static update_info_t *update_info = NULL;

extern bool cached_versions_match(void);
extern bool load_cached_graphics_offsets(bool is32bit);
extern bool load_graphics_offsets(bool is32bit);

/* temporary, will eventually be erased once we figure out how to create both
 * 32bit and 64bit versions of the helpers/hook */
#ifdef _WIN64
#define IS32BIT false
#else
#define IS32BIT true
#endif

#define USE_HOOK_ADDRESS_CACHE false

static DWORD WINAPI init_hooks(LPVOID unused)
{
	if (USE_HOOK_ADDRESS_CACHE &&
	    cached_versions_match() &&
	    load_cached_graphics_offsets(IS32BIT)) {

		load_cached_graphics_offsets(!IS32BIT);
		obs_register_source(&game_capture_info);

	} else if (load_graphics_offsets(IS32BIT)) {
		load_graphics_offsets(!IS32BIT);
	}

	UNUSED_PARAMETER(unused);
	return 0;
}

void wait_for_hook_initialization(void)
{
	static bool initialized = false;

	if (!initialized) {
		if (init_hooks_thread) {
			WaitForSingleObject(init_hooks_thread, INFINITE);
			CloseHandle(init_hooks_thread);
			init_hooks_thread = NULL;
		}
		initialized = true;
	}
}

static bool confirm_compat_file(void *param, struct file_download_data *file)
{
	if (astrcmpi(file->name, "compat.json") == 0) {
		obs_data_t *data;
		int format_version;

		data = obs_data_create_from_json((char*)file->buffer.array);
		if (!data)
			return false;

		format_version = (int)obs_data_get_int(data, "format_version");
		obs_data_release(data);

		if (format_version != COMPAT_FORMAT_VERSION)
			return false;
	}

	UNUSED_PARAMETER(param);
	return true;
}

bool obs_module_load(void)
{
	struct win_version_info ver;
	bool win8_or_above = false;

	char *local_dir = obs_module_file("");
	char *config_dir = obs_module_config_path("");

	if (config_dir) {
		os_mkdirs(config_dir);
		update_info = update_info_create(
			WIN_CAPTURE_LOG_STRING,
			WIN_CAPTURE_VER_STRING,
			COMPAT_UPDATE_URL,
			local_dir,
			config_dir,
			confirm_compat_file, NULL);
	}

	bfree(local_dir);
	bfree(config_dir);

	get_win_ver(&ver);

	win8_or_above = ver.major > 6 || (ver.major == 6 && ver.minor >= 2);

	obs_enter_graphics();

	if (win8_or_above && gs_get_device_type() == GS_DEVICE_DIRECT3D_11)
		obs_register_source(&duplicator_capture_info);
	else
		obs_register_source(&monitor_capture_info);

	obs_leave_graphics();

	obs_register_source(&window_capture_info);

	init_hooks_thread = CreateThread(NULL, 0, init_hooks, NULL, 0, NULL);
	obs_register_source(&game_capture_info);

	return true;
}

void obs_module_unload(void)
{
	wait_for_hook_initialization();
	update_info_destroy(update_info);
}
