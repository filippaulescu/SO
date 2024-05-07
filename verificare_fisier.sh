#!/bin/bash

# Verificare nr de argumente
if [ $# -lt 1 ]; then
  echo "nr incorect de argumente la executia scriptului"
  exit -1
fi

FISIER_VERIFICAT=$1

# Verificare existenta fisier infectat
if [ ! -f "$FISIER_VERIFICAT" ]; then
  echo "FISIERUL POSIBIL INFECAT NU EXISTA"
  exit -1
fi

#adaug drept de citire pt fisierul verificat
chmod 400 $FISIER_VERIFICAT

# Contorizare linii, cuvinte si caractere
LINII=$(wc -l < "$FISIER_VERIFICAT")
CUVINTE=$(wc -w < "$FISIER_VERIFICAT")
CARACTERE=$(wc -m < "$FISIER_VERIFICAT")

# Verifica daca ultima linie din fisier se termina cu un newline
if [ "$(tail -c 1 "$FISIER_VERIFICAT")" != "" ]; then
  LINII=$((LINII+1))
fi

echo "$LINII linii"

#Verificare daca fisierul e suspect
if (( LINII < 3 )) && (( CUVINTE > 1000)) && (( CARACTERE > 2000 )); then
    # Cautare cuvinte cheie si caractere non-ASCII
    continut_infectat=$(grep -P "corrupted|dangerous|risk|attack|malware|malicious|[\x80-\xFF]" "$FISIER_VERIFICAT")

    #sterg drepturile pt fisierul verificat
    chmod 000 $FISIER_VERIFICAT

    if [ ! -z "$continut_infectat" ]; then
    # Fisierul contine malware
        echo "$FISIER_VERIFICAT."
        exit 1
        #returnez 1 pentru fisier malitios
    fi
fi

#Fisierul este curat
echo "SAFE"

#returnez 0 pentru fisier curat
exit 0
