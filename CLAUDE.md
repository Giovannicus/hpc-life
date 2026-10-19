# CLAUDE.md

## Progetto
`hpc-life`: progetto didattico di HPC. Implementazione del Game of Life
(automa cellulare) in versioni sempre più parallele, per imparare HPC
passo dopo passo. Come estensione finale: una versione "falling sand"
con rendering grafico.

## Come lavorare con me
- Sono un principiante in HPC: spiega i concetti nuovi in modo breve e chiaro.
- Quando lanci un comando che non ho mai visto, spiega cosa fa in una riga.
- Risposte concise, niente lunghi preamboli. Rispondi in italiano.
- Procedi una tappa alla volta: non anticipare le tappe successive.
- Prima di modificare molti file, dimmi cosa stai per fare.
- Nel codice tutto in inglese

## Scelte già fatte
- Linguaggio: **C** per il nucleo del calcolo. **Python** solo per script,
  grafici (matplotlib) e visualizzazione.
- Ambiente: bash nel terminale di Cursor, compilatore gcc, MPI con OpenMPI
  (`mpicc`, `mpirun`).
- Git: nome ed email configurati **solo in locale** (senza `--global`).
- Branch principale: `main`. Un **tag Git per ogni tappa** completata
  (es. `tappa-1-seriale`).
- Griglia toroidale (bordi periodici), cella viva = 1, morta = 0.

## Struttura della repo
```
hpc-life/
  src/        codice C (life_serial.c, life_omp.c, life_mpi.c, ...)
  scripts/    script Python e SLURM (grafici, benchmark)
  results/    misure e grafici finali
  README.md
  CLAUDE.md
  .gitignore  (eseguibili, *.o, file di output pesanti)
```

## Regola d'oro della correttezza
`src/life_serial.c` è la versione di **riferimento**. Con seed fisso
(`srand(0)`, riempimento 30%) tutte le versioni parallele devono dare lo
**stesso numero di celle vive finale** a parità di `n` e `steps`.
Il valore di riferimento per `./life_serial 1024 100` va annotato nel README.

## Roadmap
0. Setup: repo Git, gcc, Makefile
1. Game of Life seriale (`src/life_serial.c`)
2. Misure e ottimizzazione seriale: cache, profiling, eliminare i `%` nel ciclo
3. OpenMP: memoria condivisa, speedup, race condition
4. MPI a strisce di righe + halo exchange (`MPI_Sendrecv`)
5. MPI 2D con topologia cartesiana (`MPI_Cart_create`), overlap
   calcolo/comunicazione (`MPI_Isend`/`MPI_Irecv`)
6. Ibrido MPI+OpenMP, analisi di strong e weak scaling con grafici
7. Estensione "gioco": falling sand + rendering (raylib o SDL2)

## Stato attuale
- [x] Repo locale creata, utente configurato in locale
- [x] Tappa 1: `life_serial` compilato ed eseguito, tempi e celle vive annotati
  nel README per n = 512, 1024, 2048, 4096 (100 passi)
- [x] Primo commit e collegamento a GitHub (tag `tappa-1-seriale` non creato,
  scelta dell'utente)
- [x] Tappa 2 (completata):
  - [x] `scripts/bench.sh` + `make bench` (5 ripetizioni, tempo minimo,
    controllo celle vive)
  - [x] eliminati i `%` dal ciclo interno di `step` (speedup ~1.34x)
  - [x] cache (esperimento cicli scambiati: fino a 6x più lento a n=4096)
  - [x] profiling (gprof, `make profile`)
- [x] Tappa 3 (completata):
  - [x] `src/life_omp.c` con `parallel for` su `step`, scaling 1/2/4/8 thread
    (speedup max ~3.5x), tabelle nel README
  - [x] race condition: conteggio finale delle celle vive, sbagliato senza e
    corretto con `reduction(+:alive)`
- [x] Tappa 4 (completata):
  - [x] `src/life_mpi.c`: strisce di righe, halo exchange con `MPI_Sendrecv`,
    `MPI_Reduce` per le celle vive; corretto con 1, 2, 4, 8 processi
  - [x] `bench.sh` con variabile `LAUNCHER`; confronto OMP/MPI nel README
  - [x] `life_mpi.c` pulito: stampa finale solo dal rank 0, `MPI_Barrier`
    prima di `start`, `MPI_Finalize` prima dei `return 1`
  - [x] Makefile: regole `life_mpi`, `multi_process`, `compare`

## Comandi utili
```bash
gcc -O2 -o life_serial src/life_serial.c     # compila
./life_serial 1024 100                        # esegue: n=1024, 100 passi
mpicc -O2 -o life_mpi src/life_mpi.c          # compila MPI
mpirun -np 4 ./life_mpi 1024 100              # esegue con 4 processi
```

## Codice della Tappa 1
Se `src/life_serial.c` non esiste ancora, crealo: griglia n x n di
`unsigned char`, due buffer `cur`/`next` scambiati a ogni passo, vicini
calcolati con modulo (bordi toroidali), regola: nasce con 3 vicini vivi,
sopravvive con 2 o 3. Argomenti da riga di comando: `n` (default 1024) e
`steps` (default 100). Stampa una sola riga:
`n=... steps=... tempo=...s vive=... ns_per_cella=...`
(campi separati da spazi: `$3` tempo, `$4` vive; `bench.sh` li legge così).
