# Tftp-client-server
Byl vytvořen TFTP-server(tftp-server.c) společně s TFTP-klientem(tftp-klient.c),
kde klientovi je umožněno ukládat a stahovat soubor ze serveru, společně s volbou vybrat velikost bloku, kterou bude soubor přenášen. <br>
### Komunikace probíhá následovně:
Komunikace je zahájena klientem, jež zašlě RRQ nebo WRQ paket. <br>
- V případě RRQ paketu, potvrzení komunikace se serverem je poslání DATA paketu.<br>
- V případě WRQ paketu, potvrzení komunikace se serverem je poslánín ACK paket, v opačném případě ERROR packet. <br>
- Každý DATA packet je potvrzen příslušným ACK paketem. <br>
- Komunikace je ukončena v případě, že byl přenesen celý soubor, nebo nastal ERROR a s ním byl zaslán příslušný ERORR packet
## Použití:
Client: tftp-client -h hostname [-p port] [-f filepath] -t dest_filepath <br>
Server: tftp-server [-p port] root_dirpath
## Příklad spuštění:
make <br>
./tftp-server -p 120 -t /home/user/homedir <br>
./tftp-client -h 127.0.0.1 -p 120 -f test -t test <br>
## Omezení:
Chybí timeout a option timeout.
## Seznam souboru:
- tftp-client.c <br>
- tftp-server.c <br>
- README.md <br>
- MakeFile <br>
## Informace o projektu:
- Autor: Matěj Říčný xricny01
- Datum vytvoření: 12.11.2023

