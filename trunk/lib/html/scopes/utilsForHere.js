function move_frame(frame_name, file, nav_id)
// change the frame's location, i.e. move it to the given anchor (nav_id) in
// the given file
{
   trace("scope::move_frame: " + frame_name + " " + file + " " + nav_id);
   set_cur_file(frame_name, file);
   if (frame_name.match(/^scopes/)) {
      var f = file;
      var pos = file.lastIndexOf("/");
      var base = file.substr(pos+1, f.length);
      file = base;
   } else {
      file = "../" + file;
   }
   trace("    file=" + file);
   html_file = html_location(file, nav_id);
   window.top.frames[frame_name].document.location.href = html_file;
}
