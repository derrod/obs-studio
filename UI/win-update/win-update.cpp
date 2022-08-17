#include "win-update-helpers.hpp"
#include "update-window.hpp"
#include "remote-text.hpp"
#include "qt-wrappers.hpp"
#include "win-update.hpp"
#include "obs-app.hpp"

#include <QMessageBox>

#include <string>
#include <mutex>

#include <util/windows/WinHandle.hpp>
#include <util/util.hpp>
#include <json11.hpp>
#include <blake2.h>

#include <time.h>
#include <strsafe.h>
#include <winhttp.h>
#include <shellapi.h>

#ifdef BROWSER_AVAILABLE
#include <browser-panel.hpp>
#endif

using namespace std;
using namespace json11;

struct QCef;
extern QCef *cef;

/* ------------------------------------------------------------------------ */

#ifndef WIN_MANIFEST_URL
#define WIN_MANIFEST_URL "https://bouf.rodney.io/update_studio/manifest.json"
#endif

#ifndef WIN_MANIFEST_BASE_URL
#define WIN_MANIFEST_BASE_URL "https://bouf.rodney.io/update_studio/"
#endif

#ifndef WIN_BRANCHES_URL
#define WIN_BRANCHES_URL "https://bouf.rodney.io/update_studio/branches.json"
#endif

#ifndef WIN_DEFAULT_BRANCH
#define WIN_DEFAULT_BRANCH "stable"
#endif

#ifndef WIN_WHATSNEW_URL
#define WIN_WHATSNEW_URL "https://bouf.rodney.io/update_studio/whatsnew.json"
#endif

#ifndef WIN_UPDATER_URL
#define WIN_UPDATER_URL "https://bouf.rodney.io/update_studio/updater.exe"
#endif

static __declspec(thread) HCRYPTPROV provider = 0;

#pragma pack(push, r1, 1)

typedef struct {
	BLOBHEADER blobheader;
	RSAPUBKEY rsapubkey;
} PUBLICKEYHEADER;

#pragma pack(pop, r1)

#define BLAKE2_HASH_LENGTH 20
#define BLAKE2_HASH_STR_LENGTH ((BLAKE2_HASH_LENGTH * 2) + 1)

#define TEST_BUILD

