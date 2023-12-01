/*

    diskwipe

    GNU/Linux Program for automatisk zero-pass sletting av disker.
    Løsningen er utviklet i C/C++ på Linux Mint 21.1 Cinnamon.

    Løsningen består hovedsaklig av en C/C++ komponent som skriver 
    rapportfiler som igjen blir lest av et JavaScript via en lokal 
    python webserver (server.py). Et bash skript (start.sh) tar seg 
    av oppstart av alle komponenter i samarbeid. Brukere forholder 
    seg kun til webgrensesnittet. 

    blirbehandlet/blirbehandlet.txt - genereres hvert 5 sekund.
        Denne leses for å få samme output som stdout.

    ferdigbehandlet/serienummer.txt - genereres ved ferdig behandling.
        Disse leses for få status til ferdig behandlede disker.
        Slettes automatisk hvis samme disk blir satt inn igjen.

    Bruk
        Kompilerer med g++. (g++ diskliste.cpp -o diskliste)
        Kjør som root/sudo. (cd mappe først hvis fra skript).
        Bekreft gjerne IO ytelse med iotop.
        Bekreft gjerne 0-bytes med wxHexEditor. 
            (Har genialt enkel visuell non-null byte søking.)

    Egenskaper
        Programmet søker sda-z og skriver 0-bytes over alle sektorer.
        Programmet verifiserer alle skrevne bytes mens det skrives.
        I tilfelle disker har pending/realloc problemer.
        Hvis en skriving feiler, vil programmet prøve å fortsette.
        Teorien er at alle sunne områder skal bli overskrevet.
        
    Trygghet

        ZERO-PASS

        En "pass/runde" hvor man skriver 0-bytes over hele diskens område.
        Disker som har mye skrivefeil eller firmware problemer bør heller 
        fysisk destrueres.

        En disk (til og med SSD) som blir komplett overskrevet, vil kun 
        vise HEX0 over hele overflaten og ikke være gjenopprettelig.

        Både HDD og SSD disker vil ha svært fragmenterte bits på overflaten 
        og via wear leveling og TRIM algoritmer er dette enda verre på SSD. 
        En byte er 8 bits, og for å få noe meningsfull data trenger man 
        hundrevis, om ikke tusenvis av bytes med integritet. Den integriteten 
        går tapt i sin helhet ved komplett utnulling av diskers lagerområder.

        Firmaet IBAS ONTRACK AS, som NSM og datatilsynet, anser som ekspert
        instansen på sletting og gjenoppretting av data, har på e-post bekreftet 
        at dersom en SSD har fått filer omgjort til HEX0 i stedet for f.eks. 
        bare RAW eller bare blitt offer for slettet filsystem og/eller volum, 
        er det ikke lenger mulig for dem å få filene opp å gå igjen. 


    (C)2023 All kode utviklet av Dag J Nedrelid <dj@thronic.com>
*/

#include <iostream>
#include <unistd.h>           // lseek/read/write/close().
#include <fcntl.h>            // open/fcntl().
#include <sys/ioctl.h>        // ioctl().
#include <linux/fs.h>         // BLKGETSIZE64 og BLKSSZGET.
#include <vector>
#include <string.h>
#include <thread>
#include <cmath>              // round().
#include "diskfunksjoner.h"

