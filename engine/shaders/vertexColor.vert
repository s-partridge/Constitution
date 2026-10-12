#version 450

// Model-view-projection matrix, pushed each frame as a constant.
layout(push_constant) uniform PushConstants
{
	mat4 mvp;
} pushConstants;

// One vertex from the vertex buffer.
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

// Output to the fragment shader.
layout(location = 0) out vec3 fragColor;

void main()
{
	// Transform the vertex position by the model-view-projection matrix.
	gl_Position = pushConstants.mvp * vec4(inPosition, 1.0);
	// Pass the vertex color to the fragment shader.
	fragColor = inColor;
}