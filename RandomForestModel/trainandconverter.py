# =============================================================
# 🎯 ENTRAÎNEMENT MODÈLE OPTIMISÉ
# =============================================================

import pandas as pd
import numpy as np
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import StandardScaler
from sklearn. ensemble import RandomForestClassifier
from sklearn.metrics import classification_report, accuracy_score
import emlearn
import os

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
DATA_PATH = os.environ.get("DATA_PATH", os.path.join(BASE_DIR, "dataset_merged.parquet"))
OUTPUT_DIR = os.environ.get("OUTPUT_DIR", BASE_DIR)

print("📂 Chargement...")
if not os.path.exists(DATA_PATH):
    raise FileNotFoundError(f"Dataset not found: {DATA_PATH}")
df = pd.read_parquet(DATA_PATH)
print(f"✅ {len(df):,} lignes")

# =============================================================
# 🔧 FEATURE ENGINEERING - Ajouter des features discriminantes
# =============================================================
print("\n🔧 Feature Engineering...")

# Magnitude de l'accélération
df['acc_mag'] = np.sqrt(df['ax']**2 + df['ay']**2 + df['az']**2)

# Magnitude du gyroscope
df['gyro_mag'] = np.sqrt(df['gx']**2 + df['gy']**2 + df['gz']**2)

# Ratio accélération
df['acc_ratio'] = df['ax'] / (df['ay']. abs() + 1)

print("✅ Features ajoutées")

# =============================================================
# 📊 PRÉPARER LES DONNÉES
# =============================================================
features = ['ax', 'ay', 'az', 'gx', 'gy', 'gz', 'age', 'acc_mag', 'gyro_mag', 'acc_ratio']
X = df[features]. values
y = df['label']. values

X_train, X_test, y_train, y_test = train_test_split(
    X, y, test_size=0.2, random_state=42, stratify=y
)

scaler = StandardScaler()
X_train = scaler.fit_transform(X_train)
X_test = scaler.transform(X_test)

print(f"✅ Train:  {len(X_train):,} | Test: {len(X_test):,}")

# =============================================================
# 🌲 ENTRAÎNER MODÈLE
# =============================================================
print("\n🌲 Entraînement Random Forest...")

model = RandomForestClassifier(
    n_estimators=30,
    max_depth=15,
    min_samples_leaf=10,
    random_state=42,
    n_jobs=-1,
    class_weight='balanced'  # Important pour équilibrer les classes
)

model.fit(X_train, y_train)

y_pred = model.predict(X_test)
accuracy = accuracy_score(y_test, y_pred)

print(f"\n🎯 ACCURACY: {accuracy * 100:.2f}%")
print(classification_report(y_test, y_pred, target_names=['Idle', 'Walking', 'Falling']))

# =============================================================
# 🔄 CONVERTIR EN C
# =============================================================
print("\n🔄 Conversion en C...")

cmodel = emlearn.convert(model, method='inline')
cmodel.save(file=os.path.join(OUTPUT_DIR, "fall_detection_model.h"), name='fall_model')

# =============================================================
# 💾 NOUVEAU SCALER
# =============================================================
print("💾 Génération scaler...")

with open(os.path.join(OUTPUT_DIR, "scaler_config.h"), "w") as f:
    f.write("#ifndef SCALER_CONFIG_H\n")
    f.write("#define SCALER_CONFIG_H\n\n")
    f.write(f"#define NUM_FEATURES {len(features)}\n\n")
    f.write(f"const float MEAN[NUM_FEATURES] = {{{', '.join(f'{x:.6f}f' for x in scaler.mean_)}}};\n\n")
    f.write(f"const float STD[NUM_FEATURES] = {{{', '.join(f'{x:.6f}f' for x in scaler.scale_)}}};\n\n")
    f.write("void normalize(float* input, float* output) {\n")
    f.write("    for (int i = 0; i < NUM_FEATURES; i++) {\n")
    f.write("        output[i] = (input[i] - MEAN[i]) / STD[i];\n")
    f.write("    }\n")
    f.write("}\n\n")
    f.write("#endif\n")

print("\n✅ FICHIERS GÉNÉRÉS:")
print("   - fall_detection_model.h")
print("   - scaler_config.h")