int main(int argc, char** argv)
{
    diskfunksjoner df;

    while (1) {

        // Oppdater diskregister.
        df.FinnDisker();
        std::string visning = "";
        std::string visning_slettet = "";

        for (int a=0; a<df.Disker.size(); a++) {
            
            // Hopp over visning og opprett rapport av disker som er ferdig slettet.
            if (df.Disker.at(a).ferdig_slettet && !df.Disker.at(a).ferdigrapport) {
                
                visning_slettet = u8"("+ df.Disker.at(a).disknavn +") "+ df.Disker.at(a).modell +" ";

                if (df.Disker.at(a).totalsizehuman > 1000.0) 
                    visning_slettet += u8"("+ std::to_string(lround(df.Disker.at(a).totalsizehuman / 1000.0)) +"TB) ";
                else
                    visning_slettet += u8"("+ std::to_string(lround(df.Disker.at(a).totalsizehuman)) +"GB) ";

                visning_slettet += df.Disker.at(a).serienum +"\n"+ 
                "<div style=\"float:left\">Merknader:&nbsp;</div>"+ df.Disker.at(a).merknad +"<div style=\"clear:both\"></div>\n";
                
                try {
                    std::string outpath = "ferdigbehandlet/"+ df.Disker.at(a).serienum +".txt";

                    // Åpne fil for skriving.
                    int fd = open(
                        outpath.c_str(), 
                        O_WRONLY | O_CREAT | O_TRUNC, 
                        S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH
                    );
                    
                    if (fd == -1)
                        throw "Kunne ikke open() ferdigfil.";

                    // Lås fil så den ikke .
                    struct flock f1;
                    f1.l_type = F_WRLCK;
                    f1.l_whence = SEEK_SET;
                    f1.l_start = 0;
                    f1.l_len = 0; // Hele filen.
                    
                    if (fcntl(fd, F_SETLKW, &f1) == -1)
                        throw "Kunne ikke låse ferdigfil for skriving.";

                    if (lseek(fd, 0, SEEK_SET) == -1) {
                        close(fd);
                        throw "Kunne ikke lseek() ferdigfil.";
                    } else {
                        if (write(fd, visning_slettet.c_str(), strlen(visning_slettet.c_str())) == -1) {
                            close(fd);
                            throw "Kunne ikke write() ferdigfil.";   
                        }
                    }

                    // Fjern lås.
                    f1.l_type = F_UNLCK;
                    if (fcntl(fd, F_SETLK, &f1) == -1)
                        close(fd);
                        throw "Kunne ikke fjerne lås på ferdigfil.";

                    close(fd);
                   

                } catch (const char* msg) {
                    std::cout << "\nKunne ikke opprette ferdigrapport:\n" << msg << "\n";
                }

                df.Disker.at(a).ferdigrapport = true;
                continue;
            
            // Hopp ellers over disker som er ferdig rapportert eller blitt tagget som "kanfjernes".
            } else if ((df.Disker.at(a).ferdig_slettet && df.Disker.at(a).ferdigrapport) || df.Disker.at(a).kanfjernes) {

                continue;
            }

            visning += u8""
            "----NESTEDISK----\n"
            "("+ df.Disker.at(a).disknavn +") "+ df.Disker.at(a).modell +" ";

            if (df.Disker.at(a).totalsizehuman > 1000.0) 
                visning += u8"("+ std::to_string(lround(df.Disker.at(a).totalsizehuman / 1000.0)) +"TB) ";
            else
                visning += u8"("+ std::to_string(lround(df.Disker.at(a).totalsizehuman)) +"GB) ";

            visning += df.Disker.at(a).serienum +" "+
            "<b>"+ std::to_string(df.Disker.at(a).slettet_prosent) +"%</b> "+ 
            std::to_string(df.Disker.at(a).KBps) +" KB/s\n"+
            "Merknader: "+ df.Disker.at(a).merknad +"\n\n";

            // Hopp over sletting av sda (la usb brikke stå i).
            if (df.Disker.at(a).disknavn.compare("sda") == 0)
                continue;

            // Start sletting av disk i egen tråd hvis den ikke allerede er under sletting.
            if (!df.Disker.at(a).blir_slettet) {

                // Start slettetråd.
                df.Disker.at(a).blir_slettet = true;
                std::thread _startsletting (
                    &diskfunksjoner::SlettDisk, 
                    df, 
                    df.Disker.at(a).disknavn, 
                    &df.Disker,
                    a
                );
                _startsletting.detach();
            }
        }

        // stdout rapportering.
        system("clear");
        if (visning == "")
            visning = "\nVenter på disker...";
        std::cout << visning;
        std::cout.flush();

        // Opprett behandlingsfil/rapport som kan leses via XHR.
        try {
            // Skriv behandlingsrapport.
            int fd = open(
                    "blirbehandlet/blirbehandlet.txt", 
                    O_WRONLY | O_CREAT | O_TRUNC, 
                    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH
            );

            if (fd == -1)
                throw "Kunne ikke open() behandlingsfil.";

            // Lås fil så python LOCK_EX må vente til skriving er ferdig.
            struct flock f1;
            f1.l_type = F_WRLCK;
            f1.l_whence = SEEK_SET;
            f1.l_start = 0;
            f1.l_len = 0; // Hele filen.
            
            if (fcntl(fd, F_SETLKW, &f1) == -1)
                throw "Kunne ikke låse behandlingsfil for skriving.";

            // Utfør skriving.
            if (lseek(fd, 0, SEEK_SET) == -1) {
                close(fd);
                throw "Kunne ikke lseek() behandlingsfil.";

            } else if (write(fd, visning.c_str(), strlen(visning.c_str())) == -1) {
                close(fd);
                throw "Kunne ikke write() behandlingsfil.";
            }

            // Fjern lås.
            f1.l_type = F_UNLCK;
            if (fcntl(fd, F_SETLK, &f1) == -1) {
                close(fd);
                throw "Kunne ikke fjerne lås på behandlingsfil.";
            }

            close(fd);

        } catch (const char* msg) {
            std::cout << "\nKunne ikke opprette behandlingsrapport:\n" << msg << "\n";
        }

        sleep(5);
    }

    return 0;
}