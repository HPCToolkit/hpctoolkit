/****************************************************************************
 * 
 * HPCView javascript code
 *
 * Notes:
 *   - uses DOM methods
 *
 ****************************************************************************/

var undefined;  // never assign a value here

var doc_br;
var style_br;

// we rely on HPCView to generate frames and file extension according to 
// the folowing values 

var kids_frame = 'scopes_kids'; 
var self_frame = 'scopes_self'; 
var tab_head_frame = 'tablehead';
var kids_file_ext  = ".kids"; 
var self_file_ext  = ".self"; 

function IE4GetElementById(doc, id)
{
   return doc.all[id];
}

function NN4GetElementById(doc, id)
{
  var i;
  var subdoc;
  var obj;
  
  for (i = 0; i < doc.layers.length; ++i)
  {
    if (doc.layers[i].id && id == doc.layers[i].id)
      return doc.layers[i];
      
    subdoc = doc.layers[i].document;
    obj    = NN4GetElementById(subdoc, id);
    if (obj != null)
      return obj;
  }
  return undefined;
}

if (document.layers)
{
  mgGetElementById = function( doc, id)
  {
     return NN4GetElementById(doc, id);
  }
}
else if (document.all && !document.getElementById)
{
  mgGetElementById = function( doc, id)
  {
     return IE4GetElementById( doc, id);
  }
}
else if (document.getElementById) 
{
  mgGetElementById = function( doc, id)
  {
     return doc.getElementById(id);
  }
} 
else
{
   trace('Browser unsupported');
}
  

function getStyleObject(docid, id)
{
  var elm = null;
  var styleObject = null;

  if (typeof(mgGetElementById) != "undefined")
    elm = mgGetElementById(docid, id);

  if (elm)
  {
     trace('getStyleObject: elm ' + id + ' was found!!\n');
     if (elm.style)
        styleObject = elm.style;
     else if (docid.layers)
        styleObject = elm;
  } else
  {
     trace('getStyleObject: elm ' + id + ' was not found!!\n');
  }

  return styleObject;
}

function getXPosition(fname)
{
   var x=0;
   var myframe = window.top.frames[fname];
   var mydoc = myframe.document;
   if (mydoc.getElementById) {
      if (isNaN(window.screenX)) {
         x=mydoc.body.scrollLeft;
      }
      else {
         x=myframe.pageXOffset;
      }
   }
   else if (document.all) {
      x=mydoc.body.scrollLeft;
   }
   else if (document.layers) {
      x=myframe.pageXOffset;
   }

   return x;
}

function getFrameWidth(fname)
{
   var x=0;
   var myframe = window.top.frames[fname];
   if (typeof(myframe) == "undefined")
      return 0;
      
   var mydoc = myframe.document;
   if (mydoc.getElementById) {
      if (isNaN(window.screenX)) {
         // x=mydoc.body.scrollWidth-mydoc.body.scrollLeft;
          x=mydoc.body.offsetWidth; 
      }
      else {
         x=myframe.innerWidth; //outerWidth;
      }
   }
   else if (document.all) {
     //  x=mydoc.body.scrollWidth-mydoc.body.scrollLeft;
        x=mydoc.body.offsetWidth; 
   }
   else if (document.layers) {
      x=myframe.innerWidth; //outerWidth;
   }

   return x;
}

function updateHighlighterWidth(fname)
{
   if (typeof(getStyleObject) != "undefined")
   {
      styleHL = getStyleObject(window.top.frames[fname].document, highlighter_div_name() );
   }
   if (typeof(styleHL) != "undefined" && styleHL) // && styleHL.width != "undefined")
   {
      if (!document.layers)
         styleHL.width = window.top.frameWidth[fname] - 20;
   }
}

function updateFrameWidth(fname, newWidth)
{
   if (window.top.frameWidth[fname] == undefined || window.top.frameWidth[fname] < newWidth)
   {
      window.top.frameWidth[fname] = newWidth;
      updateHighlighterWidth(fname);
   }
}


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

