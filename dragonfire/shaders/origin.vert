#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec4 vertexColor;

layout(binding = 0) uniform Matrix
{
    mat4 vp;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor = vertexColor;
    gl_Position = vp * vec4(position, 1);
}
