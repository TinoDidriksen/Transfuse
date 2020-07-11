/*
* Public domain
*/

#pragma once
#ifndef e5bd51be_BASE64_HPP__
#define e5bd51be_BASE64_HPP__

#include "string_view.hpp"
#include <string>
#include <cstdint>

// Non-standard base64-encoder meant for URL-safe outputs. Doesn't pad and uses -_ instead of +/
std::string base64_url(std::string_view input);

std::string base64_url(uint32_t input);
std::string base64_url(uint64_t input);

#endif
