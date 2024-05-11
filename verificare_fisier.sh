#!/bin/bash

# Verificare nr de argumente
if [ $# -lt 1 ]; then
  echo "nr incorect de argumente la executia scriptului"
  exit 1
fi

FISIER_VERIFICAT=$1

# Verificare existenta fisier infectat
if [ ! -f "$FISIER_VERIFICAT" ]; then
  echo "FISIERUL POSIBIL INFECTAT NU EXISTA"
  exit 1
fi

# Adaug drept de citire pt fisierul verificat
chmod 400 $FISIER_VERIFICAT

# Contorizare linii, cuvinte si caractere
LINII=$(wc -l < "$FISIER_VERIFICAT")
CUVINTE=$(wc -w < "$FISIER_VERIFICAT")
CARACTERE=$(wc -m < "$FISIER_VERIFICAT")

# Verifica daca ultima linie din fisier se termina cu un newline
if [ "$(tail -c 1 "$FISIER_VERIFICAT")" != "" ]; then
  LINII=$((LINII+1))
fi

# Cautare cuvinte cheie si caractere non-ASCII
continut_infectat=$(grep -P -io "corrupted|dangerous|risk|attack|malware|malicious|mallicious|[\x80-\xFF]" "$FISIER_VERIFICAT")

# Verifica daca fisierul e suspect prin metrici sau continut
if [ ! -z "$continut_infectat" ] || (( LINII < 3 && CUVINTE > 1000 && CARACTERE > 2000 )); then
    echo "Fisierul '$FISIER_VERIFICAT' este suspect."
    # Sterg drepturile de acces la fisier
    chmod 000 $FISIER_VERIFICAT
    exit 1  # Returnez 1 pentru fisier malitios
fi

# Daca nu sunt gasite semne de infectare
chmod 000 $FISIER_VERIFICAT  # Sterg drepturile de acces pentru siguranta
echo "SAFE"
exit 0  # Fisierul este curat
