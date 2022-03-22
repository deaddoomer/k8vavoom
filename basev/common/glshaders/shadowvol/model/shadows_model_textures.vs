#version 120
$include "common/common.inc"
//#define VV_DEBUG_NORMALS

attribute vec3 Position;

uniform mat4 ModelToWorldMat;
uniform float Inter;

attribute vec4 Vert2;
attribute vec2 TexCoord;

varying vec2 TextureCoordinate;


#ifdef VV_DEBUG_NORMALS
uniform mat3 NormalToWorldMat;
attribute vec3 Vert2Normal;
varying vec3 Normal;
#endif


void main () {
  vec4 Vert = mix(vec4(Position, 1.0), Vert2, Inter)*ModelToWorldMat;
  gl_Position = gl_ModelViewProjectionMatrix*Vert;

  TextureCoordinate = TexCoord;

#ifdef VV_DEBUG_NORMALS
  Normal = Vert2Normal*NormalToWorldMat;
  //Normal = NormalToWorldMat*Vert2Normal;
#endif
}
