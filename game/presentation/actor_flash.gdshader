shader_type canvas_item;
uniform float power : hint_range(0.0, 1.0) = 0.0;
uniform vec4 flash_color : source_color = vec4(1.0);
varying vec4 actor_tint;

void vertex() {
	actor_tint = COLOR;
}

void fragment() {
	vec4 texel = texture(TEXTURE, UV);
	COLOR = vec4(mix(texel.rgb, flash_color.rgb, power), texel.a) * actor_tint;
}
