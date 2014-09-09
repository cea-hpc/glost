#!/bin/bash

filter_line_forced="sed -r -e 's/#executed[^:]*:[[:space:]]*//g' "
filter_line=${filter_line_forced}

function PrintHelpAndExit(){
    cat <<EOF
glost_filter.sh \- filter the logs of glost_launch(1).

SYNOPSIS

   glost_filter.sh [ACTION] [PRINTCONTROL] [-R <command_file>] <logfile>[...]

   ACTION      is   [-a|-h|-n|-s <status>]
   PRINTCONTROL is   [-A|-X][-C]

DESCRIPTION

   glost_filter.sh  filter the logs of glost_launch(1).

   By default, it will simply extracts the list of commands executed.
   (equivalent to option -aX)

OPTIONS

  ACTION

  -h    
        print this help and exit.
  -H    
        list all commands and highlight the commands with non-zeros status.
        This is equivalent to -s '[^0][[:digit:]]*[[:space:]].*|$' -C . 
  -l    
        list all commands executed regarless of their status.
  -n    
        only show commands with non zeros status.
        This is equivalent to -s '[^0][[:digit:]]*[[:space:]].*' . 
  -s <status>   
        only show command with status <status>

  PRINTCONTROL

  -A    show all informations the executed command (process, time, status).
  -X    only show the executed command
  -C    colorize the matches.

  OTHER
  
  -R <command_file>
        show the remaining command from <command_file>.
        This only compatible with PRINTCONTROL "-X".

        Warning : 
        By default, this option avoids that a same command is launched multiple times.
        To do that, it will removes all occurences of <command> in <command_file>.

        If you want to avoid this, simply add a specific comment at the end of line :

        before                          after 
        <same_command>                  <same_command> #1
        <same_command>                  <same_command> #2
        
        You can see an example (on "sleep 0.1" and "sleep 0.2" ): 
        $cd examples 
        $../glost_filter.sh -R test_filter.list1 test_filter.log1 

EXAMPLES 

   cd examples
   # list all command executed
   ../glost_filter.sh test_filter.log1 
   # list all command executed , highlight non-zero status
   ../glost_filter.sh -H test_filter.log1 
   # list all command executed with non zeros status
   ../glost_filter.sh -n test_filter.log1 
   # list all command executed with non zeros status, see their status
   ../glost_filter.sh -nA test_filter.log1 
   # list all commands undone regardless of their status
   ../glost_filter.sh -R test_filter.list1 test_filter.log1 
   # list all commands undone or which exited with non zeros status
   ../glost_filter.sh -s 0 -R test_filter.list1 test_filter.log1 

SEE ALSO
glost_launch(1) # cd man ; groff -man -Tascii glost_launch.1 | less
EOF
exit
}

# show_remaining <string> <logfile>[...]
function show_bystatus()
{
    string=$1
    shift
    grep -hE '^#[^:]*:.*$' $* |\
    grep ${GCOLOR} -E "^.*status[[:space:]]${string}.*$"
}

# show_remaining <CMDFILE> <string> <logfile>[...]
function show_remaining(){
    TMPFILE=`mktemp`
    CMDFILE=$1
    string=$2
    shift 2
    show_bystatus ${string} $* | eval ${Pline} > ${TMPFILE}
    grep -vxF -f ${TMPFILE} ${CMDFILE}
    rm ${TMPFILE}
}



# MAIN
Pline_whole="cat"
Pline_command="sed -r -e 's/#executed[^:]*:[[:space:]]*//g' "
Pline=${Pline_command}

CMDFILE=""
string='.*'
while getopts hHlns:AXCR: opt
 do
    case $opt in
	# ACTION
	h) PrintHelpAndExit ;;
	l) string='.*' ;;
	s) string="${OPTARG}" ;;
	H) string='[^0][[:digit:]]*[[:space:]].*|$' 
	   GCOLOR="--color=always" ;;
	n) string='[^0][[:digit:]]*[[:space:]].*'   ;;
	# PRINTCONTROL
	A) Pline=${Pline_whole} ;;
	X) Pline=${Pline_command} ;;
	C) GCOLOR="--color=always";;
	# OTHER
	R) CMDFILE="${OPTARG}" ;;
    esac
done
shift $((OPTIND-1))

[[ "$#" == "0" ]] && PrintHelpAndExit

if [[ "${CMDFILE}" == "" ]] ; then
    show_bystatus ${string} $* | eval ${Pline}
else
    [[ "${Pline}" != "${Pline_command}" ]] && echo "Bad option, -R forces -X" && exit 1
    show_remaining $CMDFILE ${string} $* ;
fi

