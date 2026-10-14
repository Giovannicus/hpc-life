# hpc-life

Progetto didattico di **High Performance Computing (HPC)**: il Game of Life
implementato in versioni sempre più parallele, per imparare passo dopo passo
come si programma e si misura un codice parallelo.

## Obiettivo

Partire da una versione seriale in C e arrivare a una versione ibrida
MPI+OpenMP, misurando a ogni tappa le prestazioni e spiegando i risultati.
Come estensione finale, una versione "falling sand" con rendering grafico.

## Il Game of Life in breve

Griglia 2D di celle vive (1) o morte (0). A ogni passo tutte le celle si
aggiornano in base ai loro 8 vicini:

- una cella viva con 2 o 3 vicini vivi sopravvive, altrimenti muore;
- una cella morta con esattamente 3 vicini vivi nasce.

Ogni cella dipende solo dai vicini: il calcolo è locale e regolare, quindi
ideale da parallelizzare.

## Roadmap

| Tappa | Contenuto | Concetto HPC | Stato |
|---|---|---|---|
| 0 | Setup: Git, gcc, Makefile | Ambiente e strumenti | in corso |
| 1 | Versione seriale | Riferimento di correttezza | completata |
| 2 | Misure e ottimizzazione seriale | Cache, profiling | |
| 3 | OpenMP | Memoria condivisa, speedup, race condition | |
| 4 | MPI a strisce + halo exchange | Memoria distribuita | |
| 5 | MPI 2D, overlap calcolo/comunicazione | Topologie cartesiane | |
| 6 | Ibrido MPI+OpenMP | Strong/weak scaling | |
| 7 | Falling sand + rendering | Estensione "gioco" | |

Ogni tappa completata ha un tag Git (es. `tappa-1-seriale`).

## Requisiti

- Linux/bash, `gcc`, `git`
- OpenMPI (`mpicc`, `mpirun`) dalla tappa 4
- Python 3 con `matplotlib` per i grafici (dalla tappa 2)

Su Ubuntu/Debian:

```bash
sudo apt install build-essential git openmpi-bin libopenmpi-dev
```

## Compilare ed eseguire

```bash
gcc -O2 -o life_serial src/life_serial.c
./life_serial 1024 100      # griglia 1024x1024, 100 passi
```

Output: `n=1024 steps=100 tempo=... s vive=...`

## Verifica della correttezza

Con seed fisso (`srand(0)`, riempimento al 30%), tutte le versioni devono dare
lo stesso numero di celle vive finale a parità di `n` e `steps`.

La versione seriale è **deterministica**: lanciata più volte con gli stessi
`n` e `steps` dà sempre lo stesso numero di celle vive (cambia solo il tempo).
È testata e funzionante, e fa da riferimento per tutte le versioni parallele.

| n | steps | celle vive (riferimento) |
|---|---|---|
| 512 | 100 | 25790 |
| 1024 | 100 | 100277 |
| 2048 | 100 | 397219 |
| 4096 | 100 | 1599426 |

Il valore di riferimento principale è `./life_serial 1024 100` → **100277**.
Il conteggio è identico in tutte le 5 ripetizioni di ogni `n`.

## Risultati

Tempi della versione seriale (`-O2`, 100 passi, minimo di 5 ripetizioni):

| n | tempo (s) | celle vive |
|---|---|---|
| 512 | 0.153 | 25790 |
| 1024 | 0.624 | 100277 |
| 2048 | 2.617 | 397219 |
| 4096 | 10.914 | 1599426 |

I tempi variano da una ripetizione all'altra e da una sessione all'altra,
per questo si usa il minimo. Le misure si rilanciano con `make bench`
(script `scripts/bench.sh`).

Grafici di speedup ed efficienza: verranno aggiunti in `results/`.

## Struttura

```
src/        codice C
scripts/    script Python e SLURM
results/    misure e grafici
```

## Risorse

- LLNL HPC Tutorials (OpenMP, MPI)
- mpitutorial.com
- Hager, Wellein: *Introduction to High Performance Computing for Scientists and Engineers*
