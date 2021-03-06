#include "shader.hpp"

#include <assert.h>
#include "../../util/util.hpp"
#include <glad/glad.h>
#include <cassert>
#include <iostream>
#include <glm/gtc/type_ptr.hpp>
#include "texture.hpp"
#include <map>
#include <exception>

using namespace std;
using namespace glm;

unsigned getUnifSize(UnifType ut)
{
	const unsigned n = (unsigned)UnifType::COUNT;
	const unsigned lookUpTable[n] =
	{
		1 * sizeof(float), 2 * sizeof(float), 3 * sizeof(float), 4 * sizeof(float),
		1 * sizeof(int), 2 * sizeof(int), 3 * sizeof(int), 4 * sizeof(int),
		1 * sizeof(unsigned), 2 * sizeof(unsigned), 3 * sizeof(unsigned), 4 * sizeof(unsigned),
		2*2 * sizeof(float), 3*3 * sizeof(float), 4*4 * sizeof(float),
		2*3 * sizeof(float), 3*2 * sizeof(float),
		2*4 * sizeof(float), 4*2 * sizeof(float),
		3*4 * sizeof(float), 4*3 * sizeof(float),
	};
	unsigned i = (unsigned)ut;
	assert(i >= 0 && i < n);
	return lookUpTable[i];
}

unsigned getUnifNumElems(UnifType ut)
{
	const unsigned n = (unsigned)UnifType::COUNT;
	const unsigned lookUpTable[n] =
	{
		1, 2, 3, 4,
		1, 2, 3, 4,
		1, 2, 3, 4,
		2 * 2, 3 * 3, 4 * 4,
		2 * 3, 3 * 2,
		2 * 4, 4 * 2,
		3 * 4, 4 * 3,
	};
	unsigned i = (unsigned)ut;
	assert(i >= 0 && i < n);
	return lookUpTable[i];
}

UnifType getUnifBasicType(UnifType ut)
{
	const unsigned n = (unsigned) UnifType::COUNT;
	const unsigned i1 = (unsigned) UnifType::INT;
	const unsigned i2 = (unsigned) UnifType::INT4;
	const unsigned u1 = (unsigned) UnifType::UINT;
	const unsigned u2 = (unsigned) UnifType::UINT4;
	unsigned i = (unsigned)ut;
	assert(i >= 0 && i < n);
	if (i1 <= i && i <= i2) return UnifType::INT;
	if (u1 <= i && i <= u2) return UnifType::UINT;
	return UnifType::FLOAT;
}

const char* getUnifTypeName(UnifType ut)
{
	const unsigned n = (unsigned)UnifType::COUNT;
	const char * const lookUpTable[n] =
	{
		"float", "vec2", "vec3", "vec4",
		"int", "ivec2", "ivec3", "ivec4",
		"uint", "uivec2", "uivec3", "uivec4",
		"mat2", "mat3", "mat4",
		"mat2x3", "mat2x4",
		"mat2x4", "mat4x2",
		"mat3x4", "mat4x3",
	};
	unsigned i = (unsigned)ut;
	assert(i >= 0 && i < n);
	return lookUpTable[i];
}

UnifType getUnifTypeFromName(const char* name)
{
	const map<string, UnifType> lookUp =
	{
		{ "float", UnifType::FLOAT },
		{ "vec2", UnifType::VEC2 },
		{ "vec3", UnifType::VEC3 },
		{ "vec4", UnifType::VEC4 },
		{ "int", UnifType::INT },
		{ "ivec2", UnifType::INT2 },
		{ "ivec3", UnifType::INT3 },
		{ "ivec4", UnifType::INT4 },
		{ "uint", UnifType::UINT },
		{ "uivec2", UnifType::UINT2 },
		{ "uivec3", UnifType::UINT3 },
		{ "uivec4", UnifType::UINT4 },
		{ "mat2", UnifType::MATRIX_2 },
		{ "mat3", UnifType::MATRIX_3 },
		{ "mat4", UnifType::MATRIX_4 },
		{ "mat2x3", UnifType::MATRIX_2x3 },
		{ "mat3x2", UnifType::MATRIX_3x2 },
		{ "mat2x4", UnifType::MATRIX_2x4 },
		{ "mat4x2", UnifType::MATRIX_4x2 },
		{ "mat3x4", UnifType::MATRIX_3x4 },
		{ "mat4x3", UnifType::MATRIX_4x3 }
	};

	map<string, UnifType>::const_iterator it = lookUp.find(name);
	if (it == lookUp.end())
	{
		throw runtime_error("not recognized unif name type: " + string(name));
	}

	return it->second;
}

// SHADER

