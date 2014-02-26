# run:
# sh scripts/test-valgrind.sh [madx]

readonly thedate=`date "+%Y-%m-%d"`
readonly summary="valgrind-summary.txt"
readonly ext="valgrind"
readonly pattern="== Invalid (read|write) of size"

# select madx, check for existence
madx="$1"
[ "$madx" == "" ] && madx="madx" 
[ ! -x "$madx" ] && echo "error: invalid program '$madx' for tests with valgrind" && exit 1

# go to tests directory
cd tests

# clean summary, set the date
echo $thedate > $summary

# run all tests with valgrind
for i in test-*; do
  echo "running test $i (produce $i.$ext)"
  cd $i
  valgrind -v --leak-check=full --track-origins=yes ../../$madx $i.madx > $i.$ext 2>&1
  grep -E "$pattern" $i.$ext /dev/null >> ../$summary
  cd ..
done

cd ..