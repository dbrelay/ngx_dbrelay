#!/bin/sh

TESTS=`ls test[0-9]*.js | cut -f1 -d'.' | cut -b 5-`

echo
echo Testing unnamed connections
echo "---------------------------"
for TEST in $TESTS
do
  sh unittest.sh test$TEST
done

echo
echo Testing named connections
echo "-------------------------"
for TEST in $TESTS
do
  sh unittest.sh -C test$TEST
done
