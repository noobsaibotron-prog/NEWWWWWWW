import tensorflow as tf
import os

# Simple dense model for problem detection (8 outputs), input size 2048
num_bins = 2048
num_problems = 8

model = tf.keras.Sequential([
    tf.keras.layers.Input(shape=(num_bins,)),
    tf.keras.layers.Dense(128, activation='relu'),
    tf.keras.layers.Dense(64, activation='relu'),
    tf.keras.layers.Dense(num_problems, activation='sigmoid'),
])

converter = tf.lite.TFLiteConverter.from_keras_model(model)
tflite_model = converter.convert()

os.makedirs("models", exist_ok=True)
out_path = os.path.join("models", "problem_detection.tflite")
with open(out_path, "wb") as f:
    f.write(tflite_model)

print(f"Test model written to {out_path}")

