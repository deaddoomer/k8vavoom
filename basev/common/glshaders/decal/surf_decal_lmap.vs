#version 120
$include "common/common.inc"

$include "common/texlmap_vars.vs"
$include "common/glow_vars.vs"


void main () {
  // transforming the vertex
  gl_Position = gl_ModelViewProjectionMatrix*gl_Vertex;
  // pass texture coordinates
  $include "common/texlmap_calc.vs"
  $include "common/glow_calc.vs"
}
