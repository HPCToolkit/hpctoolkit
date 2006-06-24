function detect_bs() {
    if ( (!document.layers) && (!document.all) && (!document.getElementById) ) {
        alert('You must use either Netscape4.05+, Internet Explorer 4.0+ or Netscape 6+ browser!');
    }
}
