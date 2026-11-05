#version 450
// avp360: os dois estagios de textura do Direct3D 7 (SetTextureStageState) que o d3d.ren usa, sem ramificacoes: cada
// operacao vira pesos sobre termos fixos, calculados em d3d7_xenos.cpp (stage_constants). Por estagio e canal (cor,
// alfa), 7 constantes:
//   a1, a2  (2 cada): escolha do argumento: (difusa, atual, textura, fator), (especular, complemento, replica alfa, 0)
//   w0 = (A1, A2, A1*A2, vies)   w1 = (mistura, A2*A1.a, escala, ligado)   f = fonte do alfa da mistura (dif, tex, atual, 0)
// O resultado e escala * (w0.x*A1 + w0.y*A2 + w0.z*A1*A2 + w0.w + w1.x*mix(A2, A1, f) + w1.y*A2*A1.a).
// Constantes: c[0..13] estagio 0 (cor 0..6, alfa 7..13), c[14..27] estagio 1,
//   c[28] = fator de textura (TEXTUREFACTOR), c[29] = (cor da nevoa rgb, referencia do teste de alfa),
//   c[30] = pesos do teste de alfa (maior, maior ou igual, sempre, nunca), c[31] = (especular ligada, 0, 0, 0)
// O xsc nao aceita chamadas de funcao: as partes repetidas sao macros.
layout(location = 0) in vec4 v_diffuse;
layout(location = 1) in vec4 v_specular;
layout(location = 2) in vec4 v_uv;
layout(location = 3) in float v_fog;
layout(location = 0) out vec4 o_color;
layout(set = 0, binding = 0) uniform sampler2D u_tex0;
layout(set = 0, binding = 1) uniform sampler2D u_tex1;
layout(push_constant) uniform Stages { vec4 c[32]; } st;

#define ARG(out, b, cur, tex) \
	out = st.c[b].x * v_diffuse + st.c[b].y * cur + st.c[b].z * tex + st.c[b].w * st.c[28] + st.c[b + 1].x * v_specular; \
	out = mix(out, vec4(1.0) - out, st.c[b + 1].y); \
	out = mix(out, out.aaaa, st.c[b + 1].z);

#define COMBINE(out, b, cur, tex) \
	ARG(a1, b, cur, tex) \
	ARG(a2, b + 2, cur, tex) \
	f = st.c[b + 6].x * v_diffuse.a + st.c[b + 6].y * tex.a + st.c[b + 6].z * cur.a; \
	out = st.c[b + 4].x * a1 + st.c[b + 4].y * a2 + st.c[b + 4].z * a1 * a2 + vec4(st.c[b + 4].w) \
		+ st.c[b + 5].x * mix(a2, a1, f) + st.c[b + 5].y * a2 * a1.a; \
	out = clamp(out * st.c[b + 5].z, 0.0, 1.0);

#define STAGE(cur, b, tex) \
	COMBINE(rc, b, cur, tex) \
	COMBINE(ra, b + 7, cur, tex) \
	cur = mix(cur, vec4(rc.rgb, ra.a), st.c[b + 5].w);

void main()
{
	vec4 a1, a2, rc, ra;
	float f;
	vec4 t0 = texture(u_tex0, v_uv.xy);
	vec4 t1 = texture(u_tex1, v_uv.zw);
	vec4 cur = v_diffuse;
	STAGE(cur, 0, t0)
	STAGE(cur, 14, t1)

	cur.rgb += st.c[31].x * v_specular.rgb;

	float t = cur.a - st.c[29].w;
	float s = st.c[30].x * t + st.c[30].y * (t + 1.0 / 512.0) + st.c[30].z - st.c[30].w;
	if (s <= 0.0)
		discard;

	cur.rgb = mix(st.c[29].rgb, cur.rgb, clamp(v_fog, 0.0, 1.0));
	o_color = clamp(cur, 0.0, 1.0);
}
