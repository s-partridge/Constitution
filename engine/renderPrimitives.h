#ifndef CGE_RENDER_PRIMITIVES_H
#define CGE_RENDER_PRIMITIVES_H

#include <cmath>
#include <cstdint>
#include <cstddef> // for std::size_t
#include <utility> // for std::swap
#include <algorithm> // for std::copy
#include <vector>
#include <unordered_map>

namespace cge::math
{
	namespace axis
	{
		constexpr std::size_t X = 0;
		constexpr std::size_t Y = 1;
		constexpr std::size_t Z = 2;
		constexpr std::size_t W = 3;
	}

	struct Vec2f
	{
		float x;
		float y;
	};

	// If this is passed to Vulkan, it will get padded to 16 bytes in certain circumstances. This is not the case for vertex buffers where the data size is defined explicitly, but it is the case for push constants and uniform buffers.
	// In short, generally prefer Vec4f over Vec3f for uniform buffer or push constants
	struct Vec3f
	{
		float x;
		float y;
		float z;
	};

	struct Vec4f
	{
		float x;
		float y;
		float z;
		float w;
	};

	struct Ray3Df
	{
		Vec3f origin;
		Vec3f direction;
	};

	namespace matrix
	{
		using namespace axis;

		// TODO: Maybe split this. The latter four are generally fixed for a given system, and the former four can change dynamically at any time, even for a given camera.
		struct ProjectionConfig
		{
			float fov;					// Field of view in radians
			float aspect;				// Aspect ratio of the viewport (width / height)
			float nearPlane;			// Near clipping plane distance
			float farPlane;				// Far clipping plane distance
			bool rightHanded = true;	// Use a right-handed coordinate system (true) or left-handed (false)
			bool depthZeroToOne = true; // Use a depth range of [0, 1] (true) or [-1, 1] (false)
			bool yUp = true;			// Whether the Y-axis is up (true) or down (false)
			// Invert the Z-axis (true) or not (false).
			//This is useful for reverse-Z depth buffers, which can improve depth precision for far objects.
			bool flipZ = false;	
		};

		struct Mat4f
		{
			// Stored in column-major order to match OpenGL and Vulkan conventions.
			float mat[4][4];

			static constexpr Mat4f identity()
			{
				Mat4f mat = Mat4f();
				mat.mat[X][X] = 1.0f;
				mat.mat[Y][Y] = 1.0f;
				mat.mat[Z][Z] = 1.0f;
				mat.mat[W][W] = 1.0f;
				return mat;
			}

			// Euler triple rotation, applied in XYZ order. This is the same as Blender's default rotation order.
			static Mat4f rotation(float x, float y, float z)
			{
				float cosX = std::cos(x);
				float sinX = std::sin(x);
				float cosY = std::cos(y);
				float sinY = std::sin(y);
				float cosZ = std::cos(z);
				float sinZ = std::sin(z);
				Mat4f mat = identity();

				// For our coordinate system we follow Blender's convention:
				//	Z up, Y forward, X right
				// Thus:
				//   Z rotation affects [0][0], [0][1], [1][0], [1][1]
				//   Y rotation affects [0][0], [0][2], [2][0], [2][2]
				//   X rotation affects [1][1], [1][2], [2][1], [2][2]

				// Rotation matrices for each axis:
				//      | 1       0            0      |
				// Rx = | 0   cos(angle)  -sin(angle) |
				//      | 0   sin(angle)   cos(angle) |

				//      | cos(angle)   0   sin(angle) |
				// Ry = |     0        1       0      |
				//      | -sin(angle)  0   cos(angle) |

				//      | cos(angle) -sin(angle)    0 |
				// Rz = | sin(angle)  cos(angle)    0 |
				//	    |     0           0         1 |

				// Once these are multiplied together in XYZ order, the resulting matrix is:
				//     x             y                  z
				// | cz*cy   cz*sy*sx - sz*cx   cz*sy*cx + sz*sx |
				// | sz*cy   sz*sy*sx + cz*cx   sz*sy*cx - cz*sx |
				// |  -sy         cy*sx               cy*cx      |

				// Distributed matrix multiplication,
				// rather than running three matrix multiplications in sequence
				mat.mat[X][X] = cosY * cosZ;
				mat.mat[X][Y] = cosY * sinZ;
				mat.mat[X][Z] = -sinY;

				mat.mat[Y][X] = sinX * sinY * cosZ - cosX * sinZ;
				mat.mat[Y][Y] = sinX * sinY * sinZ + cosX * cosZ;
				mat.mat[Y][Z] = sinX * cosY;

				mat.mat[Z][X] = cosX * sinY * cosZ + sinX * sinZ;
				mat.mat[Z][Y] = cosX * sinY * sinZ - sinX * cosZ;
				mat.mat[Z][Z] = cosX * cosY;

				return mat;
			}

