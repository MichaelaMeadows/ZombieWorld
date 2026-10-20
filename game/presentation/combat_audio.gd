class_name CombatAudio
extends RefCounted
## Original tiny sound palette; no external audio dependencies.
static func make(kind: String) -> AudioStreamWAV:
	var sample_rate := 22050
	var duration := 0.12 if kind == "swing" else 0.18
	var count := int(sample_rate * duration)
	var bytes := PackedByteArray()
	bytes.resize(count * 2)
	var rng := RandomNumberGenerator.new()
	rng.seed = 810
	for i in range(count):
		var t := float(i) / sample_rate
		var envelope := pow(1 - float(i) / count, 2)
		var noise := rng.randf_range(-1, 1)
		var value: float
		match kind:
			"swing": value = noise * 0.22 * sin(PI * float(i) / count)
			"block": value = sin(t * 2300) * 0.32 + sin(t * 3780) * 0.18 + noise * 0.1
			"heavy": value = sin(t * 420 * (1 - t)) * 0.45 + noise * 0.35
			"miss": value = noise * 0.09
			_: value = sin(t * 880) * 0.3 + noise * 0.32
		bytes.encode_s16(i * 2, int(clampf(value * envelope, -1, 1) * 24000))
	var wav := AudioStreamWAV.new()
	wav.format = AudioStreamWAV.FORMAT_16_BITS
	wav.mix_rate = sample_rate
	wav.data = bytes
	return wav
