// We use JavaScript as a way to easily include this header
// information in several HTML pages.

// The 'top' anchor (used by footer_hpctools.js)
document.write('<a name="top"></a>');

// Locate the 'HPCToolkit' Image (height = 71 pixels)
document.write('<img src="images/header.gif">');

// Menu items
document.write('<table width="100%" border="0" cellspacing="0" cellpadding="0">');
document.write('<tr>');
document.write('<td align="left">');
document.write('[ <a href="http://www.hipersoft.rice.edu/hpctoolkit/">HPCToolkit</a> ]');
document.write('</td>');
document.write('<td align="right">');
document.write('[ <a href="index.html">DocRoot</a> |');
document.write('  <a href="hpc_bloop.html">bloop</a> |');
document.write('  <a href="hpcquick.html">hpcquick</a> |');
document.write('  <a href="hpcprof.html">hpcprof</a> |');
document.write('  <a href="hpcrun.html">hpcrun</a> |');
document.write('  <a href="hpcview.html">hpcview</a> |');
document.write('  <a href="hpcviewer.html">hpcviewer</a> |');
document.write('  <a href="hpc_xprof.html">xprof</a> ]');
document.write('</td>');
document.write('</tr>');
document.write('</table>');
document.write('<hr>');

// ***************************************************************************

// Unfortunately we have to support Netscape 4.  The right way to do
// this would be something like:

// Locate the 'HPCToolkit' Image (height = 71 pixels)
//document.write('<img style="position:absolute; top:0px; left:0px" src="images/header.gif">');

// Row 1 of the menu items (over the image)
// document.write('<div style="position:absolute; top:5px; right:15px;">');
// document.write('[ <a href="index.html">Overview</a> |');
// ...
// document.write('</div>');

