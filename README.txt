Balagiu Darian 334 CB   
TEMA 3 APD : Protocolul BitTorrent

Structuri de date folosite:

    Fiecare peer are cate o structura de tip "peer_info" in care isi retine fisierele pe care le detine si fisierele pe care le doreste.
Aceasta strctura se updateaza de fiecare data cand peer-ul descarca un chunk.
    Fiecare peer are o matrice de tip client_struct. Pe fiecare linie a acestei matrici se afla
swarm-ul descarcat de la Tracker al unui fisier pe care peer-ul si-l doreste.
    Structura de baza a proiectului o reprezinta "file_info" care retine toate informatiile esentiale despre un fisier.
Pentru a putea trimite mesaje ce contin informatii despre fisiere, am creat un tip de date MPI propriu file_info_type
prin functia create_MPI_file_type.


Logica programului :

    download_thread :

        Aceasta rutina citeste fisierul sau de input si populeaza structurile prezentate mai sus. Ii trimite tracker-ului
        fisierele sale folosind MPI_Recv cu tipul "file_info_type".
        Dupa ce primeste confirmare de la Tracker ca poate incepe cautarea, isi verifica campul owned_files define
        structura peer_info in cautarea primului chunk lipsa. 
***     Alegerea peer-ului de la care sa ceara acest chunk este facuta prin intermediul algoritmului de la linia 366. El presupune alegerea unui client care detine acel chunk in mod aleator.
        Odata la 10 pasi, peer-ul ii trimite tracker-ului starea sa actuala de fisiere detinute si ii cere acestuia ultimul sau update al swarm-urilor fisierelor de care este interesat.
        Descarcarea unui fisier se va face secvential, incepand cu chunk-ul 0 si mergand consecutiv pana la final.

    upload_thread :

        Asteapta request-uri din partea clientilor sau shutdown signal din partea tracker-ului.

    tracker :

        Raspunde clientilor la cereri si updateaza swarm_pool-ul de fiecare data cand primeste update-uri din partea acestora.
        Swarm_pool-ul este un vector de vectori de tip client_structs, unde fiecare linie este destinata swarm-ului unui fisier si coloanele sunt populate de informatiile owner-ilor care detin acel fisier.




