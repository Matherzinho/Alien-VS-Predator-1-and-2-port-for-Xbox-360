#version 450
layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 o_color;
layout(set = 0, binding = 0) uniform sampler2D u_texture;
layout(push_constant) uniform Look { vec4 k; } look;

void main()
{
	// k.x: scanline darkness, k.y: CRT RGB aperture mask, k.w: gamma boost
	float last_row = 0.5 + 0.5 * cos(6.2831853 * (fract(gl_FragCoord.y / 3.0) - 0.8333));
	float stripe = fract(gl_FragCoord.x / 3.0);
	float red = 1.0 - step(0.3333, stripe);
	float blue = step(0.6667, stripe);
	vec3 mask = vec3(1.0 - look.k.y) + look.k.y * vec3(red, 1.0 - red - blue, blue);
	vec3 c = texture(u_texture, v_uv).rgb;
	if (look.k.w > 0.0) c = pow(max(c, vec3(0.0)), vec3(1.0 / (1.0 + look.k.w)));
	o_color = vec4(c * (1.0 - look.k.x * last_row) * mask, 1.0);
}
