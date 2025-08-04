vec3 a_position			: POSITION;
vec3 a_normal			: NORMAL;
vec2 a_texcoord0		: TEXCOORD0;
ivec4 a_indices          : BLENDINDICES0;
vec4 a_weight           : BLENDWEIGHT0;
vec4 a_color0			: COLOR0;   

vec4 v_color0     : COLOR0;    // For debug colored meshes
vec3 v_worldPos   : TEXCOORD1; // World-space position for lighting
vec3 v_normal     : TEXCOORD2; // Normal vector
vec2 v_uv0        : TEXCOORD0; // Primary UV channel
vec3 v_viewDir    : TEXCOORD3; // View direction
