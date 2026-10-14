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
| 0 | Setup: Git, gcc, Makefile | Ambiente e strumenti | completata |
| 1 | Versione seriale | Riferimento di correttezza | completata |
| 2 | Misure e ottimizzazione seriale | Cache, profiling | completata |
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

### Ottimizzazione seriale

Eliminati i `%` dal ciclo interno di `step` (sostituiti da confronti con
l'operatore ternario). Tempi in secondi, minimo di 5 ripetizioni; le celle vive
restano identiche al riferimento.

| n | baseline (s) | senza `%` (s) | speedup |
|---|---|---|---|
| 512 | 0.153 | 0.115 | 1.33× |
| 1024 | 0.624 | 0.469 | 1.33× |
| 2048 | 2.617 | 1.956 | 1.34× |
| 4096 | 10.914 | 8.104 | 1.35× |

Baseline e versione ottimizzata sono state misurate in sessioni diverse, ma il
rapporto è stabile su tutte le dimensioni.

### Effetto della cache: ordine dei cicli

Esperimento: stesso `step`, ma con i cicli scambiati (colonne all'esterno, righe
all'interno), cioè leggendo la griglia a salti di `n` byte invece che in
sequenza. Una sola esecuzione per misura, 100 passi, `-O2`. Le celle vive sono
identiche, cambia solo il tempo.

| n | normale (s) | scambiato (s) | rallentamento | ns/cella: normale → scambiato |
|---|---|---|---|---|
| 1024 | 0.472 | 0.655 | 1.4× | 4.5 → 6.2 |
| 2048 | 1.985 | 4.075 | 2.1× | 4.7 → 9.7 |
| 4096 | 8.033 | 49.444 | 6.2× | 4.8 → 29.5 |

Conclusioni:

- La griglia è salvata per righe (*row-major*): le celle di una riga sono
  consecutive in memoria. La RAM trasferisce blocchi da 64 byte (*cache line*),
  quindi con il ciclo interno sulle colonne una lettura porta in cache 64 celle,
  e il prefetcher anticipa i blocchi successivi.
- Con i cicli scambiati ogni lettura cade in una cache line diversa, e le righe
  non restano in cache abbastanza a lungo da essere riusate: il costo per cella
  cresce con `n`, mentre nella versione normale resta circa costante.
- Per `n = 4096` il passo tra una riga e l'altra è 4096 byte (potenza di due),
  probabile causa dell'ulteriore peggioramento.
- Cache del PC di sviluppo (per core): L1d 48 KiB, L2 1.25 MiB, L3 12 MiB
  condivisa. Le tre righe che servono a calcolare una riga (12 KB per
  `n = 4096`) stanno in L1.
- Regola per le tappe successive: l'ordine di accesso alla memoria conta quanto
  il numero di operazioni.

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
