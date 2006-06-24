var undefined;  // never assign a value here

var doc_br;
var style_br;

// ------------------------------------------------------------------------- //
// debugging, i.e. tracing 
//    trace appends to global traceStr variable
//    hpcview generates buttons to display/clear this variable and to 
//            turn tracing on in buttons.debug.html which is included by 
//            index.debug.html
// ------------------------------------------------------------------------- //
function alert_globals()        {alert(window.top.globals());}

function trace(str)     { if (window.top.debug > 0) { 
			    window.top.traceStr += ("\n" + str); 
			  }
			}; 
function clearTrace()   { window.top.traceStr = ''; }
function showTrace()    { if (window.top.debug > 0) {
			      alert("TRACE: \n" + window.top.traceStr); 
			  } else {
			      alert("TRACING IS NOT ACTIVATED"); 
			  }
                          window.top.traceStr = ""; 
			} 
function setDebug(i)    { window.top.debug = i; }


// ------------------------------------------------------------------------- //
// START: access functions to global state 
//        i.e. variables declared in global.js 
// ------------------------------------------------------------------------- //
function global_nav_id()        { return window.top.cur_nav_id;}
function set_global_nav_id(nid) { window.top.cur_nav_id = nid; }

function scopes_dir()           { return window.top.scopes_loc;}
function set_scopes_dir(dir)    { window.top.scopes_loc = dir; }

function current_scope_id()           { return window.top.curr_scope_id;}
function set_current_scope_id(id)    { window.top.curr_scope_id = id; }

function cur_file(frame)        { return window.top.cur_file[frame];}
function set_cur_file(frame, f) 
{ 
   // trace("set_cur_file: " + frame + " " + f); 
   window.top.cur_file[frame] = f; 
}

function is_loaded(frame)       
{
   return window.top.loaded[frame] != undefined;
} 

function set_loaded(frame, val) 
{
   // trace("set_loaded: " + frame + " " + val); 
   window.top.loaded[frame] = val;
}

function highlighter_div_name()  { return 'highlight'; }

function toggler_div_name()  { return 'toggle'; }

function lines_div_name()    { return 'line'; }

function style_text_height()  { return 10; }

// This function just return this static defined color. We cannot change the
// color from javascript (because Netscape is lame, in IE would work but we
// have to be portable).
// To change the color, one need to change the variable HIGHLIGHTER_COLOR in 
// one of the HPCView header files, recompile HPCView and rerun it for the
// desired project
function highlight_color()        { return 'yellow'; }; 

// used for what???; comment this function temporarly
// function offset_y(frame)          { return window.top.offset_y[frame]; }

// Same here
/*
function set_offset_y(frame,y)
{
   if (window.top.offset_y[frame] == undefined) {
      window.top.offset_y[frame] = y;
   }
}
*/
// ------------------------------------------------------------------------- //
// END: access functions to global state 
// ------------------------------------------------------------------------- //

// ------------------------------------------------------------------------- //
// table related code 
// ------------------------------------------------------------------------- //
// all tables define the isTable variable in handle_table_onload
//    is_table(fName)  is used to determine how to navigate/highlight a frame 
//
function is_table(fName) 
{
  return window.top.frames[fName].isTable; 
} 

function it_is_a_table(fName) 
{
  window.top.frames[fName].isTable = true; 
} 

function unflatten_scope_kids() 
{
  window.top.flatten_level[window.top.nesting_level] = 
	Math.max(window.top.flatten_level[window.top.nesting_level] - 1, 0);
  direct_scopesGoto(current_scope_id());
} 

function flatten_scope_kids() 
{
  window.top.flatten_level[window.top.nesting_level] = 
	Math.min(window.top.flatten_level[window.top.nesting_level] + 1,
	         window.top.current_scope_maxflatten);
  direct_scopesGoto(current_scope_id());
} 

function is_scope_kids(fName)
{
  return window.top.frames[fName].isChildScopeFrame; 
}

// ------------------------------------------------------------------------- //
 

// ------------------------------------------------------------------------- //
// START: utility functions 
// ------------------------------------------------------------------------- //

//
// function move_frame is implemented twice
// move_frame from utilsForHere.js is used when called from table or src frame 
// move_frame from scopes/utilsForHere.js is used when called from scopes frame
// 

function get_file(nav_id) {
   var pos = nav_id.lastIndexOf("_");
   var filename = nav_id.substr(0, pos);
   trace("get_id_filename: " + nav_id + " -> " + filename);
   return filename;
}