function cur_div(frame)        { return window.top.cur_div[frame];}
function set_cur_div(frame, d) 
{ 
   // trace("set_cur_div: " + frame + " " + d); 
   window.top.cur_div[frame] = d; 
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

function get_global_x(fname)
{
   if ((fname != "source") && (typeof(window.top.global_x_scopes) != "undefined"))
       return window.top.global_x_scopes;
   if ((fname == "source") && (typeof(window.top.global_x_source) != "undefined"))
       return window.top.global_x_source;
   return 0;
}

function scopes_self_name()      { return 'scopes_self'; }

function highlighter_div_name()  { return 'highlight'; }

function toggler_div_name()  { return 'toggle'; }

function lines_div_name()    { return 'line'; }

function style_text_height()  { return 12; }

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


function get_anchor( fname, anchid )
{
   trace("get_anchor: " + fname + " " + anchid);
   var a;

// this is the old method

   if (document.layers)
   {
      trace("; layers is defined ");
      mydoc = window.top.frames[fname].document;
      if (typeof(cur_div(fname)) != "undefined")
      {
        a = eval('mydoc.layers["' + cur_div(fname) + '"].document.anchors["' 
             + anchid + '"]');
      }
      else
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
/*
   mydoc = window.top.frames[fname].document;
   if (mgGetElementById)
   {
      a = mgGetElementById(mydoc, anchid);
   }
   else
   {
      trace("; ERROR: mgGetElementById is NOT defined; LAME ");
   }
*/
   return a;
}

// next function returns the position of the anchor anchid in the frame
// fname if the anchor is found, or 0 otherwise
// the function is long because it consider both Netscape and IE
// if usebottom is true the function returns the position of bottom part of 
// the line of text on which the anchor exists
// in order to support the loops anchors, if the specified anchor cannot be
// found and there is an "X" inside the anchor name, search for the anchor
// that is a substring from 0 to position(X);
function get_position_anchor(fname, anchid, usebottom)
{
   trace("get_position_anchor: " + fname + " " + anchid + " " + usebottom);
   if (typeof(cur_div(fname)) != "undefined")
   {
      divName = cur_div(fname);
      dPos = divName.lastIndexOf("_");
      if (dPos>0)
         divId = divName.substring(dPos+1, divName.length);
      else
         divId = 0;
//      alert(fname + " has divId " + divId);
      anchid = "l" + divId + "_" + anchid;
   }
   a = get_anchor(fname, anchid);
   if ((typeof(a) == "undefined") && (typeof(anchid) != "undefined")) {
      trace(" original anchor was not found ");
      var newanchid;
      var pos = anchid.lastIndexOf("X");
      if (pos>0)
      {
         newanchid = anchid.substring(0, pos);
      } else
      {
         newanchid = anchid + "Y";
      }
      trace(" new searched anchor is " + newanchid);
      a = get_anchor(fname, newanchid);
      if (pos>0 && typeof(a) == "undefined")
      {
         newanchid = newanchid + "Y";
         trace(" 3rd searched anchor is " + newanchid);
         a = get_anchor(fname, newanchid);
      }
   }
   if (typeof(a) != "undefined") {
      trace(" Anchor was found ");
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
      if (document.getElementById)
      {
         posy = parseInt (a.offsetTop) - style_text_height();  // Netsc 6 needs some adjustment
         if (usebottom)
         {
            posy = posy + style_text_height();
         }
         global_x = parseInt (a.offsetLeft);
      }

      trace(" get_anchor_position found value: " + posy + " ");
      if (typeof(posy) != "undefined")
         return posy;
      else
         return -13;
   } else {
      trace(" anchor was NOT found ");
      return -13;
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

   // don't scroll the unscrollable frame SCOPES_SELF.
   // Otherwise IE scroll it even if it says scrolling=no
   if (fname != scopes_self_name() )
   {
      var posy = get_position_anchor(fname, anchor, false);

      if (posy != -13) {
         if (fname == 'source') {
            posy = Math.max(posy - 20, 0);
         }
      }
      else // -> we didn't find that anchor
         posy = 0;
      // scroll to anchor
      var posx = getXPosition(fname);
      if (posx == undefined)
         posx = 0;
      window.top.frames[fname].scrollTo(posx, posy);
   }
}

function html_location(file, nav_id) 
{
   trace("html_location: " + file + " " + nav_id); 
   
   htmlFile = file + ".html";
//   if (typeof(nav_id) != "undefined") {
//      htmlFile = htmlFile + "#" + nav_id;
//   }
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
   var styleObj = getStyleObject(window.top.frames[fname].document, 
                        object);
   if (styleObj)
   {
      styleObj.visibility = 'visible';
      trace('show styleObj is defined');
   }
   else
   {
      trace('show styleObj is UNdefined');
   }
}

function hide(fname, object) {
   var styleObj = getStyleObject(window.top.frames[fname].document, 
                        object);
   if (styleObj)
   {
      styleObj.visibility = 'hidden';
      trace('hide styleObj is defined');
   }
   else
   {
      trace('hide styleObj is UNdefined');
   }
}

function moveAt( fname, object, posy )
{
   var styleObj = getStyleObject(window.top.frames[fname].document, 
                        object);
   if (styleObj)
   {
      if (document.layers)
      {
         styleObj.top = posy;
         styleObj.left = get_global_x(fname);
      } else
      {
         styleObj.top = posy + 'px';
         styleObj.left = get_global_x(fname) + 'px';
      }
      trace('moveAt styleObj is defined');
   }
   else
   {
      trace('moveAt styleObj is UNdefined');
   }
}

function moveAtXPos( fname, object, posx )
{
   hide(fname, object);
   var styleObj = getStyleObject(window.top.frames[fname].document, 
                        object);
   if (styleObj)
   {
      if (document.layers)
      {
         styleObj.left = posx;
         styleObj.top = 0; //get_global_x(fname);
      } else
      {
         styleObj.left = posx + 'px';
         styleObj.top = 0 + 'px'; //get_global_x(fname) + 'px';
      }
      trace('moveAtXPos styleObj is defined');
   }
   else
   {
      trace('moveAtXPos styleObj is UNdefined');
   }
   show(fname, object);
}

// this function will probably not be used; it was written in case that for
// each frame with an highlighter there was a 'toggle' image to make the
// higlighter appear or disappear
function showhide( fname, object )
{
   var styleObj = getStyleObject(window.top.frames[fname].document, 
                        object);
   if (styleObj)
   {
      is_visible = styleObj.visibility;
      trace('showhide styleObj is defined');
      if (is_visible == "show" || is_visible == "visible")
         hide( fname, highlighter_div_name() );
      if (is_visible == "hide" || is_visible == "hidden")
         show( fname, highlighter_div_name() );
   }
   else
   {
      trace('showhide styleObj is UNdefined');
   }

}

// this simpler function will be used instead to hide all the highlighters
function clear_highlights()
{
   for (ij= 0; ij < window.top.frames.length; ij++) {
      if (is_table(ij) ) { 
         hide( window.top.frames[ij].name, highlighter_div_name() );
      }
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

   // if we have a toggler, move, show or hide it too
   // first detect if we have one
   if (typeof(mgGetElementById) != "undefined")
      mytoggler = mgGetElementById(window.top.frames[fname].document, toggler_div_name() );

// old method
//   mytoggler = eval('window.top.frames["' + fname + '"].' + doc_br +
//              toggler_div_name() );
   trace("- Toggler is " + typeof(mytoggler));

   if (posy != -13) {
      // move highlighter to anchor
      moveAt(fname, highlighter_div_name(), posy);
      if ( typeof(mytoggler) != "undefined" )
         moveAt(fname, toggler_div_name(), posy);

      // make highlighter visible
      show(fname, highlighter_div_name());
      if ( typeof(mytoggler) != "undefined" )
         show(fname, toggler_div_name());
   } else {
      hide(fname, highlighter_div_name());
      if ( typeof(mytoggler) != "undefined" )
         hide(fname, toggler_div_name());
   }
} 

function navigate_and_highlight(frame_name, new_nav_id) { 
   trace("navigate_and_highlight: " + frame_name + " " + new_nav_id); 
   if (window.name != frame_name) {
      // only move the frames that did not trigger this operation 
      scroll_to_anchor(frame_name, new_nav_id);
   }
   highlight_frame(frame_name, new_nav_id); 
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
    set_loaded(window.name, false); 
    if (cur_file(window.name) == undefined) { 
       set_cur_file(window.name, file_name); 
    }
}

function handle_on_load() {
    // it_is_a_table(window.name); 
    set_loaded(window.name, true); 
}


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
    kids_file_name = file_name; // + "." + get_flatten_depth(0);
    scopeGotoFrame(kids_frame, kids_file_ext, kids_file_name, get_flatten_depth(0)); 
    self_file_name = file_name; // + "." + get_flatten_depth(-1);
    scopeGotoFrame(self_frame, self_file_ext, self_file_name, get_flatten_depth(-1)); 
}

function scopeGotoFrame(frame, ext, file_name, flatLevel)
{
    scopes_file = file_name + ext; 
    if (cur_file(frame) != scopes_file) { 
       move_scope_frame(frame, scopes_file, global_nav_id()); 
    } else
    {
      divName = lines_div_name() + "_" + flatLevel;
      if (cur_div(frame) != divName)
      {
         hide(frame, cur_div(frame));
         show(frame, divName);
         set_cur_div(frame, divName);

         moveAtXPos( frame, highlighter_div_name(), 0 );
         if (typeof(getFrameWidth) != "undefined" && window.top.frameWidth[frame] == undefined)
         {
            window.top.frameWidth[frame] = getFrameWidth(frame);
         }
         updateHighlighterWidth(frame);
    
         move_highlighter(frame, global_nav_id());
         scroll_to_anchor(frame, global_nav_id());
//         if (frame == kids_frame)
//            check_scroll_position('loaded');
      }
    }
}

// this function handles the onload event for scopes_self frame
function handle_scope_self_onload(file, scope_id, maxflatten, scope_depth) 
{
   trace("handle_scope_self_onload: " + " " + file);
   it_is_a_table(window.name); 

   window.top.current_scope_maxflatten = maxflatten;
   // window.top.nesting_level = scope_depth;

   divName = lines_div_name() + "_" + get_flatten_depth(-1);
   set_cur_div(window.name, divName);
   show(window.name, divName);

   moveAtXPos( window.name, highlighter_div_name(), 0 );
   if (typeof(getFrameWidth) != "undefined" && window.top.frameWidth[window.name] == undefined)
   {
      window.top.frameWidth[window.name] = getFrameWidth(window.name);
   }
   updateHighlighterWidth(window.name);

   set_cur_file(window.name, file); 
   set_current_scope_id(scope_id);
   highlight_frame(window.name, global_nav_id());
   scroll_to_anchor(window.name, global_nav_id());
}


function check_scroll_position(theEvent)
{
   if (theEvent == 'loaded' || theEvent.which == 1 || theEvent.type == 'mousemove' || theEvent.type == 'scroll')
   {
      var x = getXPosition(window.name);
      var newWidth = x + getFrameWidth(window.name);
//      window.status = 'new Width is ' + newWidth + ' current width is ' + window.top.frameWidth[window.name];
      if (window.top.frameWidth[window.name] == undefined || newWidth > window.top.frameWidth[window.name])
      {
         updateFrameWidth(window.name, newWidth);
         updateFrameWidth(self_frame, newWidth);
      }

      window.top.frames[self_frame].scrollTo(x, 0);
      window.top.frames[tab_head_frame].scrollTo(x, 0);
      return x;
   }
}

function mouse_down_event()
{
   var e = arguments[0];
   if (e.which == 1) { 
      window.captureEvents(Event.MOUSEMOVE); 
      window.onmousemove=check_scroll_position; 
      check_scroll_position(e);
      return true; //false; 
   } 
   else { 
      /*Do any right mouse button processing here*/ 
      return true; 
   } 
} 

function mouse_up_event()
{
   var e = arguments[0];
   if (e.which == 1) { 
      window.releaseEvents(Event.MOUSEMOVE); 
      window.onmousemove=null;
      check_scroll_position(e);
      return true; //false; 
   } 
   else { 
      /*Do any right mouse button processing here*/ 
      return true; 
   } 
} 

function IE_scroll_event()
{
   check_scroll_position(window.event);
}

function handle_scope_frame_onload(file) 
{
    trace("handle_scope_frame_onload: " + " " + file);
    
    if (document.all)
    {
       window.onscroll=IE_scroll_event;
    }
    else
       if (document.getElementById)
       {
          window.captureEvents(Event.MOUSEUP|Event.MOUSEDOWN); 
          window.onmousedown = mouse_down_event;
          window.onmouseup = mouse_up_event;
       }

    set_cur_file(window.name, file); 
    var pos = file.lastIndexOf("/");
    var dir = file.substr(0, pos);
    set_scopes_dir(dir);
    it_is_a_table(window.name) ;
    set_loaded(window.name, true);

    divName = lines_div_name() + "_" + get_flatten_depth(0);
    set_cur_div(window.name, divName);
    show(window.name, divName);
    
    moveAtXPos( window.name, highlighter_div_name(), 0 );
    if (typeof(getFrameWidth) != "undefined" && window.top.frameWidth[window.name] == undefined)
    {
       window.top.frameWidth[window.name] = getFrameWidth(window.name);
    }
    updateHighlighterWidth(window.name);
    
    move_highlighter(window.name, global_nav_id());
    scroll_to_anchor(window.name, global_nav_id());
    check_scroll_position('loaded');
}

function handle_scope_onload(current)
{
    trace("handle_scope_onload: " + " " + current);
    set_loaded(window.name, true);
}

function move_scope_frame(frame_name, file, nav_id)
// change the frame's location, i.e. move it to the given anchor (nav_id) in
// the given file
{
   trace("scope::move_scope_frame: " + frame_name + " " + file + " " + nav_id);
   set_cur_file(frame_name, file);

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
function navframes(nav_id) 
{
   trace("\nnavframes: " + nav_id);
   trace(" NumFrames=" + window.top.frames.length); 
   // navigate and highlight all table frames 
   for (ij= 0; ij < window.top.frames.length; ij++) {
      if (is_table(ij) && window.top.frames[ij].name != 'source') { 
         navigate_and_highlight(window.top.frames[ij].name, nav_id); 
      }
   }
   set_global_nav_id(nav_id); 

   // highlight_frame('scopes_kids', nav_id)
   // source frame 
   file = get_file(nav_id); 
   if (file != cur_file('source')) {
      move_frame('source', file, nav_id); 
   }  else {
      navigate_and_highlight('source', nav_id); 
   }
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
   while (nlevel <= window.top.nesting_level && slevels < scope_depth - 1) {
	slevels += window.top.flatten_level[nlevel] + 1;
	nlevel += 1;
   }
   trace("\n    slevels=" + slevels + " nlevel=" + nlevel + " wtnl="  +
      window.top.nesting_level + " wtf[wtnl]=" +
      window.top.flatten_level[window.top.nesting_level]);
   if (slevels >= scope_depth - 1) {
      window.top.nesting_level = nlevel - 1;
      slevels -= window.top.flatten_level[window.top.nesting_level] + 1;
      window.top.flatten_level[window.top.nesting_level] = 
           (scope_depth - slevels) - 2;
      slevels = scope_depth - 1;
   }
   trace("\n    slevels=" + slevels + " nlevel=" + nlevel + " wtnl="  +
      window.top.nesting_level + " wtf[wtnl]=" +
      window.top.flatten_level[window.top.nesting_level] +
      " scope_depth=" + scope_depth);
   add_unflattened_scopes(scope_depth - slevels);
}

function select_source_location(nav_id, parent_id, scope_depth) 
{
        trace("\nselect_source_line: " + nav_id + " " + parent_id); 
	adjust_depth_and_flattening(scope_depth+1);
        direct_scopesGoto(parent_id); 
	navframes(nav_id);
}

function restart(file)
{
    window.top.location.href=file;
}

// ------------------------------------------------------------------------- //
// END: html functions
// ------------------------------------------------------------------------- //
