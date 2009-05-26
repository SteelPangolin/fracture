uniform sampler2DRect tex;

const vec2 sIncr = vec2(1.0, 0.0);
const vec2 tIncr = vec2(0.0, 1.0);

void main()
{
    vec2 sampleBase = gl_TexCoord[0].st * 2.0;
    vec4 acc = texture2DRect(tex, sampleBase);
    acc += texture2DRect(tex, sampleBase + sIncr);
    acc += texture2DRect(tex, sampleBase + tIncr);
    acc += texture2DRect(tex, sampleBase + sIncr + tIncr);
    gl_FragData[0] = acc;
}