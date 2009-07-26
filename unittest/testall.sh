#!/bin/sh

TESTS="1 2 3 4 5 6"

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