// Hard coded 4096 bit RSA public key for obsproject.com in PEM format
static const unsigned char obs_pub[] = {
	0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x42, 0x45, 0x47, 0x49, 0x4e, 0x20, 0x50,
	0x55, 0x42, 0x4c, 0x49, 0x43, 0x20, 0x4b, 0x45, 0x59, 0x2d, 0x2d, 0x2d,
	0x2d, 0x2d, 0x0a, 0x4d, 0x49, 0x49, 0x43, 0x49, 0x6a, 0x41, 0x4e, 0x42,
	0x67, 0x6b, 0x71, 0x68, 0x6b, 0x69, 0x47, 0x39, 0x77, 0x30, 0x42, 0x41,
	0x51, 0x45, 0x46, 0x41, 0x41, 0x4f, 0x43, 0x41, 0x67, 0x38, 0x41, 0x4d,
	0x49, 0x49, 0x43, 0x43, 0x67, 0x4b, 0x43, 0x41, 0x67, 0x45, 0x41, 0x74,
	0x62, 0x4d, 0x70, 0x4a, 0x35, 0x6b, 0x6d, 0x44, 0x71, 0x68, 0x74, 0x70,
	0x55, 0x49, 0x33, 0x6a, 0x7a, 0x34, 0x66, 0x0a, 0x35, 0x4d, 0x75, 0x77,
	0x75, 0x52, 0x33, 0x69, 0x57, 0x4e, 0x59, 0x6e, 0x52, 0x55, 0x56, 0x63,
	0x42, 0x76, 0x67, 0x63, 0x76, 0x69, 0x69, 0x68, 0x4d, 0x73, 0x76, 0x39,
	0x58, 0x2b, 0x76, 0x71, 0x4d, 0x45, 0x2f, 0x49, 0x36, 0x6b, 0x57, 0x72,
	0x76, 0x73, 0x74, 0x74, 0x30, 0x69, 0x6b, 0x63, 0x4d, 0x50, 0x64, 0x55,
	0x6e, 0x66, 0x6f, 0x68, 0x73, 0x34, 0x31, 0x7a, 0x51, 0x72, 0x70, 0x59,
	0x0a, 0x6f, 0x65, 0x36, 0x43, 0x43, 0x32, 0x49, 0x72, 0x2b, 0x66, 0x55,
	0x48, 0x63, 0x73, 0x4d, 0x6e, 0x53, 0x52, 0x77, 0x62, 0x6b, 0x61, 0x41,
	0x50, 0x7a, 0x41, 0x73, 0x41, 0x52, 0x69, 0x50, 0x79, 0x78, 0x42, 0x74,
	0x6a, 0x4e, 0x6b, 0x34, 0x78, 0x4b, 0x61, 0x76, 0x44, 0x51, 0x58, 0x6f,
	0x52, 0x51, 0x71, 0x6a, 0x46, 0x74, 0x58, 0x47, 0x68, 0x53, 0x71, 0x69,
	0x53, 0x46, 0x6c, 0x76, 0x69, 0x0a, 0x52, 0x66, 0x57, 0x67, 0x31, 0x76,
	0x48, 0x56, 0x38, 0x64, 0x58, 0x31, 0x4e, 0x33, 0x62, 0x41, 0x51, 0x44,
	0x34, 0x32, 0x52, 0x53, 0x52, 0x4e, 0x39, 0x6f, 0x77, 0x38, 0x35, 0x59,
	0x75, 0x2b, 0x46, 0x63, 0x45, 0x6a, 0x42, 0x64, 0x31, 0x4b, 0x65, 0x45,
	0x34, 0x4e, 0x31, 0x6a, 0x4f, 0x4e, 0x59, 0x46, 0x53, 0x51, 0x73, 0x38,
	0x47, 0x2f, 0x31, 0x41, 0x31, 0x75, 0x74, 0x5a, 0x47, 0x6d, 0x0a, 0x68,
	0x61, 0x61, 0x4f, 0x50, 0x4e, 0x7a, 0x72, 0x42, 0x77, 0x59, 0x6e, 0x69,
	0x55, 0x62, 0x5a, 0x63, 0x73, 0x62, 0x49, 0x31, 0x76, 0x74, 0x59, 0x48,
	0x45, 0x34, 0x46, 0x2f, 0x70, 0x58, 0x41, 0x4e, 0x71, 0x47, 0x43, 0x46,
	0x45, 0x37, 0x69, 0x67, 0x4a, 0x39, 0x6f, 0x59, 0x67, 0x31, 0x73, 0x50,
	0x6d, 0x66, 0x31, 0x66, 0x64, 0x53, 0x37, 0x6d, 0x62, 0x33, 0x34, 0x73,
	0x63, 0x38, 0x4d, 0x0a, 0x42, 0x52, 0x63, 0x46, 0x6b, 0x58, 0x42, 0x43,
	0x34, 0x51, 0x58, 0x67, 0x6e, 0x30, 0x6e, 0x31, 0x76, 0x49, 0x41, 0x75,
	0x6f, 0x63, 0x5a, 0x59, 0x6f, 0x45, 0x53, 0x2b, 0x7a, 0x70, 0x44, 0x6b,
	0x2b, 0x34, 0x74, 0x2f, 0x35, 0x57, 0x78, 0x69, 0x4f, 0x44, 0x32, 0x37,
	0x67, 0x68, 0x4a, 0x50, 0x46, 0x56, 0x55, 0x70, 0x57, 0x78, 0x64, 0x55,
	0x59, 0x72, 0x34, 0x4f, 0x71, 0x69, 0x4f, 0x62, 0x0a, 0x59, 0x39, 0x35,
	0x44, 0x30, 0x76, 0x43, 0x68, 0x32, 0x52, 0x78, 0x52, 0x45, 0x7a, 0x63,
	0x45, 0x31, 0x70, 0x48, 0x37, 0x4c, 0x67, 0x35, 0x68, 0x70, 0x62, 0x41,
	0x6a, 0x49, 0x45, 0x39, 0x61, 0x65, 0x59, 0x2b, 0x62, 0x59, 0x72, 0x78,
	0x71, 0x74, 0x55, 0x52, 0x4f, 0x6a, 0x76, 0x46, 0x5a, 0x74, 0x35, 0x71,
	0x77, 0x39, 0x55, 0x53, 0x41, 0x33, 0x57, 0x53, 0x66, 0x7a, 0x46, 0x72,
	0x52, 0x0a, 0x79, 0x61, 0x37, 0x56, 0x76, 0x62, 0x38, 0x75, 0x51, 0x61,
	0x41, 0x49, 0x59, 0x44, 0x67, 0x58, 0x37, 0x55, 0x77, 0x70, 0x4a, 0x2b,
	0x79, 0x44, 0x42, 0x59, 0x38, 0x6f, 0x63, 0x53, 0x4a, 0x69, 0x70, 0x6a,
	0x41, 0x32, 0x6a, 0x39, 0x54, 0x54, 0x52, 0x74, 0x51, 0x42, 0x72, 0x58,
	0x57, 0x2b, 0x58, 0x71, 0x35, 0x6d, 0x61, 0x7a, 0x79, 0x7a, 0x4c, 0x42,
	0x78, 0x50, 0x53, 0x58, 0x53, 0x6a, 0x0a, 0x41, 0x43, 0x61, 0x62, 0x74,
	0x57, 0x51, 0x63, 0x49, 0x36, 0x74, 0x41, 0x55, 0x71, 0x39, 0x6c, 0x2b,
	0x31, 0x46, 0x6f, 0x62, 0x6c, 0x55, 0x72, 0x55, 0x4a, 0x61, 0x62, 0x79,
	0x66, 0x4d, 0x56, 0x35, 0x54, 0x41, 0x67, 0x64, 0x74, 0x52, 0x41, 0x34,
	0x61, 0x54, 0x39, 0x55, 0x69, 0x6d, 0x6f, 0x35, 0x36, 0x6e, 0x76, 0x36,
	0x6b, 0x45, 0x36, 0x52, 0x4a, 0x59, 0x30, 0x4d, 0x31, 0x48, 0x4e, 0x0a,
	0x2f, 0x70, 0x59, 0x52, 0x51, 0x79, 0x50, 0x61, 0x66, 0x31, 0x51, 0x36,
	0x74, 0x76, 0x68, 0x6e, 0x4d, 0x63, 0x4d, 0x6b, 0x34, 0x79, 0x4e, 0x78,
	0x44, 0x50, 0x7a, 0x38, 0x71, 0x45, 0x56, 0x45, 0x48, 0x61, 0x63, 0x36,
	0x52, 0x6e, 0x51, 0x42, 0x53, 0x45, 0x6a, 0x43, 0x73, 0x5a, 0x53, 0x4e,
	0x39, 0x67, 0x4c, 0x49, 0x73, 0x4f, 0x65, 0x4d, 0x31, 0x36, 0x4f, 0x56,
	0x62, 0x78, 0x61, 0x4f, 0x0a, 0x2f, 0x68, 0x4f, 0x4c, 0x50, 0x47, 0x36,
	0x63, 0x64, 0x31, 0x43, 0x52, 0x4d, 0x30, 0x71, 0x73, 0x6a, 0x44, 0x73,
	0x36, 0x59, 0x49, 0x53, 0x75, 0x56, 0x52, 0x36, 0x62, 0x66, 0x50, 0x69,
	0x34, 0x31, 0x4a, 0x77, 0x62, 0x49, 0x55, 0x67, 0x78, 0x77, 0x6d, 0x6b,
	0x7a, 0x58, 0x32, 0x53, 0x4d, 0x31, 0x50, 0x39, 0x52, 0x46, 0x4d, 0x47,
	0x6b, 0x44, 0x63, 0x6c, 0x34, 0x38, 0x38, 0x33, 0x55, 0x0a, 0x6e, 0x4b,
	0x4f, 0x6d, 0x47, 0x4f, 0x64, 0x53, 0x6e, 0x72, 0x70, 0x36, 0x4b, 0x79,
	0x6c, 0x51, 0x4b, 0x5a, 0x74, 0x69, 0x77, 0x4b, 0x4d, 0x43, 0x41, 0x77,
	0x45, 0x41, 0x41, 0x51, 0x3d, 0x3d, 0x0a, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d,
	0x45, 0x4e, 0x44, 0x20, 0x50, 0x55, 0x42, 0x4c, 0x49, 0x43, 0x20, 0x4b,
	0x45, 0x59, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x0a};
