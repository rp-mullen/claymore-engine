vec3 a_position	   : POSITION;
vec3 a_normal			: NORMAL;
vec4 a_texcoord0		: TEXCOORD0; // Supports uv.xy plus extra components (e.g., blend, angle)
vec4 a_texcoord1		: TEXCOORD1 = vec4(0.0, 0.0, 0.0, 0.0);
ivec4 a_indices      : BLENDINDICES0;
vec4 a_weight        : BLENDWEIGHT0;
vec4 a_color0			: COLOR0;   

vec4 v_color0     : COLOR0;    // For debug colored meshes
vec3 v_worldPos   : TEXCOORD1; // World-space position for lighting
vec3 v_normal     : TEXCOORD2; // Normal vector
vec3 v_viewDir    : TEXCOORD3; // View direction
vec4 v_texcoord1  : TEXCOORD4; // Secondary UV channel
vec3 v_position   : TEXCOORD5; // Position in object space

// Primary UV/texcoord channel. Use .xy for UVs; .zw are available for extra data.
vec4 v_texcoord0  : TEXCOORD0;