REM	test script for MS-DOS  
erase test.out
erase test2.out
erase test3.out
gema -f testpat.dat testin.dat test.out
fc testout.dat test.out
gema -f testpat.dat -i -idchars "-_" -out test2.out test2.dat
fc test2out.dat test2.out
gema -f testpat.dat -filechars ".,:/\\-_" -out test3.out -in - < test3.dat
fc test3out.dat test3.out
