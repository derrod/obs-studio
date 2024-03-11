#include <string_view>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <future>
#include <cstring>

#include <ffnvcodec/nvEncodeAPI.h>
#include <ffnvcodec/dynlink_loader.h>

/*
 * Utility to check for NVENC support and capabilities.
 * Will check all GPUs and return INI-formatted results based on highest capability of all devices.
 */

using namespace std;
using namespace std::chrono_literals;

NV_ENCODE_API_FUNCTION_LIST nv = {NV_ENCODE_API_FUNCTION_LIST_VER};
static void *nvenc_lib = nullptr;
static CudaFunctions *cu = nullptr;

/* List of capabilities to be queried per codec */
static const vector<pair<NV_ENC_CAPS, string>> capabilities = {
	{NV_ENC_CAPS_NUM_MAX_BFRAMES, "bframes"},
	{NV_ENC_CAPS_SUPPORT_LOSSLESS_ENCODE, "lossless"},
	{NV_ENC_CAPS_SUPPORT_LOOKAHEAD, "lookahead"},
	{NV_ENC_CAPS_SUPPORT_TEMPORAL_AQ, "temporal_aq"},
	{NV_ENC_CAPS_SUPPORT_DYN_BITRATE_CHANGE, "dynamic_bitrate"},
	{NV_ENC_CAPS_SUPPORT_10BIT_ENCODE, "10bit"},
	{NV_ENC_CAPS_SUPPORT_BFRAME_REF_MODE, "bref"},
	{NV_ENC_CAPS_NUM_ENCODER_ENGINES, "engines"},
	{NV_ENC_CAPS_SUPPORT_YUV444_ENCODE, "yuv_444"},
	{NV_ENC_CAPS_WIDTH_MAX, "max_width"},
	{NV_ENC_CAPS_HEIGHT_MAX, "max_height"},
#if NVENCAPI_MAJOR_VERSION > 12 || NVENCAPI_MINOR_VERSION >= 2
	/* SDK 12.2+ features */
	{NV_ENC_CAPS_SUPPORT_TEMPORAL_FILTER, "temporal_filter"},
	{NV_ENC_CAPS_SUPPORT_LOOKAHEAD_LEVEL, "lookahead_level"},
#endif
};

static const vector<pair<string_view, GUID>> codecs = {
	{"h264", NV_ENC_CODEC_H264_GUID},
	{"hevc", NV_ENC_CODEC_HEVC_GUID},
	{"av1", NV_ENC_CODEC_AV1_GUID}};

typedef unordered_map<string, unordered_map<string, int>> codec_caps_map;

struct CUDACtx {
	CUcontext ctx;

	CUDACtx() = default;

	~CUDACtx() { cu->cuCtxDestroy(ctx); }

	bool Init(int adapter_idx)
	{
		CUdevice dev;
		if (cu->cuDeviceGet(&dev, adapter_idx) != CUDA_SUCCESS)
			return false;

		return cu->cuCtxCreate(&ctx, 0, dev) == CUDA_SUCCESS;
	}
};

struct NVSession {
	void *ptr = nullptr;

	NVSession() = default;

	~NVSession() { nv.nvEncDestroyEncoder(ptr); }

	bool OpenSession(const CUDACtx &ctx)
	{
		NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS params = {};
		params.version = NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER;
		params.apiVersion = NVENCAPI_VERSION;
		params.device = ctx.ctx;
		params.deviceType = NV_ENC_DEVICE_TYPE_CUDA;

		return nv.nvEncOpenEncodeSessionEx(&params, &ptr) ==
		       NV_ENC_SUCCESS;
	}
};

bool load_nvenc_lib(void)
{
#ifdef _WIN32
	nvenc_lib = LoadLibraryA("nvEncodeAPI64.dll");
#else
	nvenc_lib = dlopen("libnvidia-encode.so.1", RTLD_LAZY);
#endif
	return nvenc_lib != nullptr;
}

static void *load_nv_func(const char *func)
{
#ifdef _WIN32
	void *func_ptr = (void *)GetProcAddress((HMODULE)nvenc_lib, func);
#else
	void *func_ptr = dlsym(nvenc_lib, func);
#endif
	return func_ptr;
}

typedef NVENCSTATUS(NVENCAPI *NV_MAX_VER_FUNC)(uint32_t *);
typedef NVENCSTATUS(NVENCAPI *NV_CREATE_INSTANCE_FUNC)(
	NV_ENCODE_API_FUNCTION_LIST *);

