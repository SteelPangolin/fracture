uniform sampler2DRect tex;

const vec2 sIncr = vec2(1.0, 0.0);
const vec2 tIncr = vec2(0.0, 1.0);

void main()
{
    vec4 sample = texture2DRect(tex, gl_TexCoord[0].st);
    gl_FragData[0] = sample * sample;
}