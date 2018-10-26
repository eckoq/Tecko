all:
	g++ -O2 -o test test.cpp ./tfc_base_ms_timer.cpp
	g++ -O2 -o test2 test2.cpp ../tfc_base_fast_timer.cpp -I../
	g++ -O2 -o test3 test3.cpp ./AnsyTimerQueue.cpp ./IdxObjMng.cpp ./HashFunc.cpp
	g++ -O2 -o test4 test4.cpp ../tfc_base_fast_timer_str.cpp -I../

clean:
	rm -rf *.o *.so test test2 test3 test4 *.gch

love:
	make clean all
	