// ToDo Log NVENC version
static uint32_t get_nvenc_ver()
{
	NV_MAX_VER_FUNC nv_max_ver = (NV_MAX_VER_FUNC)load_nv_func(
		"NvEncodeAPIGetMaxSupportedVersion");
	if (!nv_max_ver) {
		return 0;
	}

	uint32_t ver = 0;
	if (nv_max_ver(&ver) != NV_ENC_SUCCESS)
		return 0;

	return ver;
}

static bool init_nvenc()
{
	if (!load_nvenc_lib())
		return false;

	uint32_t ver = get_nvenc_ver();
	if (ver == 0)
		return false;

	NV_CREATE_INSTANCE_FUNC nv_create_instance =
		(NV_CREATE_INSTANCE_FUNC)load_nv_func(
			"NvEncodeAPICreateInstance");
	if (!nv_create_instance)
		return false;

	return nv_create_instance(&nv) == NV_ENC_SUCCESS;
}

static bool init_cuda()
{
	if (cuda_load_functions(&cu, nullptr))
		return false;

	if (cu->cuInit(0) != CUDA_SUCCESS)
		return false;

	return true;
}

static bool get_adapter_caps(int adapter_idx, codec_caps_map &caps)
{
	CUDACtx cudaCtx;
	NVSession nvSession;

	if (!cudaCtx.Init(adapter_idx))
		return false;
	if (!nvSession.OpenSession(cudaCtx))
		return false;

	uint32_t guid_count = 0;
	if (nv.nvEncGetEncodeGUIDCount(nvSession.ptr, &guid_count) !=
	    NV_ENC_SUCCESS)
		return false;

	GUID *guids = nullptr;
	guids = (GUID *)malloc(guid_count * sizeof(GUID));
	NVENCSTATUS stat = nv.nvEncGetEncodeGUIDs(nvSession.ptr, guids,
						  guid_count, &guid_count);
	if (stat != NV_ENC_SUCCESS)
		return false;

	NV_ENC_CAPS_PARAM param = {NV_ENC_CAPS_PARAM_VER};

	for (uint32_t i = 0; i < guid_count; i++) {
		GUID *guid = &guids[i];

		std::string codec_name = "unknown";
		for (const auto &[name, codec_guid] : codecs) {
			if (memcmp(&codec_guid, guid, sizeof(GUID)) == 0) {
				codec_name = name;
				break;
			}
		}

		caps[codec_name]["codec_supported"] = 1;

		for (const auto &[cap, name] : capabilities) {
			int v;
			param.capsToQuery = cap;
			if (nv.nvEncGetEncodeCaps(nvSession.ptr, *guid, &param,
						  &v) != NV_ENC_SUCCESS)
				continue;

			if (v > caps[codec_name][name])
				caps[codec_name][name] = v;
		}
	}

	return true;
}

int check_thread()
{
	printf("[general]\n");

	/* NVENC API init */
	if (!init_nvenc()) {
		printf("nvenc_supported=false\n"
		       "reason=nvapi\n");
		return 1;
	}

	/* CUDA init */
	if (!init_cuda()) {
		printf("nvenc_supported=false\n"
		       "reason=cuda\n");
		return 1;
	}

	/* --------------------------------------------------------- */
	/* obtain adapter compatibility information                  */

	bool nvenc_supported = false;
	codec_caps_map caps;

	caps["h264"]["codec_supported"] = 0;
	caps["hevc"]["codec_supported"] = 0;
	caps["av1"]["codec_supported"] = 0;

	int num_devices = 0;
	cu->cuDeviceGetCount(&num_devices);
	if (!num_devices) {
		printf("nvenc_supported=false\n"
		       "reason=no_devices\n");
		return 1;
	}

	for (int idx = 0; idx < num_devices; idx++)
		nvenc_supported |= get_adapter_caps(idx, caps);

	if (!nvenc_supported) {
		printf("nvenc_supported=false\n"
		       "reason=other\n");
		return 1;
	}

	// ToDo log CUDA driver version
	printf("nvenc_supported=true\n"
	       "devices=%d\n",
	       num_devices);

	for (const auto &[codec, codec_caps] : caps) {
		printf("\n[%s]\n", codec.c_str());

		for (const auto &[name, value] : codec_caps) {
			printf("%s=%d\n", name.c_str(), value);
		}
	}

	return 0;
}

int main(int, char **)
{
	future<int> f = async(launch::async, check_thread);
	future_status status = f.wait_for(2.5s);

	if (status == future_status::timeout)
		exit(1);

	return f.get();
}