void ShaderObject::loadFromString(const char* src)
{
	// the shader has to be loaded only once, otherwise there will be memory leaks
	assert(shaderId >= 0 && "The shader has been already loaded");

	glShaderSource(shaderId, 1, &src, 0);
}

void ShaderObject::loadFromFile(const char* fileName)
{
	string src = loadStringFromFile(fileName);
	loadFromString(src.c_str());
}

void ShaderObject::compile()
{
	glCompileShader(shaderId);

	GLint compiled;
	glGetShaderiv(shaderId, GL_COMPILE_STATUS, &compiled);
	if (!compiled)
	{
		GLint logLen;
		glGetShaderiv(shaderId, GL_INFO_LOG_LENGTH, &logLen);
		string logString(logLen, ' ');
		glGetShaderInfoLog(shaderId, logLen, NULL, &logString[0]);
		glDeleteShader(shaderId);
		throw runtime_error(logString);
	}
}

void ShaderObject::destroy()
{
	assert(shaderId >= 0 && "Attempted to destroy shader before creating it");

	glDeleteShader(shaderId);
}

void VertexShaderObject::create() { shaderId = glCreateShader(GL_VERTEX_SHADER); }
void FragmentShaderObject::create() { shaderId = glCreateShader(GL_FRAGMENT_SHADER); }
void GeometryShaderObject::create() { shaderId = glCreateShader(GL_GEOMETRY_SHADER); }


// SHADER PROGRAM

void ShaderProgram::create()
{
	program = glCreateProgram();
}

void ShaderProgram::bindAttrib(const char* name, int loc)
{
	glBindAttribLocation(program, loc, name);
}

void ShaderProgram::setVertexShader(VertexShaderObject vertShad)
{
	glAttachShader(program, vertShad.getId());
}

void ShaderProgram::setFragmentShader(FragmentShaderObject fragShad)
{
	glAttachShader(program, fragShad.getId());
}

void ShaderProgram::setGeometryShader(GeometryShaderObject geomShad)
{
	glAttachShader(program, geomShad.getId());
}

void ShaderProgram::link()
{
	glLinkProgram(program);

	// check if the linking failed
	int linkedOk;
	glGetProgramiv(program, GL_LINK_STATUS, &linkedOk);
	if (!linkedOk)
	{
		GLint len;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
		string str = string(len, ' ');
		glGetProgramInfoLog(program, len, NULL, &str[0]);
		throw str;
	}
}

void ShaderProgram::use()
{
	assert(program >= 0 && "Attempted to use an invalid shader program");

	glUseProgram(program);
}

void ShaderProgram::useProgram()
{
	glUseProgram(program);
}

void ShaderProgram::free()
{
	assert(program >= 0 && "Attempted to free an invalid shader program");

	glDeleteProgram(program);
}

int ShaderProgram::getUniformLocation(const char* name)const
{
	return glGetUniformLocation(program, name);
}
// UNIFORM UPLOADERS
void ShaderProgram::uploadUniform(int location, float value)
{
	glUniform1f(location, value);
}

void ShaderProgram::uploadUniform(int location, const vec2& value)
{
	glUniform2fv(location, 1, &value[0]);
}

void ShaderProgram::uploadUniform(int location, const vec3& value)
{
	glUniform3fv(location, 1, &value[0]);
}

void ShaderProgram::uploadUniform(int location, const vec4& value)
{
	glUniform4fv(location, 1, &value[0]);
}

void ShaderProgram::uploadUniform(int location, int value)
{
	glUniform1i(location, value);
}

void ShaderProgram::uploadUniform(int location, const ivec2& value)
{
	glUniform2iv(location, 1, &value[0]);
}

void ShaderProgram::uploadUniform(int location, const ivec3& value)
{
	glUniform3iv(location, 1, &value[0]);
}

void ShaderProgram::uploadUniform(int location, const ivec4& value)
{
	glUniform4iv(location, 1, &value[0]);
}

void ShaderProgram::uploadUniform(int location, unsigned value)
{
	glUniform1ui(location, value);
}

void ShaderProgram::uploadUniform(int location, const glm::uvec2& value)
{
	glUniform2uiv(location, 1, &value[0]);
}

void ShaderProgram::uploadUniform(int location, const glm::uvec3& value)
{
	glUniform3uiv(location, 1, &value[0]);
}

void ShaderProgram::uploadUniform(int location, const glm::uvec4& value)
{
	glUniform4uiv(location, 1, &value[0]);
}

void ShaderProgram::uploadUniform(int location, const mat2& value)
{
	glUniformMatrix2fv(location, 1, GL_FALSE, value_ptr(value));
}

void ShaderProgram::uploadUniform(int location, const mat3& value)
{
	glUniformMatrix3fv(location, 1, GL_FALSE, value_ptr(value));
}

