//
//  Egen type for å holde styr på diskfremgang.
//
struct DiskData
{
    std::string disknavn = "";
    std::string serienum = "generisk";
    std::string modell = "generisk";
    unsigned long long totalsize = 0;
    float totalsizehuman = 0.0f;
    int sectorsize = 0;
    bool blir_slettet = false;
    bool ferdig_slettet = false;
    int slettet_prosent = 0;
    std::string merknad = "";
    unsigned long KBps = 0;
    bool ferdigrapport = false;
    bool kanfjernes = false;
};

//
//  Hjelpefunksjon for å registrere diskdata.
//
bool PopulateDiskData(DiskData* DD)
{
    //
    //  diskinfo:
    //
    std::string cmd = ""
        "udevadm info -q property --name=/dev/"+ DD->disknavn +" | grep "
        "-e ID_MODEL= "
        "-e ID_SERIAL_SHORT=";

    FILE *fp = popen(cmd.c_str(), "r");
    if (!fp) {
        std::cout << "Kunne ikke popen udevadm (PopulateDiskData 1).\n";
        return false;
    }

    char strbuf[512] = {0};
    while (fgets(strbuf, 512, fp) != NULL) {

        // Fang opp serienummer.
        if (memcmp(strbuf, "ID_SERIAL_SHORT=", 16) == 0) {
            DD->serienum = strbuf;
            DD->serienum.replace(0, 16, "");
            DD->serienum.pop_back();
            
            // Ikke ha for lange serienumre.
            if (DD->serienum.length() > 50) {
                DD->serienum = DD->serienum.substr(0,50);
                DD->serienum += "...";
            }

            // Slett eksisterende loggfil hvis tidligere behandlet.
            std::string loggfil = "ferdigbehandlet/"+ DD->serienum +".txt";
            std::remove(loggfil.c_str());
            continue;
        }

        // Fang opp modell.
        if (memcmp(strbuf, "ID_MODEL=", 9) == 0) {
            DD->modell = strbuf;
            DD->modell.replace(0, 9, "");
            DD->modell.pop_back();
        }
    }

    pclose(fp);

    //
    //  totalsize:
    //
    std::string diskpath = "/dev/" + DD->disknavn;
    int fd = open(diskpath.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd == -1) {
        std::cout << "Kunne ikke åpne disk sti for henting av totalstørrelse.\n";
        return false;
    }

    if (ioctl(fd, BLKGETSIZE64, &DD->totalsize) == -1) {
        close(fd);
        std::cout << "Kunne ikke hente totalsize (PopulateDiskData 2).\n";
        return false;
    }
    close(fd);

    // Registrer visning for GB i stedet for bytes.
    // Alle disker som skal bli slettet har sannsynligvis 1GB+ kapasitet.
    DD->totalsizehuman = (((float)DD->totalsize / 1000.0) / 1000.0) / 1000.0;

    //
    //  sectorsize:
    //
    diskpath = "/dev/" + DD->disknavn;
    fd = open(diskpath.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd == -1) {
        std::cout << "Kunne ikke åpne disk sti for henting av sektorstørrelse.\n";
        return false;
    }

    if (ioctl(fd, BLKSSZGET, &DD->sectorsize) == -1) {
        close(fd);
        std::cout << "Kunne ikke hente sectorsize (PopulateDiskData 3).\n";
        return false;
    }
    close(fd);
    
    return true;
}

// Hovedklasse for styring av diskorientering.
class diskfunksjoner
{
    private:
    std::string diskbokstaver = "bcdefghijklmnopqrstuvwxyz";
    
    public:
    std::vector<DiskData> Disker;

    public:
    void FinnDisker()
    {
        // Bygg en lsblk grep spørring for sda-z,
        // for å se hvilke disker som er tilstede.
        std::string cmd = "lsblk -f | grep";
        for (int a=0; a<diskbokstaver.length(); a++) 
            cmd += " -we sd" + diskbokstaver.substr(a,1);

        FILE *fp = popen(cmd.c_str(),"r");
        if (fp == NULL) {
            std::cout << "Kunne ikke popen lsblk (FinnDisker).\n";
            sleep(2);
            return;
        }

        // lsblk gir lange linjer.
        char disk[256] = {0};
        std::vector<std::string> lsblk_register;
        while(fgets(disk, 256, fp) != NULL) {
            std::string _s = disk;
            lsblk_register.push_back(_s.substr(0,3));
        }
        pclose(fp);

        // Deaktiver registreringer som ikke lenger er tilstede.
        for (int a=0; a<Disker.size(); a++) {
            bool aktiv = false;
            for (int b=0; b<lsblk_register.size(); b++) {
                if (Disker.at(a).disknavn == lsblk_register.at(b)) {
                    aktiv = true;
                    break;
                }
            }
            if (!aktiv)
                Disker.at(a).disknavn = "Frakoblet";
        }

        // Hent diskdetaljer.
        for (int a=0; a<lsblk_register.size(); a++) {

            DiskData DD;
            DD.disknavn = lsblk_register.at(a);

            // Sjekk om det er en eksisterende disk.
            bool disk_funnet = false;
            for (int b=0; b<Disker.size(); b++) {
                if (Disker.at(b).disknavn == DD.disknavn) {
                    disk_funnet = true;
                    break;
                }
            }

            // Eksisterende, ignorer.
            if (disk_funnet) 
                continue;

            // Ikke registrert fra før, registrerer disk.
            if (PopulateDiskData(&DD)) {

                // Bare registrer hvis det kunne hentes modell og serienummer.
                if (DD.serienum != "generisk" && DD.modell != "generisk")
                    Disker.push_back(DD);

            } else {
                std::cout << "Kunne ikke registrere diskdata. Got root?\n";
                exit(1);
            }
        }
    }

