// We use JavaScript as a way to easily include this header
// information in several HTML pages.

// The 'top' anchor (used by footer_hpctools.js)
document.write('<a name="top"></a>');

// Locate the 'HPCToolkit' Image (height = 71 pixels)
document.write('<img style="position:absolute; top:0px; left:0px" src="style/header.gif">');

// Menu
document.write('<div style="position:absolute; top:60px; right:15px; font-family: sans-serif; font-size: small;">');
document.write('\
  [ <a href="index.html">Home</a> \
  | <a href="overview.html">Overview</a> \
  | <a href="publications.html">Publications</a> ] \
  &bull; \
  [ <a href="software.html">Software</a> \
  | <a href="documentation.html">Documentation</a> ] \
  &bull; \
  [ <a href="info-contact.html">Contacts</a> | \
    <a href="info-acks.html">Acks</a> ]');
document.write('</div>');

// document.write('  <a href="info-people.html">People</a> |');

document.write('<div style="position:absolute; top:75px; left:0px;">');
document.write('\
  <table width="100%" border="0" cellspacing="0" cellpadding="0"> \
  <tr><hr></tr> \
  </table>');
document.write('</div>');

document.write('<br> <br> <br> <br>');

// ***************************************************************************

// Old version for Netscape 4.  

//document.write('<table width="100%" border="0" cellspacing="0" cellpadding="0">');
//document.write('<tr>');
//document.write('<td align="right">');
//document.write('[ <a href="index.html">HPCToolkit Home</a> |');
//document.write('  <a href="overview.html">Overview</a> |');
//document.write('  <a href="examples.html">Examples</a> |');
//document.write('  <a href="papers.html">Papers</a> |');
//document.write('  <a href="downloads.html">Downloads</a> |');
//document.write('  <a href="documentation.html">Documentation</a> ]');
//document.write('</td>');
//document.write('</tr>');
//document.write('</table>');
//document.write('<hr>');
