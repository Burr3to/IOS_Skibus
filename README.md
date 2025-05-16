# Skibus - Synchronizácia (IOS 2)

1.  Úvod & Zadanie Úlohy

    -   **Názov projektu:** Skibus - Synchronizácia
    -   **Inšpirácia:** Problém "The Senate Bus problem" z knihy Allena B. Downeyho
    -   **Hlavný cieľ:** Implementovať simuláciu systému skibusu a lyžiarov s dôrazom na synchronizáciu procesov v jazyku C.
    -   **Použité primitíva:** Semafory a zdieľaná pamäť.

    **Typy procesov:**

    -   Hlavný proces (Parent)
    -   Proces Skibus (Bus)
    -   Proces Lyžiar (Skier)

2.  Popis Úlohy

    **Scenario:** Lyžiari po raňajkách prichádzajú na nástupné zastávky, čakajú na skibus.

    **Skibus:** Jazdí postupne po nástupných zastávkach.
    -   Na zastávke: Nakladá čakajúcich lyžiarov do kapacity autobusu.
    -   Po prejdení všetkých nástupných zastávok: Dovezie lyžiarov na finálnu výstupnú zastávku pri lanovke.
    -   Na finálnej: Všetci lyžiari vystúpia.
    -   Ak sú stále lyžiari čakajúci na zastávkach: Skibus začína ďalšie kolo.
    -   Ak už nikto nečaká ani nepríde: Skibus končí.

    **Lyžiar:**
    -   Dojí raňajky (simulované čakaním).
    -   Príde na náhodne pridelenú nástupnú zastávku.
    -   Čaká na skibus.
    -   Nastúpi (ak je miesto).
    -   Počká na finálnu zastávku.
    -   Vystúpi ("ide lyžovať").

    **Kľúčové body:**
    -   Správne riadenie nastupovania/vystupovania do/z autobusu (kapacita).
    -   Žiadne aktívne čakanie – lyžiari aj bus musia čakať na seba pomocou semaforov.
    -   Logovanie všetkých dôležitých akcií do súboru s poradovým číslom.

3.  Vstupné Parametre

    Program sa spúšťa s 5 argumentmi:
    `./proj2 L Z K TL TB`

    -   **L:** Počet lyžiarov (< 20000)
    -   **Z:** Počet nástupných zastávok (0 < Z <= 10)
    -   **K:** Kapacita skibusu (10 <= K <= 100)
    -   **TL:** Max. čas čakania lyžiara pred príchodom na zastávku (v mikrosekundách, 0 <= TL <= 10000)
    -   **TB:** Max. čas jazdy busu medzi zastávkami (v mikrosekundách, 0 <= TB <= 1000)

4.  Príklad Scenára & Výstupu

    **Príklad vstupu:** `./proj2 8 4 10 4 5`
    (8 lyžiarov, 4 zastávky, kapacita 10, max. 4ms raňajky, max. 5ms jazda)

    **Popis priebehu:** Lyžiari (8) začnú, niektorí prídu na zastávky (napr. 2, 4, 2, ...). Bus štartuje, ide na zastávku 1 (príde, odíde - zatiaľ nikto). Ide na zastávku 2 (príde), kde už čaká viacero lyžiarov. Kapacita je 10, všetkých 7 čakajúcich na zastávke 2 nastúpi (7 < 10). Bus odíde zo zástavky 2. Ide na zastávku 3 (príde, odíde - nikto nečaká). Ide na zastávku 4 (príde), kde čaká 1 lyžiar. Ten nastúpi (7+1=8 < 10). Bus odíde zo zastávky 4. Bus ide na finálnu (príde). Všetkých 8 lyžiarov vystúpi ("ide lyžovať"). Bus odíde z finálnej. Keďže všetkých 8 lyžiarov už skončilo, bus vypíše "finish" a program končí.

    **Fragment príkladového výstupu (proj2.out):**
    ```text
    1: L 1: started
    ... (ďalší lyžiari štartujú a prichádzajú na zastávky)
    15: BUS: started
    ... (bus jazdí, prichádza na zastávky)
    20: BUS: arived to 2
    21: L 1: boarding
    22: L 2: boarding
    ... (ďalší lyžiari na zástavke 2 nastupujú)
    28: BUS: leaving 2
    ... (bus ide ďalej)
    31: BUS: arived to 4
    32: L 3: boarding
    33: BUS: leaving 4
    34: BUS: arived to final
    35: L 2: going to ski
    ... (všetci lyžiari vystupujú)
    43: BUS: leaving final
    44: BUS: finish
    ```
    

