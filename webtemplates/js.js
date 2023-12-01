function oppdater_blir_slettet() {
    var xmlHttp = new XMLHttpRequest();
    xmlHttp.open("GET", "/blirbehandlet");
    xmlHttp.onreadystatechange = function() {
        if (xmlHttp.readyState == 4) {

            var resultat = xmlHttp.responseText;
            var disker = resultat.split("----NESTEDISK----<br>");

            var blir_slettet_div = document.getElementById("blir_slettet");
            blir_slettet_div.innerHTML = "<span id=\"aktiv_diskliste_header\">Aktive (sanntid - modell og serienummer.)</span><br><br>";
            for (var a=0; a<disker.length; a++)
                blir_slettet_div.innerHTML += disker[a];

            // Oppdater oppdateringstidspunkt.
            var dato = new Date();
            var d = dato.getDate();
            var m = dato.getMonth();
            var y = dato.getFullYear();
            var ti = dato.getHours();
            var mi = dato.getMinutes();
            var se = dato.getSeconds();
            
            if (parseInt(ti) < 10)
                ti = "0"+ ti;
            if (parseInt(mi) < 10)
                mi = "0"+ mi;
            if (parseInt(se) < 10)
                se = "0"+ se;

            document.getElementById("ui_sist_oppdatert_1").innerHTML = ""+
            "Aktiv sletting sist oppdatert: "+ 
            d +"."+ (m+1) +"."+ y +" "+ ti +":"+ mi +":"+ se;
        }
    }
    xmlHttp.send(null);
    setTimeout(oppdater_blir_slettet, 5000);
}

function oppdater_ferdig_slettet() {
    var xmlHttp = new XMLHttpRequest();
    xmlHttp.open("GET", "/ferdigbehandlet");
    xmlHttp.onreadystatechange = function() {
        if (xmlHttp.readyState == 4) {
            var resultat = xmlHttp.responseText;
            var disker = resultat.split("----NESTEDISK----");

            var ferdig_slettet_div = document.getElementById("ferdig_slettet");
            ferdig_slettet_div.innerHTML = "<span id=\"slettet_diskliste_header\">Ferdige (Nullstilles dersom samme serienummer oppdages på nytt.)</span><br><br>";
            for (var a=0; a<disker.length; a++)
                ferdig_slettet_div.innerHTML += disker[a];

                // Oppdater oppdateringstidspunkt.
                var dato = new Date();
                var d = dato.getDate();
                var m = dato.getMonth();
                var y = dato.getFullYear();
                var ti = dato.getHours();
                var mi = dato.getMinutes();
                var se = dato.getSeconds();
                
                if (parseInt(ti) < 10)
                    ti = "0"+ ti;
                if (parseInt(mi) < 10)
                    mi = "0"+ mi;
                if (parseInt(se) < 10)
                    se = "0"+ se;
    
                document.getElementById("ui_sist_oppdatert_2").innerHTML = ""+
                "Ferdig slettet sist oppdatert: "+ 
                d +"."+ (m+1) +"."+ y +" "+ ti +":"+ mi +":"+ se;
        }
    }
    xmlHttp.send(null);
    setTimeout(oppdater_ferdig_slettet, 5000);
}