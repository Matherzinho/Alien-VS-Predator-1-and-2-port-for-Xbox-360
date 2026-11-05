#version 450
// avp360: os vertices pre-transformados ("TL") do Direct3D 7: x e y em pixels da tela, z de 0 a 1, rhw = 1/w.
// Voltam para o espaco de clip multiplicando por w, para o Xenos interpolar UV e cor com perspectiva como o D3D.
//   k[0] = (2/largura, -2/altura, -1 - 0.5*2/largura, 1 + 0.5*2/altura): pixel -> NDC, com o meio pixel do D3D
//   k[1] = (inicio da nevoa, fim da nevoa, modo: 0 sem, 1 tabela linear pela distancia (1/rhw), 2 do vertice, 0)
layout(location = 0) in vec4 a_position;
layout(location = 1) in vec4 a_diffuse;
layout(location = 2) in vec2 a_uv0;
layout(location = 3) in vec2 a_uv1;
layout(location = 4) in vec4 a_specular;
layout(location = 0) out vec4 v_diffuse;
layout(location = 1) out vec4 v_specular;
layout(location = 2) out vec4 v_uv;
layout(location = 3) out float v_fog;
layout(push_constant) uniform Screen { vec4 k[2]; } screen;

void main()
{
	float rhw = a_position.w > 1e-12 ? a_position.w : 1e-12;
	float w = 1.0 / rhw;
	vec2 ndc = vec2(a_position.x * screen.k[0].x + screen.k[0].z, a_position.y * screen.k[0].y + screen.k[0].w);
	gl_Position = vec4(ndc * w, a_position.z * w, w);

	v_diffuse = a_diffuse;
	v_specular = a_specular;
	v_uv = vec4(a_uv0, a_uv1);

	float table = clamp((screen.k[1].y - w) / max(screen.k[1].y - screen.k[1].x, 1e-6), 0.0, 1.0);
	float mode = screen.k[1].z;
	// 1 = sem nevoa; a cor final e mix(cor da nevoa, cor, v_fog).
	v_fog = mode < 0.5 ? 1.0 : (mode < 1.5 ? table : a_specular.a);
}
