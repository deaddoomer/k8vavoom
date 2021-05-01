#version 120
$include "common/common.inc"

$include "common/texlmap_vars.vs"


void main () {
  // transforming the vertex
  gl_Position = gl_ModelViewProjectionMatrix*gl_Vertex;
  // pass texture coordinates
  $include "common/texlmap_calc.vs"
}
