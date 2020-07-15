#!/bin/bash

if [ $# -ne 1 ]; then
  echo "Usage: ${0} <LOG file>"
  exit
fi

exec < $1

IFS=,
divider0=:::::::::::::::::::::
divider20=================
divider=:::::::::::::::::::::::::::::::::::::::
divider2=====================================

while true
do
  read -r tipo_log other
  case "$tipo_log" in
    ( "SUPERMERCATO" )
        #LOG supermercato
        #header
        printf "\n${divider20} SUPERMERCATO ${divider20}\n"
        printf "|${divider0}::${divider0}|\n"
        printf "| %19s | %20s |\n" "" ""
        printf "| %19s | %20s |\n" \
                "TOT CLIENTI SERVITI" "TOT PRODOTTI VENDUTI"
        printf "| %19s | %20s |\n" "" ""
        printf "|${divider0}::${divider0}|\n"
        #record

        read -r tot_cl tot_prodotti
        printf "| %10d%9s   %10d%10s |" \
              $tot_cl "" $tot_prodotti ""
        printf "\n${divider20}==============${divider20}\n\n"
        read -r end other
        if [ "$end" != "END" ]; then
          exit;
        fi
        ;;

    ( "CLIENTI" )
        #LOG clienti
        #header
        printf "\n${divider2} CLIENTI ${divider2}\n"
        printf "|${divider}:${divider}| \n"

        printf "| %-10s | %-10s | %-12s | %-14s | %-19s | \n" "" "" "" "" ""
        printf "| %-10s | %-10s | %-12s | %-14s | %-19s | \n" \
                "ID CLIENTE" "PERMANENZA" "ATTESA CASSA" "CASSE VISITATE" "PRODOTTI ACQUISTATI"
        printf "| %-10s | %10s | %12s | %-14s | %-19s | \n" "" "[sec]" "[sec]" "" ""

        printf "|${divider}:${divider}| \n"

        #record
        while read -r id tempo_perm tempo_att cambi_cassa prodotti
        do
          if [ "$id" == "END" ]; then
            printf "${divider2}=========${divider2}\n"
            break;
          fi

          if [ ""$tempo_att"" == "-1.000" ]; then
            tempo_att="-"
          fi

          printf "| %3s%04d%3s   %10s   %12s   %5s%4d%5s   %7s%5d%7s | \n" \
              "" ${id} ""  ${tempo_perm} ${tempo_att} "" $((${cambi_cassa}+1)) "" "" ${prodotti} ""
        done
        ;;

    ( "CASSIERE" )
        #LOG cassieri
        #header
        printf "\n${divider2} CASSIERI ${divider2}\n"
        printf "|${divider}:${divider}| \n"
        #record
        ;;

    ( * )
      exit;;
  esac
done







