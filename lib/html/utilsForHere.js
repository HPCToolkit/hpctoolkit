function set_table_and_scopes(tableFrame, tableFile, selfFrame, kidsFrame, metric)	 
{
   trace("set_table_and_scopes: " + tableFrame + " " + tableFile + " " 
	 + selfFrame + " " + kidsFrame + " " + metric );
	
   window.top.sortMetricName = metric;
   direct_scopesGoto(current_scope_id());
   var head_file = tableFile + ".head";
   move_frame('tablehead', head_file, undefined); 
}

function move_frame(frame_name, file, nav_id)
// change the frame's location, i.e. move it to the given anchor (nav_id) in
// the given file
{
   trace("move_frame: " + frame_name + " " + file + " " + nav_id);
   set_cur_file(frame_name, file);
   html_file = html_location(file, nav_id); 
   mydoc = window.top.frames[frame_name].document;
   mydoc.location.href = html_file;
}

function handle_src_onload(labelfile) {
    trace("handle_src_onload: " + labelfile); 
    it_is_a_table(window.name);
    move_frame('srclabel', labelfile, undefined); 
    navigate_and_highlight(window.name,  global_nav_id()); 
    scroll_to_anchor(window.name, global_nav_id());
    set_loaded(window.name, true); 
}

// loads the file into the given frame   ('sort' links in perf table header)
function load_frame(fname, filename) 
{
    trace("\nload_frame "  + fname + " " + filename + 
	  " global_nav_id=" + global_nav_id()); 
    if (cur_file(fname) != filename) {
       move_frame(fname, filename, global_nav_id()); 
    }
}

// move source frame to given file   (line in files pane) 
function gotofile(file)                              
{ 
   if (cur_file('source') != file) {
      set_global_nav_id( file ); 
      move_frame('source', file, global_nav_id()); 
      adjust_depth_and_flattening(3);

      direct_scopesGoto(file); 
   }
}

// reloads all html files 
function reset_all_frames()     
{
   trace("reset_all_frames"); 
   window.top.reset_global_vars(); 
   for (p in window.top.cur_file) {
      if ((p != '') && (window.top.cur_file[p] != undefined)) {
         move_frame(p, window.top.cur_file[p], global_nav_id()); 
      }
   }
}

function open_man() 
{
   window.open("manual.html");  
}