			static Mat4f perspective(float fov, float aspect, float nearPlane, float farPlane)
			{
				Mat4f mat = Mat4f();
				
				// Perspective is defined as a frustum, which is a truncated pyramid. The field of view (fov) defines the angle of the pyramid, and the aspect ratio defines the width-to-height ratio of the pyramid's base. The near and far planes define the distances from the camera to the near and far clipping planes, respectively.
				// f * (distance * tan(fov / 2)) / distance = 1
				// f * tan(fov / 2) = 1
				// f = 1 / tan(fov / 2)
				// f is the scaling factor that will be applied to the x and y coordinates of the vertices to project them onto the near plane
				// f is the scale factor applied to the x and y coordinates of the vertices to project them onto the near plane
				// Going forward, we'll call it yScale. xScale, then is yScale / aspect, since the x coordinate is scaled by the aspect ratio to maintain the correct width-to-height ratio of the frustum.
				// TODO: What if I want to use a horizontal field of view instead of a vertical field of view, say for a vertical screen?
				float yScale = 1.0f / std::tan(fov / 2.0f);
				mat.mat[X][X] = yScale / aspect;
				mat.mat[Y][Y] = yScale;
				mat.mat[Z][Z] = farPlane / (nearPlane - farPlane); // Normalize the z coordinate to the range [0, 1] for Vulkan

				// Normalize (read: translate) the z coordinate to the range [0, 1]
				// TODO: solve this in terms of the near plane equation, as described in the generalized projection matrix function below
				mat.mat[W][Z] = (farPlane * nearPlane) / (nearPlane - farPlane);
				mat.mat[Z][W] = -1.0f; // Move the z coordinate 
				
				mat.mat[W][W] = 0.0f; // no-op for a default matrix, but included for clarity. Zero here prevents the input w coordinate from being passed through to the output.

				return mat;
			}

			// Generalized configuration for projection matrices, allowing for more flexibility in defining the projection parameters. This can be used to create perspective or orthographic projection matrices based on the provided configuration.
			static Mat4f projection(const ProjectionConfig &config)
			{
				Mat4f mat = Mat4f();
				// Build out from the configuration

				// Look direction, either positive or negative z-axis
				// Whether the z-axis points into or out of the screen
				// This is convention-dependent. With y-up and x-right (this one never changes in any configuration), a right-handed coordinate system has the z-axis pointing out of the screen, while a left-handed coordinate system has the z-axis pointing into the screen.
				float forward = config.rightHanded == config.yUp ? -1.0f : 1.0f;

				float yScale = 1.0f / std::tan(config.fov / 2.0f);
				mat.mat[X][X] = yScale / config.aspect;
				mat.mat[Y][Y] = config.yUp ? yScale : -yScale; // Flip the y-axis if y-up is false

				float zRange = config.farPlane - config.nearPlane;
				
				// When we apply perspective to a vertex, we store the coefficients required to convert the vertex into clip space.
				// For X/Y, this is straightforward: we scale the x and y coordinates by the aspect ratio and the field of view.
				// Depth is more complicated than the other axes; We have to 
				// transform the Z axis with scale, offset, and direction
				// Direction is either -1 or 1, depending on whether the coordinate system is right-handed or left-handed. This is used to flip the z-axis if necessary.
				// Scale and offset are used to map the z-coordinate from the range of near and far planes to the near and far planes of clip space. This will be normalized to either [-1, 1] or [0, 1].
				// depthScale is mapped to [Z][Z]
				// depthOffset is mapped to [W][Z]
				// 
				// For the planes:
				// near plane: scale * forward * near distance + offset = near depth * near distance
				// far plane:  scale * forward * far distance  + offset = far depth * far distance
				// 
				// scale and offset can be solved for by solving the system of equations above. The solution is:
				// scale = forward * (far depth * far distance - near depth * near distance) / (far distance - near distance)
				// offset = (near depth - far depth) * near distance * far distance / (far distance - near distance)
				// 
				// alternatively, offset can be solved for by rearranging the near plane equation:
				// near plane: scale * forward * near distance + offset = near depth * near distance
				// offset = near depth * near distance - scale * forward * near distance
				// derivation:
				// 
				// After the GPU processes vertex through a vertex shader, it normalizes x,y, and z by dividing each axis by w.
				// This step converts the vertex from clip space to normalized device coordinates (NDC).

				// The near and far depth values are defined by the graphics API
				float ndcNear = config.depthZeroToOne ? 0.0f : -1.0f;
				float ndcFar = 1.0f;

				if(config.flipZ)
				{
					std::swap(ndcNear, ndcFar);
				}

				// If either ndcNear or ncdFar are zero, those terms will cancel out.
				// TODO: It's probably worth special-casing this to avoid extra calculations, but for now, this is fine.
				float depthScale = forward * (ndcFar * config.farPlane - ndcNear * config.nearPlane) / (config.farPlane - config.nearPlane);
			
				// Solved in terms of the near plane equation, as described above
				float depthOffset = ndcNear * config.nearPlane - depthScale * forward * config.nearPlane;

				mat.mat[Z][Z] = depthScale;				
				mat.mat[W][Z] = depthOffset;
				mat.mat[Z][W] = forward;

				return mat;
			}

