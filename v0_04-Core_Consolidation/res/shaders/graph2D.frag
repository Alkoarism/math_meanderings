#version 330 core

uniform vec3 textColor;

out vec4 fragColor;

void main() {
	fragColor = vec4(textColor, 1.0);
}