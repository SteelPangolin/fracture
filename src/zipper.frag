uniform sampler2DRect RG_tex;
uniform sampler2DRect BA_tex;

void main()
{
    vec2 tc = gl_TexCoord[0].st;
    vec4 s1 = texture2DRect(RG_tex, tc);
    vec4 s2 = texture2DRect(BA_tex, tc);
    gl_FragData[0] = vec4(s1.r, s1.g, s2.r, s2.g);
}