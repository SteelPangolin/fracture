uniform sampler2DRect tex;

void main()
{
    vec4 sample = texture2DRect(tex, gl_TexCoord[0].st);
    gl_FragData[0] = sample * sample;
}