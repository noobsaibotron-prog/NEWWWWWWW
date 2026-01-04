# TensorFlow Lite Model Placement

Place your TFLite model here so the plugin can load it at runtime.

Expected path (relative to the executable/VST3):
- `models/problem_detection.tflite`

The code loads:
```
<exe_or_vst3_dir>/models/problem_detection.tflite
```

If the file is missing, the plugin falls back to the classical ML engine and logs a warning.

Quick checklist:
1) Put your `.tflite` model in this folder.
2) Ensure the TFLite runtime libraries are available and `AIEQ_ENABLE_TFLITE` is defined in your build.
3) Rebuild the plugin and check logs for “TFLite model loaded”.

