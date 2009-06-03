uniform sampler2DRect tex;

const vec2 sIncr = vec2(1.0, 0.0);
const vec2 tIncr = vec2(0.0, 1.0);

void main()
{
    vec2 tc = gl_TexCoord[0].st - vec2(0.5, 0.5);
    
    vec4 bestP = texture2DRect(tex, tc);
    float bestMSE = bestP.r;
    vec4 P;
    float MSE;
    
    P = texture2DRect(tex, tc + sIncr);
    MSE = P.r;
    if (MSE < bestMSE)
    {
        bestP = P;
        bestMSE = MSE;
    }
    
    P = texture2DRect(tex, tc + tIncr);
    MSE = P.r;
    if (MSE < bestMSE)
    {
        bestP = P;
        bestMSE = MSE;
    }
    
    P = texture2DRect(tex, tc + sIncr + tIncr);
    MSE = P.r;
    if (MSE < bestMSE)
    {
        bestP = P;
        bestMSE = MSE;
    }
    
    gl_FragData[0] = bestP;
}