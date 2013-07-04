#!/bin/bash


# There are two parts to this script:
# 1) Generate tutorial main page sections by substituting code sections into
#    the html templates under $COGL_SRC/tutorials
# 2) Generate final web pages by substituting page sections into a
#    template.html file according to a set of "mustache" markers.
#
#    This script greps for all the "mustache" markers in template.html like
#    {{foo}} and {{bar}} and then for each sub directory (PAGE) under pages/ we
#    make a www/PAGE.html by copying template.html and substituting any
#    pages/DIR/MUSTACHE_MARKER_NAME.html files we find that matches the names
#    we found in the template. Markers without a corresponding file are simply
#    stripped.

if test "$COGL_SRC" = ""; then
  COGL_SRC=.
fi

for tutorial in `find $COGL_SRC/tutorials -maxdepth 1 -mindepth 1 -iname '*.c'`
do
  tutorial=`basename $tutorial`
  tutorial=${tutorial%.c}
  output=pages/$tutorial/main.html

  echo "Generating $output"

  cp tutorials/$tutorial.html $output

  for marker in `grep '{{' $output|sed -e 's/{{\([a-zA-Z_-]\+\)}}/\1/'`
  do
      printf "  substituting %-30s" "{{$marker}}: "
      if csplit -q tutorials/$tutorial.c "%//BEGIN($marker)%1" "/\/\/END($marker)/"; then
        echo "OK"
      else
        echo "FAILED"
      fi
      sed -i -e "/\/\/BEGIN(/ d; /\/\/END(/ d; s/</\&lt;/g" xx00
      sed -i -e "/{{$marker}}/ r xx00" -e "/{{$marker}}/ a </pre>" -e "s/{{$marker}}/<pre name=\"code\" class=\"c\">/g" $output
      rm -f xx*
  done
done

function mustache_substitute_pages()
{
  local pages_dir=$1
  local template=$2
  local output_dir=$3

  for page in `find $pages_dir -maxdepth 1 -mindepth 1 -type d`
  do
    echo "Generating $output_dir/`basename $page`.html"
    cp template.html $output_dir/`basename $page`.html

    for marker in `grep '{{' template.html|sed -e 's/{{\([a-zA-Z_-]\+\)}}/\1/'`
    do
      if test -f $page/$marker.html; then
          echo "  substituting {{$marker}} with $page/$marker.html"
          sed -i -e "/{{$marker}}/ r $page/$marker.html" www/`basename $page`.html
      fi
    done

    # Remove any remaining un-substituted mustache markers.
    sed -i 's/{{\([a-zA-Z_-]\+\)}}//' www/`basename $page`.html
  done
}

mustache_substitute_pages ./pages template.html ./www
