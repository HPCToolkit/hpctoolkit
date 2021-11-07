#!/bin/sh -e

git_version='unknown (not a git repo)'

# Scan up to / for a .git directory
while ! test -d .git && ! test "${PWD}" = /; do cd ..; done

if test -d .git ; then
  git_version='unknown (git not available)'

  if command -v git >/dev/null; then
    mesg=`cd "$srcdir" && git log -n 1 --date=short 2>/dev/null`
    if echo "$mesg" | grep -i commit >/dev/null ; then
      git_hash=`echo "$mesg" | grep -i commit | head -1 | awk '{print $2}'`
      git_hash=`expr "$git_hash" : '\(................\)'`
      git_date=`echo "$mesg" | grep -i date | head -1 | awk '{print $2}'`

      mesg=`cd "$srcdir" && git status 2>/dev/null | head -1`
      if echo "$mesg" | grep -i branch >/dev/null ; then
        git_branch=`echo "$mesg" | sed -e 's/^.*branch//i' | awk '{print $1}'`

      elif echo "$mesg" | grep -i detach >/dev/null ; then
        # detached commits don't have a branch name
        git_branch=detached

      else
        git_branch=unknown
      fi

      git_version="$git_branch at $git_date ($git_hash)"
    fi
  fi
fi

echo "$git_version"
