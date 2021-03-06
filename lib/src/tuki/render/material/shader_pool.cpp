#include "shader_pool.hpp"

#include "../gl/shader.hpp"

using namespace std;

ShaderPool::ShaderPool()
{

}

ShaderProgram ShaderPool::getShaderProgram(
	const string& vertShadPath, const string& fragShadPath,
	const string& geomShadPath,
	AttribInitilizer attribInitializer)
{
	auto vsIt = vertShaderNameToId.find(vertShadPath);
	auto fsIt = fragShaderNameToId.find(fragShadPath);
	auto gsIt = fragShaderNameToId.find(geomShadPath);

	int vs, fs, gs = -1;

	if (vsIt != vertShaderNameToId.end() && fsIt != fragShaderNameToId.end())
	{
		vs = vsIt->second;
		fs = vsIt->second;
		if (geomShadPath == "" ||
		   (geomShadPath != "" && gsIt != geomShaderNameToId.end()))
		{
			gs = gsIt->second;
			auto progIt = shadersToProgram.find({ vs, fs, gs });
			if (progIt != shadersToProgram.end())
			{
				// the program is already loaded
				return programs[progIt->second];
			}
		}
	}
	
	// create vertex shader if needed
	if (vsIt != vertShaderNameToId.end())
	{
		vs = vsIt->second;
	}
	else
	{
		VertexShaderObject vertShad;
		vertShad.create();
		vertShad.loadFromFile(vertShadPath.c_str());
		try {
			vertShad.compile();
		}
		catch (runtime_error e) {
			throw runtime_error(vertShadPath + ": " + e.what());
		}
		vs = vertShaders.size();

		vertShaders.push_back(vertShad);
		vertShaderNameToId[vertShadPath] = vs;
	}

	// create fragment shader id needed
	if (fsIt != fragShaderNameToId.end())
	{
		fs = fsIt->second;
	}
	else
	{
		FragmentShaderObject fragShad;
		fragShad.create();
		fragShad.loadFromFile(fragShadPath.c_str());
		try {
			fragShad.compile();
		}
		catch (runtime_error e) {
			throw runtime_error(vertShadPath + ": " + e.what());
		}
		fs = fragShaders.size();

		fragShaders.push_back(fragShad);
		fragShaderNameToId[fragShadPath] = fs;
	}

	// create geometry shader id needed
	if (geomShadPath != "")
	{
		if (gsIt != geomShaderNameToId.end())
		{
			gs = gsIt->second;
		}
		else
		{
			GeometryShaderObject geomShad;
			geomShad.create();
			geomShad.loadFromFile(geomShadPath.c_str());
			try {
				geomShad.compile();
			}
			catch (runtime_error e) {
				throw runtime_error(geomShadPath + ": " + e.what());
			}
			gs = geomShaders.size();

			geomShaders.push_back(geomShad);
			geomShaderNameToId[geomShadPath] = gs;
		}
	}

	// create shader program
	ShaderProgram prog;
	prog.create();

	prog.setVertexShader(vertShaders[vs]);
	prog.setFragmentShader(fragShaders[fs]);
	if (gs != -1) prog.setGeometryShader(geomShaders[gs]);

	AttribInitilizers::generic(prog);

	try {
		prog.link();
	}
	catch (runtime_error e) {
		throw runtime_error(
			"link error (" +
			vertShadPath + ", " +
			fragShadPath + ", " +
			geomShadPath + ")");
	}

	int progId = programs.size();
	programs.push_back(prog);
	shadersToProgram[{vs, fs, gs}] = progId;
	programToShaders.push_back({ vs, fs, gs });

	return prog;
}
