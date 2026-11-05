import pandas as pd
import joblib
from sklearn.model_selection import train_test_split
from sklearn.tree import DecisionTreeClassifier, export_text
from sklearn.metrics import accuracy_score, confusion_matrix, classification_report

# 1. Baca dataset
df = pd.read_csv("dataset.csv")
print("Jumlah data:", len(df))
print(df["label"].value_counts())

# 2. Pisahkan fitur (A) dan label (B)
FITUR = ["debit_pipa", "level_cm", "laju_turun_level", "selisih_neraca"]
X = df[FITUR]
y = df["label"]

# 3. Bagi data: 70% latih, 15% validasi, 15% uji
X_train, X_temp, y_train, y_temp = train_test_split(
    X, y, test_size=0.30, random_state=42, stratify=y)
X_val, X_test, y_val, y_test = train_test_split(
    X_temp, y_temp, test_size=0.50, random_state=42, stratify=y_temp)

# 4. Latih Decision Tree
model = DecisionTreeClassifier(max_depth=3, random_state=42)
model.fit(X_train, y_train)

# 5. Evaluasi
print("\nAkurasi validasi:", accuracy_score(y_val, model.predict(X_val)))
pred_test = model.predict(X_test)
print("Akurasi uji     :", accuracy_score(y_test, pred_test))

print("\nConfusion matrix (uji):")
print(confusion_matrix(y_test, pred_test, labels=["AMAN", "WASPADA", "BOCOR"]))
print("\nLaporan klasifikasi:")
print(classification_report(y_test, pred_test))

# 6. Tampilkan aturan yang dipelajari model
print("\nAturan pohon keputusan:")
print(export_text(model, feature_names=FITUR))

# 7. Simpan model untuk dipakai di langkah berikutnya
joblib.dump(model, "model_pipahemat.pkl")
print("Model disimpan: model_pipahemat.pkl")

# 8. Coba prediksi satu data baru
contoh = pd.DataFrame([[5.0, 70, 3.0, 2.5]], columns=FITUR)
print("\nPrediksi contoh:", model.predict(contoh)[0])