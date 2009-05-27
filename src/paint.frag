uniform sampler2DRect imgTex;

uniform float w, h;

void main() {
    gl_FragData[0] = texture2DRect(imgTex, gl_TexCoord[0].st);
}