#version 450
// Os modos de textura do glTexEnv que o AvP usa, e o teste de alfa.
// k.y = 1: GL_COMBINE do AvP (RGB da cor do vertice, alfa da textura vezes o do vertice)
// k.z = 1: GL_REPLACE (so a textura)
// k.w: referencia do glAlphaFunc(GL_GREATER); abaixo de 0 desliga o teste
// Sem textura ligada, xenos_gl.c liga uma textura branca 1x1, e GL_MODULATE vira so a cor.
layout(location = 0) in vec2 v_uv;
layout(location = 1) in vec4 v_color;
layout(location = 0) out vec4 o_color;
layout(set = 0, binding = 0) uniform sampler2D u_texture;
layout(push_constant) uniform Mode { vec4 k; } mode;

void main()
{
	vec4 t = texture(u_texture, v_uv);
	vec3 rgb = mix(mix(t.rgb * v_color.rgb, v_color.rgb, mode.k.y), t.rgb, mode.k.z);
	float a = mix(t.a * v_color.a, t.a, mode.k.z);
	if (a <= mode.k.w) discard;
	o_color = vec4(rgb, a);
}
