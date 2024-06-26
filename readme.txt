structura mesajelor peste TCP:
absolut toate mesajele contin un prexix de fix 4 octeti ce reprezinta lungimea pachetului propriu-zis pe care il preceda
toate mesajele generate de clientii udp incep cu id-ul clientului (max 10 octeti utili + \0)
mesasele generate de clientii udp sunt trimite cu prefixiul pentru lungime si cu un \0 suplimentar la final

trimitere mesaje:
-int recv_all(int sockfd, void *buffer)                     
 se citeste in buffer din sockfd prefixul de lungime(in network order) urmat de mesajul propriu-zis
-int send_all(int sockfd, void *buffer, u_int32_t *len)
 se trimite pe sockfd lungimea care se gaseste la adresa len urmata de exact atati octeti din buffer

clientul tcp:
-pregateste un socket pentru comunicarea cu serverul
-se conecteaza la server
-trimite un prim pachet catre server care contine id-ul sau
-creeaza un poll pentru a primi atat mesaje se la stdin cat si de la server
-cand citeste un mesaj de la stdin, il pregateste pentru expediazerea catre server si afiseaza
 la stdin efectul pe care il are deindata ajuns la server(de abonare sau dezabonare de la un anumit topic/colectie de topicuri)
 iar pentru mesajul "exit" efectul e de inchidere a conexiuni pe partea clientului urmata de inchiderea clinetului in sine
-cand primeste de la server mesajul "exit"(lucru ce se intampla la inchiderea serverul) conexiunea cu serverul si clientul in sine sunt inchise
-cand primeste de la server un mesaj de la clientii udp il interpreteaza in functie de octetul de tip si il afiseaza corespunzator

serverul:
>tine evidenta tuturor clientilor care s-au conectat vreodata la server
 retinand pentru fiecare id-ul si lista de subscriptii(asa cum au fost primite de la client, cu tot cu wildcard-uri)
>tine evidenta clientilor conectati, anume perechea formata din indexul in vectorul tuturor clientilor si file descriptorul conexiunii asociata fiecaruia in parte

-pregateste un socket pentru initierea de conexiuni tcp si un socket pentru comunicararea cu clientii udp
-creeaza un poll cu cei doi file descpriptori si cu file descriptorul pentru stdin
-la sosirea unei cereri de conexiune cu un client tcp, se asteapta venirea mesajului ce contine id-ul clientului
 si se verifica ca respectivul id nu e folosit de alt client conectat in momentul respectiv;
 in caz ca e deja folosit, se trimite un mesaj inapoi catre noul client care il spune ca trebuie sa inchisa conexiunea si sa se inchida, iar conexiunea nou deschisa e inchisa si de catre server
 daca nu, id-ul e marcat ca fiind folosit, iar daca asta se intampla pentru prima oara se creeaza si o intrare in vectorul de clienti 
-la sosirea unui mesaj de la un client udp, e pregatit pentru expediarea catre clientii tcp marcati ca fiind conectati 
 si la care se gaseste macar o subscriptie care sa includa sau sa coincida cu topicul mesajui primit 
-la sosirea unui mesaj de un client tcp care indica dorinta de deconectare, serverul inchide si el conexiunea si elimina din poll file descpriptul asociat ei
-la sosirea unui mesaj de subscribe/unsubscribe venit de la un client tcp, se cauta clientul in vectorul clientilor si se adauga/elimina subscriptia din vectorul de subscriptii asociat lui
-la primirea de la stdin a mesajului "exit", se trimite mesajul tutor clientilor tcp conectati, se inchid toate conexiunile si serverul se inchide
