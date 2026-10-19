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
| 3 | OpenMP | Memoria condivisa, speedup, race condition | completata |
| 4 | MPI a strisce + halo exchange | Memoria distribuita | completata |
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

## OpenMP (Tappa 3)

`src/life_omp.c` è la copia di `life_serial.c` con `#pragma omp parallel for`
sul ciclo esterno (righe) di `step`. Compilazione: `make life_omp`
(`-fopenmp`). Thread controllati con `OMP_NUM_THREADS`; misure con
`./scripts/bench.sh ./life_omp <thread>` (minimo di 5 ripetizioni, 100 passi).
Le celle vive coincidono con il riferimento in tutte le configurazioni.

La macchina ha 8 thread hardware su 4 core fisici (hyper-threading).

Tempo (s):

| n | seriale | 1 thread | 2 | 4 | 8 |
|---|---|---|---|---|---|
| 512 | 0.116 | 0.123 | 0.064 | 0.040 | 0.034 |
| 1024 | 0.471 | 0.483 | 0.288 | 0.179 | 0.135 |
| 2048 | 1.943 | 2.133 | 1.139 | 0.755 | 0.629 |
| 4096 | 8.504 | 8.752 | 5.363 | 3.701 | 2.902 |

Speedup (tempo seriale / tempo parallelo):

| n | 2 thread | 4 | 8 |
|---|---|---|---|
| 512 | 1.81× | 2.90× | 3.41× |
| 1024 | 1.64× | 2.63× | 3.49× |
| 2048 | 1.71× | 2.57× | 3.09× |
| 4096 | 1.59× | 2.30× | 2.93× |

Efficienza (speedup / numero di thread):

| n | 2 thread | 4 | 8 |
|---|---|---|---|
| 512 | 90% | 73% | 43% |
| 1024 | 82% | 66% | 44% |
| 2048 | 85% | 64% | 39% |
| 4096 | 79% | 57% | 37% |

Osservazioni:

- Con 1 thread OpenMP è circa uguale al seriale (3-9% più lento): è il costo
  della parte OpenMP.
- Lo speedup massimo è circa 3.5× con 8 thread. L'efficienza cala all'aumentare
  dei thread; oltre i 4 core fisici i thread aggiuntivi aiutano poco.
- Lo speedup peggiora con `n` grande: con griglie piccole i dati stanno nelle
  cache, con `n = 4096` (32 MB) si legge dalla RAM, la cui banda è condivisa
  tra i core. Il problema diventa *memory-bound*.
- Con più thread dei thread hardware (16) il tempo peggiora (misura singola su
  `n = 1024`: 0.151 s contro 0.119 s con 8).

### Race condition: il conteggio delle celle vive

Il conteggio finale `alive += cur[k]` in `main` è un ciclo che si può
parallelizzare, ma `alive` è dichiarata fuori dalla regione parallela, quindi
è **condivisa**: tutti i thread la modificano. (In `step`, invece, le variabili
`up`, `down`, `left`, `right` e `alive` sono dichiarate dentro il ciclo, quindi
sono private di ogni thread e non ci sono conflitti: ogni thread scrive righe
diverse di `next` e legge solo `cur`.)

Esperimento (`n = 1024`, 100 passi, 8 thread, 5 esecuzioni):

| Versione | `vive` | Corretto? |
|---|---|---|
| `#pragma omp parallel for` (senza protezione) | 12275, 12358, 24930, 24639, 12364 | No, cambia a ogni esecuzione |
| `#pragma omp parallel for reduction(+:alive)` | 100277 ×5 | Sì |

Senza protezione gli aggiornamenti si sovrascrivono a vicenda e il risultato
si riduce a circa un ottavo o un quarto del valore corretto, senza alcun errore
o warning: la regola d'oro (stesso numero di celle vive del seriale) serve a
scoprirlo. Con `reduction(+:alive)` ogni thread somma su una copia privata e
le copie vengono combinate alla fine, in modo sicuro.

## MPI a strisce di righe (Tappa 4)

