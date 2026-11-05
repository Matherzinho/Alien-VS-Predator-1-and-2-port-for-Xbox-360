#version 450
// Scale3x edge-directed filter sem branches em 720p (1280x720 Widescreen 16:9).
layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 o_color;
layout(set = 0, binding = 0) uniform sampler2D u_texture;
layout(push_constant) uniform Look { vec4 k; } look;

#define SAME(p, q) (1.0 - step(0.02, dot(abs((p) - (q)), vec3(1.0))))

void main()
{
	const vec2 size = vec2(1280.0, 720.0);
	const vec2 texel = vec2(1.0 / 1280.0, 1.0 / 720.0);
	vec2 p = v_uv * size;
	vec2 at = floor(p);
	vec2 cell = (p - at) * 3.0;
	vec2 c = (at + vec2(0.5)) * texel;
	vec3 e = texture(u_texture, c).rgb;
	vec3 a = texture(u_texture, c + vec2(-texel.x, -texel.y)).rgb;
	vec3 b = texture(u_texture, c + vec2(0.0, -texel.y)).rgb;
	vec3 k = texture(u_texture, c + vec2(texel.x, -texel.y)).rgb;
	vec3 d = texture(u_texture, c + vec2(-texel.x, 0.0)).rgb;
	vec3 f = texture(u_texture, c + vec2(texel.x, 0.0)).rgb;
	vec3 g = texture(u_texture, c + vec2(-texel.x, texel.y)).rgb;
	vec3 h = texture(u_texture, c + vec2(0.0, texel.y)).rgb;
	vec3 i = texture(u_texture, c + vec2(texel.x, texel.y)).rgb;
	float on = (1.0 - SAME(b, h)) * (1.0 - SAME(d, f));
	float db = SAME(d, b) * on;
	float bf = SAME(b, f) * on;
	float dh = SAME(d, h) * on;
	float hf = SAME(h, f) * on;
	float ea = 1.0 - SAME(e, a);
	float ek = 1.0 - SAME(e, k);
	float eg = 1.0 - SAME(e, g);
	float ei = 1.0 - SAME(e, i);
	float x0 = 1.0 - step(1.0, cell.x);
	float x2 = step(2.0, cell.x);
	float x1 = 1.0 - x0 - x2;
	float y0 = 1.0 - step(1.0, cell.y);
	float y2 = step(2.0, cell.y);
	float y1 = 1.0 - y0 - y2;
	float to_b = y0 * x1 * clamp(db * ek + bf * ea, 0.0, 1.0);
	float to_d = clamp(y0 * x0 * db + y2 * x0 * dh + y1 * x0 * clamp(db * eg + dh * ea, 0.0, 1.0), 0.0, 1.0);
	float to_f = clamp(y0 * x2 * bf + y2 * x2 * hf + y1 * x2 * clamp(bf * ei + hf * ek, 0.0, 1.0), 0.0, 1.0);
	float to_h = y2 * x1 * clamp(dh * ei + hf * eg, 0.0, 1.0);
	vec3 o = e;
	o = mix(o, b, to_b);
	o = mix(o, d, to_d);
	o = mix(o, f, to_f);
	o = mix(o, h, to_h);
	if (look.k.w > 0.0) o = pow(max(o, vec3(0.0)), vec3(1.0 / (1.0 + look.k.w)));
	float last_row = 0.5 + 0.5 * cos(6.2831853 * (fract(gl_FragCoord.y / 3.0) - 0.8333));
	float stripe = fract(gl_FragCoord.x / 3.0);
	float red = 1.0 - step(0.3333, stripe);
	float blue = step(0.6667, stripe);
	vec3 mask = vec3(1.0 - look.k.y) + look.k.y * vec3(red, 1.0 - red - blue, blue);
	o_color = vec4(o * (1.0 - look.k.x * last_row) * mask, 1.0);
}
