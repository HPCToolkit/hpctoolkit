var debug; 
	// if defined ==> trace from utils.js collects trace strings
	// otherwise trace becomes noop

var undefined;    
	// never to be set, used to compare 

function reset_global_vars() {
  cur_nav_id = undefined;  
}

var frameWidth = new Object; 
	// frameWidth[frame] = the maximum width known for that frame

var sortMetricName;   
	// the current performance metric of focus

var cur_nav_id;   
	// the currently selected anchor, initially undefined 

var offset_y = new Object; 
	// the offset used when moving a frame's highlighter

var loaded = new Object;   
	// loaded[frame] <=> frame is completely loaded 

var cur_file = new Object; 
	// cur_file[frame] <=> currently (partially) loaded file

var cur_div = new Object; 
	// cur_div[frame] <=> currently (visible) line div

var scopes_loc; 
	// the dir that contains the scopes html file 

var nesting_level = 0; 
	// the depth of the current node in the scope tree

var current_scope_maxflatten = 0; 

// to be set by the program
var global_x_scopes = 0;
var global_x_source = 0;

function flatten_levels()
{
	this[0] = 0;
}
var flatten_level = new flatten_levels();   
	// flatten_level[nesting_level] <=> how many layers of kids 
        //                                 to flatten in the display 
        //                                 at this level

var curr_scope_id; 
	// the id of the current scope 

// prints global state (used by button in buttons.debug.html) 
function globals() {
   ps = '\n'; 
   ps += 'cur_nav_id: ' + cur_nav_id + ";\n"; 
   ps += 'scopes_dir: ' + scopes_loc + '\n'; 
   ps += 'curr_scope_id: ' + curr_scope_id + '\n'; 
   ps += 'cur_file:\n' + props(cur_file); 
   ps += 'cur_div:\n' + props(cur_div); 
   ps += 'loaded:\n' + props(loaded); 
   ps += 'offset_y:\n' + props(offset_y); 
   ps += 'current_scope_maxflatten: ' + current_scope_maxflatten + '\n'; 
   ps += 'nesting_level: ' + nesting_level + ";\n"; 
   ps += 'flatten_level: \n'; 
   for (i= 0; i <= nesting_level; i++) {
      ps += '      ' + flatten_level[i] + '\n'; 
   }
   ps += 'frames: \n'; 
   for (i= 0; i < window.top.frames.length; i++) {
      ps += '      ' + window.top.frames[i].name + 
            ' isTable=' +  window.top.frames[i].isTable;
      ps += '\n'; 
   } 
   ps += 'sortMetricName: ' + sortMetricName + ";\n"; 
   return ps; 
}


function props(e) 
{
   if (e == undefined) {
      return "undefined\n"; 
   } 
   var list = new Object;
   list["object"] = ""; 
   list["function"] = ""; 
   str = ""; 
   prefix = "      "; 
   for (p in e) {
     if ((e[p] != top.traceStr) && (e[p] != str) && (e[p] != list)) { 
      if (e[p] == top.traceStr) {
         // noop
      } else {
	if ((typeof e[p] == "object") || 
	    (typeof e[p] == "function")) {
	  if (e[p] != top.undefined) {
	    list[typeof e[p]] += " " + p; 
	  }
	} else {
	  str += prefix + p + "=" + e[p] + ";\n" ; 
	} 
      } 
     }
   }
   if (list["object"] != "") {
      str += "\n  **objects:." + list["object"]; 
   }
   delete list; 
   return str; 
}

// alert("GLOBALS: " + globals()); 