`src/life_mpi.c`: la griglia è divisa in strisce di `n / nprocs` righe, una per
processo (`n` deve essere divisibile per il numero di processi). Ogni processo
tiene le sue righe più due righe di *halo* (sopra e sotto). A ogni passo i
processi si scambiano le righe di confine con due `MPI_Sendrecv` (prima riga
verso `up`, ultima verso `down`; `up` e `down` sono vicini in cerchio, quindi il
toroide verticale è gestito dallo scambio), poi ognuno calcola le sue righe. La
griglia iniziale usa la stessa sequenza di `rand()` del seriale (ogni processo
scarta i numeri delle righe dei processi precedenti). Il totale delle celle vive
si ottiene con `MPI_Reduce`.

Compilazione: `mpicc -O2 -o life_mpi src/life_mpi.c`. Esecuzione:
`mpirun --use-hwthread-cpus -np 4 ./life_mpi 1024 100` (l'opzione serve per
usare gli 8 thread hardware come slot). Misure:
`LAUNCHER="mpirun --use-hwthread-cpus -np 4" ./scripts/bench.sh ./life_mpi`.
Le celle vive coincidono con il riferimento con 1, 2, 4 e 8 processi.

### Confronto seriale / OpenMP / MPI

Tutte le misure nella stessa sessione, 100 passi, minimo di 5 ripetizioni.

Tempo (s):

| n | seriale | OMP 1 | OMP 2 | OMP 4 | OMP 8 | MPI 1 | MPI 2 | MPI 4 | MPI 8 |
|---|---|---|---|---|---|---|---|---|---|
| 512 | 0.120 | 0.112 | 0.062 | 0.049 | 0.034 | 0.115 | 0.052 | 0.036 | 0.027 |
| 1024 | 0.484 | 0.456 | 0.264 | 0.201 | 0.137 | 0.470 | 0.258 | 0.188 | 0.121 |
| 2048 | 1.977 | 1.941 | 1.111 | 0.836 | 0.594 | 1.904 | 1.207 | 0.922 | 0.591 |
| 4096 | 8.266 | 7.920 | 4.952 | 3.458 | 2.408 | 8.001 | 5.014 | 3.734 | 2.640 |

Speedup (tempo seriale / tempo parallelo):

| n | OMP 2 | OMP 4 | OMP 8 | MPI 2 | MPI 4 | MPI 8 |
|---|---|---|---|---|---|---|
| 512 | 1.94× | 2.45× | 3.53× | 2.31× | 3.33× | 4.44× |
| 1024 | 1.83× | 2.41× | 3.53× | 1.88× | 2.57× | 4.00× |
| 2048 | 1.78× | 2.36× | 3.33× | 1.64× | 2.14× | 3.35× |
| 4096 | 1.67× | 2.39× | 3.43× | 1.65× | 2.21× | 3.13× |

Efficienza (speedup / numero di thread o processi):

| n | OMP 2 | OMP 4 | OMP 8 | MPI 2 | MPI 4 | MPI 8 |
|---|---|---|---|---|---|---|
| 512 | 97% | 61% | 44% | 116% | 83% | 56% |
| 1024 | 92% | 60% | 44% | 94% | 64% | 50% |
| 2048 | 89% | 59% | 42% | 82% | 54% | 42% |
| 4096 | 84% | 60% | 43% | 83% | 55% | 39% |

Osservazioni:

- OpenMP ha uno speedup molto regolare al variare di `n` (circa 2.4× a 4 thread,
  3.5× a 8). MPI è migliore per griglie piccole (4.4× a 8 processi per
  `n = 512`) e un po' peggiore per griglie grandi (3.1× contro 3.4× a
  `n = 4096`).
- Il tetto è simile per entrambi (circa 3-4.5×): 4 core fisici con 8 thread
  hardware e memoria condivisa. L'efficienza cala oltre i 4 core.
- Con 1 unità MPI è circa uguale al seriale; la causa del peggioramento di MPI
  per `n` grandi non è stata verificata.
- Le differenze del 5% tra i due modelli sono nell'errore di misura.
- Nei risultati di `bench.sh` per MPI il campo `threads=1` è solo il parametro
  dello script e non riguarda i processi (indicati in `launcher`).

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