static const unsigned int obs_pub_len = 800;

/* ------------------------------------------------------------------------ */

static bool QuickWriteFile(const char *file, const void *data, size_t size)
try {
	BPtr<wchar_t> w_file;
	if (os_utf8_to_wcs_ptr(file, 0, &w_file) == 0)
		return false;

	WinHandle handle = CreateFileW(w_file, GENERIC_WRITE, 0, nullptr,
				       CREATE_ALWAYS, FILE_FLAG_WRITE_THROUGH,
				       nullptr);

	if (handle == INVALID_HANDLE_VALUE)
		throw strprintf("Failed to open file '%s': %lu", file,
				GetLastError());

	DWORD written;
	if (!WriteFile(handle, data, (DWORD)size, &written, nullptr))
		throw strprintf("Failed to write file '%s': %lu", file,
				GetLastError());

	return true;

} catch (string &text) {
	blog(LOG_WARNING, "%s: %s", __FUNCTION__, text.c_str());
	return false;
}

static bool QuickReadFile(const char *file, string &data)
try {
	BPtr<wchar_t> w_file;
	if (os_utf8_to_wcs_ptr(file, 0, &w_file) == 0)
		return false;

	WinHandle handle = CreateFileW(w_file, GENERIC_READ, FILE_SHARE_READ,
				       nullptr, OPEN_EXISTING, 0, nullptr);

	if (handle == INVALID_HANDLE_VALUE)
		throw strprintf("Failed to open file '%s': %lu", file,
				GetLastError());

	DWORD size = GetFileSize(handle, nullptr);
	data.resize(size);

	DWORD read;
	if (!ReadFile(handle, &data[0], size, &read, nullptr))
		throw strprintf("Failed to write file '%s': %lu", file,
				GetLastError());

	return true;

} catch (string &text) {
	blog(LOG_WARNING, "%s: %s", __FUNCTION__, text.c_str());
	return false;
}

