#version 450
// Vertices do AvP ja vem transformados (clip space homogeneo do OpenGL), cor RGBA8 e UV.
layout(location = 0) in vec4 a_position;
layout(location = 1) in vec4 a_color;
layout(location = 2) in vec2 a_uv;
layout(location = 0) out vec2 v_uv;
layout(location = 1) out vec4 v_color;

void main()
{
	v_uv = a_uv;
	v_color = a_color;
	// Profundidade do OpenGL vai de -w a w; a do Xenos, de 0 a w.
	gl_Position = vec4(a_position.x, a_position.y, 0.5 * (a_position.z + a_position.w), a_position.w);
}
