const float epsilon = 0.0001;
uniform int originXMult;

uniform sampler2DRect sumD_sumD2_sumDr_tex;
uniform sampler2DRect sumR_sumR2_tex;

uniform float n;
uniform float r_i;
uniform float r_j;

void main()
{
    vec2 tc = gl_TexCoord[0].st;
    float packedOrigin = (tc.x * float(originXMult) + tc.y) * 2.0;
    
    vec4 PD = texture2DRect(sumD_sumD2_sumDr_tex, tc);
    float sumD  = PD.r;
    float sumD2 = PD.g;
    float sumDr = PD.b;
    
    vec4 PR = texture2DRect(sumR_sumR2_tex, vec2(r_i, r_j));
    float sumR  = PR.r;
    float sumR2 = PR.g;
    
    float S_lo = n * sumD2 + sumD * sumD;
    float S, O, squaredError;
    if (abs(S_lo) > epsilon)
    {
        float S_hi = n * sumDr + sumR * sumD;
        S = clamp(S_hi / S_lo, -1.0 + epsilon, 1.0 - epsilon);
        O = (sumR - S * sumD) / n;
        squaredError = S * (S * sumD2 + 2.0 * (O * sumD - sumDr));
    }
    else
    {
        S = 0.0;
        O = sumR / n;
        squaredError = 0.0;
    }
    squaredError += sumR2 + O * (n * O - 2.0 * sumR);
    float MSE = squaredError / n;
    
    gl_FragData[0] = vec4(MSE, S, O, packedOrigin);
}