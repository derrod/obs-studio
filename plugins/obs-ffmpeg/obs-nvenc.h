#pragma once

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <obs-module.h>
#include <ffnvcodec/nvEncodeAPI.h>
#include <ffnvcodec/dynlink_cuda.h>

#include "obs-nvenc-ver.h"

/* Missing from ffnvcodec cuda header, but not necessary until I figure out NV12 */
// typedef CUresult CUDAAPI tcuArrayGetPlane(CUarray *pPlaneArray, CUarray hArray, unsigned int planeIdx);

typedef struct CudaFunctions {
	tcuInit *cuInit;

	tcuDeviceGetCount *cuDeviceGetCount;
	tcuDeviceGet *cuDeviceGet;

	tcuCtxCreate_v2 *cuCtxCreate;
	tcuCtxDestroy_v2 *cuCtxDestroy;
	tcuCtxPushCurrent_v2 *cuCtxPushCurrent;
	tcuCtxPopCurrent_v2 *cuCtxPopCurrent;

	tcuArray3DCreate *cuArray3DCreate;
	tcuArrayDestroy *cuArrayDestroy;
	// tcuArrayGetPlane *cuArrayGetPlane;
	tcuMemcpy2D_v2 *cuMemcpy2D;

#ifndef _WIN32
	tcuGraphicsGLRegisterImage *cuGraphicsGLRegisterImage;
	tcuGraphicsUnregisterResource *cuGraphicsUnregisterResource;
	tcuGraphicsMapResources *cuGraphicsMapResources;
	tcuGraphicsUnmapResources *cuGraphicsUnmapResources;
	tcuGraphicsSubResourceGetMappedArray
		*cuGraphicsSubResourceGetMappedArray;
#endif
} CudaFunctions;

typedef NVENCSTATUS(NVENCAPI *NV_CREATE_INSTANCE_FUNC)(
	NV_ENCODE_API_FUNCTION_LIST *);

extern const char *nv_error_name(NVENCSTATUS err);
extern NV_ENCODE_API_FUNCTION_LIST nv;
extern NV_CREATE_INSTANCE_FUNC nv_create_instance;
extern CudaFunctions *cu;
extern int cuTexDeviceIdx;
extern uint32_t get_nvenc_ver(void);
extern bool init_nvenc(obs_encoder_t *encoder);
extern bool init_cuda(obs_encoder_t *encoder);
bool nv_fail2(obs_encoder_t *encoder, void *session, const char *format, ...);
bool nv_failed2(obs_encoder_t *encoder, void *session, NVENCSTATUS err,
		const char *func, const char *call);

#define nv_fail(encoder, format, ...) \
	nv_fail2(encoder, enc->session, format, ##__VA_ARGS__)

#define nv_failed(encoder, err, func, call) \
	nv_failed2(encoder, enc->session, err, func, call)
