#version 330 core

in vec2 vUV;
out vec4 fragColor;

uniform sampler2D iChannel0;
uniform vec2 texScale;

void main()
{
    fragColor = texture(iChannel0, vUV * texScale);
}