/*
function highlight_layer(frame_name, nav_id, color)
{
   trace("highlight_layer: " + frame_name + " " + nav_id + " " + color);
   layer = parent.frames[frame_name].document.layers[nav_id];
   if (layer !== undefined) {
      layer.bgColor = color;
   } else {
      if (nav_id != undefined) {
	 trace("    no such layer!!"); 
      } 
   }
}
*/

// next function returns the position of the anchor anchid in the frame
// fname if the anchor is found, or 0 otherwise
// the function is long because it consider both Netscape and IE
// if usebottom is true the function returns the position bottom part of the
// line of text on which the anchor exists
function get_position_anchor(fname, anchid, usebottom)
{
   trace("get_position_anchor: " + fname + " " + anchid + " " + usebottom);
   if (document.layers)
   {
      trace("; layers is defined ");
      mydoc = window.top.frames[fname].document;
      a = mydoc.line.document.anchors[anchid];
   } else
   if (document.all)
   {
      trace("; document.all is defined ");
      mydoc = window.top.frames[fname].document;
      a = mydoc.all[anchid];
   } else
   if (document.anchors)
   {
      trace("; document.anchors is defined ");
      mydoc = window.top.frames[fname].document;
      a = mydoc.anchors[anchid];
   } else
   {
      trace("; neither all nor layers nor anchors is defined; LAME ");
   }

   if (typeof(a) != "undefined") {
      trace(" anchor was found ");
      if (document.layers)
      {
         posy = a.y;
         if (usebottom)
            posy = posy + style_text_height();
      }
      else
      if (document.all)
      {
         posy = a.offsetTop;
         if (usebottom)
            posy = posy + style_text_height();
      }
      else
      if (document.anchors)
      {
         // no documentation; don't know what to do
      }

      trace(" get_anchor_position found value: " + posy + " ");
      if (typeof(posy) != "undefined")
         return posy;
      else
         return 0;
   } else {
      trace(" anchor was NOT found ");
      return 0;
   }
}


function scroll_to_anchor(fname, anchor)
// scrolls the frame with name fname to the given anchor 
//     layer == undefined:  the anchor is defined within the frame's document
//     layer != undefined:  the anchor is defined within the given layer 
//                          within the given frame
// 
{
   trace("scroll_to_anchor: " + fname + " " + anchor); 

   var posy = get_position_anchor(fname, anchor, false);

   if (posy != 0) {
      // scroll to anchor
      window.top.frames[fname].scrollTo(0, posy);
   }
   // else -> we didn't find that anchor
}

function html_location(file, nav_id) 
{
   trace("html_location: " + file + " " + nav_id); 
   
   htmlFile = file + ".html";
   if (typeof(nav_id) != "undefined") {
      htmlFile = htmlFile + "#" + nav_id;
   }
   // trace("html_location: " + htmlFile); 
   return htmlFile; 
}

// ------------------------------------------------------------------------- //
// END: utility functions 
// ------------------------------------------------------------------------- //
      
// ------------------------------------------------------------------------- //
// START: highlighting 
// ------------------------------------------------------------------------- //
// table rows are highlighted by moving the highlighter under the 
// currently selected anchor 

function show(fname, object) {
   if (document.layers || document.all)
      eval( 'window.top.frames["' + fname + '"].' + doc_br + object + 
            style_br + '.visibility = "visible"');
}

function hide(fname, object) {
   if (document.layers || document.all)
      eval( 'window.top.frames["' + fname + '"].' + doc_br + object + 
            style_br + '.visibility = "hidden"');
}

function moveAt( fname, object, posy )
{
   if (document.layers || document.all)
   {
      eval( 'window.top.frames["' + fname + '"].' + doc_br + object + 
            style_br + '.top = ' + posy);
   }
}

function showhide( fname, object )
{
   if (document.layers || document.all)
   {
      eval( 'is_visible = window.top.frames["' + fname + '"].' + doc_br + 
             object + style_br + '.visibility');
      trace("\nShowhide: the highliter was: " + is_visible);
      if (is_visible == "show" || is_visible == "visible")
         hide( fname, highlighter_div_name() );
      if (is_visible == "hide" || is_visible == "hidden")
         show( fname, highlighter_div_name() );
   }
}


function highlight_frame(frame_name, nav_id) 
{
   trace("highlight_frame: " + frame_name + " " + nav_id); 
   if (is_table(frame_name)) {
      move_highlighter(frame_name, nav_id);  
			      // nav_id === undefined ===> hide highlighter
   }
}

