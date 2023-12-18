#pragma once

#include <unordered_map>
#include <string>
#include <string_view>
#include "utils.hpp"
#include <shaderc/shaderc.h>

namespace tk
{

struct CompileResult
{
	shaderc_compilation_result_t result = nullptr;
	std::string customErrMsg;

	~CompileResult();
	bool ok()const;
	ZStrView getErrorMsgs()const;
	CSpan<u32> getSpirvSrc()const;
};

struct PreprocDefine { StrView name, value = ""; };

struct ShaderCompiler
{
	typedef std::unordered_map<std::string, std::string> CacheMap;

	shaderc_compiler_t compiler = nullptr;
	CacheMap glslSrcsCache;

	ShaderCompiler() {}
	~ShaderCompiler();
	void init();
	CompileResult glslToSpv(ZStrView filePath, CSpan<PreprocDefine> defines);
	CacheMap::const_iterator getOrLoadGlsl(ZStrView filePath);
};

}