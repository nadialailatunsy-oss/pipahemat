import os
import pandas as pd

DROP_MISMATCH = False   # True = buang baris yang label-nya beda dari status_firmware

df = pd.read_csv("dataset_asli.csv")
print("Total baris:", len(df))
print("\nJumlah per label:")
print(df["label"].value_counts())

print("\nLabel vs status_firmware:")
print(pd.crosstab(df["label"], df["status_firmware"]))

FITUR = ["debit_pipa", "level_cm", "laju_turun_level", "selisih_neraca"]
print("\nRata-rata & jumlah nilai unik per label:")
print(df.groupby("label")[FITUR].agg(["mean", "nunique"]).round(2).T)

if DROP_MISMATCH:
    df = df[df["label"] == df["status_firmware"]]

# seimbangkan: ambil jumlah sama di tiap kelas
n_min = df["label"].value_counts().min()
seimbang = pd.concat(
    [g.sample(n_min, random_state=42) for _, g in df.groupby("label")]
).sample(frac=1, random_state=42)

if os.path.exists("dataset.csv") and not os.path.exists("dataset_buatan.csv"):
    os.rename("dataset.csv", "dataset_buatan.csv")   # simpan data buatan lama

seimbang[FITUR + ["label"]].to_csv("dataset.csv", index=False)
print(f"\ndataset.csv dibuat: {n_min} baris x 3 kelas = {len(seimbang)} baris")