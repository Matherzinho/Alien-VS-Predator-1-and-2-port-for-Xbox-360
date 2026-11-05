#version 450
// Sharp bilinear em 720p (1280x720 Widescreen 16:9).
layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 o_color;
layout(set = 0, binding = 0) uniform sampler2D u_texture;
layout(push_constant) uniform Look { vec4 k; } look;

void main()
{
	const vec2 size = vec2(1280.0, 720.0);
	vec2 p = v_uv * size;
	vec2 cell = floor(p);
	vec2 from_centre = p - cell - vec2(0.5);
	vec2 past = from_centre - clamp(from_centre, vec2(-0.33333, -0.33333), vec2(0.33333, 0.33333));
	vec3 c = texture(u_texture, (cell + vec2(0.5) + past * 3.0) / size).rgb;
	if (look.k.w > 0.0) c = pow(max(c, vec3(0.0)), vec3(1.0 / (1.0 + look.k.w)));
	float last_row = 0.5 + 0.5 * cos(6.2831853 * (fract(gl_FragCoord.y / 3.0) - 0.8333));
	float stripe = fract(gl_FragCoord.x / 3.0);
	float red = 1.0 - step(0.3333, stripe);
	float blue = step(0.6667, stripe);
	vec3 mask = vec3(1.0 - look.k.y) + look.k.y * vec3(red, 1.0 - red - blue, blue);
	o_color = vec4(c * (1.0 - look.k.x * last_row) * mask, 1.0);
}
