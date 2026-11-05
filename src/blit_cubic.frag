#version 450
// Catmull-Rom 9-tap bicubic filter em 720p (1280x720 Widescreen 16:9).
layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 o_color;
layout(set = 0, binding = 0) uniform sampler2D u_texture;
layout(push_constant) uniform Look { vec4 k; } look;

void main()
{
	const vec2 size = vec2(1280.0, 720.0);
	const vec2 texel = vec2(1.0 / 1280.0, 1.0 / 720.0);
	vec2 p = v_uv * size - vec2(0.5);
	vec2 f = fract(p);
	vec2 base = (p - f + vec2(0.5)) * texel;
	vec2 w0 = f * (vec2(-0.5) + f * (vec2(1.0) - 0.5 * f));
	vec2 w1 = vec2(1.0) + f * f * (vec2(-2.5) + 1.5 * f);
	vec2 w2 = f * (vec2(0.5) + f * (vec2(2.0) - 1.5 * f));
	vec2 w3 = f * f * (vec2(-0.5) + 0.5 * f);
	vec2 w12 = w1 + w2;
	vec2 t0 = base - texel;
	vec2 t12 = base + (w2 / w12) * texel;
	vec2 t3 = base + 2.0 * texel;
	vec3 c = texture(u_texture, vec2(t0.x, t0.y)).rgb * w0.x * w0.y
		+ texture(u_texture, vec2(t12.x, t0.y)).rgb * w12.x * w0.y
		+ texture(u_texture, vec2(t3.x, t0.y)).rgb * w3.x * w0.y
		+ texture(u_texture, vec2(t0.x, t12.y)).rgb * w0.x * w12.y
		+ texture(u_texture, vec2(t12.x, t12.y)).rgb * w12.x * w12.y
		+ texture(u_texture, vec2(t3.x, t12.y)).rgb * w3.x * w12.y
		+ texture(u_texture, vec2(t0.x, t3.y)).rgb * w0.x * w3.y
		+ texture(u_texture, vec2(t12.x, t3.y)).rgb * w12.x * w3.y
		+ texture(u_texture, vec2(t3.x, t3.y)).rgb * w3.x * w3.y;
	if (look.k.w > 0.0) c = pow(max(c, vec3(0.0)), vec3(1.0 / (1.0 + look.k.w)));
	float last_row = 0.5 + 0.5 * cos(6.2831853 * (fract(gl_FragCoord.y / 3.0) - 0.8333));
	float stripe = fract(gl_FragCoord.x / 3.0);
	float red = 1.0 - step(0.3333, stripe);
	float blue = step(0.6667, stripe);
	vec3 mask = vec3(1.0 - look.k.y) + look.k.y * vec3(red, 1.0 - red - blue, blue);
	o_color = vec4(c * (1.0 - look.k.x * last_row) * mask, 1.0);
}
