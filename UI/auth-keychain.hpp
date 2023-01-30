#pragma once

#include <QString>

bool KeychainSave(const std::string &key, const std::string &username,
		  const std::string &data);
bool KeychainLoad(const std::string &key, std::string &username,
		  std::string &out);
bool KeychainDelete(const std::string &key);