    void SlettDisk(std::string disknavn, std::vector<DiskData>* Disker, int DiskerID)
    {
        // Foreta en lazy detach av evt. montert filsystem.
        std::string cmd = "umount -l /dev/"+ disknavn +" > /dev/null 2>&1";
        system(cmd.c_str());

        // Åpne en kobling til disk.
        std::string devpath = "/dev/"+ disknavn;
        int fd = open(devpath.c_str(), O_RDWR);
        
        // Kobling feilet.
        if (fd == -1) {
            Disker->at(DiskerID).merknad = "Kobling mot disk feilet.";
            Disker->at(DiskerID).kanfjernes = true;
            return;
        }

        // Start overskriving.
        int sectorsize = Disker->at(DiskerID).sectorsize;
        unsigned long long totalbytes = Disker->at(DiskerID).totalsize;
        unsigned long long bytes_written = 0;
        unsigned long long bps = 0;
        int write_num_bytes = 1048576; // 1M om gangen for hastighet.
        unsigned char write_buf[write_num_bytes] = {0x00};
        unsigned char read_buf[write_num_bytes] = {0x00};
        time_t SekundTeller = time(0);
        bool SisteRunde = false;

        // Skriveloop.
        while (bytes_written < totalbytes) {

            // Nedjuster bytes som skal skrives hvis det overskrider kapasitet i siste runde.
            if ((bytes_written + write_num_bytes) >= totalbytes) {
                write_num_bytes = totalbytes - bytes_written;
                SisteRunde = true;
            }

            // Sett startpunkt for skriverunde.
            if (lseek(fd, bytes_written, SEEK_SET) == -1) {
                Disker->at(DiskerID).merknad = "-1";
                SisteRunde = true;
            }

            // Skriv 0-verdier fra skrivebuffer.
            if (write(fd, write_buf, write_num_bytes) == -1) {
                Disker->at(DiskerID).merknad = "-1"; 
                SisteRunde = true;
            }


            //
            //  Les tilbake skrevede bytes for å verifisere 0-verdier.
            //
            if (lseek(fd, bytes_written, SEEK_SET) == -1) {
                Disker->at(DiskerID).merknad = "-1"; 
                SisteRunde = true;
            }

            if (read(fd, read_buf, write_num_bytes) == -1) {
                Disker->at(DiskerID).merknad = "-1"; 
                SisteRunde = true;
            }

            for (int a=0; a<write_num_bytes; a++) {
                if (read_buf[a] != 0x00) {
                    Disker->at(DiskerID).merknad = "-1";
                    SisteRunde = true;
                    break;
                }
            }


            // Oppdater skrevne bytes for neste runde.
            bytes_written += write_num_bytes;

            // Oppdater prosentvisning.
            Disker->at(DiskerID).slettet_prosent = std::round(bytes_written * 100 / totalbytes);

            // Rapporter skrivehastighet, testet mot iotop.
            bps += write_num_bytes;
            if ((time(0) - SekundTeller) >= 1) {
                Disker->at(DiskerID).KBps = (unsigned long)(bps / 1024);
                bps = 0;
                SekundTeller = time(0);
            }

            if (Disker->at(DiskerID).merknad != "-1")
                Disker->at(DiskerID).merknad = "Blanker disk...";
            else
                Disker->at(DiskerID).merknad = "Kunne ikke fullføre skriving/verifisering. <br>"
                                               "Anbefaler fysisk destruksjon hvis den ikke virker skikkelig.";

            // Avslutt slettetråd dersom det er siste antall bytes som har blitt skrevet.
            if (SisteRunde) {
                Disker->at(DiskerID).KBps = 0;
                break;
            }
        }

        // Oppdater verifiseringstatus av ZPOK verdier.
        if (Disker->at(DiskerID).merknad.substr(0,10).compare("Kunne ikke") == 0)
            Disker->at(DiskerID).merknad = "<div class=\"merknader\"><span class=\"slettestatus2\">FUBAR. "+ Disker->at(DiskerID).merknad +"</span></div>";
        else 
            Disker->at(DiskerID).merknad = "<div class=\"merknader\"><span class=\"slettestatus1\">ZPOK! Alle bytes nullet ut (verifisert).</span></div>";
            

        // Registrer disk som ferdig slettet.
        Disker->at(DiskerID).ferdig_slettet = true;
        Disker->at(DiskerID).kanfjernes = true;
        close(fd);
    }
};