static void HashToString(const uint8_t *in, char *out)
{
	const char alphabet[] = "0123456789abcdef";

	for (int i = 0; i != BLAKE2_HASH_LENGTH; ++i) {
		out[2 * i] = alphabet[in[i] / 16];
		out[2 * i + 1] = alphabet[in[i] % 16];
	}

	out[BLAKE2_HASH_LENGTH * 2] = 0;
}

static bool CalculateFileHash(const char *path, uint8_t *hash)
try {
	blake2b_state blake2;
	if (blake2b_init(&blake2, BLAKE2_HASH_LENGTH) != 0)
		return false;

	BPtr<wchar_t> w_path;
	if (os_utf8_to_wcs_ptr(path, 0, &w_path) == 0)
		return false;

	WinHandle handle = CreateFileW(w_path, GENERIC_READ, FILE_SHARE_READ,
				       nullptr, OPEN_EXISTING, 0, nullptr);
	if (handle == INVALID_HANDLE_VALUE)
		throw strprintf("Failed to open file '%s': %lu", path,
				GetLastError());

	vector<BYTE> buf;
	buf.resize(65536);

	for (;;) {
		DWORD read = 0;
		if (!ReadFile(handle, buf.data(), (DWORD)buf.size(), &read,
			      nullptr))
			throw strprintf("Failed to read file '%s': %lu", path,
					GetLastError());

		if (!read)
			break;

		if (blake2b_update(&blake2, buf.data(), read) != 0)
			return false;
	}

	if (blake2b_final(&blake2, hash, BLAKE2_HASH_LENGTH) != 0)
		return false;

	return true;

} catch (string &text) {
	blog(LOG_DEBUG, "%s: %s", __FUNCTION__, text.c_str());
	return false;
}

/* ------------------------------------------------------------------------ */

static bool VerifyDigitalSignature(uint8_t *buf, size_t len, uint8_t *sig,
				   size_t sigLen)
{
	/* ASN of PEM public key */
	BYTE binaryKey[1024];
	DWORD binaryKeyLen = sizeof(binaryKey);

	/* Windows X509 public key info from ASN */
	LocalPtr<CERT_PUBLIC_KEY_INFO> publicPBLOB;
	DWORD iPBLOBSize;

	/* RSA BLOB info from X509 public key */
	LocalPtr<PUBLICKEYHEADER> rsaPublicBLOB;
	DWORD rsaPublicBLOBSize;

	/* Handle to public key */
	CryptKey keyOut;

	/* Handle to hash context */
	CryptHash hash;

	/* Signature in little-endian format */
	vector<BYTE> reversedSig;

	if (!CryptStringToBinaryA((LPCSTR)obs_pub, obs_pub_len,
				  CRYPT_STRING_BASE64HEADER, binaryKey,
				  &binaryKeyLen, nullptr, nullptr))
		return false;

	if (!CryptDecodeObjectEx(X509_ASN_ENCODING, X509_PUBLIC_KEY_INFO,
				 binaryKey, binaryKeyLen,
				 CRYPT_DECODE_ALLOC_FLAG, nullptr, &publicPBLOB,
				 &iPBLOBSize))
		return false;

	if (!CryptDecodeObjectEx(X509_ASN_ENCODING, RSA_CSP_PUBLICKEYBLOB,
				 publicPBLOB->PublicKey.pbData,
				 publicPBLOB->PublicKey.cbData,
				 CRYPT_DECODE_ALLOC_FLAG, nullptr,
				 &rsaPublicBLOB, &rsaPublicBLOBSize))
		return false;

	if (!CryptImportKey(provider, (const BYTE *)rsaPublicBLOB.get(),
			    rsaPublicBLOBSize, 0, 0, &keyOut))
		return false;

	if (!CryptCreateHash(provider, CALG_SHA_512, 0, 0, &hash))
		return false;

	if (!CryptHashData(hash, buf, (DWORD)len, 0))
		return false;

	/* Windows requires signature in little-endian. Every other crypto
	 * provider is big-endian of course. */
	reversedSig.resize(sigLen);
	for (size_t i = 0; i < sigLen; i++)
		reversedSig[i] = sig[sigLen - i - 1];

	if (!CryptVerifySignature(hash, reversedSig.data(), (DWORD)sigLen,
				  keyOut, nullptr, 0))
		return false;

	return true;
}