void ShaderProgram::uploadUniform(int location, const mat4& value)
{
	glUniformMatrix4fv(location, 1, GL_FALSE, value_ptr(value));
}

void ShaderProgram::uploadUniform(int location, const mat2x3& value)
{
	glUniformMatrix2x3fv(location, 1, GL_FALSE, value_ptr(value));
}

void ShaderProgram::uploadUniform(int location, const mat3x2& value)
{
	glUniformMatrix3x2fv(location, 1, GL_FALSE, value_ptr(value));
}

void ShaderProgram::uploadUniform(int location, const mat2x4& value)
{
	glUniformMatrix2x4fv(location, 1, GL_FALSE, value_ptr(value));
}

void ShaderProgram::uploadUniform(int location, const mat4x2& value)
{
	glUniformMatrix4x2fv(location, 1, GL_FALSE, value_ptr(value));
}

void ShaderProgram::uploadUniform(int location, const mat3x4& value)
{
	glUniformMatrix3x4fv(location, 1, GL_FALSE, value_ptr(value));
}

void ShaderProgram::uploadUniform(int location, const mat4x3& value)
{
	glUniformMatrix4x3fv(location, 1, GL_FALSE, value_ptr(value));
}

void ShaderProgram::uploadUniform(int location, TextureUnit value)
{
	glUniform1i(location, (int)value);
}

void ShaderProgram::uploadUniformData(UnifType type, int loc, const char* data)
{
	switch (type)
	{

	case UnifType::FLOAT:
	{
		const float* x = (float*)data;
		uploadUniform(loc, *x);
		break;
	}
	case UnifType::VEC2:
	{
		const vec2* x = (vec2*)data;
		uploadUniform(loc, *x);
		break;
	}
	case UnifType::VEC3:
	{
		const vec3* x = (vec3*)data;
		uploadUniform(loc, *x);
		break;
	}
	case UnifType::VEC4:
	{
		const vec4* x = (vec4*)data;
		uploadUniform(loc, *x);
		break;
	}

	case UnifType::INT:
	{
		const int* x = (int*)data;
		uploadUniform(loc, *x);
		break;
	}
	case UnifType::INT2:
	{
		const ivec2* x = (ivec2*)data;
		uploadUniform(loc, *x);
		break;
	}
	case UnifType::INT3:
	{
		const ivec3* x = (ivec3*)data;
		uploadUniform(loc, *x);
		break;
	}
	case UnifType::INT4:
	{
		const ivec4* x = (ivec4*)data;
		uploadUniform(loc, *x);
		break;
	}

	case UnifType::UINT:
	{
		const unsigned* x = (unsigned*)data;
		uploadUniform(loc, *x);
		break;
	}
	case UnifType::UINT2:
	{
		const uvec2* x = (uvec2*)data;
		uploadUniform(loc, *x);
		break;
	}
	case UnifType::UINT3:
	{
		const uvec3* x = (uvec3*)data;
		uploadUniform(loc, *x);
		break;
	}
	case UnifType::UINT4:
	{
		const uvec4* x = (uvec4*)data;
		uploadUniform(loc, *x);
		break;
	}

	case UnifType::MATRIX_2:
	{
		const mat2* x = (mat2*)data;
		uploadUniform(loc, *x);
		break;
	}
	case UnifType::MATRIX_3:
	{
		const mat3* x = (mat3*)data;
		uploadUniform(loc, *x);
		break;
	}
	case UnifType::MATRIX_4:
	{
		const mat4* x = (mat4*)data;
		uploadUniform(loc, *x);
		break;
	}

	case UnifType::MATRIX_2x3:
	{
		const mat2x3* x = (mat2x3*)data;
		uploadUniform(loc, *x);
		break;
	}
	case UnifType::MATRIX_3x2:
	{
		const mat3x2* x = (mat3x2*)data;
		uploadUniform(loc, *x);
		break;
	}

	case UnifType::MATRIX_2x4:
	{
		const mat2x4* x = (mat2x4*)data;
		uploadUniform(loc, *x);
		break;
	}
	case UnifType::MATRIX_4x2:
	{
		const mat4x2* x = (mat4x2*)data;
		uploadUniform(loc, *x);
		break;
	}

	case UnifType::MATRIX_3x4:
	{
		const mat3x4* x = (mat3x4*)data;
		uploadUniform(loc, *x);
		break;
	}
	case UnifType::MATRIX_4x3:
	{
		const mat4x3* x = (mat4x3*)data;
		uploadUniform(loc, *x);
		break;
	}

	default:
		throw runtime_error("UnifType not recognized");

	} // end switch
}
