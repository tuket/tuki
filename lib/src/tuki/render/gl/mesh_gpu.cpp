#include "mesh_gpu.hpp"

#include <glad/glad.h>
#include <cassert>

#include "../mesh/mesh.hpp"

// upload vertex attrib data and set the pointer
inline void setVertexAttrib(
	int attribLocation, Vbo vbo, unsigned numVerts, unsigned numDims, const void* data)
{
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	if (data != nullptr)
	{
		glBufferData(GL_ARRAY_BUFFER, numVerts * numDims * sizeof(float),
			data, GL_STATIC_DRAW);
		glVertexAttribPointer(attribLocation, numDims, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(attribLocation);
	}
	else
	{
		glDisableVertexAttribArray(attribLocation);
	}
}

// upload vertex indices
inline void setVertexIndices(Vbo vbo, unsigned numInds, const void* data)
{
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, numInds * sizeof(unsigned), data, GL_STATIC_DRAW);
}

void IMeshGpu::bind()const
{
	glBindVertexArray(vao);
}

unsigned MeshGpuGeneric::getNumElements()const
{
	return numElements;
}

void MeshGpuGeneric::load(const IMesh& mesh)
{
	glGenVertexArrays(1, (GLuint*)&vao);
	glBindVertexArray(vao);

	const unsigned nv = mesh.getNumVertices();
	const unsigned ni = mesh.getNumIndices();

	attribBitMask = AttribBitMask::NONE;

	// vertex attributes
	for (unsigned i = 0; i < (unsigned)AttribLocation::NUM_ATTRIBS; i++)
	{
		AttribLocation curAttrib = (AttribLocation)i;
		if (mesh.hasAttribData((AttribLocation)i))
		{
			attribBitMask |= (AttribBitMask)(1 << i);
			const unsigned numComp = ATTRIB_NUM_COMPONENTS[i];
			const void* data = mesh.getAttribData(curAttrib);
			glGenBuffers(1, (GLuint*)&vboSet.attribs[i]);
			setVertexAttrib(i, vboSet.attribs[i], nv, numComp, data);
		}
		else
		{
			// it's important to set the vbo to 0, oterwise if there is garbage
			// when deleting the vbo we could be deleting unwanted vbos
			vboSet.attribs[i] = 0;
		}
	}

	// elements
	if (mesh.hasIndices())
	{
		glGenBuffers(1, (GLuint*)&vboSet.indices);
		setVertexIndices(vboSet.indices, ni, (void*)mesh.getIndices());
		numElements = mesh.getNumIndices();
	}
	else
	{
		// it's important to set the vbo to 0, oterwise if there is garbage
		// when deleting the vbo we could be deleting unwanted vbos
		vboSet.indices = 0;
		numElements = mesh.getNumVertices();
	}
}

void MeshGpuGeneric::free()
{
	freeVao(vao);
	freeVboSet(vboSet);
}

void UvPlaneMeshGpu::load()
{
	const unsigned nv = 4;
	const float uvCoords[2 * nv] =
	{
		0, 1,
		0, 0,
		1, 1,
		1, 0
	};
	const unsigned loc = (unsigned)AttribLocation::TEX_COORD;

	glGenVertexArrays(1, (GLuint*)&vao);
	glBindVertexArray(vao);

	glGenBuffers(1, (GLuint*)&vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);

	glBufferData(GL_ARRAY_BUFFER, sizeof(uvCoords), (void*)uvCoords, GL_STATIC_DRAW);
	glVertexAttribPointer(loc, 2, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(loc);
}

void UvPlaneMeshGpu::free()
{
	freeVao(vao);
	freeVbo(vbo);
}

void freeVao(Vao vao)
{
	glDeleteBuffers(1, (GLuint*)&vao);
}

void freeVbo(Vbo vbo)
{
	glDeleteBuffers(1, (GLuint*)&vbo);
}

void freeVaos(const Vao* vaos, unsigned num)
{
	glDeleteBuffers(num, (const GLuint*)vaos);
}

void freeVbos(const Vbo* vbos, unsigned num)
{
	glDeleteBuffers(num, (const GLuint*)vbos);
}

void freeVboSet(const VboSetFull& vboSet)
{
	const unsigned na = (unsigned)AttribLocation::NUM_ATTRIBS;
	// we don't need to check if the vbos are valid because 0s are ignored by OpenGL API
	// in the creation, we have been careful to initialize unused vbos to 0
	for (unsigned i = 0; i < na; i++)
	{
		freeVbo(vboSet.attribs[i]);
	}
	freeVbo(vboSet.indices);
}