function move_highlighter(fname, anchor)
// move the frame's highlight layer to the position of the given 
// anchor 
{
   trace("move_highlighter: " + fname + " " + anchor); 
   var posy = get_position_anchor(fname, anchor, false);

   if (posy != 0) {
      // move highlighter to anchor
      moveAt(fname, highlighter_div_name(), posy);
      moveAt(fname, toggler_div_name(), posy);

      // make highlighter visible
      show(fname, highlighter_div_name());
      show(fname, toggler_div_name());
//      show(fname, lines_div_name());
   } else {
      hide(fname, highlighter_div_name());
      hide(fname, toggler_div_name());
   }
} 

function navigate_and_highlight(frame_name, new_nav_id) { 
   trace("navigate_and_highlight: " + frame_name + " " + new_nav_id); 
//   if (window.name != frame_name) {
      // only move the frames that did not trigger this operation 
//      move_frame(frame_name, cur_file(frame_name), new_nav_id); 
//   }
   highlight_frame(frame_name, new_nav_id); 
   scroll_to_anchor(frame_name, new_nav_id);
}
// ------------------------------------------------------------------------- //
// END: highlighting 
// ------------------------------------------------------------------------- //

// ------------------------------------------------------------------------- //
// START: html functions
//        that is functions called from html files generated by hpcview 
// ------------------------------------------------------------------------- //
// unfortunately when resizing the NETSCAPE window
// the table body is repositioned at the top, 
//     instead of leaving it where it (as in the source frame). 
//     or positioning it such the currently selected anchor is visible 
// therefore: we actively reposition so that the currently selected anchor 
//            appears at the top.
function resize_table(fname)
{
  trace("\nresize_table " + fname); 
  scroll_to_anchor(fname, global_nav_id()); 
}

//
// hpcview generates html files such that they call handle_load_start at 
// the beginning and one of the *_on_load funtions at the end of loading 
// an html file's body (onload callback)
// 
// thus cur_file[frame] gets the filename that is loaded and 
//      loaded[frame]   becomes false at the beginning of loading 
//                      stays false while loading and 
//                      becomes true at the end (in one of the on_load fcts) 
// if loading is interrupted reset_all_frames can reload all files 
//
// see also utilsForHere.js and scopes/utilsForHere.js 
//
function handle_load_start(file_name) {
    trace("\nhandle_load_start: " + file_name); 
/*
    set_loaded(window.name, false); 
    if (cur_file(window.name) == undefined) { 
       set_cur_file(window.name, file_name); 
    }
*/
    if (document.layers)
    {
       doc_br = 'document.';
       style_br = '';
    }

    // if browser is MSIE 4.x or 5.x
    if (document.all)
    {
       doc_br = 'document.all.';
       style_br = '.style';
    }

    trace(" Document: " + doc_br + " ; style: " + style_br );
}

function handle_on_load() {
    // it_is_a_table(window.name); 
    set_loaded(window.name, true); 
}

// we rely on HPCView to generate frames and file extension according to 
// the folowing values 

var kids_frame = 'scopes_kids'; 
var self_frame = 'scopes_self'; 
var kids_file_ext  = ".kids"; 
var self_file_ext  = ".self"; 

function get_flatten_depth(offset)
{
	level = window.top.nesting_level + offset;
	if (level < 0) {
	  return 0;
        }
	return window.top.flatten_level[level];
}


function scopesGoto(file_name) 
{
    trace("scopesGoto: file=" + file_name);
    kids_file_name = file_name + "." + get_flatten_depth(0);
    scopeGotoFrame(kids_frame, kids_file_ext, kids_file_name); 
    self_file_name = file_name + "." + get_flatten_depth(-1);
    scopeGotoFrame(self_frame, self_file_ext, self_file_name); 
}

function scopeGotoFrame(frame, ext, file_name)
{
    scopes_file = file_name + ext; 
    if (cur_file(frame) != scopes_file) { 
       move_scope_frame(frame, scopes_file, global_nav_id()); 
    }
}

function handle_scope_self_onload(file, scope_id, maxflatten, scope_depth) 
{
    trace("handle_scope_self_onload: " + " " + file);
    it_is_a_table(window.name); 

    window.top.current_scope_maxflatten = maxflatten;
    // window.top.nesting_level = scope_depth;

    set_cur_file(window.name, file); 
    set_current_scope_id(scope_id);
    highlight_frame(window.name, global_nav_id());
}

