uniform sampler2DRect D_tex;
uniform sampler2DRect R_tex;

uniform float r_size;
uniform float r_x;
uniform float r_y;

void main()
{
    vec2 d_pos = gl_TexCoord[0].st;
    vec2 r_pos = mod(d_pos, r_size) + vec2(r_x, r_y);
    gl_FragData[0] = texture2DRect(D_tex, d_pos) * texture2DRect(R_tex, r_pos);
}