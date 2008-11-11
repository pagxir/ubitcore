#!/bin/sh

echo -n 'd5:nodesl'

for i in `cat $1`;
do
    IP=$(echo -n $i|sed 's/:.*//');
    PORT=$(echo -n $i|sed 's/.*:\([0-9][0-9]*\).*/\1/');
    N=$(echo -n $IP|wc -c);
    echo -n l $N ':' $IP 'i' $PORT 'ee'|sed 's/ //g'
done;

echo -n 'ee'
