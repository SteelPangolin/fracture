uniform float w, h;

void main()
{
    gl_TexCoord[0] = vec4(gl_MultiTexCoord0.s * w, gl_MultiTexCoord0.t * h, 0.0, 1.0);
    vec2 scrn = gl_Vertex.xy * 2.0 - 1.0;
    gl_Position = vec4(scrn.x, scrn.y, 0.0, 1.0);
}
