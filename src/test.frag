uniform sampler2DRect imgTex;

void main() {
    gl_FragData[0] = 2.0 * texture2DRect(imgTex, gl_TexCoord[0].st);
}