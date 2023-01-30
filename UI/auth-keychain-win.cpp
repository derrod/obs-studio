#include "auth-keychain.hpp"

#include <util/platform.h>
#include <util/util.hpp>

#include <windows.h>
#include <wincrypt.h>
#include <wincred.h>

bool KeychainSave(const std::string &key, const std::string &data,
		  const std::string &username)
{
	BPtr<wchar_t> target_name;
	BPtr<wchar_t> user_name;

	os_utf8_to_wcs_ptr(key.c_str(), 0, &target_name);
	os_utf8_to_wcs_ptr(username.c_str(), 0, &user_name);

	CREDENTIAL cred = {0};
	cred.CredentialBlob = (LPBYTE)data.data();
	cred.CredentialBlobSize = (DWORD)data.size();
	cred.Persist = CRED_PERSIST_LOCAL_MACHINE;
	cred.TargetName = target_name;
	cred.Type = CRED_TYPE_GENERIC;
	cred.UserName = user_name;

	if (CredWriteW(&cred, 0))
		return true;

	blog(LOG_ERROR, "Saving to keychain failed with error code: %d",
	     GetLastError());

	return false;
}

bool KeychainLoad(const std::string &key, std::string &username,
		  std::string &out)
{
	BPtr<wchar_t> target_name;
	BPtr<wchar_t> user_name;
	os_utf8_to_wcs_ptr(key.c_str(), 0, &target_name);

	PCREDENTIALW cred;

	if (!CredReadW(target_name, CRED_TYPE_GENERIC, 0, &cred)) {
		DWORD err = GetLastError();

		if (err == ERROR_NOT_FOUND)
			blog(LOG_WARNING,
			     "Keychain entry \"%s\" no longer exists",
			     key.c_str());
		else
			blog(LOG_ERROR,
			     "Keychain entry \"%s\" could not be read: %d",
			     key.c_str(), err);

		return false;
	}

	out.resize(cred->CredentialBlobSize);
	out = (char *)cred->CredentialBlob;

	size_t len = os_wcs_to_utf8(cred->UserName, 0, nullptr, 0);
	username.resize(len);
	os_wcs_to_utf8(cred->UserName, 0, &username[0], len + 1);

	CredFree(cred);

	return true;
}

bool KeychainDelete(const std::string &key)
{
	BPtr<wchar_t> target_name;
	os_utf8_to_wcs_ptr(key.c_str(), 0, &target_name);

	if (!CredDeleteW(target_name, CRED_TYPE_GENERIC, 0)) {
		DWORD err = GetLastError();
		if (err == ERROR_NOT_FOUND)
			return true;

		blog(LOG_ERROR,
		     "Keychain entry \"%s\" could not be deleted: %d",
		     key.c_str(), err);
	}

	return true;
}