			static Mat4f translation(float x, float y, float z)
			{
				Mat4f mat = identity();
				mat.mat[W][X] = x;
				mat.mat[W][Y] = y;
				mat.mat[W][Z] = z;
				return mat;
			}

			static Mat4f scale(float x, float y, float z)
			{
				Mat4f mat = identity();
				mat.mat[X][X] = x;
				mat.mat[Y][Y] = y;
				mat.mat[Z][Z] = z;
				return mat;
			}

			void rotate(float x, float y, float z)
			{
				*this *= rotation(x, y, z);
			}

			void translate(float x, float y, float z)
			{
				*this *= translation(x, y, z);
			}

			void rescale(float x, float y, float z)
			{
				*this *= scale(x, y, z);
			}

			float set(int row, int col, float value)
			{
				mat[col][row] = value;
				return value;
			}

			float at(int row, int col) const
			{
				return mat[col][row];
			}

			Mat4f &operator*=(const Mat4f &rhs);

			Mat4f &operator*=(const Vec4f &rhs);
		};

		// Overload the multiplication operator for Mat4f to allow for matrix multiplication.
		inline Mat4f operator*(const Mat4f &lhs, const Mat4f &rhs)
		{
			Mat4f result = Mat4f();
			// quickly multiply each row of lhs by each column of rhs, storing the result in the corresponding cell of result
			for(unsigned row = 0; row < 4; ++row)
			{
				for(unsigned col = 0; col < 4; ++col)
				{
					// Dot product of the row of lhs and the column of rhs
					result.mat[col][row] = lhs.mat[X][row] * rhs.mat[col][X]
											+ lhs.mat[Y][row] * rhs.mat[col][Y]
											+ lhs.mat[Z][row] * rhs.mat[col][Z]
											+ lhs.mat[W][row] * rhs.mat[col][W];
				}
			}
			return result;
		}

		// Overload the multiplication operator for Mat4f and Vec4f to allow for matrix-vector multiplication.
		inline Vec4f operator*(const Mat4f &lhs, const Vec4f &rhs)
		{
			Vec4f result = Vec4f();
			// Inputs are applied per column of the matrix, and each index of the output corresponds to a row.
			//output xyzw             input x                 input y                 input z                 input w
			result.x = lhs.mat[X][X] * rhs.x + lhs.mat[Y][X] * rhs.y + lhs.mat[Z][X] * rhs.z + lhs.mat[W][X] * rhs.w;
			result.y = lhs.mat[X][Y] * rhs.x + lhs.mat[Y][Y] * rhs.y + lhs.mat[Z][Y] * rhs.z + lhs.mat[W][Y] * rhs.w;
			result.z = lhs.mat[X][Z] * rhs.x + lhs.mat[Y][Z] * rhs.y + lhs.mat[Z][Z] * rhs.z + lhs.mat[W][Z] * rhs.w;
			result.w = lhs.mat[X][W] * rhs.x + lhs.mat[Y][W] * rhs.y + lhs.mat[Z][W] * rhs.z + lhs.mat[W][W] * rhs.w;
			return result;
		}

		inline Mat4f &Mat4f::operator*=(const Mat4f &rhs)
		{
			*this = *this * rhs;
			return *this;
		}
	}

	// Namespace resolution for math::matrix::Mat4f, allowing for the use of Mat4f without the full namespace prefix.
	// This list will probably expand as more types are added
	using Mat4f = matrix::Mat4f;
}

namespace cge::color
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

namespace cge::primitives
{
	// Vertex structure for a 3D model, containing position and color information.
	struct Vertex
	{
		math::Vec3f position;
		color::RGB color;
	};

	// Indices for positions and face colors for a 3D model, used to construct vertices for rendering.
	struct FaceCorner
	{
		std::uint32_t position;
		std::uint32_t color;
	};

	// Hard-coding a generic 1-unit cube centered at the origin.
	const math::Vec3f cubePositions[8] = {
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

namespace cge::mesh
{
	struct DrawableBase
	{
		math::Vec3f position;
		math::Vec3f rotation;
		math::Vec3f scale;
	};

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

	inline Mesh buildSimpleMesh(const primitives::FaceCorner faces[][4], size_t faceCount, const math::Vec3f *positions, size_t positionCount, const color::RGB *colorPalette, size_t colorCount)
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

#endif