function handle_scope_frame_onload(file) 
{
    trace("handle_scope_frame_onload: " + " " + file);

    set_cur_file(window.name, file); 
    var pos = file.lastIndexOf("/");
    var dir = file.substr(0, pos);
    set_scopes_dir(dir);
    it_is_a_table(window.name) ;
    set_loaded(window.name, true);
    move_highlighter(window.name, global_nav_id());
    scroll_to_anchor(window.name, global_nav_id());
}

function handle_scope_onload(current)
{
    trace("handle_scope_onload: " + " " + current);
    set_loaded(window.name, true);
    // scopesGoto(current);
    // move_highlighter(window.name, 'under', global_nav_id(), 'over');
}

function move_scope_frame(frame_name, file, nav_id)
// change the frame's location, i.e. move it to the given anchor (nav_id) in
// the given file
{
   trace("scope::move_scope_frame: " + frame_name + " " + file + " " + nav_id);
   set_cur_file(frame_name, file);
//   if (frame_name.match(/^scopes/)) {
//      var f = file; 
//      var pos = file.lastIndexOf("/");
//      var base = file.substr(pos+1, f.length); 
//      file = base;
//   } else {
     if (frame_name == 'source') {
      file = "../" + file; 
   } 
   trace("    file=" + file); 
   html_file = html_location(file, nav_id); 
   mydoc = window.top.frames[frame_name].document;
   mydoc.location.href = html_file;
}

// highlights layer corresponding to nav_id in the 'source' frame and the 
// row of the PerfTable
// scrolls frames as necessary 
function navigate(nav_id) 
{
   trace("\nnavframes: " + nav_id);
   trace(" NumFrames=" + window.top.frames.length); 
   // navigate and highlight all table frames 
   for (ij= 0; ij < window.top.frames.length; ij++) {
      if (is_table(ij) && window.top.frames[ij].name != 'source') { 
         navigate_and_highlight(window.top.frames[ij].name, nav_id); 
      }
   }
   // highlight_frame('scopes_kids', nav_id)
   // source frame 
/*
   file = get_file(nav_id); 
   if (file != cur_file('source')) {
      move_frame('source', file, nav_id); 
      navigate_and_highlight('source', nav_id); 
   }  else {
      navigate_and_highlight('source', nav_id); 
   }
   set_global_nav_id(nav_id); 
*/
}

function direct_scopesGoto(parent_id) 
{
	var file_name = scopes_dir() + "/" + parent_id + "." + window.top.sortMetricName;  
	scopesGoto(file_name);
}
function add_unflattened_scopes(newlevels) 
{
   trace("\nadd_unflattened_scopes: " + newlevels);
   while(newlevels > 0) {
      window.top.nesting_level = window.top.nesting_level + 1;
      window.top.flatten_level[window.top.nesting_level] = 0;
      newlevels -= 1;
   }
}

function navscopes(file_name, in_out) 
{
   newlevel = window.top.nesting_level + in_out;
   if (window.top.nesting_level < newlevel) {
      window.top.flatten_level[newlevel] = 0;
   }
   window.top.nesting_level = newlevel;
   scopesGoto(file_name);
}

function adjust_depth_and_flattening(scope_depth) 
{
   trace("\nadjust_depth_and_flattening " + scope_depth);
   var nlevel = 0;
   var slevels = 0;
   while (nlevel <= window.top.nesting_level && slevels < scope_depth) {
	slevels += window.top.flatten_level[nlevel] + 1;
	nlevel += 1;
   }
   trace("\n    slevels=" + slevels + " nlevel=" + nlevel + " wtnl="  +
	window.top.nesting_level + " wtf[wtnl]=" +
	window.top.flatten_level[window.top.nesting_level]);
    if (slevels >= scope_depth) {
	window.top.nesting_level = nlevel - 1;
	slevels -= window.top.flatten_level[window.top.nesting_level] + 1;
	window.top.flatten_level[window.top.nesting_level] = 
	  (scope_depth - slevels) - 1;
	slevels = scope_depth;
   }
   add_unflattened_scopes(scope_depth - slevels);
}

function select_source_location(nav_id, parent_id, scope_depth) 
{
        trace("\nselect_source_line: " + nav_id + " " + parent_id); 
	adjust_depth_and_flattening(scope_depth + 1);
        direct_scopesGoto(parent_id); 
	navframes(nav_id);
}

// ------------------------------------------------------------------------- //
// END: html functions
// ------------------------------------------------------------------------- //