(Čísla akcií a poradie sa môžu líšiť v závislosti od načasovania procesov a náhody)

1. Implementácia

    -   **Jazyk:** C
    -   **Procesy:** Používa sa `fork()` na vytvorenie jedného procesu pre skibus a L procesov pre lyžiarov. Hlavný proces iba inicializuje a čaká.
    -   **Zdieľaná pamäť:** Použitá pre zdieľanú štruktúru `SharedData`.
        -   **Obsahuje:**
            -   počty čakajúcich lyžiarov na každej zastávke (`skiersWaiting`)
            -   aktuálnu zastávku busu
            -   počet lyžiarov v buse (`skiersOnBus`)
            -   globálne poradové číslo akcie pre logovanie (`order`)
            -   počty skončených lyžiarov (`left`, `started`)
        -   Alokované pomocou `mmap` s `MAP_SHARED`.
    -   **Synchronizácia (Semafory):** Používajú sa pomenované semafory (`sem_open`, `sem_wait`, `sem_post`, `sem_close`, `sem_unlink`).
        -   **`mutex`**: Binárny semafor na ochranu kritických sekcií – prístup k zdieľanej pamäti a zápis do výstupného súboru, aby akcie boli logované správne a v poradí.
        -   **`Stop[11]`**: Pole semaforov (jeden pre každú zastávku + index 0 nepoužitý). Lyžiar `sem_waituje` na semafore svojej zastávky. Bus `sem_postuje` na semafore aktuálnej zastávky pre každého lyžiara, ktorého chce naložiť (podľa kapacity a počtu čakajúcich).
        -   **`bus_ReadyToUnBoard`**: Semafór, na ktorom lyžiari čakajú pred vystúpením. Bus `sem_postuje` na tento semafor pre každého lyžiara, ktorý je v buse, keď príde na finálnu.
        -   **`boarded`**: Semafór, na ktorom bus čaká `n` krát (počet lyžiarov, ktorých sa pokúša naložiť) po tom, čo "povolia" nástup na `Stop[currentStop]`. Lyžiari `sem_postujú` na tento semafor po tom, čo sa "nalodia" (teda po zápise "boarding").
        -   **`gotOff`**: Semafór, na ktorom bus čaká `exiting` krát (počet lyžiarov v buse) po tom, čo povolí vystúpenie na `bus_ReadyToUnBoard`. Lyžiari `sem_postujú` na tento semafor po tom, čo "vystúpia" (po zápise "going to ski").
    -   **Logovanie:** Všetky procesy zapisujú do jediného súboru `proj2.out`. Použitie `mutex` semaforu zaisťuje, že zápisy do súboru sú serializované a globálne poradové číslo (`shared_data->order`) je inkrementované atómovo pre každý výpis. Buffering súboru je vypnutý (`setbuf(file, NULL)`).
    -   **Čakanie:** Na simuláciu času raňajok lyžiara a jazdy busu sa používa `usleep()` s náhodným časom v zadanom rozsahu. Pre synchronizačné čakanie procesov sa používajú výhradne semafory (`sem_wait`).
    -   **Ošetrenie chýb a uvoľnenie zdrojov:**
        -   Validácia vstupných argumentov je prvá vec, ktorá sa robí.
        -   V prípade chyby pri `fork`, `sem_open`, `mmap` program vypíše chybové hlásenie na `stderr`, uvoľní dovtedy alokované zdroje (semafory, zdieľaná pamäť) a skončí s návratovým kódom 1.
        -   Na konci behu programu (po skončení všetkých procesov) hlavný proces uvoľní semafory (`sem_close`, `sem_unlink`) a zdieľanú pamäť (`munmap`).
