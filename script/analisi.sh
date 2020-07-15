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
        #header e record insieme
        printf "\n${divider20} SUPERMERCATO ${divider20}\n"
        printf "|${divider0}::${divider0}|\n"
        printf "| %19s | %20s |\n" "" ""
        printf "| %19s | %20s |\n" \
                "TOT CLIENTI SERVITI" "TOT PRODOTTI VENDUTI"

        read -r tot_cl tot_prodotti
        printf "| %10d%9s | %10d%10s |" \
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

    ( "CASSIERI" )
        #LOG cassieri
        #header
        printf "\n${divider20}============= CASSIERI ${divider20}=============\n"
        printf "|${divider0}${divider0}:::${divider0}| \n"

        printf "| %-5s | %-9s | %-7s | %-9s | %-11s | %-8s | \n" "" "" "" "" "" ""

        printf "| %1s%-4s | %-9s | %-7s | %-9s | %-11s | %-8s | \n" \
                "" "ID" "PRODOTTI " "CLIENTI" "TOT TEMPO" \
                "TEMPO MEDIO" "NUM"
        printf "| %5s | %-9s | %-7s | %-9s | %-11s | %-8s | \n" \
                "CASSA" "ELABORATI" "SERVITI" "APERTURA" "SERVIZIO" "CHIUSURE"

        printf "| %-5s | %-9s | %-7s | %9s | %11s | %-8s | \n" "" "" "" "[sec]" "[sec]" ""
        printf "|${divider0}${divider0}:::${divider0}| \n"

        #record
        while read -r id prod cli chius lista
        do
          if [ "$id" == "END" ]; then
            printf "${divider20}${divider20}${divider20}${divider20}====\n"
            break;
          fi

          WAY=1
          TEMPO_AP=0
          MEDIA_SERV=0
          #tokenizzo lista
          for word in $lista; do
            if [ "$word" != "LISTA" ]; then
              if [ "$word" == "END" ]; then
                if [ $WAY -eq 1 ]; then
                  WAY=2
                  #tempo totale apertura pronto
                  TEMPO_AP=$(bc -l <<< "scale=3; $TEMPO_AP/1000")
                else
                  MEDIA_SERV=$(bc -l <<< "scale=3; $MEDIA_SERV/($cli * 1000)")
                  break;
                fi
              fi

              if [ $WAY -eq 1 ]; then
                ((TEMPO_AP=TEMPO_AP+word))
              else
                ((MEDIA_SERV=MEDIA_SERV+word))
              fi

            fi
          done

          printf "| %2s%-3d | %-9s | %-7s | %-9s | 0%-10s | %-8s | \n" \
                "" $id  $prod $cli $TEMPO_AP $MEDIA_SERV $chius
        done
        ;;

    ( * )
      exit;;
  esac
done