static inline void HexToByteArray(const char *hexStr, size_t hexLen,
				  vector<uint8_t> &out)
{
	char ptr[3];

	ptr[2] = 0;

	for (size_t i = 0; i < hexLen; i += 2) {
		ptr[0] = hexStr[i];
		ptr[1] = hexStr[i + 1];
		out.push_back((uint8_t)strtoul(ptr, nullptr, 16));
	}
}

static bool CheckDataSignature(const string &data, const char *name,
			       const char *hexSig, size_t sigLen)
try {
	if (sigLen == 0 || sigLen > 0xFFFF || (sigLen & 1) != 0)
		throw strprintf("Missing or invalid signature for %s", name);

	/* Convert TCHAR signature to byte array */
	vector<uint8_t> signature;
	signature.reserve(sigLen);
	HexToByteArray(hexSig, sigLen, signature);

	if (!VerifyDigitalSignature((uint8_t *)data.data(), data.size(),
				    signature.data(), signature.size()))
		throw strprintf("Signature check failed for %s", name);

	return true;

} catch (string &text) {
	blog(LOG_WARNING, "%s: %s", __FUNCTION__, text.c_str());
	return false;
}

/* ------------------------------------------------------------------------ */

static bool FetchUpdaterModule(const char *url)
try {
	long responseCode;
	uint8_t updateFileHash[BLAKE2_HASH_LENGTH];
	vector<string> extraHeaders;
	CryptProvider localProvider;

	/* ----------------------------------- *
	 * create signature provider           */

	if (!CryptAcquireContext(&localProvider, nullptr, MS_ENH_RSA_AES_PROV,
				 PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
		throw strprintf("CryptAcquireContext failed: %lu",
				GetLastError());

	provider = localProvider;

	BPtr<char> updateFilePath =
		GetConfigPathPtr("obs-studio\\updates\\updater.exe");

	if (CalculateFileHash(updateFilePath, updateFileHash)) {
		char hashString[BLAKE2_HASH_STR_LENGTH];
		HashToString(updateFileHash, hashString);

		string header = "If-None-Match: ";
		header += hashString;
		extraHeaders.push_back(move(header));
	}

	string signature;
	string error;
	string data;

	bool success = GetRemoteFile(url, data, error, &responseCode, nullptr,
				     "", nullptr, extraHeaders, &signature);

	if (!success || (responseCode != 200 && responseCode != 304)) {
		if (responseCode == 404)
			return false;

		throw strprintf("Could not fetch '%s': %s", url, error.c_str());
	}

	/* A new file must be digitally signed */
	if (responseCode == 200) {
		bool valid = CheckDataSignature(data, url, signature.data(),
						signature.size());
		if (!valid)
			throw string("Invalid updater module signature");

		if (!QuickWriteFile(updateFilePath, data.data(), data.size()))
			return false;
	}

	return true;

} catch (string &text) {
	blog(LOG_WARNING, "%s: %s", __FUNCTION__, text.c_str());
	return false;
}

/* ------------------------------------------------------------------------ */

#if defined(OBS_RELEASE_CANDIDATE) && OBS_RELEASE_CANDIDATE > 0
#define CUR_VER                                                               \
	((uint64_t)OBS_RELEASE_CANDIDATE_VER << 16ULL | OBS_RELEASE_CANDIDATE \
								<< 8ULL)
#define PRE_RELEASE true
#elif OBS_BETA > 0
#define CUR_VER ((uint64_t)OBS_BETA_VER << 16ULL | OBS_BETA)
#define PRE_RELEASE true
#elif defined(OBS_COMMIT)
#define CUR_COMMIT OBS_COMMIT
#define PRE_RELEASE true
#define CUR_VER ((uint64_t)LIBOBS_API_VER << 16ULL)
#else
#define CUR_VER ((uint64_t)LIBOBS_API_VER << 16ULL)
#define PRE_RELEASE false
#endif

#ifndef OBS_COMMIT
#define CUR_COMMIT "00000000"
#endif

static bool ParseUpdateManifest(const char *manifest, bool *updatesAvailable,
				string &notes_str, uint64_t &updateVer,
				string &branch)
try {

	string error;
	Json root = Json::parse(manifest, error);
	if (!error.empty())
		throw strprintf("Failed reading json string: %s",
				error.c_str());

	if (!root.is_object())
		throw string("Root of manifest is not an object");

	int major = root["version_major"].int_value();
	int minor = root["version_minor"].int_value();
	int patch = root["version_patch"].int_value();
	int rc = root["rc"].int_value();
	int beta = root["beta"].int_value();
	string commit_hash = root["commit"].string_value();

	if (major == 0 && commit_hash.empty())
		throw strprintf("Invalid version number: %d.%d.%d", major,
				minor, patch);

	const Json &notes = root["notes"];
	if (!notes.is_string())
		throw string("'notes' value invalid");

	notes_str = notes.string_value();

	const Json &packages = root["packages"];
	if (!packages.is_array())
		throw string("'packages' value invalid");

	uint64_t cur_ver;
	uint64_t new_ver;

	if (commit_hash.empty()) {
		cur_ver = CUR_VER;
		new_ver = MAKE_SEMANTIC_VERSION(
			(uint64_t)major, (uint64_t)minor, (uint64_t)patch);
		new_ver <<= 16;
		/* RC builds are shifted so that rc1 and beta1 versions do not result
	     * in the same new_ver. */
		if (rc > 0)
			new_ver |= (uint64_t)rc << 8;
		else if (beta > 0)
			new_ver |= (uint64_t)beta;

		updateVer = new_ver;
	} else {
		/* Test or nightly builds may not have a (valid) version number,
		 * so compare commit hashes instead. */
		cur_ver = stoul(CUR_COMMIT, nullptr, 16);
		new_ver = stoul(commit_hash.substr(0, 8), nullptr, 16);
	}

	/* When using a pre-release build or non-default branch we only check if
	 * the manifest version is different, so that it can be rolled-back. */
	if (branch != WIN_DEFAULT_BRANCH || PRE_RELEASE)
		*updatesAvailable = new_ver != cur_ver;
	else
		*updatesAvailable = new_ver > cur_ver;

	return true;

} catch (string &text) {
	blog(LOG_WARNING, "%s: %s", __FUNCTION__, text.c_str());
	return false;
}

#undef CUR_COMMIT
#undef CUR_VER
#undef PRE_RELEASE

/* ------------------------------------------------------------------------ */

void GenerateGUID(string &guid)
{
	BYTE junk[20];

	if (!CryptGenRandom(provider, sizeof(junk), junk))
		return;

	guid.resize(41);
	HashToString(junk, &guid[0]);
}

string GetProgramGUID()
{
	static mutex m;
	lock_guard<mutex> lock(m);

	/* NOTE: this is an arbitrary random number that we use to count the
	 * number of unique OBS installations and is not associated with any
	 * kind of identifiable information */
	const char *pguid =
		config_get_string(GetGlobalConfig(), "General", "InstallGUID");
	string guid;
	if (pguid)
		guid = pguid;

	if (guid.empty()) {
		GenerateGUID(guid);

		if (!guid.empty())
			config_set_string(GetGlobalConfig(), "General",
					  "InstallGUID", guid.c_str());
	}

	return guid;
}

/* ------------------------------------------------------------------------ */

bool GetBranchAndUrl(string &selectedBranch, string &manifestUrl)
{
	const char *config_branch =
		config_get_string(GetGlobalConfig(), "General", "UpdateBranch");
	if (!config_branch)
		return true;

	bool found = false;
	for (const UpdateBranch &branch : App()->GetBranches()) {
		if (branch.name != config_branch)
			continue;
		/* A branch that is found but disabled
		 * will just fall back to the default.
		 * But if the branch was removed entirely,
		 * warn the user. */
		found = true;

		if (branch.is_enabled) {
			selectedBranch = branch.name.toStdString();
			manifestUrl = WIN_MANIFEST_BASE_URL +
				      branch.manifest_file.toStdString();
		}
		break;
	}

	return found;
}

/* ------------------------------------------------------------------------ */

static bool FetchAndVerifyFile(const char *name, const char *file,
			       const char *url, string &text)
{
	long responseCode;
	vector<string> extraHeaders;
	string error;
	string signature;
	CryptProvider localProvider;
	BYTE fileHash[BLAKE2_HASH_LENGTH];
	bool success;

	BPtr<char> filePath = GetConfigPathPtr(file);

	/* ----------------------------------- *
	 * create signature provider           */

	if (!CryptAcquireContext(&localProvider, nullptr, MS_ENH_RSA_AES_PROV,
				 PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
		throw strprintf("CryptAcquireContext failed: %lu",
				GetLastError());

	provider = localProvider;

	/* ----------------------------------- *
	 * avoid downloading json again        */

	if (CalculateFileHash(filePath, fileHash)) {
		char hashString[BLAKE2_HASH_STR_LENGTH];
		HashToString(fileHash, hashString);

		string header = "If-None-Match: ";
		header += hashString;
		extraHeaders.push_back(move(header));
	}

	/* ----------------------------------- *
	 * get current install GUID            */

	string guid = GetProgramGUID();

	if (!guid.empty()) {
		string header = "X-OBS2-GUID: ";
		header += guid;
		extraHeaders.push_back(move(header));
	}

	/* ----------------------------------- *
	 * get json from server                */

	success = GetRemoteFile(url, text, error, &responseCode, nullptr, "",
				nullptr, extraHeaders, &signature);

	if (!success || (responseCode != 200 && responseCode != 304)) {
		if (responseCode == 404)
			return false;

		throw strprintf("Failed to fetch %s file: %s", name,
				error.c_str());
	}

	/* ----------------------------------- *
	 * verify file signature               */

	if (responseCode == 200) {
		success = CheckDataSignature(text, name, signature.data(),
					     signature.size());
		if (!success)
			throw strprintf("Invalid %s signature", name);
	}

	/* ----------------------------------- *
	 * write or load json                  */

	if (responseCode == 200) {
		if (!QuickWriteFile(filePath, text.data(), text.size()))
			throw strprintf("Could not write file '%s'",
					filePath.Get());
	} else {
		if (!QuickReadFile(filePath, text))
			throw strprintf("Could not read file '%s'",
					filePath.Get());
	}

	/* ----------------------------------- *
	 * success                             */
	return true;
}

/* ------------------------------------------------------------------------ */

void AutoUpdateThread::infoMsg(const QString &title, const QString &text)
{
	OBSMessageBox::information(App()->GetMainWindow(), title, text);
}

void AutoUpdateThread::info(const QString &title, const QString &text)
{
	QMetaObject::invokeMethod(this, "infoMsg", Qt::BlockingQueuedConnection,
				  Q_ARG(QString, title), Q_ARG(QString, text));
}

int AutoUpdateThread::queryUpdateSlot(bool localManualUpdate,
				      const QString &text)
{
	OBSUpdate updateDlg(App()->GetMainWindow(), localManualUpdate, text);
	return updateDlg.exec();
}

int AutoUpdateThread::queryUpdate(bool localManualUpdate, const char *text_utf8)
{
	int ret = OBSUpdate::No;
	QString text = text_utf8;
	QMetaObject::invokeMethod(this, "queryUpdateSlot",
				  Qt::BlockingQueuedConnection,
				  Q_RETURN_ARG(int, ret),
				  Q_ARG(bool, localManualUpdate),
				  Q_ARG(QString, text));
	return ret;
}

bool AutoUpdateThread::queryRepairSlot()
{
	QMessageBox::StandardButton res = OBSMessageBox::question(
		App()->GetMainWindow(), QTStr("Updater.RepairConfirm.Title"),
		QTStr("Updater.RepairConfirm.Text"),
		QMessageBox::Yes | QMessageBox::Cancel);

	return res == QMessageBox::Yes;
}

bool AutoUpdateThread::queryRepair()
{
	bool ret = false;
	QMetaObject::invokeMethod(this, "queryRepairSlot",
				  Qt::BlockingQueuedConnection,
				  Q_RETURN_ARG(bool, ret));
	return ret;
}

void AutoUpdateThread::run()
try {
	string text;
	string branch = WIN_DEFAULT_BRANCH;
	string manifestUrl = WIN_MANIFEST_URL;
	bool updatesAvailable = false;

	struct FinishedTrigger {
		inline ~FinishedTrigger()
		{
			QMetaObject::invokeMethod(App()->GetMainWindow(),
						  "updateCheckFinished");
		}
	} finishedTrigger;

	/* ----------------------------------- *
	 * get branches from server            */

	if (FetchAndVerifyFile("branches", "obs-studio\\updates\\branches.json",
			       WIN_BRANCHES_URL, text))
		App()->SetBranchData(text);

	/* ----------------------------------- *
	 * get branches from server            */

	if (!GetBranchAndUrl(branch, manifestUrl)) {
		config_set_string(GetGlobalConfig(), "General", "UpdateBranch",
				  WIN_DEFAULT_BRANCH);
		info(QTStr("Updater.BranchNotFound.Title"),
		     QTStr("Updater.BranchNotFound.Text"));
	}

	/* ----------------------------------- *
	 * get manifest from server            */

	text.clear();
	if (!FetchAndVerifyFile("manifest",
				"obs-studio\\updates\\manifest.json",
				manifestUrl.c_str(), text))
		return;

	/* ----------------------------------- *
	 * check manifest for update           */

	string notes;
	uint64_t updateVer = 0;

	if (!ParseUpdateManifest(text.c_str(), &updatesAvailable, notes,
				 updateVer, branch))
		throw string("Failed to parse manifest");

	if (!updatesAvailable && !repairMode) {
		if (manualUpdate)
			info(QTStr("Updater.NoUpdatesAvailable.Title"),
			     QTStr("Updater.NoUpdatesAvailable.Text"));
		return;
	} else if (updatesAvailable && repairMode) {
		info(QTStr("Updater.RepairButUpdatesAvailable.Title"),
		     QTStr("Updater.RepairButUpdatesAvailable.Text"));
		return;
	}

	/* ----------------------------------- *
	 * skip this version if set to skip    */

	uint64_t skipUpdateVer = config_get_uint(GetGlobalConfig(), "General",
						 "SkipUpdateVersion");
	if (!manualUpdate && updateVer == skipUpdateVer && !repairMode)
		return;

	/* ----------------------------------- *
	 * fetch updater module                */

	if (!FetchUpdaterModule(WIN_UPDATER_URL))
		return;

	/* ----------------------------------- *
	 * query user for update               */

	if (repairMode) {
		if (!queryRepair())
			return;
	} else {
		int queryResult = queryUpdate(manualUpdate, notes.c_str());

		if (queryResult == OBSUpdate::No) {
			if (!manualUpdate) {
				long long t = (long long)time(nullptr);
				config_set_int(GetGlobalConfig(), "General",
					       "LastUpdateCheck", t);
			}
			return;

		} else if (queryResult == OBSUpdate::Skip) {
			config_set_uint(GetGlobalConfig(), "General",
					"SkipUpdateVersion", updateVer);
			return;
		}
	}

	/* ----------------------------------- *
	 * get working dir                     */

	wchar_t cwd[MAX_PATH];
	GetModuleFileNameW(nullptr, cwd, _countof(cwd) - 1);
	wchar_t *p = wcsrchr(cwd, '\\');
	if (p)
		*p = 0;

	/* ----------------------------------- *
	 * execute updater                     */

	BPtr<char> updateFilePath =
		GetConfigPathPtr("obs-studio\\updates\\updater.exe");
	BPtr<wchar_t> wUpdateFilePath;

	size_t size = os_utf8_to_wcs_ptr(updateFilePath, 0, &wUpdateFilePath);
	if (!size)
		throw string("Could not convert updateFilePath to wide");

	/* note, can't use CreateProcess to launch as admin. */
	SHELLEXECUTEINFO execInfo = {};

	execInfo.cbSize = sizeof(execInfo);
	execInfo.lpFile = wUpdateFilePath;

	string parameters = "";
	if (App()->IsPortableMode())
		parameters += "--portable";
	if (branch != WIN_DEFAULT_BRANCH)
		parameters += "--branch=" + branch;

	BPtr<wchar_t> lpParameters;
	size = os_utf8_to_wcs_ptr(parameters.c_str(), 0, &lpParameters);
	if (!size && !parameters.empty())
		throw string("Could not convert parameters to wide");

	execInfo.lpParameters = lpParameters;
	execInfo.lpDirectory = cwd;
	execInfo.nShow = SW_SHOWNORMAL;

	if (!ShellExecuteEx(&execInfo)) {
		QString msg = QTStr("Updater.FailedToLaunch");
		info(msg, msg);
		throw strprintf("Can't launch updater '%s': %d",
				updateFilePath.Get(), GetLastError());
	}

	/* force OBS to perform another update check immediately after updating
	 * in case of issues with the new version */
	config_set_int(GetGlobalConfig(), "General", "LastUpdateCheck", 0);
	config_set_int(GetGlobalConfig(), "General", "SkipUpdateVersion", 0);

	QMetaObject::invokeMethod(App()->GetMainWindow(), "close");

} catch (string &text) {
	blog(LOG_WARNING, "%s: %s", __FUNCTION__, text.c_str());
}

/* ------------------------------------------------------------------------ */

void WhatsNewInfoThread::run()
try {
	string text;

	if (FetchAndVerifyFile("whatsnew", "obs-studio\\updates\\whatsnew.json",
			       WIN_WHATSNEW_URL, text)) {
		emit Result(QString::fromStdString(text));
	}

} catch (string &text) {
	blog(LOG_WARNING, "%s: %s", __FUNCTION__, text.c_str());
}

/* ------------------------------------------------------------------------ */

void WhatsNewBrowserInitThread::run()
{
#ifdef BROWSER_AVAILABLE
	cef->wait_for_browser_init();
#endif
	emit Result(url);
}
