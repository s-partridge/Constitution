#ifndef CGE_RENDER_PRIMITIVES_H
#define CGE_RENDER_PRIMITIVES_H

#include <cstdint>
#include <vector>
#include <unordered_map>

namespace cge
{
	namespace math
	{
		struct Point2Df
		{
			float x;
			float y;
		};

		struct Point3Df
		{
			float x;
			float y;
			float z;
		};

		struct Ray3Df
		{
			math::Point3Df origin;
			math::Point3Df direction;
		};
	}

	namespace color
	{
		struct RGB
		{
			float r;
			float g;
			float b;
		};

		struct RGBA
		{
			float r;
			float g;
			float b;
			float a;
		};
	}

	namespace primitives
	{
		// Vertex structure for a 3D model, containing position and color information.
		struct Vertex
		{
			math::Point3Df position;
			color::RGB color;
		};

		// Indices for positions and face colors for a 3D model, used to construct vertices for rendering.
		struct FaceCorner
		{
			std::uint32_t position;
			std::uint32_t color;
		};

		// Hard-coding a generic 1-unit cube centered at the origin.
		const math::Point3Df cubePositions[8] = {
			{-0.5f, -0.5f, -0.5f},
			{0.5f, -0.5f, -0.5f},
			{0.5f, 0.5f, -0.5f},
			{-0.5f, 0.5f, -0.5f},
			{-0.5f, -0.5f, 0.5f},
			{0.5f, -0.5f, 0.5f},
			{0.5f, 0.5f, 0.5f},
			{-0.5f, 0.5f, 0.5f}
		};

		const color::RGB colorPalette[6] = {
			{1.0f, 0.2f, 0.2f}, // Red
			{0.2f, 1.0f, 0.2f}, // Green
			{0.2f, 0.2f, 1.0f}, // Blue
			{1.0f, 1.0f, 0.2f}, // Yellow
			{0.2f, 1.0f, 1.0f}, // Cyan
			{1.0f, 0.2f, 1.0f}  // Magenta
		};

		// Cube map. Counter-clockwise winding order for each face
		const FaceCorner cubeFaces[6][4] = {
			{ {4, 0}, {5, 0}, {6, 0}, {7, 0} },	// +Z
			{ {1, 3}, {0, 3}, {3, 1}, {2, 1} }, // -Z
			{ {5, 2}, {1, 2}, {2, 2}, {6, 3} }, // +X
			{ {0, 3}, {4, 3}, {7, 3}, {3, 2} }, // -X
			{ {7, 4}, {6, 4}, {2, 4}, {3, 4} }, // +Y
			{ {0, 0}, {1, 0}, {5, 5}, {4, 5} }	// -Y
		};
	}

	namespace mesh
	{
		struct VMesh
		{
			std::vector<primitives::Vertex> vertices;
			std::vector<std::uint32_t> indices;
		};

		struct Mesh
		{
			primitives::Vertex *vertices;
			size_t vertexCount;
			std::uint32_t *indices;
			size_t indexCount;

			Mesh() : vertices(nullptr), vertexCount(0), indices(nullptr), indexCount(0) {}
			// Copy constructor
			Mesh(const Mesh &other) : vertexCount(other.vertexCount), indexCount(other.indexCount)
			{
				vertices = new primitives::Vertex[vertexCount];
				indices = new std::uint32_t[indexCount];
				std::copy(other.vertices, other.vertices + vertexCount, vertices);
				std::copy(other.indices, other.indices + indexCount, indices);
			}

			// Move constructor
			Mesh(Mesh &&other) noexcept : vertices(other.vertices), vertexCount(other.vertexCount), indices(other.indices), indexCount(other.indexCount)
			{
				other.vertices = nullptr;
				other.indices = nullptr;
				other.vertexCount = 0;
				other.indexCount = 0;
			}

			// Copy assignment operator
			Mesh& operator=(const Mesh &other)
			{
				if(this != &other)
				{
					clear();
					vertexCount = other.vertexCount;
					indexCount = other.indexCount;
					vertices = new primitives::Vertex[vertexCount];
					indices = new std::uint32_t[indexCount];

					std::copy(other.vertices, other.vertices + vertexCount, vertices);
					std::copy(other.indices, other.indices + indexCount, indices);
				}
				return *this;
			}

			// Move assignment operator
			Mesh& operator=(Mesh &&other) noexcept
			{
				if(this != &other)
				{
					clear();
					vertices = other.vertices;
					vertexCount = other.vertexCount;
					indices = other.indices;
					indexCount = other.indexCount;

					other.vertices = nullptr;
					other.indices = nullptr;
					other.vertexCount = 0;
					other.indexCount = 0;
				}
				return *this;
			}

			static Mesh fromVMesh(const VMesh &vMesh)
			{
				Mesh mesh;
				mesh.vertexCount = vMesh.vertices.size();
				mesh.indexCount = vMesh.indices.size();
				mesh.vertices = new primitives::Vertex[mesh.vertexCount];
				mesh.indices = new std::uint32_t[mesh.indexCount];
				std::copy(vMesh.vertices.begin(), vMesh.vertices.end(), mesh.vertices);
				std::copy(vMesh.indices.begin(), vMesh.indices.end(), mesh.indices);
				return mesh;
			}

			~Mesh()
			{
				clear();
			}

		private:
			void clear()
			{
				delete[] vertices;
				delete[] indices;
				vertices = nullptr;
				indices = nullptr;
				vertexCount = 0;
				indexCount = 0;
			}
		};

		inline Mesh buildSimpleMesh(const primitives::FaceCorner faces[][4], size_t faceCount, const math::Point3Df *positions, size_t positionCount, const color::RGB *colorPalette, size_t colorCount)
		{
			VMesh mesh;
			std::unordered_map<std::uint64_t, std::uint32_t> seen; // Maps a unique vertex (position + color) to its index in the mesh's vertex list.

			for(size_t faceIdx = 0; faceIdx < faceCount; ++faceIdx)
			{
				std::uint32_t corner[4];
				for(unsigned cornerIdx = 0; cornerIdx < 4; ++cornerIdx)
				{
					const primitives::FaceCorner &faceCorner = faces[faceIdx][cornerIdx];

					// Create a unique key 
					std::uint64_t key = (static_cast<std::uint64_t>(faceCorner.position) << 32) | faceCorner.color;
					std::unordered_map<std::uint64_t, std::uint32_t>::iterator found = seen.find(key);

					if(found != seen.end())
					{
						corner[cornerIdx] = found->second;
						continue;
					}

					primitives::Vertex vertex;
					vertex.position = positions[faceCorner.position];
					vertex.color = colorPalette[faceCorner.color];

					std::uint32_t vertexIndex = static_cast<std::uint32_t>(mesh.vertices.size());
					seen.emplace(key, vertexIndex);
					mesh.vertices.push_back(vertex);
					corner[cornerIdx] = vertexIndex;
				}

				// Convert the quad into two triangles for rendering, using the face's corners.
				mesh.indices.push_back(corner[0]);
				mesh.indices.push_back(corner[1]);
				mesh.indices.push_back(corner[2]);

				mesh.indices.push_back(corner[0]);
				mesh.indices.push_back(corner[2]);
				mesh.indices.push_back(corner[3]);
			}

			return Mesh::fromVMesh(mesh);
		}
	}
}

#endif