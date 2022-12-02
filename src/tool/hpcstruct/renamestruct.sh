#!/bin/sh

#  Script is invoked with two arguments, $1 = new path needed, $2 = structure-file

#  Find the old name embedded in the structure file

oldname=$(grep "<LM" "$2"  | head -1 | awk '{print $3}' | sed '/n="/s///' | sed '/gpubin\.[0-9a-f]\+/s//gpubin/' | sed '/"/s///')

# echo DEBUG oldname = $oldname
# echo DEBUG newname = $1

#  Check to see if  the two names match
if [ "$oldname" = "$1" ] ; then
    # they match, we're done
    # echo DEBUG Pathname matched
    exit 0
fi

# if they don't match, run a sed script to change the pathname in the structure file
# First, make sure neither name has a pipe character in it
err=$(echo "$oldname" | grep -c "|")
if [ "$err" -ne 0 ]; then
    echo "The original filename in the struct file has an embedded pipe character; not rewriting"
    exit 255
fi

err=$(echo "$1" | grep -c "|")
if [ "$err" -ne 0 ]; then
    echo "The new filename for the struct file has an embedded pipe character; not rewriting"
    exit 255
fi

# echo DEBUG Pathname mismatch found

# The pathname in the structure file is not correct
#  It needs to be fixed
/bin/rm -f "$2".old "$2".new
mv "$2" "$2".old

# echo DEBUG cat $2.old | sed "s|$oldname|$1|" > $2.new
sed "s|$oldname|$1|" "$2".old > "$2"

/bin/rm -f "$2".old

exit 1
