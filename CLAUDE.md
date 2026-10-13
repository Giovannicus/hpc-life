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
- [ ] Tappa 1: compilare ed eseguire `life_serial`, annotare tempi e celle vive
  per n = 512, 1024, 2048, 4096 (100 passi)
- [ ] Primo commit, collegamento a GitHub, tag `tappa-1-seriale`

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
`steps` (default 100). Stampa:
`n=... steps=... tempo=... s vive=...`.
