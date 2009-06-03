uniform sampler2DRect tex;

void main()
{
    float value = texture2DRect(tex, gl_TexCoord[0].st).r;
    gl_FragData[0] = vec4(value, value * value, 0.0, 0.0);
}