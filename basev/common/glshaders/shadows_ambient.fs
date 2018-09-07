#version 120

uniform vec4 Light;
uniform sampler2D Texture;

varying vec2 TextureCoordinate;


void main () {
  vec4 TexColour = texture2D(Texture, TextureCoordinate);
  if (TexColour.w <= 0.1) discard;
  gl_FragColor = Light;
}
