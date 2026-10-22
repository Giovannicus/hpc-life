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
- Tutto va documentato: ogni tappa ha codice corretto (celle vive uguali al
  riferimento), misure in `results/benchmarks.csv` e una sezione nel README,
  perché alla fine ci sarà un confronto finale tra tutte le versioni.

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
- [x] Tappa 5 (completata):
  - [x] `src/life_mpi_2d.c`: blocchi con `MPI_Cart_create`, halo con
    `MPI_Sendrecv` (colonne con `MPI_Type_vector`); corretto con 1, 2, 4, 8
    processi
  - [x] `src/life_mpi_2d_overlap.c`: 8 scambi `MPI_Isend`/`MPI_Irecv` (lati e
    angoli, con tag), calcolo dell'interno prima di `MPI_Waitall`; corretto
  - [x] `results/benchmarks.csv`: `bench.sh` accoda ogni misura (variabili
    `TAPPA`, `PROCS`); misure di Tappa 1-5 raccolte
  - [x] Risultato: il 2D è più veloce delle strisce (parte del merito è di
    `step` senza ternari); l'overlap non dà un guadagno misurabile su una sola
    macchina, e le misure a 8 processi sono rumorose (vedi README)
  - [x] Makefile: regole `life_mpi_2d`, `multi_process_2d`,
    `life_mpi_2d_overlap`, `multi_process_2d_overlap`
- [x] Tappa 6 (completata):
  - [x] `src/life_hybrid.c`: `life_mpi_2d.c` + OpenMP in `step` e nel conteggio,
    `MPI_Init_thread` (FUNNELED); corretto con 1x1, 4x2, 2x4, 1x8
  - [x] misure con `mpirun --use-hwthread-cpus --bind-to none` (il binding di
    default lega 1-2 processi a un core e rallenta i thread di 3x)
  - [x] strong scaling (n = 2048, 4096) e weak scaling (n = 1024, 1448, 2048,
    2896) nel CSV e nel README; `bench.sh` accetta `SIZES`
  - [x] `scripts/plot_results.py` per i grafici in `results/` (richiede
    matplotlib)
  - [x] commenti del codice tradotti in inglese
- [ ] Tappa 7: falling sand + rendering (in corso)
  - [x] 7a: `viewer/` (SDL2): `viewer.h`/`viewer.c` modulo di rendering e
    `life_view.c` (Life seriale in una finestra); `make life_view`
  - [ ] 7b: falling sand (`src/life_sand.c` per la logica, poi sul viewer)
  - [x] 7c: `viewer/life_view_hybrid.c`: viewer della versione ibrida (finestra
    solo sul rank 0, `MPI_Gather` dei blocchi, uscita sincronizzata con
    `MPI_Bcast`); `scripts/view_bench.sh` misura calcolo/raccolta/disegno per
    immagine in `results/view_benchmarks.csv`: il disegno domina (67-90%),
    la configurazione migliore è con pochi processi e molti thread
- [x] Confronto finale (`make final`, tappa `final` nel CSV): tutte le versioni
  in una sola sessione, tabelle e grafici nel README (`make plots`). A n = 4096
  e 8 unità: seriale 8.9 s, OpenMP 3.0 s, strisce 2.9 s, 2D 2.0 s, overlap
  2.0 s, ibrido 1.9 s (4.65x); overlap e ibrido non migliorano su un nodo

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
