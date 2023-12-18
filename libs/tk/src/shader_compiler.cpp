#include "shader_compiler.hpp"
#include <shaderc/shaderc.h>
#include <assert.h>
#include <format>
#include <filesystem>

namespace tk
{

CompileResult::~CompileResult()
{
	shaderc_result_release(result);
}

bool CompileResult::ok()const
{
	if (result) {
		auto status = shaderc_result_get_compilation_status(result);
		return status == shaderc_compilation_status_success;
	}
	return false;
}

ZStrView CompileResult::getErrorMsgs()const
{
	if (result) {
		auto msg = shaderc_result_get_error_message(result);
		return msg;
	}
	else
		return customErrMsg;
}

CSpan<u32> CompileResult::getSpirvSrc()const
{
	const size_t len = shaderc_result_get_length(result);
	assert(len % 4 == 0);
	return CSpan<u32>((const u32*)shaderc_result_get_bytes(result), len / 4);
}

void ShaderCompiler::init()
{
	compiler = shaderc_compiler_initialize();
}

ShaderCompiler::~ShaderCompiler()
{
	shaderc_compiler_release(compiler);
}

static shaderc_include_result* include_resolve_callback (
	void* user_data, const char* requested_source, int type,
	const char* requesting_source, size_t include_depth)
{
	if (include_depth > 32)
		return nullptr;
	
	namespace fs = std::filesystem;
	fs::path sourcePath(requesting_source);
	sourcePath.remove_filename();
	fs::path headerPath;
	if (type == shaderc_include_type_relative)
		headerPath = fs::absolute(sourcePath / requested_source);
	else {
		headerPath = fs::absolute(requested_source);
	}

	auto shaderCompiler = (ShaderCompiler*)user_data;
	auto it = shaderCompiler->getOrLoadGlsl(headerPath.string().c_str());
	auto res = new shaderc_include_result;
	if (it == shaderCompiler->glslSrcsCache.end()) {
		const size_t errMsgMaxSize = 256;
		auto errMsg = new char[errMsgMaxSize];
		int errMsgLen = snprintf(errMsg, errMsgMaxSize, "Failed #include: '%s'", requested_source);
		*res = {
			.source_name = "",
			.source_name_length = 0,
			.content = errMsg,
			.content_length = size_t(errMsgLen),
		};
	}
	else {
		*res = {
			.source_name = it->first.c_str(),
			.source_name_length = it->first.size(),
			.content = it->second.c_str(),
			.content_length = it->second.size(),
		};
	}

	return res;
}
// An includer callback type for destroying an include result.
static void include_release_callback(
	void* user_data, shaderc_include_result* include_result)
{
	if (include_result->source_name_length == 0) {
		delete[] include_result->content;
	}
	delete include_result;
}

CompileResult ShaderCompiler::glslToSpv(ZStrView filePath, CSpan<PreprocDefine> defines)
{
	auto options = shaderc_compile_options_initialize();
	
	for(auto& d : defines)
		shaderc_compile_options_add_macro_definition(options, d.name.data(), d.name.length(), d.value.data(), d.value.length());

	shaderc_compile_options_set_include_callbacks(options, include_resolve_callback, include_release_callback, this);

	auto it = getOrLoadGlsl(filePath);
	if (it == glslSrcsCache.end()) {
		return CompileResult { .customErrMsg = std::format("Could not load '{}'\n", filePath.substr()) };
	}
	
	return CompileResult {
		.result = shaderc_compile_into_spv(compiler, it->second.c_str(), it->second.length(),
			shaderc_glsl_infer_from_source, filePath, "main", options)
	};
}

ShaderCompiler::CacheMap::const_iterator ShaderCompiler::getOrLoadGlsl(ZStrView filePath)
{
	if (auto it = glslSrcsCache.find(filePath); it == glslSrcsCache.end()) {
		std::string src = loadTextFile(filePath);
		if (src.empty())
			return glslSrcsCache.end();
		return glslSrcsCache.insert({ filePath, src }).first;
	}
	else {
		return it;
	}
}

}